#include "sender.h"

#include "keyboard/input_event.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace flows
{

  int run_sender(const cli::Options& opt, net::Server& server, keyboard::Keyboard& kb)
  {
    auto s = server.createSender(opt.ip, opt.port);
    auto listener = kb.createListener();
    net::Sender* sender_ptr = s.get();

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
        {
          return;
        }
        std::lock_guard<std::mutex> lock(m);
        agg_dx += dx;
        agg_dy += dy;
      }
      void take(int& dx, int& dy)
      {
        std::lock_guard<std::mutex> lock(m);
        dx = agg_dx;
        dy = agg_dy;
        agg_dx = 0;
        agg_dy = 0;
      }
    } aggregator;

    // Background sender that flushes aggregated movement every ~3-8 ms via UDP
    std::thread moveSender(
        [&]()
        {
          using namespace std::chrono;
          const int throttle_ms = 8; // current throttle; adjust as needed
          while (aggregator.running.load(std::memory_order_relaxed))
          {
            std::this_thread::sleep_for(milliseconds(throttle_ms));
            int dx = 0, dy = 0;
            aggregator.take(dx, dy);
            if (dx == 0 && dy == 0)
            {
              continue;
            }
            keyboard::InputEvent mv{};
            mv.type = keyboard::InputEvent::Type::Mouse;
            mv.action = keyboard::InputEvent::Action::Move;
            mv.code = 0;
            mv.dx = dx;
            mv.dy = dy;
            const std::string payload = keyboard::InputEventJSONConverter::encode(mv);
            std::cerr << "[sender] via UDP (coalesced move): " << payload << std::endl;
            sender_ptr->send_udp(payload);
          }
        });

    listener->onEvent(
        [sender_ptr, &aggregator](const keyboard::InputEvent& ev)
        {
          // Coalesce mouse Move events
          if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Move)
          {
            aggregator.add(ev.dx, ev.dy);
            return;
          }
          // All other events are sent immediately (Scroll over UDP, rest over TCP)
          const bool use_udp =
              (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Scroll);
          const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
          std::cerr << "[sender] via " << (use_udp ? "UDP" : "TCP") << ": " << payload << std::endl;
          if (use_udp)
          {
            sender_ptr->send_udp(payload);
          }
          else
          {
            sender_ptr->send_tcp(payload);
          }
        });

    int rc = listener->run();
    aggregator.running.store(false, std::memory_order_relaxed);
    if (moveSender.joinable())
    {
      moveSender.join();
    }
    return rc;
  }

} // namespace flows
