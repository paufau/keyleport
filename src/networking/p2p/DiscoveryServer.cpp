#include "DiscoveryServer.h"

#include <iostream>
#include <nlohmann/json.hpp>

namespace net
{
  namespace p2p
  {

    DiscoveryServer::DiscoveryServer(asio::io_context& io, const std::string& instance_id, uint64_t boot_id,
                                     uint16_t session_port)
        : io_(io), instance_id_(instance_id), boot_id_(boot_id), session_port_(session_port),
          multicast_endpoint_(asio::ip::make_address(MULTICAST_ADDR), DISCOVERY_PORT), socket_(io), heartbeat_timer_(io)
    {
      // Open UDP socket and join multicast group
      asio::ip::udp::endpoint listen_ep(asio::ip::udp::v4(), DISCOVERY_PORT);
      socket_.open(listen_ep.protocol());
      socket_.set_option(asio::ip::udp::socket::reuse_address(true));
      socket_.bind(listen_ep);
      // Join the multicast group on all interfaces (best-effort)
      socket_.set_option(asio::ip::multicast::join_group(asio::ip::make_address(MULTICAST_ADDR)));
      // Ensure multicast traffic can traverse one hop (same subnet) and reflect locally for self-discovery during dev
      asio::ip::multicast::enable_loopback loop_opt(true);
      asio::ip::multicast::hops hops_opt(1);
      asio::error_code ec;
      socket_.set_option(loop_opt, ec);
      socket_.set_option(hops_opt, ec);
      // Allow UDP broadcast fallbacks
      socket_.set_option(asio::socket_base::broadcast(true), ec);
      // Choose a sensible outbound interface for multicast on platforms that require it (e.g., Windows)
      {
        auto best_ip = get_best_local_ip(io_);
        asio::error_code ec_if;
        auto v4 = asio::ip::make_address_v4(best_ip, ec_if);
        if (!ec_if)
        {
          socket_.set_option(asio::ip::multicast::outbound_interface(v4), ec);
        }
      }
      (void)ec;
    }

    void DiscoveryServer::set_state(State s)
    {
      state_ = s;
    }

    void DiscoveryServer::start()
    {
      start_receive();
      schedule_heartbeat();
      send_discover(); // Kick things off
    }

    void DiscoveryServer::send_discover()
    {
      Message m = make_discover(instance_id_, boot_id_);
      m.ip = get_best_local_ip(io_);
      m.port = session_port_;
      std::cerr << "[p2p][disc] send DISCOVER via multicast and broadcast from ip=" << m.ip << " port=" << m.port
                << std::endl;
      send_message(m);
      send_broadcast(m);
    }

    void DiscoveryServer::send_status()
    {
      Message m = make_status(instance_id_, boot_id_, state_ == State::Busy, get_best_local_ip(io_), session_port_);
      std::cerr << "[p2p][disc] send STATUS '" << m.state << "'" << std::endl;
      send_message(m);
      send_broadcast(m);
    }

    void DiscoveryServer::start_receive()
    {
      socket_.async_receive_from(asio::buffer(recv_buffer_), sender_endpoint_,
                                 [this](std::error_code ec, std::size_t bytes_recvd)
                                 {
                                   if (!ec && bytes_recvd > 0)
                                   {
                                     std::string payload(recv_buffer_.data(), bytes_recvd);
                                     std::cerr << "[p2p][disc] <= from " << sender_endpoint_ << ": " << payload
                                               << std::endl;
                                     handle_receive(payload, sender_endpoint_);
                                   }
                                   start_receive();
                                 });
    }

    void DiscoveryServer::handle_receive(const std::string& payload, const asio::ip::udp::endpoint& remote)
    {
      Message m;
      if (!parse_json(payload, m))
      {
        return;
      }
      if (m.proto != PROTOCOL)
      {
        return;
      }
      if (m.from == instance_id_)
      {
        return; // Ignore self
      }

      // Update peer entry
      Peer& p = peer_table_[m.from];
      p.instance_id = m.from;
      p.boot_id = m.boot;
      // Prefer the source address observed by us; fall back to advertised IP if needed
      p.ip_address = remote.address().to_string();
      if (p.ip_address.empty() && !m.ip.empty())
      {
        p.ip_address = m.ip;
      }
      if (m.port)
      {
        p.session_port = m.port;
      }
      if (!m.state.empty())
      {
        p.state = state_from_string(m.state);
      }
      p.last_heartbeat = std::chrono::steady_clock::now();

      if (on_peer_update_)
      {
        on_peer_update_(p);
      }

      // If DISCOVER from peer, respond with ANNOUNCE directly to sender (unicast) for reliability across OSes
      if (m.type == "DISCOVER")
      {
        Message reply = make_announce(instance_id_, boot_id_, get_best_local_ip(io_), session_port_, state_);
        // Unicast back to the discoverer
        asio::ip::udp::endpoint unicast_dest(remote.address(), remote.port());
        std::cerr << "[p2p][disc] => ANNOUNCE (unicast) to " << unicast_dest << std::endl;
        send_message_to(reply, unicast_dest);
        // Also multicast to help others update
        std::cerr << "[p2p][disc] => ANNOUNCE (multicast)" << std::endl;
        send_message(reply);
      }
    }

    void DiscoveryServer::send_message(const Message& m)
    {
      auto data = dump_json(m);
      socket_.async_send_to(asio::buffer(data), multicast_endpoint_, [](std::error_code, std::size_t) {});
    }

    void DiscoveryServer::send_message_to(const Message& m, const asio::ip::udp::endpoint& dest)
    {
      auto data = dump_json(m);
      socket_.async_send_to(asio::buffer(data), dest, [](std::error_code, std::size_t) {});
    }

    void DiscoveryServer::send_broadcast(const Message& m)
    {
      // Limited broadcast 255.255.255.255 on the discovery port
      asio::ip::udp::endpoint bcast(asio::ip::address_v4::broadcast(), DISCOVERY_PORT);
      auto data = dump_json(m);
      asio::error_code ec;
      socket_.send_to(asio::buffer(data), bcast, 0, ec);
      (void)ec; // best-effort
    }

    void DiscoveryServer::schedule_heartbeat()
    {
      heartbeat_timer_.expires_after(std::chrono::seconds(2));
      heartbeat_timer_.async_wait(
          [this](std::error_code ec)
          {
            if (ec)
            {
              return;
            }
            // Heartbeat as ANNOUNCE with current state
            Message hb = make_announce(instance_id_, boot_id_, get_best_local_ip(io_), session_port_, state_);
            send_message(hb);
            // Remove peers that haven't heartbeated for a while
            prune_stale_peers();
            schedule_heartbeat();
          });
    }

    void DiscoveryServer::prune_stale_peers()
    {
      const auto now = std::chrono::steady_clock::now();
      // Consider peers stale after 7 seconds without heartbeat
      const auto ttl = std::chrono::seconds(7);
      for (auto it = peer_table_.begin(); it != peer_table_.end();)
      {
        const Peer& p = it->second;
        if (now - p.last_heartbeat > ttl)
        {
          if (on_peer_remove_)
          {
            on_peer_remove_(p);
          }
          it = peer_table_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    std::string DiscoveryServer::get_best_local_ip(asio::io_context& io)
    {
      try
      {
        // Use a UDP socket connected to a public IP to infer the outbound interface
        asio::ip::udp::socket s(io, asio::ip::udp::v4());
        asio::ip::udp::endpoint dst(asio::ip::make_address("8.8.8.8"), 53);
        s.connect(dst);
        auto local = s.local_endpoint().address();
        if (local.is_v4() && !local.is_loopback())
        {
          return local.to_string();
        }
      }
      catch (...)
      {
        try
        {
          asio::ip::tcp::resolver res(io);
          auto results = res.resolve(asio::ip::host_name(), "");
          for (auto& e : results)
          {
            auto addr = e.endpoint().address();
            if (addr.is_v4() && !addr.is_loopback())
            {
              return addr.to_string();
            }
          }
        }
        catch (...)
        {
        }
      }
      return std::string("127.0.0.1");
    }

  } // namespace p2p
} // namespace net
