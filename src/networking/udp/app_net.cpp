#include "networking/udp/app_net.h"

#include <iostream>

namespace net
{
  namespace udp
  {

    app_net& app_net::instance()
    {
      static app_net inst;
      return inst;
    }

    bool app_net::start(int listen_port)
    {
      std::lock_guard<std::mutex> lock(mtx_);
      // Default to ephemeral ENet port (0) to reduce conflicts across multiple
      // instances
      const int enet_port = listen_port; // 0 means ephemeral
      const int bcast_port = (enet_port > 0) ? enet_port + 1 : 33334;
      if (!net_.start(enet_port, bcast_port))
      {
        // still run discovery to allow connecting to others
        discovery_.reset();
        net_.stop();
        return false;
      }
      discovery_ = std::make_unique<discovery_service>(net_);
      // Announce the actual ENet port (which may be ephemeral if 0 was
      // requested)
      discovery_->start_discovery(net_.enet_port());
      // Accept inbound connections
      net_.on_connect.subscribe([this](const network_event_connect& ev)
                                { handle_incoming_connect_(ev); });
      // Adopt inbound peer connections as the active session if we're idle
      net_.on_new_peer.subscribe(
          [this](const std::shared_ptr<peer_connection>& pc)
          {
            std::lock_guard<std::mutex> lock(mtx_);
            if (current_conn_)
            {
              return; // already have an active (likely outbound) session
            }
            if (!pc)
            {
              return;
            }
            peer_info peer{};
            peer.device_id = pc->address() + ":" + std::to_string(pc->port());
            peer.device_name =
                ""; // unknown; can be enriched from discovery later
            peer.ip_address = pc->address();
            peer.port = pc->port();
            current_conn_ = pc;
            current_peer_ = peer;
            bind_receive_handler_();
            on_session_start_.emit(peer);
          });
      return true;
    }

    void app_net::stop()
    {
      std::lock_guard<std::mutex> lock(mtx_);
      clear_receive_handler_();
      current_conn_.reset();
      current_peer_.reset();
      if (discovery_)
      {
        discovery_->stop_discovery();
        discovery_.reset();
      }
      net_.stop();
    }

    event_emitter<peer_info>& app_net::on_discovery()
    {
      if (discovery_)
      {
        return discovery_->on_discovery;
      }
      return discovery_fallback_on_discovery_;
    }

    event_emitter<peer_info>& app_net::on_lose()
    {
      if (discovery_)
      {
        return discovery_->on_lose;
      }
      return discovery_fallback_on_lose_;
    }

    void app_net::connect_to_peer(const peer_info& peer)
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (current_conn_)
      {
        current_conn_->disconnect();
        clear_receive_handler_();
        current_conn_.reset();
        current_peer_.reset();
      }
      auto conn = net_.connect_to(peer.ip_address, peer.port);
      if (!conn)
      {
        return;
      }
      current_conn_ = conn;
      current_peer_ = peer;
      bind_receive_handler_();
      // Outbound connect should not trigger receiver-side UI transition.
    }

    void app_net::disconnect()
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (!current_conn_)
      {
        return;
      }
      auto peer = current_peer_;
      current_conn_->disconnect();
      clear_receive_handler_();
      current_conn_.reset();
      current_peer_.reset();
      if (peer)
      {
        on_session_end_.emit(*peer);
      }
    }

    bool app_net::has_active_session() const
    {
      std::lock_guard<std::mutex> lock(mtx_);
      return static_cast<bool>(current_conn_);
    }

    event_emitter<peer_info>& app_net::on_session_start()
    {
      return on_session_start_;
    }

    event_emitter<peer_info>& app_net::on_session_end()
    {
      return on_session_end_;
    }

    void app_net::send_reliable(const std::string& data)
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (current_conn_)
      {
        current_conn_->send_reliable(data);
      }
    }

    void app_net::send_unreliable(const std::string& data)
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (current_conn_)
      {
        current_conn_->send_unreliable(data);
      }
    }

    void app_net::set_on_receive(on_receive_cb cb)
    {
      std::lock_guard<std::mutex> lock(mtx_);
      on_receive_ = std::move(cb);
    }

    void app_net::bind_receive_handler_()
    {
      clear_receive_handler_();
      if (current_conn_)
      {
        recv_sub_id_ = current_conn_->on_receive_data.subscribe(
            [this](const network_event_data& ev)
            {
              if (on_receive_)
              {
                on_receive_(ev.data);
              }
            });
      }
    }

    void app_net::clear_receive_handler_()
    {
      if (current_conn_ && recv_sub_id_ != 0)
      {
        current_conn_->on_receive_data.unsubscribe(recv_sub_id_);
        recv_sub_id_ = 0;
      }
    }

    void app_net::handle_incoming_connect_(const network_event_connect&)
    {
      // For now, inbound connect is auto-handled via ENet; app initiates
      // session explicitly.
    }

  } // namespace udp
} // namespace net
