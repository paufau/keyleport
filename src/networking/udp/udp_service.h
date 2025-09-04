// udp_service: low-level UDP networking service with ENet for
// reliability/unreliability channels. Uses ENet for data plane and POSIX UDP
// sockets for LAN broadcast discovery.

#pragma once

#include <atomic>
#include <chrono>
#include <enet/enet.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Platform socket type
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

#include "networking/udp/events.h"
#include "networking/udp/peer_connection.h"
#include "utils/event_emitter/event_emitter.h"

namespace net
{
  namespace udp
  {

    // Event payloads are declared in events.h

    // A minimal UDP service that supports:
    // - broadcast discovery messages
    // - connect/disconnect book-keeping
    // - send_reliable (simulated via resend on a dedicated strand) and
    // send_unreliable
    // - events for broadcast, connect, disconnect, and received data
    class udp_service
    {
    public:
      udp_service();
      ~udp_service();

      // Non-copyable
      udp_service(const udp_service&) = delete;
      udp_service& operator=(const udp_service&) = delete;

      // listen_port: ENet host port for data plane.
      // broadcast_port: UDP port used for LAN discovery broadcasts (defaults to
      // listen_port+1 or 33334).
      bool start(int listen_port = 0, int broadcast_port = 0);
      void stop();

      // Broadcast a small discovery payload on the local subnet.
      void broadcast(const std::string& data);

      // Maintain a logical connection map (address:port) even though UDP is
      // connectionless.
      peer_connection_ptr connect_to(const std::string& address, int port);
      // Per-peer messaging is available via peer_connection.

      // Flush pending ENet sends (used by peer_connection)
      void flush();

      // Introspection
      int enet_port() const { return listen_port_; }
      int discovery_broadcast_port() const { return broadcast_port_; }

      // Events
      event_emitter<network_event_broadcast> on_receive_broadcast;
      event_emitter<network_event_connect> on_connect;
      event_emitter<network_event_disconnect> on_disconnect;
      // Emitted whenever a peer_connection wrapper is created for a connected
      // peer
      event_emitter<std::shared_ptr<peer_connection>> on_new_peer;

      // Thread-safe wrappers for ENet operations used by peer_connection
      void send_reliable_packet(_ENetPeer* peer, const std::string& data);
      void send_unreliable_packet(_ENetPeer* peer, const std::string& data);
      void disconnect_peer(_ENetPeer* peer);

    private:
      void run_service_loop_();
      void run_broadcast_recv_loop_();
      void compute_broadcast_targets_();

      std::atomic<bool> running_{false};
      std::thread service_thread_;
      std::thread broadcast_thread_;
      int listen_port_{0};
      int broadcast_port_{0};

      // ENet host for connections and data
      bool enet_inited_{false};
      ENetHost* host_{nullptr};
      mutable std::mutex host_mutex_;

      // Maintain peers map (key "ip:port" -> ENetPeer*)
      std::unordered_map<std::string, ENetPeer*> peers_;
      std::mutex peers_mutex_;
      // Map ENetPeer* to peer_connection wrapper to route per-peer events
      std::unordered_map<ENetPeer*, std::shared_ptr<peer_connection>>
          peer_wrappers_;
      // optional weak map of live peer_connection wrappers can be added later

      // UDP sockets for broadcast send/receive
      socket_t bcast_send_sock_
#ifdef _WIN32
          {INVALID_SOCKET}
#else
          {-1}
#endif
      ;
      socket_t bcast_recv_sock_
#ifdef _WIN32
          {INVALID_SOCKET}
#else
          {-1}
#endif
      ;

      // List of broadcast targets (255.255.255.255 and per-interface directed
      // broadcast)
      std::vector<sockaddr_in> bcast_targets_;

#ifdef _WIN32
      bool wsa_inited_{false};
#endif
    };

  } // namespace udp
} // namespace net
