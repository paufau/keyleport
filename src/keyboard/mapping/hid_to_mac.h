#pragma once

#if defined(__APPLE__)
#include <CoreGraphics/CGEventTypes.h> // CGKeyCode
#include <cstdint>

namespace keyboard
{
  namespace mapping
  {

    // Translate USB HID (SDL scancode) to macOS virtual keycode (US layout).
    // Returns true when a mapping exists and writes the CGKeyCode to 'vk'.
    bool hid_to_mac_vk(uint16_t hid, CGKeyCode& vk);

  } // namespace mapping
} // namespace keyboard

#endif // __APPLE__
