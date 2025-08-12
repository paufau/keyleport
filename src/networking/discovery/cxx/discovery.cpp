#if !defined(__APPLE__) && !defined(_WIN32)

#include "networking/discovery/discovery.h"

#include <memory>
#include <string>
#include <vector>

namespace net
{
  namespace discovery
  {
    class DummyResponder : public DiscoveryResponder
    {
    public:
      void start_async() override {}
    };

    std::unique_ptr<DiscoveryResponder> make_responder(int, const std::string&)
    {
      return std::unique_ptr<DiscoveryResponder>(new DummyResponder());
    }

    std::vector<ServerInfo> discover(int, int)
    {
      return {};
    }
  } // namespace discovery
} // namespace net

#endif
