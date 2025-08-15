// A top-level UI Scene that manages inner layout/content of a window
#pragma once

#include <SDL3/SDL.h>

namespace gui
{
  namespace framework
  {
    struct UIInputEvent
    {
      SDL_Event raw_event;          // The raw SDL event data
      bool should_propagate = true; // Whether the event should continue propagating

      void stopPropagation() const { const_cast<UIInputEvent*>(this)->should_propagate = false; }
    };

    class UIScene
    {
    public:
      virtual ~UIScene() = default;

      // Lifecycle hooks
      virtual void didMount() {}
      virtual void willUnmount() {}
      virtual void handleInput(const UIInputEvent& event) {}

      // Render the scene's content (called each frame by the window)
      virtual void render() = 0;
    };

  } // namespace framework
} // namespace gui
