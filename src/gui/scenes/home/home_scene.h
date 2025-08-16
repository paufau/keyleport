// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "gui/framework/ui_scene.h"
#include "networking/p2p/DiscoveryServer.h"
#include "networking/p2p/SessionServer.h"

#include <asio.hpp>
#include <memory>
#include <thread>
#include <unordered_map>

class HomeScene : public gui::framework::UIScene
{
public:
  void didMount() override;
  void willUnmount() override;
  ~HomeScene() override;

private:
  void render() override;

  // P2P components
  std::unique_ptr<asio::io_context> io_;
  std::unique_ptr<net::p2p::SessionServer> session_server_;
  std::unique_ptr<net::p2p::DiscoveryServer> discovery_;
  std::thread io_thread_;

  // Track active outgoing sessions to send messages to peers (key: ip:port)
  std::unordered_map<std::string, std::shared_ptr<net::p2p::Session>> sessions_;
  std::string instance_id_;
  uint64_t boot_id_ = 1;
};
