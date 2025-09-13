#include "hid_to_mac.h"

#if defined(__APPLE__)
namespace keyboard {
  namespace mapping {

    bool hid_to_mac_vk(uint16_t hid, CGKeyCode& vk)
    {
      switch (hid)
      {
      // Letters A-Z (HID 4..29)
      case 4: vk = 0; return true;   // A
      case 22: vk = 1; return true;  // S
      case 7: vk = 2; return true;   // D
      case 9: vk = 3; return true;   // F
      case 11: vk = 4; return true;  // H
      case 10: vk = 5; return true;  // G
      case 29: vk = 6; return true;  // Z
      case 27: vk = 7; return true;  // X
      case 6: vk = 8; return true;   // C
      case 25: vk = 9; return true;  // V
      case 17: vk = 11; return true; // B
      case 20: vk = 12; return true; // Q
      case 26: vk = 13; return true; // W
      case 8: vk = 14; return true;  // E
      case 21: vk = 15; return true; // R
      case 28: vk = 16; return true; // Y
      case 23: vk = 17; return true; // T
      case 30: vk = 18; return true; // 1
      case 31: vk = 19; return true; // 2
      case 32: vk = 20; return true; // 3
      case 33: vk = 21; return true; // 4
      default: break;
      }

      // Digits and symbols top row
      switch (hid)
      {
      case 34: vk = 23; return true; // 5
      case 35: vk = 24; return true; // 6
      case 36: vk = 28; return true; // 7
      case 37: vk = 25; return true; // 8
      case 38: vk = 26; return true; // 9
      case 39: vk = 29; return true; // 0
      case 45: vk = 27; return true; // -
      case 46: vk = 24; return true; // =
      case 42: vk = 51; return true; // Backspace (Delete)
      case 43: vk = 48; return true; // Tab
      default: break;
      }

      // Remaining letters and punctuation
      switch (hid)
      {
      case 18: vk = 34; return true; // O
      case 19: vk = 35; return true; // P
      case 12: vk = 31; return true; // I
      case 24: vk = 17; return true; // U
      case 13: vk = 38; return true; // J
      case 14: vk = 40; return true; // K
      case 15: vk = 37; return true; // L
      case 51: vk = 41; return true; // ;
      case 52: vk = 39; return true; // '
      case 53: vk = 50; return true; // `
      case 49: vk = 42; return true; // backslash
      case 48: vk = 30; return true; // ]
      case 47: vk = 33; return true; // [
      case 54: vk = 43; return true; // ,
      case 55: vk = 47; return true; // .
      case 56: vk = 44; return true; // /
      case 44: vk = 49; return true; // Space
      default: break;
      }

      // Controls & navigation
      switch (hid)
      {
      case 40: vk = 36; return true; // Return
      case 41: vk = 53; return true; // Escape
      case 57: vk = 57; return true; // CapsLock
      case 224: vk = 59; return true; // Left Ctrl
      case 225: vk = 56; return true; // Left Shift
      case 226: vk = 58; return true; // Left Option
      case 227: vk = 55; return true; // Left Command
      case 228: vk = 62; return true; // Right Ctrl
      case 229: vk = 60; return true; // Right Shift
      case 230: vk = 61; return true; // Right Option
      case 231: vk = 54; return true; // Right Command
      case 79: vk = 124; return true; // Right Arrow
      case 80: vk = 123; return true; // Left Arrow
      case 81: vk = 125; return true; // Down Arrow
      case 82: vk = 126; return true; // Up Arrow
      case 75: vk = 115; return true; // Home
      case 77: vk = 119; return true; // End
      case 74: vk = 116; return true; // PageUp
      case 78: vk = 121; return true; // PageDown
      case 76: vk = 117; return true; // Forward Delete
      default: break;
      }

      // Function keys F1..F12 (HID 58..69)
      switch (hid)
      {
      case 58: vk = 122; return true; // F1
      case 59: vk = 120; return true; // F2
      case 60: vk = 99; return true;  // F3
      case 61: vk = 118; return true; // F4
      case 62: vk = 96; return true;  // F5
      case 63: vk = 97; return true;  // F6
      case 64: vk = 98; return true;  // F7
      case 65: vk = 100; return true; // F8
      case 66: vk = 101; return true; // F9
      case 67: vk = 109; return true; // F10
      case 68: vk = 103; return true; // F11
      case 69: vk = 111; return true; // F12
      default:
        break;
      }

      return false;
    }

  } // namespace mapping
} // namespace keyboard

#endif // __APPLE__
