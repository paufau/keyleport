#pragma once

#include "Session.h"

#include <asio.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>

namespace net {
namespace p2p {

class SessionServer {
public:
  using NewSessionHandler = std::function<void(Session::Ptr)>;

  SessionServer(asio::io_context& io, uint16_t port = 0) : io_(io), acceptor_(io) {
    asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);
    acceptor_.open(ep.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen();
  }

  uint16_t port() const { return acceptor_.local_endpoint().port(); }

  void async_accept(NewSessionHandler on_new) {
    auto sock = std::make_shared<asio::ip::tcp::socket>(io_);
    acceptor_.async_accept(*sock,
      [this, sock, on_new](std::error_code ec) {
        if (!ec) {
          auto session = std::make_shared<Session>(std::move(*sock));
          if (on_new) on_new(session);
        }
        async_accept(on_new);
      });
  }

private:
  asio::io_context& io_;
  asio::ip::tcp::acceptor acceptor_;
};

} // namespace p2p
} // namespace net
