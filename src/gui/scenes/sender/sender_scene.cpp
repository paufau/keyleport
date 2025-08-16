#include "sender_scene.h"

#include "gui/framework/ui_input_manager.h"
#include "gui/framework/ui_window.h"
#include "keyboard/input_event.h"
#include "networking/p2p/service.h"
#include "store.h"

#include <SDL3/SDL.h>
#include <imgui.h>

void SenderScene::didMount()
{
  gui::framework::get_window().apply_mouse_confinement();
  is_mouse_contained_ = true;
}

void SenderScene::willUnmount()
{
  gui::framework::get_window().release_mouse_confinement();
  is_mouse_contained_ = false;
}

void SenderScene::handleInput(const gui::framework::UIInputEvent& event)
{
  // if pressed option + control + esc, then release mouse confinement and allow for events to propagate
  if (event.raw_event.type == SDL_EVENT_KEY_DOWN &&
      gui::framework::UIInputManager::instance().is_pressed(SDL_SCANCODE_LALT) &&
      gui::framework::UIInputManager::instance().is_pressed(SDL_SCANCODE_LCTRL) &&
      gui::framework::UIInputManager::instance().is_pressed(SDL_SCANCODE_ESCAPE))
  {
    gui::framework::get_window().release_mouse_confinement();
    is_mouse_contained_ = false;
  }

  if (is_mouse_contained_)
  {
    // Convert and send via P2P service as a single JSON line
    const auto ev = keyboard::InputEvent::fromSDL(event.raw_event);
    const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
    net::p2p::Service::instance().send_to_peer(payload);
    event.stopPropagation();
  }
}

void SenderScene::render()
{
  // Root window for the scene (fullscreen, no decorations)
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
  ImGui::Begin("Sender", nullptr, rootFlags);

  // Safely read the currently connected device (may be null)
  const auto device = store::connection_state().connected_device.get();

  if (device)
  {
    ImGui::Text("You are sending events to %s", device->ip().c_str());
  }
  else
  {
    ImGui::TextDisabled("You are sending event to <no device>");
  }

  ImGui::End();
}
