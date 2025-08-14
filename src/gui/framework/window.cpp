#include "window.h"

#include "ui_scene.h"
#include "window_sdl_imgui.h"

#include <memory>

namespace gui
{
  namespace framework
  {

    namespace
    {
      std::unique_ptr<SdlImGuiWindow>& singleton()
      {
        static std::unique_ptr<SdlImGuiWindow> inst;
        return inst;
      }
    } // namespace

    void init_window()
    {
      auto& inst = singleton();
      if (!inst)
      {
        inst.reset(new SdlImGuiWindow());
      }
      inst->init();
    }

    void deinit_window()
    {
      auto& inst = singleton();
      if (inst)
      {
        inst->deinit();
        inst.reset();
      }
    }

    void set_window_scene(UIScene* scene)
    {
      auto& inst = singleton();
      if (!inst)
      {
        inst.reset(new SdlImGuiWindow());
        inst->init();
      }
      inst->setScene(scene);
    }

    bool window_frame()
    {
      auto& inst = singleton();
      if (!inst)
      {
        return false;
      }
      return inst->frame();
    }

  } // namespace framework
} // namespace gui
