#include "receiver.h"

#include "keyboard/input_event.h"

#include <iostream>

namespace flows
{

  int run_receiver(const cli::Options& opt, net::Server& server, keyboard::Keyboard& kb)
  {
    // Create keyboard emitter and feed decoded events into it
    auto emitter = kb.createEmitter();
    keyboard::Emitter* emitter_ptr = emitter.get();

    auto r = server.createReceiver(opt.port);
    r->onReceive(
        [emitter_ptr](const std::string& payload)
        {
          if (payload.empty())
          {
            return;
          }
          keyboard::InputEvent ev = keyboard::InputEventJSONConverter::decode(payload);
          emitter_ptr->emit(ev);
        });
    return r->run();
  }

} // namespace flows
