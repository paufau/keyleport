#include "window_sdl_imgui.h"

#include "ui_input_manager.h"

#include <stdexcept>
#include <string>
// extras
#include <cstdlib>

// SDL3 headers
#include <SDL3/SDL.h>
// ImGui
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <iostream>

namespace gui
{
  namespace framework
  {

    SdlImGuiWindow::SdlImGuiWindow(const char* title, int width, int height)
        : title_(title), width_(width), height_(height)
    {
    }

    SdlImGuiWindow::~SdlImGuiWindow()
    {
      deinit();
    }

    void SdlImGuiWindow::init()
    {
      if (initialized_)
      {
        return;
      }

      SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

      // Create window + renderer
      SDL_Window* window = SDL_CreateWindow(title_, width_, height_, SDL_WINDOW_RESIZABLE);
      if (!window)
      {
        std::string err = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + err);
      }

      SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
      if (!renderer)
      {
        std::string err = SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error("SDL_CreateRenderer failed: " + err);
      }

      window_ = window;
      renderer_ = renderer;

      // Setup Dear ImGui context
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO();
      (void)io;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();

      // Make default font/UI a bit larger and respect system/monitor scale.
      // SDL/ImGui backends handle HiDPI framebuffer, but we still scale UI metrics
      // and font rendering to track the system content scale.
      {
        // Derive system content scale from window logical vs pixel size
        int lw = 0, lh = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(window_, &lw, &lh);
        SDL_GetWindowSizeInPixels(window_, &pw, &ph);
        float sx = (lw > 0) ? (float)pw / (float)lw : 1.0f;
        float sy = (lh > 0) ? (float)ph / (float)lh : 1.0f;
        // Use the average content scale to be conservative across dimensions
        float system_scale = (sx > 0.0f && sy > 0.0f) ? (sx + sy) * 0.5f : 1.0f;

        // Baseline bump above ImGui defaults ("a bit larger")
        const float baseline_scale = 1.0f;
        const float scale = baseline_scale * system_scale;

        // Scale widget metrics to match font size increase
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale);

        // Rasterize font at target pixel height for crisp text
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig cfg;

        cfg.SizePixels = 24.0f * scale; // ImGui default is ~13px; scale to target
        io.Fonts->AddFontDefault(&cfg);
        io.FontGlobalScale = 1.0f; // avoid atlas scaling blur
      }

      // Setup Platform/Renderer backends
      ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
      ImGui_ImplSDLRenderer3_Init(renderer_);

      initialized_ = true;
    }

    void SdlImGuiWindow::deinit()
    {
      if (!initialized_)
      {
        return;
      }

      // Shutdown ImGui backends and context
      ImGui_ImplSDLRenderer3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();

      // Destroy SDL resources
      if (renderer_)
      {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
      }
      if (window_)
      {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
      }
      SDL_Quit();

      initialized_ = false;
    }

    void SdlImGuiWindow::setScene(UIScene* scene)
    {
      if (scene_ == scene)
      {
        return;
      }
      if (scene_)
      {
        scene_->willUnmount();
      }
      scene_ = scene;
      if (scene_)
      {
        scene_->didMount();
      }
    }

    void update_mouse_confinement_rect(SDL_Window* win, int w, int h)
    {
      if (!win || w <= 0 || h <= 0)
      {
        return;
      }
      SDL_Rect rect{0, 0, w, h};
      if (!SDL_SetWindowMouseRect(win, &rect))
      {
        std::cerr << "SDL_SetWindowMouseRect failed: " << SDL_GetError() << std::endl;
      }
    }

    void SdlImGuiWindow::apply_mouse_confinement()
    {
      if (!window_)
      {
        return;
      }
      if (!SDL_SetWindowMouseGrab(window_, true))
      {
        std::cerr << "SDL_SetWindowMouseGrab failed: " << SDL_GetError() << std::endl;
      }
      int ww = 0, wh = 0;
      SDL_GetWindowSize(window_, &ww, &wh);
      update_mouse_confinement_rect(window_, ww, wh);
      if (!SDL_SetWindowRelativeMouseMode(window_, true))
      {
        std::cerr << "SDL_SetWindowRelativeMouseMode failed: " << SDL_GetError() << std::endl;
      }
    }

    void SdlImGuiWindow::release_mouse_confinement()
    {
      if (!window_)
      {
        return;
      }
      SDL_SetWindowRelativeMouseMode(window_, false);
      SDL_SetWindowMouseGrab(window_, false);
      SDL_SetWindowMouseRect(window_, nullptr);
    }

    bool SdlImGuiWindow::frame()
    {
      if (!initialized_)
      {
        return false;
      }

      bool running = true;

      SDL_Event ev;
      while (SDL_PollEvent(&ev))
      {
        if (ev.type == SDL_EVENT_QUIT)
        {
          // Global quit request (e.g., Cmd+Q on macOS)
          running = false;
        }
        else if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
          // Close button pressed on a window; only act if it's our window
          if (window_ && ev.window.windowID == SDL_GetWindowID(window_))
          {
            running = false;
          }
        }

        // Maintain global pressed-keys state
        switch (ev.type)
        {
        case SDL_EVENT_KEY_DOWN:
          gui::framework::UIInputManager::instance().press(static_cast<SDL_Scancode>(ev.key.scancode));
          break;
        case SDL_EVENT_KEY_UP:
          gui::framework::UIInputManager::instance().release(static_cast<SDL_Scancode>(ev.key.scancode));
          break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
          // Clear pressed keys when focus is lost to avoid stuck keys
          gui::framework::UIInputManager::instance().clear();
          break;
        default:
          break;
        }

        UIInputEvent input_event;
        input_event.raw_event = ev;

        if (scene_)
        {
          // Pass the event to the current scene
          scene_->handleInput(input_event);
          if (!input_event.should_propagate)
          {
            continue; // Stop further processing if propagation is stopped
          }
        }

        ImGui_ImplSDL3_ProcessEvent(&ev);
      }

      // Start the Dear ImGui frame
      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      if (scene_)
      {
        scene_->render();
      }

      // Rendering
      ImGui::Render();
      SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
      SDL_RenderClear(renderer_);
      ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
      SDL_RenderPresent(renderer_);

      return running;
    }

  } // namespace framework
} // namespace gui
