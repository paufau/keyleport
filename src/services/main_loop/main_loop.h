#pragma once

#include "utils/event_emitter/event_emitter.h"

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
  };
} // namespace services
