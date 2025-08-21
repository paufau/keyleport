#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "networking/p2p/runtime.h"
#include "store.h"

int main(int argc, char* argv[])
{
  store::init();

  // Start P2P runtime independent from scenes
  net::p2p::Runtime::instance().start();

  // Initiate a UIWindow & UIScene and run UI loop
  gui::framework::init_window();
  HomeScene scene;
  gui::framework::set_window_scene(&scene);

  while (gui::framework::window_frame())
  {
    // No-op; frame is driven by window
  }

  //
  // Shutdown P2P runtime after UI closes
  net::p2p::Runtime::instance().stop();
  gui::framework::deinit_window();
}