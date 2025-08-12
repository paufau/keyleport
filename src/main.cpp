#include <iostream>
#include <string>
#include "utils/get_platform/platform.h"
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"
#include "keyboard/input_event.h"
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include "flows/receiver/receiver.h"
#include "flows/sender/sender.h"

int main(int argc, char *argv[])
{
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

  auto server = net::make_server();
  auto kb = keyboard::make_keyboard();

  if (opt.mode == "receiver")
  {
    return flows::run_receiver(opt, *server, *kb);
  }

  return flows::run_sender(opt, *server, *kb);
}