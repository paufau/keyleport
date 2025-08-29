#include "sender.h"

#include "keyboard/input_event.h"
#include "networking/udp/app_net.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace flows
{

  bool SenderFlow::start()
  {
    // Ensure a session exists in UDP stack
    if (!net::udp::app_net::instance().has_active_session())
    {
      std::cerr << "[sender] No active session" << std::endl;
      return false;
    }

    // Start loops
    moveAgg_.running.store(true, std::memory_order_relaxed);
    scrollAgg_.running.store(true, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);

    moveThread_ = std::thread(&SenderFlow::move_loop, this);
    scrollThread_ = std::thread(&SenderFlow::scroll_loop, this);
    return true;
  }

  void SenderFlow::stop()
  {
    if (!running_.exchange(false, std::memory_order_relaxed))
    {
      return;
    }
    moveAgg_.running.store(false, std::memory_order_relaxed);
    scrollAgg_.running.store(false, std::memory_order_relaxed);
    if (moveThread_.joinable())
    {
      moveThread_.join();
    }
    if (scrollThread_.joinable())
    {
      scrollThread_.join();
    }
    // nothing specific to disconnect; session lifecycle managed by app_net
  }

  void SenderFlow::push_event(const keyboard::InputEvent& ev)
  {
    if (!running_.load(std::memory_order_relaxed))
    {
      return;
    }
    if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Move)
    {
      moveAgg_.add(ev.dx, ev.dy);
      return;
    }
    if (ev.type == keyboard::InputEvent::Type::Mouse && ev.action == keyboard::InputEvent::Action::Scroll)
    {
      scrollAgg_.add(ev.dx, ev.dy);
      return;
    }
    const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
    // Non-aggregated events: send reliably (ENet reliable channel)
    net::udp::app_net::instance().send_reliable(payload);
  }

  void SenderFlow::push_event(const SDL_Event& sdl_ev)
  {
    const auto ev = keyboard::InputEvent::fromSDL(sdl_ev);
    push_event(ev);
  }

  void SenderFlow::move_loop()
  {
    using namespace std::chrono;
    const int throttle_ms = 8;
    while (moveAgg_.running.load(std::memory_order_relaxed))
    {
      std::this_thread::sleep_for(milliseconds(throttle_ms));
      int dx = 0, dy = 0;
      moveAgg_.take(dx, dy);
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
      net::udp::app_net::instance().send_unreliable(payload);
    }
  }

  void SenderFlow::scroll_loop()
  {
    using namespace std::chrono;
    const int throttle_ms = 8;
    while (scrollAgg_.running.load(std::memory_order_relaxed))
    {
      std::this_thread::sleep_for(milliseconds(throttle_ms));
      int sx = 0, sy = 0;
      scrollAgg_.take(sx, sy);
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
      net::udp::app_net::instance().send_unreliable(payload);
    }
  }

} // namespace flows
