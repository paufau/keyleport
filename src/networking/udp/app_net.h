// app_net: high-level singleton that wires udp_service + discovery_service
// and exposes a simple API for the app (session start/stop, send, receive, discovery events).

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "networking/udp/discovery_service.h"
#include "networking/udp/event_emitter.h"
#include "networking/udp/peer_connection.h"
#include "networking/udp/peer_info.h"
#include "networking/udp/udp_service.h"

namespace net { namespace udp {

class app_net {
public:
  static app_net& instance();

  // Start/stop network stack. listen_port is used for ENet host and discovery broadcast.
  bool start(int listen_port = 0);
  void stop();

  // Discovery events (forwarded from discovery_service)
  event_emitter<peer_info>& on_discovery();
  event_emitter<peer_info>& on_lose();

  // Session management
  void connect_to_peer(const peer_info& peer);
  void disconnect();

  // Session state and events
  bool has_active_session() const;
  event_emitter<peer_info>& on_session_start();
  event_emitter<peer_info>& on_session_end();

  // Data plane for current session
  void send_reliable(const std::string& data);
  void send_unreliable(const std::string& data);

  using on_receive_cb = std::function<void(const std::string&)>;
  void set_on_receive(on_receive_cb cb);

private:
  app_net() = default;
  app_net(const app_net&) = delete;
  app_net& operator=(const app_net&) = delete;

  void bind_receive_handler_();
  void clear_receive_handler_();
  void handle_incoming_connect_(const network_event_connect& ev);

  mutable std::mutex mtx_;
  udp_service net_{};
  std::unique_ptr<discovery_service> discovery_;

  // current session
  peer_connection_ptr current_conn_;
  std::optional<peer_info> current_peer_;

  // event emitters for app
  event_emitter<peer_info> on_session_start_;
  event_emitter<peer_info> on_session_end_;

  // receive callback hook for UI (receiver scene)
  on_receive_cb on_receive_;
  event_emitter<network_event_data>::subscription_id recv_sub_id_{0};

  // Safe fallback emitters when discovery_ is not ready
  event_emitter<peer_info> discovery_fallback_on_discovery_;
  event_emitter<peer_info> discovery_fallback_on_lose_;
};

}} // namespace net::udp
