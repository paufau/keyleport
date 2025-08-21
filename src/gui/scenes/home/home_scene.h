// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "gui/framework/ui_scene.h"
#include <memory>
#include <unordered_map>

namespace net
{
  namespace p2p
  {
    class Session;
  }
}

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
  // Track active outgoing sessions to send messages to peers (key: ip:port)
  std::unordered_map<std::string, std::shared_ptr<net::p2p::Session>> sessions_;
};
