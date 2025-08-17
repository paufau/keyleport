#include "input_event.h"

#include <SDL3/SDL.h>

namespace keyboard
{

  static inline InputEvent make_key_event(SDL_KeyboardEvent const& k, InputEvent::Action a)
  {
    InputEvent ev{};
    ev.type = InputEvent::Type::Key;
    ev.action = a;
    ev.code = static_cast<uint16_t>(k.scancode);
    ev.dx = 0;
    ev.dy = 0;
    return ev;
  }

  static inline InputEvent make_mouse_move(int dx, int dy)
  {
    InputEvent ev{};
    ev.type = InputEvent::Type::Mouse;
    ev.action = InputEvent::Action::Move;
    ev.code = 0;
    ev.dx = dx;
    ev.dy = dy;
    return ev;
  }

  static inline InputEvent make_scroll(int dx, int dy)
  {
    InputEvent ev{};
    ev.type = InputEvent::Type::Mouse;
    ev.action = InputEvent::Action::Scroll;
    ev.code = 0;
    ev.dx = dx;
    ev.dy = dy;
    return ev;
  }

  static inline InputEvent make_mouse_button(uint16_t button, bool down)
  {
    InputEvent ev{};
    ev.type = InputEvent::Type::Mouse;
    ev.action = down ? InputEvent::Action::Down : InputEvent::Action::Up;
    ev.code = button; // SDL button codes: 1=left, 2=middle, 3=right, 4=X1, 5=X2
    ev.dx = 0;
    ev.dy = 0;
    return ev;
  }

  InputEvent InputEvent::fromSDL(const SDL_Event& e)
  {
    switch (e.type)
    {
    case SDL_EVENT_KEY_DOWN:
      return make_key_event(e.key, InputEvent::Action::Down);
    case SDL_EVENT_KEY_UP:
      return make_key_event(e.key, InputEvent::Action::Up);
    case SDL_EVENT_MOUSE_MOTION:
      // SDL stores relative motion in motion.xrel / yrel when using relative mode; otherwise derive delta
      return make_mouse_move(static_cast<int>(e.motion.xrel), static_cast<int>(e.motion.yrel));
    case SDL_EVENT_MOUSE_WHEEL:
      // SDL wheel gives precise_x/y (float) and x/y (int). Use x/y ints for simplicity.
      return make_scroll(static_cast<int>(e.wheel.x), static_cast<int>(e.wheel.y));
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      return make_mouse_button(static_cast<uint16_t>(e.button.button), true);
    case SDL_EVENT_MOUSE_BUTTON_UP:
      return make_mouse_button(static_cast<uint16_t>(e.button.button), false);
    default:
      break;
    }
    return InputEvent{}; // unknown/unhandled
  }

} // namespace keyboard
