#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "store.h"

int main(int argc, char* argv[])
{
  store::init();

  // Initiate a UIWindow & UIScene and run UI loop
  gui::framework::init_window();
  HomeScene scene;
  gui::framework::set_window_scene(&scene);

  while (gui::framework::window_frame())
  {
    // No-op; frame is driven by window
  }

  //

  gui::framework::deinit_window();
}