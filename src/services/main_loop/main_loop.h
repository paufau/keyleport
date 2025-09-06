#pragma once

#include "utils/event_emitter/event_emitter.h"

#include <atomic>
#include <thread>

namespace services
{
  class main_loop
  {
  private:
  public:
    main_loop();
    ~main_loop();

    void init();
    void run();
    void cleanup();
    void shutdown();

  private:
    bool running_ = false;
    std::thread services_thread_;
    std::atomic<bool> services_running_{false};
  };
} // namespace services
