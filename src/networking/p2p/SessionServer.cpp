#include "SessionServer.h"

#include <memory>

namespace net {
namespace p2p {

SessionServer::SessionServer(asio::io_context& io, uint16_t port)
  : io_(io), acceptor_(io) {
  asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);
  acceptor_.open(ep.protocol());
  acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen();
}

uint16_t SessionServer::port() const { return acceptor_.local_endpoint().port(); }

void SessionServer::async_accept(NewSessionHandler on_new) {
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

} // namespace p2p
} // namespace net
