#pragma once

namespace services
{
  class main_loop
  {
  private:
    
  public:
    main_loop(/* args */);
    ~main_loop();

  void init();
  void run();
  void cleanup();
  void shutdown();

  private:
    bool running_ = false;
  };
} // namespace main_loop
