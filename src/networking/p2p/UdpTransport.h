#pragma once

#include <array>
#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>

namespace net
{
  namespace p2p
  {

    class UdpReceiver : public std::enable_shared_from_this<UdpReceiver>
    {
    public:
      using Ptr = std::shared_ptr<UdpReceiver>;

      UdpReceiver(asio::io_context& io, uint16_t listen_port)
          : socket_(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), listen_port))
      {
      }

      void set_on_message(std::function<void(const std::string&)> cb) { on_message_ = std::move(cb); }

      void start() { do_receive(); }

      void stop()
      {
        asio::error_code ec;
        socket_.close(ec);
      }

      uint16_t port() const
      {
        asio::error_code ec;
        auto ep = socket_.local_endpoint(ec);
        return ec ? 0 : ep.port();
      }

    private:
      void do_receive()
      {
        auto self = shared_from_this();
        socket_.async_receive_from(asio::buffer(buf_), sender_,
                                   [this, self](std::error_code ec, std::size_t n)
                                   {
                                     if (!ec && n > 0)
                                     {
                                       std::string s(buf_.data(), n);
                                       if (on_message_)
                                       {
                                         on_message_(s);
                                       }
                                     }
                                     if (!ec)
                                     {
                                       do_receive();
                                     }
                                   });
      }

      asio::ip::udp::socket socket_;
      std::array<char, 4096> buf_{};
      asio::ip::udp::endpoint sender_{};
      std::function<void(const std::string&)> on_message_;
    };

    class UdpSender : public std::enable_shared_from_this<UdpSender>
    {
    public:
      using Ptr = std::shared_ptr<UdpSender>;

      UdpSender(asio::io_context& io, const asio::ip::udp::endpoint& dest)
          : socket_(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)), dest_(dest)
      {
      }

      template <typename Handler = std::function<void(std::error_code)>>
      void send_line(const std::string& line, Handler done = {})
      {
        auto self = shared_from_this();
        std::string out = line;
        if (out.empty() || out.back() != '\n')
        {
          out.push_back('\n');
        }
        socket_.async_send_to(asio::buffer(out), dest_,
                              [self, done](std::error_code ec, std::size_t)
                              {
                                if (done)
                                {
                                  done(ec);
                                }
                              });
      }

      const asio::ip::udp::endpoint& destination() const { return dest_; }

    private:
      asio::ip::udp::socket socket_;
      asio::ip::udp::endpoint dest_;
    };

  } // namespace p2p
} // namespace net
