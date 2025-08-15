#include "home_scene.h"

#include "gui/framework/ui_window.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "gui/scenes/sender/sender_scene.h"
#include "store.h"

#include <algorithm>
#include <imgui.h>
#include <iostream>
#include <networking/discovery/discovery.h>
void HomeScene::didMount()
{
  // Create and start the discovery process
  auto& disc = net::discovery::Discovery::instance();
  disc.start_discovery(store::connection_state().port.get());

  // When discovered add to available connection state
  disc.onDiscovered(
      [&](const entities::ConnectionCandidate& cc)
      {
        std::cerr << "[discovery] Discovered server: " << cc.ip() << ":" << cc.port() << std::endl;
        store::connection_state().available_devices.get().push_back(cc);
      });

  disc.onLost(
      [&](const entities::ConnectionCandidate& cc)
      {
        std::cerr << "[discovery] Lost server: " << cc.ip() << ":" << cc.port() << std::endl;
        auto& devices = store::connection_state().available_devices.get();
        devices.erase(std::remove_if(devices.begin(), devices.end(),
                                     [&](const entities::ConnectionCandidate& d) { return d.ip() == cc.ip(); }),
                      devices.end());
      });

  disc.onMessage(
      [&](const entities::ConnectionCandidate& cc, const std::string& msg)
      {
        std::cerr << "[discovery] Message from " << cc.ip() << ":" << cc.port() << " - " << msg << std::endl;

        if (msg == "become_receiver")
        {
          store::connection_state().connected_device.set(std::make_shared<entities::ConnectionCandidate>(cc));
          gui::framework::set_window_scene<ReceiverScene>();
          disc.stop_discovery();
          net::discovery::Discovery::destroy_instance();
        }
      });
}

void HomeScene::render()
{
  // Root window for the scene
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
  ImGui::Begin("Home", nullptr, rootFlags);

  ImGui::TextUnformatted("Available devices");
  ImGui::Separator();

  // Snapshot to avoid holding a mutable ref during UI
  const auto devices = store::connection_state().available_devices.get();

  if (devices.empty())
  {
    ImGui::TextDisabled("No devices discovered yet...");
    ImGui::End();
    return;
  }

  if (ImGui::BeginTable("device_table", 2,
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
  {
    ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch, 4.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();

    int i = 0;
    for (const auto& d : devices)
    {
      ImGui::TableNextRow();

      // Left column: vertical layout -> device name, device ip
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(d.name().c_str());
      ImGui::TextDisabled("%s", d.ip().c_str());

      // Right column: connect button (non-functional for now)
      ImGui::TableSetColumnIndex(1);
      ImGui::BeginDisabled(d.is_busy());
      if (ImGui::Button((std::string("Connect##") + std::to_string(i)).c_str()))
      {
        // Persist selected device and set sender scene
        store::connection_state().connected_device.set(std::make_shared<entities::ConnectionCandidate>(d));
        net::discovery::Discovery::instance().sendMessage(d, "become_receiver");
        gui::framework::set_window_scene<SenderScene>();
      }
      ImGui::EndDisabled();
      ++i;
    }

    ImGui::EndTable();
  }

  ImGui::End();
}
