#pragma once

#include "networking/p2p/udp_broadcast_server.h"
#include "services/discovery/discovery_peer.h"
#include "services/service_lifecycle_listener.h"

#include <memory>
#include <networking/p2p/udp_broadcast_client.h>
#include <string>
#include <vector>

namespace services
{
  class discovery_service : public service_lifecycle_listener
  {
  public:
    discovery_service();
    ~discovery_service() override;

    void init() override;
    void update() override;
    void cleanup() override;

    std::vector<discovery_peer> discovered_peers;

  private:
    std::unique_ptr<p2p::udp_broadcast_client> broadcast_client_;
    std::unique_ptr<p2p::udp_broadcast_server> broadcast_server_;

    std::vector<uint64_t> peer_last_seen_timestamps_;

    void remove_stale_peers();
    void update_peer_state(discovery_peer& peer);
    void broadcast_own_state();
    void update_connection_candidates();

    int last_broadcast_time_ms_ = 0;
    int state_broadcast_interval_ms_ = 5000;
    int peer_stale_timeout_ms_ = 15000;
    int default_peer_port_ = 8800;
    std::string self_ip_address_ = "127.0.0.1";
    discovery_peer self_peer_;
  };
} // namespace services