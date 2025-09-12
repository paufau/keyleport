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
    // Start background services update loop
    services_running_ = true;
    services_thread_ = std::thread(
        []()
        {
          using namespace std::chrono_literals;
          auto& repo = service_locator::instance().repository;
          while (
              service_locator::instance().main_loop->services_running_.load())
          {
            // Snapshot to avoid holding locks during update()
            auto snapshot = repo.get_services_snapshot();
            for (const auto& svc : snapshot)
            {
              if (svc)
              {
                svc->update();
              }
            }
            std::this_thread::sleep_for(1ms);
          }
        });

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
    // Stop services thread
    if (services_running_.exchange(false))
    {
      if (services_thread_.joinable())
      {
        services_thread_.join();
      }
    }

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