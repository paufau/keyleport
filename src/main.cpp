#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "networking/udp/app_net.h"
#include "store.h"

int main(int argc, char* argv[])
{
  store::init();

  // Start UDP networking
  net::udp::app_net::instance().start();

  // Initiate a UIWindow & UIScene and run UI loop
  gui::framework::init_window();
  HomeScene scene;
  gui::framework::set_window_scene(&scene);

  while (gui::framework::window_frame())
  {
    // No-op; frame is driven by window
  }

  //
  // Shutdown networking after UI closes
  net::udp::app_net::instance().stop();
  gui::framework::deinit_window();
}