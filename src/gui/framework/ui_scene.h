// A top-level UI Scene that manages inner layout/content of a window
#pragma once

namespace gui
{
  namespace framework
  {

    class UIScene
    {
    public:
      virtual ~UIScene() = default;

      // Lifecycle hooks
      virtual void didMount() {}
      virtual void willUnmount() {}

      // Render the scene's content (called each frame by the window)
      virtual void render() = 0;
    };

  } // namespace framework
} // namespace gui
