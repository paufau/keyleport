#include "runtime.h"

#include "service.h"

#include <iostream>
#include <random>

namespace
{
  static std::string gen_id()
  {
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(rng), b = dist(rng);
    char buf[37];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx", (unsigned)(a & 0xffffffff),
                  (unsigned)((a >> 32) & 0xffff), (unsigned)(b & 0xffff), (unsigned)((b >> 16) & 0xffff),
                  (unsigned long long)(b >> 32));
    return std::string(buf);
  }
} // namespace

namespace net
{
  namespace p2p
  {
    Runtime& Runtime::instance()
    {
      static Runtime r;
      return r;
    }

    void Runtime::start()
    {
      if (io_)
      {
        return; // already started
      }

      io_.reset(new asio::io_context());
      instance_id_ = gen_id();

      // Session server on ephemeral port
      session_server_.reset(new SessionServer(*io_, 0));
      const uint16_t sess_port = session_server_->port();

      start_accepting();

      // Discovery server
      discovery_.reset(new DiscoveryServer(*io_, instance_id_, boot_id_, sess_port));
      discovery_->set_state(State::Idle);
      discovery_->start();

      // Run io in background
      io_thread_ = std::thread([this]() { io_->run(); });
    }

    void Runtime::start_accepting()
    {
      if (!session_server_)
      {
        return;
      }

      session_server_->async_accept(
          [this](std::shared_ptr<Session> s)
          {
            std::cerr << "[p2p] Incoming TCP session from " << s->socket().remote_endpoint() << std::endl;
            Service::instance().set_server_session(s);
            // Create UDP receiver on ephemeral port; share port over TCP after handshake
            std::shared_ptr<UdpReceiver> udp_rx;
            if (io_)
            {
              udp_rx = std::make_shared<UdpReceiver>(*io_, 0);
              udp_rx->start();
              Service::instance().set_server_udp_receiver(udp_rx);
            }
            s->start_server(
                [this, s, udp_rx]
                {
                  if (discovery_)
                  {
                    discovery_->set_state(State::Busy);
                  }

                  // Inform UI about peer for ReceiverScene
                  try
                  {
                    const std::string peer_ip = s->socket().remote_endpoint().address().to_string();
                    const std::string peer_port = std::to_string(s->socket().remote_endpoint().port());
                    if (on_server_ready_)
                    {
                      on_server_ready_(peer_ip, peer_port);
                    }
                  }
                  catch (...)
                  {
                  }

                  // Inform peer about our UDP port
                  if (udp_rx)
                  {
                    uint16_t udp_port = udp_rx->port();
                    s->send_line(std::string("udp_port ") + std::to_string(udp_port) + "\n");
                    std::cerr << "[p2p][server] Announced UDP port " << udp_port << " to client" << std::endl;
                  }
                });
          });
    }

    void Runtime::stop()
    {
      if (!io_)
      {
        return;
      }

      if (discovery_)
      {
        discovery_->stop();
      }
      io_->stop();

      if (io_thread_.joinable())
      {
        if (std::this_thread::get_id() == io_thread_.get_id())
        {
          io_thread_.detach();
        }
        else
        {
          io_thread_.join();
        }
      }

      Service::instance().stop();
      session_server_.reset();
      discovery_.reset();
      io_.reset();
    }

    asio::io_context& Runtime::io()
    {
      return *io_;
    }

    void Runtime::set_discovery_state(State s)
    {
      if (discovery_)
      {
        discovery_->set_state(s);
      }
    }

    State Runtime::discovery_state() const
    {
      if (discovery_)
      {
        return discovery_->state();
      }
      return State::Idle;
    }

    uint16_t Runtime::session_port() const
    {
      if (session_server_)
      {
        return session_server_->port();
      }
      return 0;
    }

    void Runtime::set_on_peer_update(DiscoveryServer::PeerUpdateHandler cb)
    {
      if (discovery_)
      {
        discovery_->set_on_peer_update(std::move(cb));
      }
    }

    void Runtime::set_on_peer_remove(DiscoveryServer::PeerRemoveHandler cb)
    {
      if (discovery_)
      {
        discovery_->set_on_peer_remove(std::move(cb));
      }
    }

    void Runtime::set_on_server_ready(std::function<void(const std::string& ip, const std::string& port)> cb)
    {
      on_server_ready_ = std::move(cb);
    }

  } // namespace p2p
} // namespace net
