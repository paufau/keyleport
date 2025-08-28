#include "receiver_scene.h"

#include "gui/framework/ui_window.h"
#include "keyboard/input_event.h"
#include "keyboard/keyboard.h"
#include "networking/udp/app_net.h"
#include "store.h"

#include <imgui.h>

void ReceiverScene::didMount()
{
  // Create keyboard and emitter for injecting received events
  kb_ = keyboard::make_keyboard();
  emitter_ = kb_ ? kb_->createEmitter() : nullptr;

  // Subscribe to received data and emit as input events
  keyboard::Emitter* emitter_ptr = emitter_.get();
  net::udp::app_net::instance().set_on_receive(
      [emitter_ptr](const std::string& payload) {
        if (!emitter_ptr || payload.empty()) return;
        auto ev = keyboard::InputEventJSONConverter::decode(payload);
        emitter_ptr->emit(ev);
      });
}

void ReceiverScene::willUnmount()
{
  // Stop receiving injections in this scene
  net::udp::app_net::instance().set_on_receive(nullptr);
  emitter_.reset();
  kb_.reset();
}

void ReceiverScene::render()
{
  // Fullscreen root window without decorations
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
  ImGui::Begin("Receiver", nullptr, rootFlags);

  // Resolve text to display
  const auto device = store::connection_state().connected_device.get();
  std::string text =
      device ? (std::string("Receiving input from ") + device->ip()) : std::string("Receiving input from <no device>");

  // Center text within the window using absolute screen coordinates
  const ImVec2 win_pos = ImGui::GetWindowPos();
  const ImVec2 win_size = ImGui::GetWindowSize();
  const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  const ImVec2 text_pos =
      ImVec2(win_pos.x + (win_size.x - text_size.x) * 0.5f, win_pos.y + (win_size.y - text_size.y) * 0.5f);

  ImGui::SetCursorScreenPos(text_pos);
  ImGui::TextUnformatted(text.c_str());

  ImGui::End();
}
