#pragma once

#include <memory>
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"

namespace flows
{

  // Runs the sender loop: sets up listener, coalesces mouse moves, and sends
  // individual events via UDP/TCP according to type.
  int run_sender(const cli::Options &opt,
                 net::Server &server,
                 keyboard::Keyboard &kb);

} // namespace flows
