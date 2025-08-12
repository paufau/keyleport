#pragma once

#include <memory>

#include "listener.h"
#include "emitter.h"

namespace keyboard
{

  class Keyboard
  {
  public:
    virtual ~Keyboard() = default;
    virtual std::unique_ptr<Listener> createListener() = 0;
    virtual std::unique_ptr<Emitter> createEmitter() = 0;
  };

  std::unique_ptr<Keyboard> make_keyboard();

} // namespace keyboard
