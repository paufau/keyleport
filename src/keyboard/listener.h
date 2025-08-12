#pragma once

#include "input_event.h"

#include <functional>
#include <vector>

namespace keyboard
{

  using EventHandler = std::function<void(const InputEvent&)>;

  class Listener
  {
  public:
    virtual ~Listener() = default;
    virtual void onEvent(EventHandler handler) = 0;
    virtual int  run() = 0;
  };

} // namespace keyboard
