#include "./discovery_service.h"

#include "networking/p2p/message.h"
#include "networking/p2p/peer.h"
#include "networking/p2p/udp_broadcast_server.h"
#include "networking/p2p/udp_server_configuration.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

services::discovery_service::discovery_service() = default;

services::discovery_service::~discovery_service() = default;

void services::discovery_service::init()
{
  p2p::udp_server_configuration config;
  config.set_port(8800);

  broadcast_server_ = std::make_unique<p2p::udp_broadcast_server>(config);

  // Subscribe to incoming messages
  broadcast_server_->on_message.subscribe(
      [this](const p2p::message& msg)
      {
        // Process incoming message and update discovered_peers list
        // For simplicity, assume payload is device_id and device_name
        // by comma
        std::string payload = msg.get_payload();
        size_t delimiter_pos = payload.find(',');
        if (delimiter_pos != std::string::npos)
        {
          discovered_peer peer;
          peer.device_id = payload.substr(0, delimiter_pos);
          peer.device_name = payload.substr(delimiter_pos + 1);
          peer.ip_address = msg.get_from().get_ip_address();

          // Check if peer already exists
          auto it =
              std::find_if(discovered_peers.begin(), discovered_peers.end(),
                           [&peer](const discovered_peer& p)
                           { return p.device_id == peer.device_id; });
          if (it == discovered_peers.end())
          {
            discovered_peers.push_back(peer);
          }
        }
      });
}

void services::discovery_service::update()
{
  if (broadcast_server_)
  {
    broadcast_server_->poll_events();
  }
}

void services::discovery_service::cleanup()
{
  broadcast_server_.reset();
}