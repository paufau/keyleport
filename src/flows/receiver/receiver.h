#pragma once

#include <memory>
#include <string>
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"

namespace flows
{

  // Runs the receiver loop: creates a keyboard emitter, wires up the server receiver,
  // decodes single-event JSON, emits to OS, and enters the blocking run loop.
  // Returns the exit code from the underlying receiver run().
  int run_receiver(const cli::Options &opt,
                   net::Server &server,
                   keyboard::Keyboard &kb);

} // namespace flows
