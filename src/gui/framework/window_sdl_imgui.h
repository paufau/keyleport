// SDL3 + ImGui implementation of UIWindow
#pragma once

#include "ui_scene.h"
#include "window.h"

// Forward declarations to avoid including SDL headers in the interface
struct SDL_Window;
struct SDL_Renderer;

namespace gui
{
  namespace framework
  {
    class SdlImGuiWindow : public UIWindow
    {
    public:
      SdlImGuiWindow(const char* title = "Keyleport", int width = 960, int height = 600);
      ~SdlImGuiWindow() override;

      // Initialize SDL window, SDL_Renderer and ImGui (platform + renderer backends)
      void init() override;
      void deinit() override;

      void setScene(UIScene* scene) override;

      // Pump one frame: process events, begin/end ImGui frame, render scene
      // Returns false when the application should quit.
      bool frame();

      // Accessors (non-owning pointers)
      SDL_Window* window() const { return window_; }
      SDL_Renderer* renderer() const { return renderer_; }

    private:
      const char* title_;
      int width_;
      int height_;

      SDL_Window* window_ = nullptr;
      SDL_Renderer* renderer_ = nullptr;
      bool initialized_ = false;

      UIScene* scene_ = nullptr; // non-owning; lifetime managed by caller
    };
  } // namespace framework
} // namespace gui
