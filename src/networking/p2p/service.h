#pragma once

#include "Session.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace net
{
  namespace p2p
  {
    // Lightweight global holder for active P2P sessions
    class Service
    {
    public:
      static Service& instance();

      void set_client_session(Session::Ptr s);
      void set_server_session(Session::Ptr s);

      Session::Ptr client_session();
      Session::Ptr server_session();

      // Set handler for messages received on the server-side session
      void set_on_server_message(std::function<void(const std::string&)> cb);

      // Send a single line to the peer (prefers client session, falls back to server)
  void send_to_peer_tcp(const std::string& line);
  void send_to_peer_udp(const std::string& line);

      // Clear sessions and handlers
      void stop();

    private:
      std::mutex mtx_;
      Session::Ptr client_;
      Session::Ptr server_;
      std::function<void(const std::string&)> on_server_message_;
    };

  } // namespace p2p
} // namespace net
