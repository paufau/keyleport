#pragma once

#include "Message.h"

#include <asio.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>

namespace net {
namespace p2p {

class Session : public std::enable_shared_from_this<Session> {
public:
  using Ptr = std::shared_ptr<Session>;

  explicit Session(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

  void set_on_message(std::function<void(const std::string&)> cb) { on_message_ = std::move(cb); }

  template <typename Handler = std::function<void(std::error_code)>>
  void send_line(const std::string& line, Handler done = {}) {
    auto self = shared_from_this();
    std::string out = line;
    if (out.empty() || out.back() != '\n') out.push_back('\n');
    asio::async_write(socket_, asio::buffer(out),
      [self, done](std::error_code ec, std::size_t) {
        if (done) done(ec);
      });
  }

  void start_server(std::function<void()> on_ready) {
    auto self = shared_from_this();
    asio::async_read_until(socket_, asio::dynamic_buffer(buffer_), '\n',
      [this, self, on_ready](std::error_code ec, std::size_t) {
        if (ec) {
          std::cerr << "[p2p][server] read hello failed: " << ec.message() << std::endl;
          return;
        }
        auto pos = buffer_.find('\n');
        std::string line = (pos == std::string::npos) ? buffer_ : buffer_.substr(0, pos);
        if (pos != std::string::npos) buffer_.erase(0, pos + 1);
        if (line == "hello") {
          std::cerr << "[p2p][server] <= hello" << std::endl;
          write_line("welcome\n",
            [this, self, on_ready](std::error_code ec2, std::size_t) {
              if (ec2) {
                std::cerr << "[p2p][server] write welcome failed: " << ec2.message() << std::endl;
                return;
              }
              std::cerr << "[p2p][server] => welcome" << std::endl;
              write_line("ready\n",
                [this, self, on_ready](std::error_code ec3, std::size_t) {
                  if (ec3) {
                    std::cerr << "[p2p][server] write ready failed: " << ec3.message() << std::endl;
                    return;
                  }
                  std::cerr << "[p2p][server] => ready (handshake done)" << std::endl;
                  if (on_ready) on_ready();
                  this->read_loop();
                });
            });
        } else {
          std::cerr << "[p2p][server] unexpected line: '" << line << "'" << std::endl;
        }
      });
  }

  void start_client(std::function<void()> on_ready) {
    auto self = shared_from_this();
    write_line("hello\n",
      [this, self, on_ready](std::error_code ec, std::size_t) {
        if (ec) {
          std::cerr << "[p2p][client] write hello failed: " << ec.message() << std::endl;
          return;
        }
        std::cerr << "[p2p][client] => hello" << std::endl;
        asio::async_read_until(socket_, asio::dynamic_buffer(buffer_), '\n',
          [this, self, on_ready](std::error_code ec2, std::size_t) {
            if (ec2) {
              std::cerr << "[p2p][client] read welcome failed: " << ec2.message() << std::endl;
              return;
            }
            auto pos = buffer_.find('\n');
            std::string line = (pos == std::string::npos) ? buffer_ : buffer_.substr(0, pos);
            if (pos != std::string::npos) buffer_.erase(0, pos + 1);
            if (line == "welcome") {
              std::cerr << "[p2p][client] <= welcome" << std::endl;
              asio::async_read_until(socket_, asio::dynamic_buffer(buffer_), '\n',
                [this, self, on_ready](std::error_code ec3, std::size_t) {
                  if (ec3) {
                    std::cerr << "[p2p][client] read ready failed: " << ec3.message() << std::endl;
                    return;
                  }
                  auto pos2 = buffer_.find('\n');
                  std::string line2 = (pos2 == std::string::npos) ? buffer_ : buffer_.substr(0, pos2);
                  if (pos2 != std::string::npos) buffer_.erase(0, pos2 + 1);
                  if (line2 == "ready") {
                    std::cerr << "[p2p][client] <= ready (handshake done)" << std::endl;
                    if (on_ready) on_ready();
                    this->read_loop();
                  } else {
                    std::cerr << "[p2p][client] unexpected line: '" << line2 << "'" << std::endl;
                  }
                });
            } else {
              std::cerr << "[p2p][client] unexpected line: '" << line << "'" << std::endl;
            }
          });
      });
  }

  asio::ip::tcp::socket& socket() { return socket_; }

private:
  void read_loop() {
    auto self = shared_from_this();
    asio::async_read_until(socket_, asio::dynamic_buffer(buffer_), '\n',
      [this, self](std::error_code ec, std::size_t) {
        if (ec) {
          return; // closed
        }
        auto pos = buffer_.find('\n');
        if (pos != std::string::npos) {
          std::string line = buffer_.substr(0, pos);
          buffer_.erase(0, pos + 1);
          if (on_message_) on_message_(line);
        }
        read_loop();
      });
  }
  template <typename Handler>
  void write_line(const std::string& s, Handler&& h) {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(s), std::forward<Handler>(h));
  }

  asio::ip::tcp::socket socket_;
  std::string buffer_;
  std::function<void(const std::string&)> on_message_;
};

} // namespace p2p
} // namespace net
