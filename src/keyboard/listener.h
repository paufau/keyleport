#pragma once

#include <functional>
#include <vector>

#include "input_event.h"

namespace keyboard
{

  using EventHandler = std::function<void(const InputEvent &)>;

  class Listener
  {
  public:
    virtual ~Listener() = default;
    virtual void onEvent(EventHandler handler) = 0;
    virtual int run() = 0;
  };

} // namespace keyboard
