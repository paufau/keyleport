#include "sender.h"

#include "keyboard/input_event.h"
#include "services/communication/communication_service.h"
#include "services/communication/packages/keyboard_input_package.h"
#include "services/service_locator.h"
#include "store.h"

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
    communication_service_ =
        services::service_locator::instance()
            .repository.get_service<services::communication_service>();

    if (!communication_service_)
    {
      std::cerr << "[sender] Unable to start: communication_service not found"
                << std::endl;
      return false;
    }

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
    communication_service_ = nullptr;
  }

  void SenderFlow::push_event(const keyboard::InputEvent& ev)
  {
    if (!running_.load(std::memory_order_relaxed))
    {
      return;
    }
    if (ev.type == keyboard::InputEvent::Type::Mouse &&
        ev.action == keyboard::InputEvent::Action::Move)
    {
      moveAgg_.add(ev.dx, ev.dy);
      return;
    }
    if (ev.type == keyboard::InputEvent::Type::Mouse &&
        ev.action == keyboard::InputEvent::Action::Scroll)
    {
      scrollAgg_.add(ev.dx, ev.dy);
      return;
    }

    if (communication_service_)
    {
      communication_service_->send_package_reliable(
          services::keyboard_input_package::build(ev));
    }
    else
    {
      std::cerr
          << "[sender] Unable to send event: communication_service_ is null"
          << std::endl;
    }
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

      if (communication_service_)
      {
        communication_service_->send_package_unreliable(
            services::keyboard_input_package::build(mv));
      }
      else
      {
        std::cerr
            << "[sender] Unable to send move: communication_service_ is null"
            << std::endl;
      }
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

      if (communication_service_)
      {
        communication_service_->send_package_unreliable(
            services::keyboard_input_package::build(sc));
      }
      else
      {
        std::cerr
            << "[sender] Unable to send scroll: communication_service_ is null"
            << std::endl;
      }
    }
  }

} // namespace flows
