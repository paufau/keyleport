#pragma once

#include <cstdint>

namespace keyboard
{

  struct InputEvent
  {
    uint8_t type;   // 0 = key, 1 = mouse
    uint8_t action; // 0 = down, 1 = up, 2 = move, 3 = scroll
    uint16_t code;  // keycode или кнопка мыши
    int32_t x;      // для мыши
    int32_t y;      // для мыши
    int32_t delta;  // для скролла
  };

} // namespace keyboard
