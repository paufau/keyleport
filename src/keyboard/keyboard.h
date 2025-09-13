#pragma once

#include "emitter.h"

#include <memory>

namespace keyboard
{

  class Keyboard
  {
  public:
    virtual ~Keyboard() = default;
    virtual std::unique_ptr<Emitter> createEmitter() = 0;
  };

  std::unique_ptr<Keyboard> make_keyboard();

} // namespace keyboard
