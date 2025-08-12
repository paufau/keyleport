// Windows emitter implementation using Win32 SendInput
#include <memory>
#include <string>
#include "../emitter.h"
#include <Windows.h>

namespace keyboard
{

  class WindowsEmitter : public Emitter
  {
  public:
    int emit(const InputEvent &event) override
    {
      switch (event.type)
      {
      case 0: // keyboard
        return emitKey(event.code, event.action == 0 /*down*/);
      case 1: // mouse
        switch (event.action)
        {
        case 2:
          return emitMouseMove(event.x, event.y);
        case 3:
          return emitMouseWheel(event.delta);
        case 0:
          return emitMouseButton(event.code, true);
        case 1:
          return emitMouseButton(event.code, false);
        default:
          return 0;
        }
      default:
        return 0;
      }
    }

  private:
    static int sendInput(const INPUT &in)
    {
      INPUT tmp = in;
      UINT sent = ::SendInput(1, &tmp, sizeof(INPUT));
      return (sent == 1) ? 0 : -1;
    }

    static int emitKey(uint16_t scanCode, bool down)
    {
      INPUT in{};
      in.type = INPUT_KEYBOARD;
      in.ki.wVk = 0; // using scancode
      in.ki.wScan = static_cast<WORD>(scanCode);
      in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
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
