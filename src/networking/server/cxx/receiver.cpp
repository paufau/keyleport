// Cross-platform Receiver implemented with Asio (standalone)

#include "networking/server/receiver.h"

#include "networking/discovery/discovery.h"

#include <asio.hpp>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace net
{

  class AsioReceiver : public Receiver
  {
  public:
    explicit AsioReceiver(int port) : port_(port), io_(), acceptor_(io_), udp_sock_(io_)
    {
      asio::error_code ec;
      // TCP acceptor setup with reuse_address
      acceptor_.open(asio::ip::tcp::v4(), ec);
      if (ec)
      {
        throw std::system_error(ec);
      }
      acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
      // Some platforms also benefit from enabling v6-only false; but we stick to v4 for now.
      acceptor_.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(port_)), ec);
      if (ec)
      {
        throw std::system_error(ec);
      }
      acceptor_.listen(asio::socket_base::max_listen_connections, ec);
      if (ec)
      {
        throw std::system_error(ec);
      }

      // UDP socket setup with reuse_address
      udp_sock_.open(asio::ip::udp::v4(), ec);
      if (ec)
      {
        throw std::system_error(ec);
      }
      udp_sock_.set_option(asio::socket_base::reuse_address(true), ec);
      udp_sock_.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), static_cast<unsigned short>(port_)), ec);
      if (ec)
      {
        throw std::system_error(ec);
      }
    }

    void onReceive(ReceiveHandler handler) override { handler_ = std::move(handler); }
    void stop() override
    {
      asio::error_code ec;
      acceptor_.close(ec);
      udp_sock_.close(ec);
      io_.stop();
    }
    int run() override
    {
      // Start UDP discovery responder for clients with no configured IP
      auto responder = discovery::make_responder(port_, "keyleport");
      if (responder)
      {
        responder->start_async();
      }
      // Accept TCP connections in a background thread
      std::thread([this]() { this->accept_loop(); }).detach();
      // Blocking UDP loop
      for (;;)
      {
        char buf[65536];
        asio::ip::udp::endpoint from;
        asio::error_code ec;
        size_t n = udp_sock_.receive_from(asio::buffer(buf, sizeof(buf)), from, 0, ec);
        if (ec)
        {
          if (ec == asio::error::operation_aborted)
          {
            break; // socket closed
          }
          continue;
        }
        if (n > 0 && handler_)
        {
          handler_(std::string(buf, buf + n), from.address().to_string());
        }
      }
      return 0;
    }

  private:
    void accept_loop()
    {
      for (;;)
      {
        asio::error_code ec;
        asio::ip::tcp::socket sock(io_);
        acceptor_.accept(sock, ec);
        if (ec)
        {
          if (ec == asio::error::operation_aborted)
          {
            break; // acceptor closed
          }
          continue;
        }
        std::thread([this](asio::ip::tcp::socket s) mutable { this->tcp_session(std::move(s)); }, std::move(sock))
            .detach();
      }
    }

    void tcp_session(asio::ip::tcp::socket s)
    {
      std::string remote_ip;
      try
      {
        remote_ip = s.remote_endpoint().address().to_string();
      }
      catch (...)
      {
        remote_ip.clear();
      }
      std::vector<char> buffer;
      buffer.reserve(4096);
      asio::error_code ec;
      for (;;)
      {
        char temp[4096];
        size_t n = s.read_some(asio::buffer(temp, sizeof(temp)), ec);
        if (ec)
        {
          break;
        }
        buffer.insert(buffer.end(), temp, temp + n);
        // parse frames
        for (;;)
        {
          if (buffer.size() < 4)
          {
            break;
          }
          uint32_t nlen_be = 0;
          std::memcpy(&nlen_be, buffer.data(), 4);
          uint32_t nlen = ntohl(nlen_be);
          if (buffer.size() < 4u + nlen)
          {
            break;
          }
          std::string msg(buffer.begin() + 4, buffer.begin() + 4 + nlen);
          buffer.erase(buffer.begin(), buffer.begin() + 4 + nlen);
          if (handler_)
          {
            handler_(msg, remote_ip);
          }
        }
      }
      asio::error_code ec2;
      s.shutdown(asio::ip::tcp::socket::shutdown_receive, ec2);
      s.close(ec2);
    }

    int port_;
    ReceiveHandler handler_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    asio::ip::udp::socket udp_sock_;
  };

  std::unique_ptr<Receiver> make_receiver(int port)
  {
    return std::unique_ptr<Receiver>(new AsioReceiver(port));
  }

} // namespace net
