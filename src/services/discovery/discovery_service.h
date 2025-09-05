#pragma once

#include "networking/p2p/udp_broadcast_server.h"
#include "services/service_lifecycle_listener.h"

#include <memory>
#include <string>
#include <vector>

namespace services
{
  enum class discovered_peer_state
  {
    idle,
    busy
  };

  struct discovered_peer
  {
    std::string device_id;
    std::string device_name;
    std::string ip_address;
    discovered_peer_state state = discovered_peer_state::idle;
  };

  class discovery_service : public service_lifecycle_listener
  {
  public:
    discovery_service();
    ~discovery_service() override;

    void init() override;
    void update() override;
    void cleanup() override;

    std::vector<discovered_peer> discovered_peers;

  private:
    std::unique_ptr<p2p::udp_broadcast_server> broadcast_server_;
  };
} // namespace services