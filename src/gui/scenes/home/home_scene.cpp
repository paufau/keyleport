#include "home_scene.h"

#include "gui/framework/ui_dispatch.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "gui/scenes/sender/sender_scene.h"
#include "networking/p2p/runtime.h"
#include "networking/p2p/UdpTransport.h"
#include "networking/p2p/service.h"
#include "store.h"

#include <algorithm>
#include <imgui.h>
#include <iostream>
#include <random>

// no local gen_id; handled by net::p2p::Runtime

void HomeScene::didMount()
{
  // Reset devices list
  store::connection_state().available_devices.get().clear();

  auto& rt = net::p2p::Runtime::instance();
  (void)rt.session_port();

  // Update store on peer updates
  rt.set_on_peer_update(
      [this](const net::p2p::Peer& p)
      {
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.session_port);
        const bool busy = (p.state == net::p2p::State::Busy);
        const std::string name = p.device_name.empty() ? p.instance_id : p.device_name;

        auto it = std::find_if(devices.begin(), devices.end(), [&](const entities::ConnectionCandidate& d)
                               { return d.ip() == ip && d.port() == port; });
        entities::ConnectionCandidate cc(busy, name, ip, port);
        if (it == devices.end())
        {
          devices.push_back(cc);
        }
        else
        {
          *it = cc;
        }
      });
  rt.set_on_peer_remove(
      [this](const net::p2p::Peer& p)
      {
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.session_port);
        devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const entities::ConnectionCandidate& d)
                                     { return d.ip() == ip && d.port() == port; }),
                      devices.end());
      });

  // Incoming server session -> populate store and switch to ReceiverScene
  rt.set_on_server_ready(
      [](const std::string& peer_ip, const std::string& peer_port)
      {
        gui::framework::post_to_ui(
            [peer_ip, peer_port]
            {
              store::connection_state().connected_device.set(
                  std::make_shared<entities::ConnectionCandidate>(false, /*name*/ peer_ip, peer_ip, peer_port));
              gui::framework::set_window_scene<ReceiverScene>();
            });
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
        // Initiate TCP connection (no extra app message required)
        if (true)
        {
          auto sock = std::make_shared<asio::ip::tcp::socket>(net::p2p::Runtime::instance().io());
          asio::ip::tcp::endpoint ep(asio::ip::make_address(d.ip()), static_cast<unsigned short>(std::stoi(d.port())));
          sock->async_connect(
              ep,
              [this, sock](std::error_code ec)
              {
                if (ec)
                {
                  std::cerr << "[p2p] Connect failed: " << ec.message() << std::endl;
                  return;
                }
                auto session = std::make_shared<net::p2p::Session>(std::move(*sock));
                // Register client session globally for sender scene to consume
                net::p2p::Service::instance().set_client_session(session);
                session->start_client(
                    [this, session]
                    {
                      // Mark busy state in discovery when client handshaked (sender role)
                      net::p2p::Runtime::instance().set_discovery_state(net::p2p::State::Busy);
                      std::cerr << "[p2p] Client handshake done; switching to SenderScene" << std::endl;
                      // Create a UDP sender targeting the peer using same IP and UDP port read from control channel
                      // later. For first iteration, assume server uses same TCP port for UDP. If server announces a UDP
                      // port line, we'll update sender later in on_message.
            {
                        try
                        {
                          auto rep = session->socket().remote_endpoint();
                          // Provisional guess: UDP port equals TCP port until updated
                          asio::ip::udp::endpoint dest(rep.address(), rep.port());
              auto udp_tx = std::make_shared<net::p2p::UdpSender>(net::p2p::Runtime::instance().io(), dest);
                          net::p2p::Service::instance().set_client_udp_sender(udp_tx);
                        }
                        catch (...)
                        {
                        }
            }
                      // Hook a temporary TCP on_message to capture "udp_port <n>" announcement
                      session->set_on_message(
                          [this](const std::string& line)
                          {
                            if (line.rfind("udp_port ", 0) == 0)
                            {
                              try
                              {
                                uint16_t p = static_cast<uint16_t>(std::stoi(line.substr(9)));
                                auto sess = net::p2p::Service::instance().client_session();
                                if (sess)
                                {
                                  auto rep = sess->socket().remote_endpoint();
                                  auto udp_tx = std::make_shared<net::p2p::UdpSender>(
                                      net::p2p::Runtime::instance().io(), asio::ip::udp::endpoint(rep.address(), p));
                                  net::p2p::Service::instance().set_client_udp_sender(udp_tx);
                                  std::cerr << "[p2p][client] Set UDP dest to " << rep.address().to_string() << ":" << p
                                            << std::endl;
                                }
                              }
                              catch (...)
                              {
                              }
                              return; // don't forward control line to UI
                            }
                          });
                      gui::framework::post_to_ui([] { gui::framework::set_window_scene<SenderScene>(); });
                    });
                // keep reference while active using key
                const std::string key = session->socket().remote_endpoint().address().to_string() + ":" +
                                        std::to_string(session->socket().remote_endpoint().port());
                sessions_[key] = session;
              });
        }
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
