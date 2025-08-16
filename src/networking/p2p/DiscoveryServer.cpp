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
      socket_.set_option(asio::ip::multicast::join_group(asio::ip::make_address(MULTICAST_ADDR)));
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
      send_message(m);
    }

    void DiscoveryServer::send_status()
    {
      Message m = make_status(instance_id_, boot_id_, state_ == State::Busy, get_best_local_ip(io_), session_port_);
      send_message(m);
    }

    void DiscoveryServer::start_receive()
    {
      socket_.async_receive_from(asio::buffer(recv_buffer_), sender_endpoint_,
                                 [this](std::error_code ec, std::size_t bytes_recvd)
                                 {
                                   if (!ec && bytes_recvd > 0)
                                   {
                                     std::string payload(recv_buffer_.data(), bytes_recvd);
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
      p.ip_address = !m.ip.empty() ? m.ip : remote.address().to_string();
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

      // If DISCOVER or ANNOUNCE from peer, respond with ANNOUNCE
      if (m.type == "DISCOVER")
      {
        Message reply = make_announce(instance_id_, boot_id_, get_best_local_ip(io_), session_port_, state_);
        send_message(reply);
      }
    }

    void DiscoveryServer::send_message(const Message& m)
    {
      auto data = dump_json(m);
      socket_.async_send_to(asio::buffer(data), multicast_endpoint_, [](std::error_code, std::size_t) {});
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
            schedule_heartbeat();
          });
    }

    std::string DiscoveryServer::get_best_local_ip(asio::io_context& io)
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
      return std::string("127.0.0.1");
    }

  } // namespace p2p
} // namespace net
