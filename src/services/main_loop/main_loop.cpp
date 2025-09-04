#include "./main_loop.h"

#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"

namespace services
{
  main_loop::main_loop(/* args */)
  {
  }

  main_loop::~main_loop()
  {
  }

  void main_loop::init()
  {
    // TODO store gui reference in service locator
    gui::framework::init_window();
    gui::framework::set_window_scene<HomeScene>();
  }

  void main_loop::run()
  {
    running_ = true;

    while (running_)
    {
      // TODO call shutdown from gui framework when window closes
      if (!gui::framework::window_frame())
      {
        shutdown();
      }
    }
  }

  void main_loop::cleanup()
  {
    gui::framework::deinit_window();
  }

  void main_loop::shutdown()
  {
    running_ = false;
  }

} // namespace services