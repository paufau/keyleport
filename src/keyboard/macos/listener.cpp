#if defined(__APPLE__) && defined(USE_SDL3)
#include <memory>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>
#include <iostream>

#include "../listener.h"

namespace keyboard
{

  class MacListener : public Listener
  {
  public:
    void onEvent(EventHandler handler) override { handler_ = std::move(handler); }
    int run() override { return listen_loop(); }

  private:
    void update_mouse_confinement_rect(SDL_Window *win, int w, int h)
    {
      if (!win || w <= 0 || h <= 0)
        return;
      SDL_Rect rect{0, 0, w, h};
      if (!SDL_SetWindowMouseRect(win, &rect))
      {
        std::cerr << "SDL_SetWindowMouseRect failed: " << SDL_GetError() << std::endl;
      }
    }

    void apply_mouse_confinement(SDL_Window *win)
    {
      if (!win)
        return;
      if (!SDL_SetWindowMouseGrab(win, true))
      {
        std::cerr << "SDL_SetWindowMouseGrab failed: " << SDL_GetError() << std::endl;
      }
      int ww = 0, wh = 0;
      SDL_GetWindowSize(win, &ww, &wh);
      update_mouse_confinement_rect(win, ww, wh);
      if (!SDL_SetWindowRelativeMouseMode(win, true))
      {
        std::cerr << "SDL_SetWindowRelativeMouseMode failed: " << SDL_GetError() << std::endl;
      }
    }

    void release_mouse_confinement(SDL_Window *win)
    {
      if (!win)
        return;
      SDL_SetWindowRelativeMouseMode(win, false);
      SDL_SetWindowMouseGrab(win, false);
      SDL_SetWindowMouseRect(win, nullptr);
    }

    int listen_loop()
    {
      SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
      SDL_Window *win = SDL_CreateWindow("Keyleport", 800, 600, 0);
      if (!win)
      {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 2;
      }
      // Confine the mouse and enable relative mode
      apply_mouse_confinement(win);
      bool running = true;
      while (running)
      {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
          if (ev.type == SDL_EVENT_QUIT)
          {
            running = false;
            break;
          }
          if (!handler_)
            continue;
          EventBatch batch;
          switch (ev.type)
          {
          case SDL_EVENT_KEY_DOWN:
          case SDL_EVENT_KEY_UP:
          {
            InputEvent ie{};
            ie.type = 0;
            ie.action = (ev.type == SDL_EVENT_KEY_DOWN) ? 0 : 1;
            ie.code = static_cast<uint16_t>(ev.key.scancode);
            ie.x = 0;
            ie.y = 0;
            ie.delta = 0;
            batch.push_back(ie);
            break;
          }
          case SDL_EVENT_MOUSE_MOTION:
          {
            InputEvent ie{};
            ie.type = 1;
            ie.action = 2;
            ie.code = 0;
            ie.x = ev.motion.x;
            ie.y = ev.motion.y;
            ie.dx = ev.motion.xrel;
            ie.dy = ev.motion.yrel;
            ie.delta = 0;
            batch.push_back(ie);
            break;
          }
          case SDL_EVENT_MOUSE_WHEEL:
          {
            InputEvent ie{};
            ie.type = 1;
            ie.action = 3;
            ie.code = 0;
            ie.x = 0;
            ie.y = 0;
            ie.delta = static_cast<int32_t>(ev.wheel.y);
            batch.push_back(ie);
            break;
          }
          case SDL_EVENT_MOUSE_BUTTON_DOWN:
          case SDL_EVENT_MOUSE_BUTTON_UP:
          {
            InputEvent ie{};
            ie.type = 1;
            ie.action = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? 0 : 1;
            ie.code = static_cast<uint16_t>(ev.button.button);
            ie.x = ev.button.x;
            ie.y = ev.button.y;
            ie.dx = 0;
            ie.dy = 0;
            ie.delta = 0;
            batch.push_back(ie);
            break;
          }
          case SDL_EVENT_WINDOW_RESIZED:
          {
            // Keep the confine rect in sync with window size
            int newW = ev.window.data1;
            int newH = ev.window.data2;
            update_mouse_confinement_rect(win, newW, newH);
            break;
          }
          default:
            break;
          }
          if (!batch.empty())
            handler_(batch);
        }
        SDL_Delay(5);
      }
      // Release any grabs and relative mode before shutdown
      release_mouse_confinement(win);
      SDL_DestroyWindow(win);
      SDL_Quit();
      return 0;
    }

    EventHandler handler_;
  };

  std::unique_ptr<Listener> make_macos_listener()
  {
    return std::unique_ptr<Listener>(new MacListener());
  }

} // namespace keyboard

#endif