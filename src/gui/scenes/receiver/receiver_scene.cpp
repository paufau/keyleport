#include "receiver_scene.h"

#include "gui/framework/ui_dispatch.h"
#include "gui/framework/ui_window.h"
#include "keyboard/input_event.h"
#include "keyboard/keyboard.h"
#include "services/communication/communication_service.h"
#include "services/communication/packages/keyboard_input_package.h"
#include "services/service_locator.h"
#include "store.h"

#include <imgui.h>
#include <iostream>

void ReceiverScene::didMount()
{
  // Create keyboard and emitter for injecting received events
  kb_ = keyboard::make_keyboard();
  emitter_ = kb_ ? kb_->createEmitter() : nullptr;

  // Subscribe to received data and emit as input events
  keyboard::Emitter* emitter_ptr = emitter_.get();

  auto communication_service =
      services::service_locator::instance()
          .repository.get_service<services::communication_service>();

  if (communication_service)
  {
    communication_subscription_id_ =
        communication_service->on_package.subscribe(
            [emitter_ptr](const services::typed_package& package)
            {
              if (!emitter_ptr ||
                  !services::keyboard_input_package::is(package))
              {
                return;
              }
              auto ev =
                  services::keyboard_input_package::decode(package.payload)
                      .event;

              gui::framework::post_to_ui(
                  [emitter_ptr, ev]
                  {
                    if (!emitter_ptr)
                    {
                      return;
                    }
                    emitter_ptr->emit(ev);
                  });
            });

    std::cout << "[receiver_scene] Subscribed to communication_service with id "
              << communication_subscription_id_ << std::endl;
  }
  else
  {
    std::cerr << "[receiver_scene] communication_service not found"
              << std::endl;
  }
}

void ReceiverScene::willUnmount()
{
  emitter_.reset();
  kb_.reset();
  auto communication_service =
      services::service_locator::instance()
          .repository.get_service<services::communication_service>();

  if (communication_service)
  {
    communication_service->on_package.unsubscribe(
        communication_subscription_id_);
    std::cout << "[receiver_scene] Unsubscribed from communication_service id "
              << communication_subscription_id_ << std::endl;
  }
}

void ReceiverScene::render()
{
  // Fullscreen root window without decorations
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags rootFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoTitleBar;
  ImGui::Begin("Receiver", nullptr, rootFlags);

  // Resolve text to display
  const auto device = store::connection_state().connected_device.get();
  std::string text = device
                         ? (std::string("Receiving input from ") + device->ip())
                         : std::string("Receiving input from <no device>");

  // Center text within the window using absolute screen coordinates
  const ImVec2 win_pos = ImGui::GetWindowPos();
  const ImVec2 win_size = ImGui::GetWindowSize();
  const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  const ImVec2 text_pos = ImVec2(win_pos.x + (win_size.x - text_size.x) * 0.5f,
                                 win_pos.y + (win_size.y - text_size.y) * 0.5f);

  ImGui::SetCursorScreenPos(text_pos);
  ImGui::TextUnformatted(text.c_str());

  ImGui::End();
}
