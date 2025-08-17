#include "home_scene.h"

#include "gui/framework/ui_dispatch.h"
#include "gui/framework/ui_window.h"
#include "gui/scenes/receiver/receiver_scene.h"
#include "gui/scenes/sender/sender_scene.h"
#include "networking/p2p/DiscoveryServer.h"
#include "networking/p2p/SessionServer.h"
#include "networking/p2p/service.h"
#include "store.h"

#include <algorithm>
#include <imgui.h>
#include <iostream>
#include <random>

static std::string gen_id()
{
  std::random_device rd;
  std::mt19937_64 rng(rd());
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(rng), b = dist(rng);
  char buf[37];
  std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx", (unsigned)(a & 0xffffffff),
                (unsigned)((a >> 32) & 0xffff), (unsigned)(b & 0xffff), (unsigned)((b >> 16) & 0xffff),
                (unsigned long long)(b >> 32));
  return std::string(buf);
}

void HomeScene::didMount()
{
  // Reset devices list
  store::connection_state().available_devices.get().clear();

  // Prepare io + servers
  io_.reset(new asio::io_context());
  instance_id_ = gen_id();

  // TCP session server on ephemeral port
  session_server_.reset(new net::p2p::SessionServer(*io_, 0));
  const uint16_t sess_port = session_server_->port();

  // Accept incoming sessions and handle handshake
  session_server_->async_accept(
      [this](std::shared_ptr<net::p2p::Session> s)
      {
        std::cerr << "[p2p] Incoming TCP session from " << s->socket().remote_endpoint() << std::endl;
        // Register server session globally for receiver scene to consume
        net::p2p::Service::instance().set_server_session(s);
        s->start_server(
            [this, s]
            {
              // Mark busy state in discovery when handshaked
              if (discovery_)
              {
                discovery_->set_state(net::p2p::State::Busy);
              }

              // Extract peer endpoint to show in ReceiverScene
              const std::string peer_ip = s->socket().remote_endpoint().address().to_string();
              const std::string peer_port = std::to_string(s->socket().remote_endpoint().port());

              // Update store on UI thread and then switch scene
              gui::framework::post_to_ui(
                  [peer_ip, peer_port]
                  {
                    store::connection_state().connected_device.set(
                        std::make_shared<entities::ConnectionCandidate>(false, /*name*/ peer_ip, peer_ip, peer_port));
                    gui::framework::set_window_scene<ReceiverScene>();
                  });
            });
      });

  // Discovery
  discovery_.reset(new net::p2p::DiscoveryServer(*io_, instance_id_, boot_id_, sess_port));
  discovery_->set_state(net::p2p::State::Idle);
  discovery_->set_on_peer_update(
      [this](const net::p2p::Peer& p)
      {
        // Update devices list in store
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.session_port);
        const bool busy = (p.state == net::p2p::State::Busy);
        const std::string name = p.instance_id;

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
  // Remove peers that went offline
  discovery_->set_on_peer_remove(
      [this](const net::p2p::Peer& p)
      {
        auto& devices = store::connection_state().available_devices.get();
        const std::string ip = p.ip_address;
        const std::string port = std::to_string(p.session_port);
        devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const entities::ConnectionCandidate& d)
                                     { return d.ip() == ip && d.port() == port; }),
                      devices.end());
      });
  discovery_->start();

  // Run IO in background thread
  io_thread_ = std::thread([this]() { io_->run(); });
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
        if (io_)
        {
          auto sock = std::make_shared<asio::ip::tcp::socket>(*io_);
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
                    [session]
                    {
                      std::cerr << "[p2p] Client handshake done; switching to SenderScene" << std::endl;
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
  // Keep P2P IO + sessions alive across scene transitions.
  // They are used by Sender/Receiver scenes via net::p2p::Service and own background io thread.
  // Cleanup is deferred to app shutdown.
}

HomeScene::~HomeScene()
{
  // Final cleanup on application shutdown
  if (io_)
  {
    io_->stop();
  }
  if (io_thread_.joinable())
  {
    if (std::this_thread::get_id() == io_thread_.get_id())
    {
      io_thread_.detach();
    }
    else
    {
      io_thread_.join();
    }
  }
  sessions_.clear();
  discovery_.reset();
  session_server_.reset();
  net::p2p::Service::instance().stop();
  io_.reset();
}
