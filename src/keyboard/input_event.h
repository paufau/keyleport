#pragma once

#include <cstdint>

namespace keyboard
{

  struct InputEvent
  {
    enum class Action : uint8_t
    {
      Down = 0,
      Up = 1,
      Move = 2,
      Scroll = 3
    };

    enum class Type : uint8_t
    {
      Key = 0,
      Mouse = 1
    };

    Type type;     // input source
    Action action; // key/mouse action
    uint16_t code; // keycode или кнопка мыши
    int32_t dx;    // relative movement x (for mouse move/scroll)
    int32_t dy;    // relative movement y (for mouse move/scroll)
  };

} // namespace keyboard
