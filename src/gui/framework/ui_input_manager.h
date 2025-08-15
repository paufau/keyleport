// Tracks currently pressed keys for the UI as a simple, globally-accessible singleton
#pragma once

#include <SDL3/SDL.h>
#include <unordered_set>
#include <vector>

namespace gui
{
  namespace framework
  {
    class UIInputManager
    {
    public:
      // Singleton accessor
      static UIInputManager& instance()
      {
        static UIInputManager inst;
        return inst;
      }

      // Record key press/release using SDL scancodes (layout-independent)
      inline void press(SDL_Scancode sc) { pressed_.insert(sc); }

      inline void release(SDL_Scancode sc) { pressed_.erase(sc); }

      // Bulk helpers
      inline void clear() { pressed_.clear(); }

      // Queries
      inline bool is_pressed(SDL_Scancode sc) const { return pressed_.find(sc) != pressed_.end(); }

      inline bool empty() const { return pressed_.empty(); }

      inline size_t count() const { return pressed_.size(); }

      // Copy out list of currently pressed scancodes (stable API surface)
      inline std::vector<SDL_Scancode> pressed() const
      {
        return std::vector<SDL_Scancode>(pressed_.begin(), pressed_.end());
      }

    private:
      UIInputManager() = default;
      std::unordered_set<SDL_Scancode> pressed_;
    };
  } // namespace framework
} // namespace gui
