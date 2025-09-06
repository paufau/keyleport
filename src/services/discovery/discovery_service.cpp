#include "./discovery_service.h"

#include "entities/connection_candidate.h"
#include "networking/p2p/message.h"
#include "networking/p2p/peer.h"
#include "networking/p2p/udp_broadcast_server.h"
#include "networking/p2p/udp_server_configuration.h"
#include "services/discovery/discovery_peer.h"
#include "states/connection/connection_state.h"
#include "store.h"
#include "utils/date/date.h"
#include "utils/device_name/device_name.h"
#include "utils/get_platform/platform.h"
#include "utils/random_id/random_id.h"

#include <algorithm>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

services::discovery_service::discovery_service() = default;

services::discovery_service::~discovery_service() = default;

void services::discovery_service::remove_stale_peers()
{
  uint64_t now = utils::date::now();

  auto it = discovered_peers.begin();
  auto ts_it = peer_last_seen_timestamps_.begin();

  while (it != discovered_peers.end() &&
         ts_it != peer_last_seen_timestamps_.end())
  {
    if (now - *ts_it > static_cast<uint64_t>(peer_stale_timeout_ms_))
    {
      it = discovered_peers.erase(it);
      ts_it = peer_last_seen_timestamps_.erase(ts_it);
    }
    else
    {
      ++it;
      ++ts_it;
    }
  }
}

void services::discovery_service::update_peer_state(discovery_peer& peer)
{
  auto it = std::find_if(discovered_peers.begin(), discovered_peers.end(),
                         [&peer](const discovery_peer& p)
                         { return p.device_id == peer.device_id; });

  uint64_t now = utils::date::now();

  if (it != discovered_peers.end())
  {
    // Update existing peer
    *it = peer;
    size_t index = std::distance(discovered_peers.begin(), it);
    if (index < peer_last_seen_timestamps_.size())
    {
      peer_last_seen_timestamps_[index] = now;
    }
  }
  else
  {
    // Add new peer
    discovered_peers.push_back(peer);
    peer_last_seen_timestamps_.push_back(now);
  }

  remove_stale_peers();
}

void services::discovery_service::broadcast_own_state()
{
  uint64_t now = utils::date::now();
  bool should_broadcast =
      now - static_cast<uint64_t>(last_broadcast_time_ms_) >
          static_cast<uint64_t>(state_broadcast_interval_ms_) ||
      last_broadcast_time_ms_ == 0;

  if (should_broadcast && broadcast_client_)
  {
    last_broadcast_time_ms_ = static_cast<int>(now);
    broadcast_client_->broadcast(self_peer.encode());
  }
}

void services::discovery_service::update_connection_candidates()
{
  std::vector<entities::ConnectionCandidate> candidates;
  for (const auto& peer : discovered_peers)
  {
    if (peer.ip_address == self_ip_address_)
    {
      continue;
    }
    candidates.emplace_back(peer.state == discovery_peer_state::busy,
                            peer.device_name, peer.ip_address,
                            std::to_string(default_peer_port_));
  }
  store::connection_state().available_devices.set(candidates);
}

void services::discovery_service::init()
{
  self_peer.device_id = get_random_id();
  self_peer.device_name = get_device_name();
  self_peer.ip_address = self_ip_address_;
  self_peer.platform = get_platform();
  self_peer.state = discovery_peer_state::idle;

  p2p::udp_server_configuration config;
  config.set_port(default_peer_port_);

  p2p::udp_client_configuration client_config;
  client_config.set_port(default_peer_port_);

  broadcast_server_ = std::make_unique<p2p::udp_broadcast_server>(config);
  broadcast_client_ =
      std::make_unique<p2p::udp_broadcast_client>(client_config);

  broadcast_server_->on_message.subscribe(
      [this](const p2p::message& msg)
      {
        discovery_peer peer = discovery_peer::decode(msg.get_payload());
        // Always trust the packet's real sender IP so we don't rely on what
        // the other side put in payload (could be blank).
        peer.ip_address = msg.get_from().get_ip_address();

        if ((peer.ip_address == self_ip_address_))
        {
          return;
        }

        update_peer_state(peer);
      });

  broadcast_own_state();
}

void services::discovery_service::update()
{
  if (broadcast_server_)
  {
    broadcast_server_->poll_events();
  }

  broadcast_own_state();
  remove_stale_peers();
  update_connection_candidates();
}

void services::discovery_service::cleanup()
{
  broadcast_server_.reset();
  broadcast_client_.reset();
  discovered_peers.clear();
  peer_last_seen_timestamps_.clear();
  last_broadcast_time_ms_ = 0;
}