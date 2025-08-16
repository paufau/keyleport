#include "flows/receiver/receiver.h"
#include "flows/sender/sender.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/home/home_scene.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "keyboard/input_event.h"
#include "keyboard/keyboard.h"
#include "networking/discovery/discovery.h"
#include "networking/server/server.h"
#include "store.h"
#include "utils/cli/args.h"
#include "utils/get_platform/platform.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

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

  gui::framework::deinit_window();
}