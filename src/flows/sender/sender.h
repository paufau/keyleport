#pragma once

#include "keyboard/input_event.h"
#include "move_aggregator.h"

#include <atomic>
#include <memory>
#include <thread>

union SDL_Event;

namespace flows
{

  class SenderFlow
  {
  public:
    // Start the sender: connects to the device from store::connection_state and spawns workers.
    // Returns true if started, false if no target or connection failed.
    bool start();
    // Stop background workers and disconnect.
    void stop();
    // Submit an input event to be sent; coalesces move/scroll.
    void push_event(const keyboard::InputEvent& ev);
    void push_event(const SDL_Event& sdl_ev);

  private:
    // Background workers
    void move_loop();
    void scroll_loop();

    // State
    MoveAggregator moveAgg_;
    MoveAggregator scrollAgg_;
    std::thread moveThread_;
    std::thread scrollThread_;
    std::atomic<bool> running_{false};
  };

} // namespace flows
