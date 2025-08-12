#pragma once

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

    // Broadcast a discovery probe on the local network and collect responses within timeoutMs.
    std::vector<ServerInfo> discover(int servicePort, int timeoutMs);
  } // namespace discovery
} // namespace net
