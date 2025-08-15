#include "receiver.h"

#include "keyboard/input_event.h"
#include "store.h"

#include <iostream>

namespace flows
{

  int run_receiver()
  {
    auto server = net::make_server();
    auto kb = keyboard::make_keyboard();

    // Create keyboard emitter and feed decoded events into it
    auto emitter = kb->createEmitter();
    keyboard::Emitter* emitter_ptr = emitter.get();

    int port = store::connection_state().port.get();
    auto r = server->createReceiver(port);

    r->onReceive(
        [emitter_ptr](const std::string& payload, const std::string& /*remote*/)
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
