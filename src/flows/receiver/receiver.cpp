#include "receiver.h"

#include "keyboard/input_event.h"

#include <iostream>

namespace flows
{

  int run_receiver(const cli::Options& opt, net::Server& server, keyboard::Keyboard& kb)
  {
    // Create keyboard emitter and feed decoded events into it
    auto               emitter = kb.createEmitter();
    keyboard::Emitter* emitter_ptr = emitter.get();

    auto r = server.createReceiver(opt.port);
    r->onReceive(
        [emitter_ptr](const std::string& payload)
        {
          if (payload.empty())
          {
            return;
          }
          // Debug: show received payload
          std::cerr << "[receiver] payload: " << payload << std::endl;
          keyboard::InputEvent ev = keyboard::InputEventJSONConverter::decode(payload);
          std::cerr << "[receiver] decoded type=" << static_cast<int>(ev.type)
                    << " action=" << static_cast<int>(ev.action) << " code=" << ev.code << " dx=" << ev.dx
                    << " dy=" << ev.dy << std::endl;
          emitter_ptr->emit(ev);
        });
    return r->run();
  }

} // namespace flows
