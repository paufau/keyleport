#include "sender_scene.h"

#include "store.h"

#include <imgui.h>

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
