#include "flows/receiver/receiver.h"
#include "flows/sender/sender.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/my_custom_scene.h"
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

  cli::Options opt = cli::parse(argc, argv);

  if (opt.help)
  {
    cli::print_usage(argv[0]);
    return 0;
  }

  if (!opt.valid)
  {
    std::cout << opt.error << std::endl;
    cli::print_usage(argv[0]);
    return 1;
  }

  // Create and start the discovery process
  auto disc = net::discovery::make_discovery();
  disc->start_discovery(opt.port);

  // When discovered add to available connection state
  disc->onDiscovered(
      [&](const entities::ConnectionCandidate& cc)
      {
        std::cerr << "[discovery] Discovered server: " << cc.ip() << ":" << cc.port() << std::endl;
        store::connection_state().available_devices.get().push_back(cc);
      });

  // Initiate a UIWindow & UIScene and run UI loop
  gui::framework::init_window();
  MyCustomScene scene;
  gui::framework::set_window_scene(&scene);
  while (gui::framework::window_frame())
  {
    // No-op; frame is driven by window
  }
  gui::framework::deinit_window();

  // auto server = net::make_server();
  // auto kb = keyboard::make_keyboard();

  // if (opt.mode == "receiver")
  // {
  //   return flows::run_receiver(opt, *server, *kb);
  // }

  // return flows::run_sender(opt, *server, *kb);
}