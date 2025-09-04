#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "networking/udp/app_net.h"
#include "services/main_loop/main_loop.h"
#include "services/service_locator.h"
#include "store.h"

int main(int argc, char* argv[])
{
  store::init();

  // Start UDP networking
  net::udp::app_net::instance().start();

  services::main_loop main_loop{};
  services::service_locator::instance().main_loop =
      std::make_unique<services::main_loop>(main_loop);

  main_loop.init();
  main_loop.run();
  main_loop.cleanup();

  // Shutdown networking after UI closes
  net::udp::app_net::instance().stop();
}