#include "../server.h"

#include <memory>
#include <string>

namespace net
{

  // Forward declarations for cxx factories implemented in this folder
  std::unique_ptr<Sender> make_sender(const std::string& ip, int port);
  std::unique_ptr<Receiver> make_receiver(int port);

  class CxxServer : public Server
  {
  public:
    std::unique_ptr<Sender> createSender(const std::string& ip, int port) override { return make_sender(ip, port); }
    std::unique_ptr<Receiver> createReceiver(int port) override { return make_receiver(port); }
  };

  std::unique_ptr<Server> make_server()
  {
    return std::unique_ptr<Server>(new CxxServer());
  }

} // namespace net
