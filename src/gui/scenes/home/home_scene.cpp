#include "home_scene.h"

#include "gui/framework/ui_dispatch.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "gui/scenes/sender/sender_scene.h"
#include "networking/udp/app_net.h"
#include "store.h"

#include <algorithm>
#include <imgui.h>
#include <iostream>
#include <random>

// discovery and session lifecycle handled by net::udp::app_net

void HomeScene::didMount()
{
  // Reset devices list
  store::connection_state().available_devices.get().clear();

  auto& appnet = net::udp::app_net::instance();

  // Update store on peer discovery
  appnet.on_discovery().subscribe(
      [this](const net::udp::peer_info& p)
      {
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.port);
        const bool busy = false; // no busy state in udp prototype yet
        const std::string name = p.device_name.empty() ? p.device_id : p.device_name;

        auto it = std::find_if(devices.begin(), devices.end(), [&](const entities::ConnectionCandidate& d)
                               { return d.ip() == ip && d.port() == port; });
        entities::ConnectionCandidate cc(busy, name, ip, port);
        if (it == devices.end()) devices.push_back(cc);
        else *it = cc;
      });
  appnet.on_lose().subscribe(
      [this](const net::udp::peer_info& p)
      {
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.port);
        devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const entities::ConnectionCandidate& d)
                                     { return d.ip() == ip && d.port() == port; }),
                      devices.end());
      });

  // ReceiverScene is entered explicitly elsewhere when acting as a receiver.
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
        net::udp::peer_info peer{};
        peer.device_id = d.ip() + ":" + d.port();
        peer.device_name = d.name();
        peer.ip_address = d.ip();
        peer.port = std::stoi(d.port());
  net::udp::app_net::instance().connect_to_peer(peer);
        gui::framework::post_to_ui([] { gui::framework::set_window_scene<SenderScene>(); });
      }
      ImGui::EndDisabled();
      ++i;
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

void HomeScene::willUnmount()
{
  // Nothing to do; P2P runtime persists across scenes.
}

HomeScene::~HomeScene()
{
  // Nothing to do; P2P runtime is shut down in main.
}
