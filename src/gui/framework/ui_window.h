#pragma once

#include "ui_scene.h"

#include <memory>
#include <utility>

namespace gui
{
  namespace framework
  {

    class UIWindow
    {
    public:
      virtual ~UIWindow() = default;

      virtual void init() = 0;

      virtual void deinit() = 0;

      // Set or replace the key scene. Implementations should call willUnmount on the
      // previous scene (if any) and didMount on the new one.
      virtual void setScene(UIScene* scene) = 0;

      // Constrain the mouse to this window (implementation-defined behavior)
      // Used by scenes that need locked cursor or capture, e.g., for drag/resize
      // or relative mouse input.
      virtual void apply_mouse_confinement() = 0;

      // Release any mouse confinement applied previously.
      virtual void release_mouse_confinement() = 0;
    };

    // Initializes the concrete window (SDL + ImGui) instance.
    // Safe to call multiple times; no-op if already initialized.
    void init_window();

    // Deinitializes and destroys the concrete window instance and its resources.
    // Safe to call multiple times; no-op if not initialized.
    void deinit_window();

    // Set or replace the key scene on the window singleton
    void set_window_scene(UIScene* scene);

    // Convenience: set scene by type. Holds a static instance per T for the app lifetime.
    template <typename T, typename... Args> void set_window_scene(Args&&... args)
    {
      static std::unique_ptr<T> holder;
      if (!holder)
      {
        holder.reset(new T(std::forward<Args>(args)...));
      }
      set_window_scene(static_cast<UIScene*>(holder.get()));
    }

    // Get a reference to the window singleton. Initializes the window if needed.
    UIWindow& get_window();

    // Run one UI frame; returns false if the window requested quit.
    bool window_frame();

  } // namespace framework
} // namespace gui