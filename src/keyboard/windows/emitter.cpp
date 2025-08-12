#ifdef _WIN32
#include "../emitter.h"

#include <Windows.h>
#include <memory>
#include <string>
// Shared HID → Windows scancode mapping
#include "../mapping/hid_to_win.h"

namespace keyboard
{

  class WindowsEmitter : public Emitter
  {
  public:
    int emit(const InputEvent& event) override
    {
      switch (event.type)
      {
      case InputEvent::Type::Key: // keyboard
        return emitKey(event.code, event.action == InputEvent::Action::Down);
      case InputEvent::Type::Mouse: // mouse
        switch (event.action)
        {
        case InputEvent::Action::Move:
          return emitMouseMove(event.dx, event.dy);
        case InputEvent::Action::Scroll:
          // Use dy for vertical, dx for horizontal
          if (event.dy != 0)
          {
            return emitMouseWheel(event.dy);
          }
          if (event.dx != 0)
          {
            return emitMouseHWheel(event.dx);
          }
          return 0;
        case InputEvent::Action::Down:
          return emitMouseButton(event.code, true);
        case InputEvent::Action::Up:
          return emitMouseButton(event.code, false);
        default:
          return 0;
        }
      default:
        return 0;
      }
    }

  private:
    static int sendInput(const INPUT& in)
    {
      INPUT tmp = in;
      UINT  sent = ::SendInput(1, &tmp, sizeof(INPUT));
      return (sent == 1) ? 0 : -1;
    }

    static int emitKey(uint16_t hidCode, bool down)
    {
      bool extended = false;
      WORD scan = static_cast<WORD>(mapping::hid_to_win_scan(hidCode, extended));
      if (scan == 0)
      {
        return 0;
      }
      INPUT in{};
      in.type = INPUT_KEYBOARD;
      in.ki.wVk = 0; // using scancode
      in.ki.wScan = scan;
      in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
      return sendInput(in);
    }

    static int emitMouseMove(int dx, int dy)
    {
      INPUT in{};
      in.type = INPUT_MOUSE;
      in.mi.dx = dx;
      in.mi.dy = dy;
      in.mi.dwFlags = MOUSEEVENTF_MOVE;
      return sendInput(in);
    }

    static int emitMouseWheel(int delta)
    {
      INPUT in{};
      in.type = INPUT_MOUSE;
      in.mi.mouseData = static_cast<DWORD>(delta * WHEEL_DELTA);
      in.mi.dwFlags = MOUSEEVENTF_WHEEL;
      return sendInput(in);
    }

    static int emitMouseHWheel(int delta)
    {
      INPUT in{};
      in.type = INPUT_MOUSE;
      in.mi.mouseData = static_cast<DWORD>(delta * WHEEL_DELTA);
      in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
      return sendInput(in);
    }

    static int emitMouseButton(uint16_t code, bool down)
    {
      DWORD flags = 0;
      INPUT in{};
      in.type = INPUT_MOUSE;
      switch (code)
      {
      case 1:
        flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
      case 2:
        flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
      case 3:
        flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
      case 4:
        flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        in.mi.mouseData = XBUTTON1;
        break;
      case 5:
        flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        in.mi.mouseData = XBUTTON2;
        break;
      default:
        return 0; // unsupported button code
      }
      in.mi.dwFlags = flags;
      return sendInput(in);
    }
  };

  std::unique_ptr<Emitter> make_windows_emitter()
  {
    return std::unique_ptr<Emitter>(new WindowsEmitter());
  }

} // namespace keyboard
#endif