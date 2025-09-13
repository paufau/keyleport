#if defined(__APPLE__)
#include "../emitter.h"

#include "../mapping/hid_to_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace keyboard
{
  namespace
  {
    inline CGPoint current_cursor_pos()
    {
      CGEventRef e = CGEventCreate(nullptr);
      CGPoint p = CGEventGetLocation(e);
      CFRelease(e);
      return p;
    }

    inline void post_mouse_move_relative(int dx, int dy)
    {
      // macOS global coordinates use origin at bottom-left; SDL dy>0 is down.
      // Subtract dy so a positive dy (down) lowers the on-screen cursor.
      CGPoint p = current_cursor_pos();
      CGPoint np = CGPointMake(p.x + dx, p.y + dy);
      CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, np,
                                                kCGMouseButtonLeft);
      if (move)
      {
        CGEventPost(kCGHIDEventTap, move);
        CFRelease(move);
      }
    }

    inline void post_mouse_button(uint16_t sdl_button, bool down)
    {
      CGMouseButton btn = kCGMouseButtonLeft;
      CGEventType type = kCGEventLeftMouseDown;
      switch (sdl_button)
      {
      case 1: // left
        btn = kCGMouseButtonLeft;
        type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
        break;
      case 3: // right
        btn = kCGMouseButtonRight;
        type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
        break;
      case 2: // middle
      default:
        btn = kCGMouseButtonCenter;
        type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
        break;
      }

      CGPoint p = current_cursor_pos();
      CGEventRef ev = CGEventCreateMouseEvent(nullptr, type, p, btn);
      if (ev)
      {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
      }
    }

    inline void post_scroll(int dx, int dy)
    {
      // Use line-based scrolling (1 or -1 typical units). SDL dy>0 = scroll up.
      CGEventRef ev = CGEventCreateScrollWheelEvent(
          nullptr, kCGScrollEventUnitLine, 2 /*vertical,horizontal*/, dy, dx);
      if (ev)
      {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
      }
    }

    inline void post_key(CGKeyCode vk, bool down)
    {
      CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, vk, down);
      if (ev)
      {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
      }
    }
  } // namespace

  class MacEmitter : public Emitter
  {
  public:
    int emit(const InputEvent& event) override
    {
      switch (event.type)
      {
      case InputEvent::Type::Mouse:
        switch (event.action)
        {
        case InputEvent::Action::Move:
          post_mouse_move_relative(event.dx, event.dy);
          return 0;
        case InputEvent::Action::Scroll:
          post_scroll(event.dx, event.dy);
          return 0;
        case InputEvent::Action::Down:
          post_mouse_button(event.code, true);
          return 0;
        case InputEvent::Action::Up:
          post_mouse_button(event.code, false);
          return 0;
        default:
          return -1;
        }
        break;

      case InputEvent::Type::Key:
      {
        CGKeyCode vk;
        if (!mapping::hid_to_mac_vk(event.code, vk))
        {
          return -2; // unsupported key
        }
        const bool down = (event.action == InputEvent::Action::Down);
        const bool up = (event.action == InputEvent::Action::Up);
        if (down)
        {
          post_key(vk, true);
          return 0;
        }
        if (up)
        {
          post_key(vk, false);
          return 0;
        }
        return -1;
      }
      default:
        break;
      }
      return 0;
    }
  };

  std::unique_ptr<Emitter> make_macos_emitter()
  {
    return std::unique_ptr<Emitter>(new MacEmitter());
  }

} // namespace keyboard

#endif
