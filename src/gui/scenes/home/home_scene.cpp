#include "home_scene.h"

#include "gui/framework/ui_dispatch.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "gui/scenes/sender/sender_scene.h"
#include "networking/p2p/message.h"
#include "networking/p2p/peer.h"
#include "services/communication/communication_service.h"
#include "services/communication/packages/become_receiver_package.h"
#include "services/discovery/discovery_service.h"
#include "services/service_locator.h"
#include "store.h"

#include <algorithm>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <random>
#include <string>

// discovery and session lifecycle handled by net::udp::app_net

void HomeScene::didMount()
{
  // Reset devices list
  store::connection_state().available_devices.set({});

  discovery_service_ = std::make_shared<services::discovery_service>();
  services::service_locator::instance().repository.add_service(
      discovery_service_);

  communication_service_ = std::make_shared<services::communication_service>();
  services::service_locator::instance().repository.add_service(
      communication_service_);

  communication_subscription_id_ = communication_service_->on_package.subscribe(
      [this](const services::typed_package& package)
      {
        if (!services::become_receiver_package::is(package))
        {
          return;
        }

        std::cout << "[home_scene] Received package type='"
                  << package.__typename << "' size=" << package.payload.size()
                  << std::endl;

        auto become_receiver =
            services::become_receiver_package::decode(package.payload);

        std::cout << "[home_scene] Decoded become_receiver device_id='"
                  << become_receiver.device_id << "'" << std::endl;

        // Try to find the device by ID in the store's available devices
        const auto devices =
            store::connection_state().available_devices.value();

        std::shared_ptr<entities::ConnectionCandidate> candidate;
        if (auto it = std::find_if(
                devices.begin(), devices.end(), [&](const auto& d)
                { return d.ip() == package.meta.get_from().get_ip_address(); });
            it != devices.end())
        {
          candidate = std::make_shared<entities::ConnectionCandidate>(*it);
          std::cout << "[home_scene] Matched candidate for device_id="
                    << become_receiver.device_id << std::endl;
        }
        else
        {
          std::cerr << "Received become_receiver package from unknown device: "
                    << become_receiver.device_id << std::endl;
          return;
        }

        // Accept the connection request
        store::connection_state().connected_device.set(candidate);
        std::cout << "[home_scene] Set connected_device ip=" << candidate->ip()
                  << std::endl;

        // Pin the communication service to this peer
        communication_service_->pin_connection(p2p::peer(candidate->ip()));

        std::cout << "[home_scene] Pinned communication to " << candidate->ip()
                  << std::endl;

        // Switch to the receiver scene
        gui::framework::post_to_ui(
            [] { gui::framework::set_window_scene<ReceiverScene>(); });
        std::cout << "[home_scene] Posted UI task to switch to ReceiverScene"
                  << std::endl;
      });
}

void HomeScene::render()
{
  // Root window for the scene
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags rootFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoTitleBar;
  ImGui::Begin("Home", nullptr, rootFlags);

  ImGui::TextUnformatted("Available devices");
  ImGui::Separator();

  // Snapshot to avoid holding a mutable ref during UI
  const auto devices = store::connection_state().available_devices.value();

  if (devices.empty())
  {
    ImGui::TextDisabled("No devices discovered yet...");
    ImGui::End();
    return;
  }

  if (ImGui::BeginTable("device_table", 2,
                        ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_BordersInnerV))
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
        std::cout << "[home_scene] Connect clicked for device ip=" << d.ip()
                  << " name=" << d.name() << std::endl;
        store::connection_state().connected_device.set(
            std::make_shared<entities::ConnectionCandidate>(d));
        std::cout << "[home_scene] connected_device set ip=" << d.ip()
                  << std::endl;

        // Pin the communication service to this peer
        communication_service_->pin_connection(p2p::peer(d.ip()));
        // Send become_receiver package
        services::typed_package pkg;
        pkg.__typename = services::become_receiver_package::__typename;

        services::become_receiver_package become_receiver_pkg;
        become_receiver_pkg.device_id = discovery_service_->self_peer.device_id;
        pkg.payload = become_receiver_pkg.encode();

        std::cout << "[home_scene] Sending become_receiver to " << d.ip()
                  << " self_id=" << become_receiver_pkg.device_id
                  << " encoded_size=" << pkg.payload.size() << std::endl;
        communication_service_->send_package_reliable(pkg);

        gui::framework::post_to_ui(
            [] { gui::framework::set_window_scene<SenderScene>(); });
        std::cout << "[home_scene] Posted UI task to switch to SenderScene"
                  << std::endl;
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
  if (discovery_service_)
  {
    services::service_locator::instance().repository.remove_service(
        discovery_service_);
    discovery_service_.reset();
  }

  communication_service_->on_package.unsubscribe(
      communication_subscription_id_);
}
