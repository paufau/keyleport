#include "sender.h"

#include "keyboard/input_event.h"
#include "move_aggregator.h"
#include "networking/discovery/discovery.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
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
      // Try to discover a server on the local network (async API with blocking wait for first result)
      auto disc = net::discovery::make_discovery();
      std::mutex m;
      std::condition_variable cv;
      bool got = false;
      disc->onDiscovered(
          [&](const entities::ConnectionCandidate& cc)
          {
            std::unique_lock<std::mutex> lk(m);
            if (!got)
            {
              ip = cc.ip();
              got = true;
            }
            lk.unlock();
            cv.notify_one();
          });
      disc->start_discovery(opt.port);
      std::unique_lock<std::mutex> lk(m);
      if (!cv.wait_for(lk, std::chrono::milliseconds(1200), [&] { return got; }))
      {
        disc->stop_discovery();
        std::cerr << "[discovery] No server found on the local network. Provide --ip to specify target." << std::endl;
        return 2;
      }
      disc->stop_discovery();
      std::cerr << "[discovery] Using discovered server: " << ip << ":" << opt.port << std::endl;
    }

    auto s = server.createSender(ip, opt.port);
    auto listener = kb.createListener();
    net::Sender* sender_ptr = s.get();

    // Establish persistent connections (TCP + UDP) once
    int connect_rc = sender_ptr->connect();
    if (connect_rc != 0)
    {
      std::cerr << "[sender] failed to connect to " << ip << ":" << opt.port << ", rc=" << connect_rc << std::endl;
      return connect_rc;
    }

    // Aggregators for mouse move and scroll coalescing
    MoveAggregator moveAgg;
    MoveAggregator scrollAgg;

    // Background sender that flushes aggregated movement every ~3-8 ms via UDP
    std::thread moveSender(
        [&]()
        {
          using namespace std::chrono;
          const int throttle_ms = 8; // current throttle; adjust as needed
          while (moveAgg.running.load(std::memory_order_relaxed))
          {
            std::this_thread::sleep_for(milliseconds(throttle_ms));
            int dx = 0, dy = 0;
            moveAgg.take(dx, dy);
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

    // Background sender that flushes aggregated scroll every ~3-8 ms via UDP
    std::thread scrollSender(
        [&]()
        {
          using namespace std::chrono;
          const int throttle_ms = 8; // keep consistent with move for now
          while (scrollAgg.running.load(std::memory_order_relaxed))
          {
            std::this_thread::sleep_for(milliseconds(throttle_ms));
            int sx = 0, sy = 0;
            scrollAgg.take(sx, sy);
            if (sx == 0 && sy == 0)
            {
              continue;
            }
            keyboard::InputEvent sc{};
            sc.type = keyboard::InputEvent::Type::Mouse;
            sc.action = keyboard::InputEvent::Action::Scroll;
            sc.code = 0;
            sc.dx = sx;
            sc.dy = sy;
            const std::string payload = keyboard::InputEventJSONConverter::encode(sc);
            std::cerr << "[sender] via UDP (coalesced scroll): " << payload << std::endl;
            sender_ptr->send_udp(payload);
          }
        });

    listener->onEvent(
        [sender_ptr, &moveAgg, &scrollAgg](const keyboard::InputEvent& ev)
        {
          // Coalesce mouse Move events
          if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Move)
          {
            moveAgg.add(ev.dx, ev.dy);
            return;
          }
          // Coalesce mouse Scroll events
          if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Scroll)
          {
            scrollAgg.add(ev.dx, ev.dy);
            return;
          }
          // All other events are sent immediately over TCP
          const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
          std::cerr << "[sender] via TCP: " << payload << std::endl;
          sender_ptr->send_tcp(payload);
        });

    int rc = listener->run();
    moveAgg.running.store(false, std::memory_order_relaxed);
    scrollAgg.running.store(false, std::memory_order_relaxed);
    if (moveSender.joinable())
    {
      moveSender.join();
    }
    if (scrollSender.joinable())
    {
      scrollSender.join();
    }
    sender_ptr->disconnect();
    return rc;
  }

} // namespace flows
