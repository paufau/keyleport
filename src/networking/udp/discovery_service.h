// discovery_service: maintains a set of discovered peers based on UDP broadcasts.
// Emits on_discovery when a new peer is found and on_lose when a peer times out.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "networking/udp/event_emitter.h"
#include "networking/udp/udp_service.h"
#include "networking/udp/peer_info.h"

namespace net { namespace udp {

// peer_state and peer_info are provided by peer_info.h

class discovery_service {
public:
  explicit discovery_service(udp_service& net);
  ~discovery_service();

  void start_discovery(int listen_port = 33333, std::chrono::seconds lose_after = std::chrono::seconds(5));
  void stop_discovery();

  std::vector<peer_info> get_known_peers();

  event_emitter<peer_info> on_discovery;
  event_emitter<peer_info> on_lose;

private:
  void loop_broadcast_();
  void loop_expire_();

  udp_service& net_;
  std::unordered_map<std::string, peer_info> peers_; // key = device_id
  std::mutex mutex_;

  std::atomic<bool> running_{false};
  std::thread broadcast_thread_;
  std::thread expire_thread_;
  std::chrono::seconds lose_after_{5};
  int listen_port_{33333};
};

}} // namespace net::udp
