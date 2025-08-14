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
      using DiscoveredHandler = std::function<void(const entities::ConnectionCandidate&)>;
      virtual ~Discovery() = default;
      virtual void onDiscovered(DiscoveredHandler handler) = 0;
      virtual void start_discovery(int servicePort) = 0;
      virtual void stop_discovery() = 0;
    };

    // Factory for discovery client
    std::unique_ptr<Discovery> make_discovery();
  } // namespace discovery
} // namespace net
