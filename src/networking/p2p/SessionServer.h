#pragma once

#include "Session.h"

#include <asio.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace net
{
  namespace p2p
  {

    class SessionServer
    {
    public:
      using NewSessionHandler = std::function<void(Session::Ptr)>;

      SessionServer(asio::io_context& io, uint16_t port = 0);

      uint16_t port() const;

      void async_accept(NewSessionHandler on_new);

    private:
      asio::io_context& io_;
      asio::ip::tcp::acceptor acceptor_;
    };

  } // namespace p2p
} // namespace net
