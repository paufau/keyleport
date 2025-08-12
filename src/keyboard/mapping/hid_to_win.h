#pragma once

#include <cstdint>

namespace keyboard
{
  namespace mapping
  {

    // Translate USB HID (SDL scancode) to Windows Set 1 scancode.
    // Returns 0 if unsupported. Sets 'extended' for E0-prefixed keys.
    uint16_t hid_to_win_scan(uint16_t hid, bool &extended);

  } // namespace mapping
} // namespace keyboard
