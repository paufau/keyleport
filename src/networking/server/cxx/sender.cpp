// Cross-platform Sender implemented with Asio (standalone)

#include "networking/server/sender.h"

#include <asio.hpp>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace net
{

  class AsioSender : public Sender
  {
  public:
    AsioSender(std::string ip, int port) : ip_(std::move(ip)), port_(port), io_(), tcp_sock_(io_), udp_sock_(io_) {}

    int connect() override
    {
      asio::error_code ec;
      // TCP connect
      if (!tcp_sock_.is_open())
      {
        asio::ip::tcp::resolver res(io_);
        auto eps = res.resolve(ip_, std::to_string(port_), ec);
        if (ec)
        {
          return 2;
        }
        asio::connect(tcp_sock_, eps, ec);
        if (ec)
        {
          return 2;
        }
        asio::ip::tcp::no_delay nd(true);
        tcp_sock_.set_option(nd, ec);
      }
      // UDP open/connect
      if (!udp_sock_.is_open())
      {
        udp_sock_.open(asio::ip::udp::v4(), ec);
        if (ec)
        {
          return 2;
        }
        asio::ip::udp::endpoint ep(asio::ip::make_address(ip_, ec), static_cast<unsigned short>(port_));
        if (ec)
        {
          return 2;
        }
        udp_sock_.connect(ep, ec); // optional
      }
      return 0;
    }

    int disconnect() override
    {
      asio::error_code ec;
      if (tcp_sock_.is_open())
      {
        tcp_sock_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        tcp_sock_.close(ec);
      }
      if (udp_sock_.is_open())
      {
        udp_sock_.close(ec);
      }
      return 0;
    }

    int run() override { return 0; }

    int send_tcp(const std::string& data) override
    {
      if (!tcp_sock_.is_open())
      {
        int rc = connect();
        if (rc != 0)
        {
          return rc;
        }
      }
      std::lock_guard<std::mutex> lk(mu_);
      // framing: 4-byte big-endian length + payload
      uint32_t nlen = htonl(static_cast<uint32_t>(data.size()));
      char hdr[4];
      std::memcpy(hdr, &nlen, sizeof(nlen));
      asio::error_code ec;
      asio::write(tcp_sock_, asio::buffer(hdr, sizeof(hdr)), ec);
      if (ec)
      {
        if (!reconnect())
        {
          return 3;
        }
        ec = {};
        asio::write(tcp_sock_, asio::buffer(hdr, sizeof(hdr)), ec);
        if (ec)
        {
          return 3;
        }
      }
      asio::write(tcp_sock_, asio::buffer(data.data(), data.size()), ec);
      if (ec)
      {
        return 3;
      }
      return 0;
    }

    int send_udp(const std::string& data) override
    {
      if (data.empty())
      {
        return 0;
      }
      if (!udp_sock_.is_open())
      {
        int rc = connect();
        if (rc != 0)
        {
          return rc;
        }
      }
      asio::error_code ec;
      auto n = udp_sock_.send(asio::buffer(data.data(), data.size()), 0, ec);
      if (ec || n != data.size())
      {
        return 3;
      }
      return 0;
    }

  private:
    bool reconnect()
    {
      asio::error_code ec;
      tcp_sock_.close(ec);
      asio::ip::tcp::resolver res(io_);
      auto eps = res.resolve(ip_, std::to_string(port_), ec);
      if (ec)
      {
        return false;
      }
      asio::connect(tcp_sock_, eps, ec);
      return !ec;
    }

    std::string ip_;
    int port_;
    asio::io_context io_;
    asio::ip::tcp::socket tcp_sock_;
    asio::ip::udp::socket udp_sock_;
    std::mutex mu_;
  };

  std::unique_ptr<Sender> make_sender(const std::string& ip, int port)
  {
    return std::unique_ptr<Sender>(new AsioSender(ip, port));
  }

} // namespace net
