#include "hid_to_win.h"

namespace keyboard
{
  namespace mapping
  {

    uint16_t hid_to_win_scan(uint16_t hid, bool &extended)
    {
      extended = false;
      switch (hid)
      {
      // Digits (top row)
      case 30:
        return 0x02; // 1
      case 31:
        return 0x03; // 2
      case 32:
        return 0x04; // 3
      case 33:
        return 0x05; // 4
      case 34:
        return 0x06; // 5
      case 35:
        return 0x07; // 6
      case 36:
        return 0x08; // 7
      case 37:
        return 0x09; // 8
      case 38:
        return 0x0A; // 9
      case 39:
        return 0x0B; // 0
      case 45:
        return 0x0C; // -
      case 46:
        return 0x0D; // =
      case 42:
        return 0x0E; // Backspace
      case 43:
        return 0x0F; // Tab

      // Letters
      case 4:
        return 0x1E; // A
      case 5:
        return 0x30; // B
      case 6:
        return 0x2E; // C
      case 7:
        return 0x20; // D
      case 8:
        return 0x12; // E
      case 9:
        return 0x21; // F
      case 10:
        return 0x22; // G
      case 11:
        return 0x23; // H
      case 12:
        return 0x17; // I
      case 13:
        return 0x24; // J
      case 14:
        return 0x25; // K
      case 15:
        return 0x26; // L
      case 16:
        return 0x32; // M
      case 17:
        return 0x31; // N
      case 18:
        return 0x18; // O
      case 19:
        return 0x19; // P
      case 20:
        return 0x10; // Q
      case 21:
        return 0x13; // R
      case 22:
        return 0x1F; // S
      case 23:
        return 0x14; // T
      case 24:
        return 0x16; // U
      case 25:
        return 0x2F; // V
      case 26:
        return 0x11; // W
      case 27:
        return 0x2D; // X
      case 28:
        return 0x15; // Y
      case 29:
        return 0x2C; // Z

      // Brackets, punctuation, backslash, backtick
      case 47:
        return 0x1A; // [
      case 48:
        return 0x1B; // ]
      case 51:
        return 0x27; // ;
      case 52:
        return 0x28; // '
      case 53:
        return 0x29; // `
      case 49:
        return 0x2B; // \
        case 54: return 0x33; // ,
      case 55:
        return 0x34; // .
      case 56:
        return 0x35; // /

      // Controls
      case 40:
        return 0x1C; // Enter
      case 41:
        return 0x01; // Escape
      case 44:
        return 0x39; // Space
      case 57:
        return 0x3A; // CapsLock

      // Modifiers
      case 224:
        return 0x1D; // Left Ctrl
      case 225:
        return 0x2A; // Left Shift
      case 226:
        return 0x38; // Left Alt
      case 227:
        extended = true;
        return 0x5B; // Left GUI (Win)
      case 228:
        extended = true;
        return 0x1D; // Right Ctrl (E0 1D)
      case 229:
        return 0x36; // Right Shift
      case 230:
        extended = true;
        return 0x38; // Right Alt (AltGr)
      case 231:
        extended = true;
        return 0x5C; // Right GUI

      // Navigation (extended)
      case 73:
        extended = true;
        return 0x52; // Insert
      case 76:
        extended = true;
        return 0x53; // Delete
      case 74:
        extended = true;
        return 0x49; // PageUp
      case 78:
        extended = true;
        return 0x51; // PageDown
      case 75:
        extended = true;
        return 0x47; // Home
      case 77:
        extended = true;
        return 0x4F; // End
      case 82:
        extended = true;
        return 0x48; // Up Arrow
      case 81:
        extended = true;
        return 0x50; // Down Arrow
      case 80:
        extended = true;
        return 0x4B; // Left Arrow
      case 79:
        extended = true;
        return 0x4D; // Right Arrow

      // Function keys
      case 58:
        return 0x3B; // F1
      case 59:
        return 0x3C; // F2
      case 60:
        return 0x3D; // F3
      case 61:
        return 0x3E; // F4
      case 62:
        return 0x3F; // F5
      case 63:
        return 0x40; // F6
      case 64:
        return 0x41; // F7
      case 65:
        return 0x42; // F8
      case 66:
        return 0x43; // F9
      case 67:
        return 0x44; // F10
      case 68:
        return 0x57; // F11
      case 69:
        return 0x58; // F12

      default:
        return 0;
      }
    }

  } // namespace mapping
} // namespace keyboard
