#pragma once

#include "entities/connection_candidate.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace net
{
  namespace discovery
  {
    struct ServerInfo
    {
      std::string ip;       // source IP of the response
      int port = 0;         // service port
      std::string name;     // server name
      std::string platform; // "macos" | "windows" | other
    };

    class DiscoveryResponder
    {
    public:
      virtual ~DiscoveryResponder() = default;
      virtual void start_async() = 0;
    };

    // Create a responder bound to the given service port; replies to discovery probes.
    std::unique_ptr<DiscoveryResponder> make_responder(int servicePort, const std::string& name);

    // Async discovery client API
    class Discovery
    {
    public:
      // Process-wide singleton accessor.
      static Discovery& instance();

      using DiscoveredHandler = std::function<void(const entities::ConnectionCandidate&)>;
      using LostHandler = std::function<void(const entities::ConnectionCandidate&)>;
      // Called when a TCP message is received from a discovered server.
      // The first argument identifies which server sent the message.
      using MessageHandler = std::function<void(const entities::ConnectionCandidate&, const std::string&)>;
      virtual ~Discovery() = default;
      virtual void onDiscovered(DiscoveredHandler handler) = 0;
      // Called when a previously discovered device is no longer accessible.
      // The argument identifies which server was lost.
      virtual void onLost(LostHandler handler) = 0;
      // Register a handler to be invoked for inbound TCP messages from servers.
      virtual void onMessage(MessageHandler handler) = 0;
      virtual void start_discovery(int servicePort) = 0;
      virtual void stop_discovery() = 0;
      // Send a TCP message to a specific discovered server. Returns 0 on success.
      virtual int sendMessage(const entities::ConnectionCandidate& target, const std::string& message) = 0;
    };

    // Factory for discovery client
    std::unique_ptr<Discovery> make_discovery();
  } // namespace discovery
} // namespace net
