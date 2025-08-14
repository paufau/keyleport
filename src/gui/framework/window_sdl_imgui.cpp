#include "window_sdl_imgui.h"

#include <stdexcept>
#include <string>

// SDL3 headers
#include <SDL3/SDL.h>
// ImGui
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

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
          running = false;
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
