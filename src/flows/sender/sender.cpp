#include "sender.h"

#include "keyboard/input_event.h"
#include "move_aggregator.h"
#include "networking/discovery/discovery.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace flows
{

  int run_sender(const cli::Options& opt, net::Server& server, keyboard::Keyboard& kb)
  {
    std::string ip = opt.ip;
    if (ip.empty())
    {
      // Try to discover a server on the local network
      auto results = net::discovery::discover(opt.port, 1200);
      if (!results.empty())
      {
        ip = results.front().ip;
        std::cerr << "[discovery] Using discovered server: " << ip << ":" << opt.port << std::endl;
      }
      else
      {
        std::cerr << "[discovery] No server found on the local network. Provide --ip to specify target." << std::endl;
        return 2;
      }
    }

    auto s = server.createSender(ip, opt.port);
    auto listener = kb.createListener();
    net::Sender* sender_ptr = s.get();

    // Aggregator for mouse move coalescing
    MoveAggregator aggregator;

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
