#pragma once

#include "DiscoveryServer.h"
#include "SessionServer.h"
#include "service.h"

#include <asio.hpp>
#include <memory>
#include <thread>
#include <functional>

namespace net
{
  namespace p2p
  {
    // Owns IO, TCP SessionServer, and DiscoveryServer lifecycle.
    class Runtime
    {
    public:
      static Runtime& instance();

      void start();
      void stop();

      // Access IO context for client-side connectors/senders.
      asio::io_context& io();

      // Discovery state control (e.g., set Busy after handshake)
      void set_discovery_state(State s);
      State discovery_state() const;

      uint16_t session_port() const;

  // Forward DiscoveryServer handlers
  void set_on_peer_update(DiscoveryServer::PeerUpdateHandler cb);
  void set_on_peer_remove(DiscoveryServer::PeerRemoveHandler cb);

  // Notify UI when an incoming server session handshake completes
  void set_on_server_ready(std::function<void(const std::string& ip, const std::string& port)> cb);

    private:
      Runtime() = default;
      Runtime(const Runtime&) = delete;
      Runtime& operator=(const Runtime&) = delete;

      void start_accepting();

      std::unique_ptr<asio::io_context> io_;
      std::unique_ptr<SessionServer> session_server_;
      std::unique_ptr<DiscoveryServer> discovery_;
      std::thread io_thread_;

      std::string instance_id_;
      uint64_t boot_id_ = 1;

  std::function<void(const std::string&, const std::string&)> on_server_ready_;
    };
  } // namespace p2p
} // namespace net
