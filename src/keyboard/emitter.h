#pragma once

#include <string>
#include "input_event.h"

namespace keyboard
{

  class Emitter
  {
  public:
    virtual ~Emitter() = default;
    virtual int emit(const InputEvent &event) = 0;
  };

} // namespace keyboard
