#pragma once

#include "Session.h"
#include "UdpTransport.h"

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

      void set_client_udp_sender(std::shared_ptr<UdpSender> s);
      void set_server_udp_receiver(std::shared_ptr<UdpReceiver> r);

      Session::Ptr client_session();
      Session::Ptr server_session();

      // Set handler for messages received on the server-side session
      void set_on_server_message(std::function<void(const std::string&)> cb);

      // Send a single line to the peer (TCP is reliable)
      void send_to_peer_tcp(const std::string& line);
      // Send a single line via UDP only (no TCP fallback)
      void send_to_peer_udp(const std::string& line);

      // Clear sessions and handlers
      void stop();

    private:
      std::mutex mtx_;
      Session::Ptr client_;
      Session::Ptr server_;
      std::shared_ptr<UdpSender> udp_client_sender_;
      std::shared_ptr<UdpReceiver> udp_server_receiver_;
      std::function<void(const std::string&)> on_server_message_;
    };

  } // namespace p2p
} // namespace net
