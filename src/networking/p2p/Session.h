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

  explicit Session(asio::ip::tcp::socket socket);

  void set_on_message(std::function<void(const std::string&)> cb);

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

  void start_server(std::function<void()> on_ready);

  void start_client(std::function<void()> on_ready);

  asio::ip::tcp::socket& socket();

private:
  void read_loop();
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
