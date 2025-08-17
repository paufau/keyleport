#pragma once

#include "Message.h"
#include "Peer.h"

#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace net
{
  namespace p2p
  {

    class DiscoveryServer
    {
    public:
      using PeerTable = std::unordered_map<std::string, Peer>; // key: instance_id
      using PeerUpdateHandler = std::function<void(const Peer&)>;

      DiscoveryServer(asio::io_context& io, const std::string& instance_id, uint64_t boot_id, uint16_t session_port);

      void set_state(State s);
      State state() const { return state_; }

      void set_on_peer_update(PeerUpdateHandler cb) { on_peer_update_ = std::move(cb); }

      const PeerTable& peers() const { return peer_table_; }

      // Begin async operations (receive + heartbeats)
      void start();

      // Send immediate discover
      void send_discover();

      // Broadcast a one-off status (busy/free)
      void send_status();

      // Utilities
      static std::string get_best_local_ip(asio::io_context& io);

    private:
      void start_receive();
      void handle_receive(const std::string& payload, const asio::ip::udp::endpoint& remote);

      void send_message(const Message& m);
  void send_message_to(const Message& m, const asio::ip::udp::endpoint& dest);
      void schedule_heartbeat();

      asio::io_context& io_;
      std::string instance_id_;
      uint64_t boot_id_ = 0;
      uint16_t session_port_ = 0;
      State state_ = State::Idle;

      asio::ip::udp::endpoint multicast_endpoint_;
      asio::ip::udp::socket socket_;
      asio::steady_timer heartbeat_timer_;

      std::array<char, 2048> recv_buffer_{};
      asio::ip::udp::endpoint sender_endpoint_;

      PeerUpdateHandler on_peer_update_;
      PeerTable peer_table_;
    };

  } // namespace p2p
} // namespace net
