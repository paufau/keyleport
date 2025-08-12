#include <iostream>
#include <string>
#include "utils/get_platform/platform.h"
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"
#include "keyboard/input_event.h"
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

int main(int argc, char *argv[])
{
  // Parse command-line arguments
  cli::Options opt = cli::parse(argc, argv);

  if (opt.help)
  {
    cli::print_usage(argv[0]);
    return 0;
  }

  if (!opt.valid)
  {
    std::cout << opt.error << std::endl;
    cli::print_usage(argv[0]);
    return 1;
  }

  auto server = net::make_server();
  auto kb = keyboard::make_keyboard();

  if (opt.mode == "receiver")
  {
    // Create keyboard emitter and feed decoded events into it

    auto emitter = kb->createEmitter();
    keyboard::Emitter *emitter_ptr = emitter.get();

    auto r = server->createReceiver(opt.port);
    r->onReceive([emitter_ptr](const std::string &payload)
                 {
  if (payload.empty()) return; 
  // Debug: show received payload
  std::cerr << "[receiver] payload: " << payload << std::endl;
    keyboard::InputEvent ev = keyboard::InputEventJSONConverter::decode(payload);
    std::cerr << "[receiver] decoded type=" << static_cast<int>(ev.type)
              << " action=" << static_cast<int>(ev.action)
              << " code=" << ev.code
              << " dx=" << ev.dx
              << " dy=" << ev.dy << std::endl;
    emitter_ptr->emit(ev); });
    return r->run();
  }
  if (opt.ip.empty())
    return 2;
  auto s = server->createSender(opt.ip, opt.port);

  auto listener = kb->createListener();

  net::Sender *sender_ptr = s.get();

  // Aggregator for mouse move coalescing
  struct MoveAggregator
  {
    std::atomic<bool> running{true};
    std::mutex m;
    int agg_dx{0};
    int agg_dy{0};
    void add(int dx, int dy)
    {
      if (dx == 0 && dy == 0)
        return;
      std::lock_guard<std::mutex> lock(m);
      agg_dx += dx;
      agg_dy += dy;
    }
    void take(int &dx, int &dy)
    {
      std::lock_guard<std::mutex> lock(m);
      dx = agg_dx;
      dy = agg_dy;
      agg_dx = 0;
      agg_dy = 0;
    }
  } aggregator;

  // Background sender that flushes aggregated movement every 3 ms via UDP
  std::thread moveSender([&]()
                         {
    int throttle = 8;

    using namespace std::chrono;
    while (aggregator.running.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(milliseconds(throttle));
      int dx = 0, dy = 0;
      aggregator.take(dx, dy);
      if (dx == 0 && dy == 0) continue;
      keyboard::InputEvent mv{};
      mv.type = keyboard::InputEvent::Type::Mouse;
      mv.action = keyboard::InputEvent::Action::Move;
      mv.code = 0;
      mv.dx = dx;
      mv.dy = dy;
      const std::string payload = keyboard::InputEventJSONConverter::encode(mv);
      std::cerr << "[sender] via UDP (coalesced move): " << payload << std::endl;
      sender_ptr->send_udp(payload);
    } });

  listener->onEvent([sender_ptr, &aggregator](const keyboard::InputEvent &ev)
                    {
    // Coalesce mouse Move events: accumulate and send on a 3 ms cadence via background thread
    if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Move) {
      aggregator.add(ev.dx, ev.dy);
      return;
    }

    // All other events are sent immediately (Scroll over UDP, rest over TCP)
    const bool use_udp = (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Scroll);
    const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
    std::cerr << "[sender] via " << (use_udp ? "UDP" : "TCP") << ": " << payload << std::endl;
    if (use_udp) {
      sender_ptr->send_udp(payload);
    } else {
      sender_ptr->send_tcp(payload);
    } });

  int rc = listener->run();
  aggregator.running.store(false, std::memory_order_relaxed);
  if (moveSender.joinable())
    moveSender.join();
  return rc;
}