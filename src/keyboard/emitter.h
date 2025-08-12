#pragma once

#include "input_event.h"

#include <string>

namespace keyboard
{

  class Emitter
  {
  public:
    virtual ~Emitter() = default;
    virtual int emit(const InputEvent& event) = 0;
  };

} // namespace keyboard
