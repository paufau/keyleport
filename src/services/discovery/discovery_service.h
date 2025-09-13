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
    discovery_peer self_peer;

  private:
    std::unique_ptr<p2p::udp_broadcast_client> broadcast_client_;
    std::unique_ptr<p2p::udp_broadcast_server> broadcast_server_;

    std::vector<uint64_t> peer_last_seen_timestamps_;

    void remove_stale_peers();
    // Updates or inserts a peer. Returns true if a new peer was added.
    bool update_peer_state(discovery_peer& peer);
    // Broadcast our discovery state. If force is true, bypass interval gating.
    void broadcast_own_state(bool force = false);
    void update_connection_candidates();

  // Remove a peer (and its timestamp) by device id. Returns true if found.
  bool remove_peer_by_device_id(const std::string& device_id);

    uint64_t last_broadcast_time_ms_ = 0;
    int state_broadcast_interval_ms_ = 5000;
    int peer_stale_timeout_ms_ = 15000;
    int default_peer_port_ = 8800;
    std::string self_ip_address_ = "127.0.0.1";
  };
} // namespace services