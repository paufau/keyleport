#include "./main_loop.h"

#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "services/service_locator.h"

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

    // Init each service
    for (const auto& service :
         service_locator::instance().repository.get_services())
    {
      service->init();
    }
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

      // Update each service
      for (const auto& service :
           service_locator::instance().repository.get_services())
      {
        service->update();
      }
    }
  }

  void main_loop::cleanup()
  {
    gui::framework::deinit_window();

    // Cleanup each service
    for (const auto& service :
         service_locator::instance().repository.get_services())
    {
      service->cleanup();
    }
  }

  void main_loop::shutdown()
  {
    running_ = false;
  }

} // namespace services