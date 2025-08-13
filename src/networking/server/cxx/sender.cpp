// Only provide the dummy fallback on non-Apple, non-Windows platforms.
#if !defined(__APPLE__) && !defined(_WIN32)

#include "networking/server/sender.h"

#include <iostream>
#include <memory>
#include <string>

namespace net
{

  class DummySender : public Sender
  {
  public:
    DummySender(std::string ip, int port) : ip_(std::move(ip)), port_(port) {}
    int connect() override { return 1; }
    int disconnect() override { return 1; }
    int run() override
    {
      std::cerr << "[sender] cxx dummy: networking not supported on this platform (" << ip_ << ":" << port_ << ")\n";
      return 1;
    }
    int send_tcp(const std::string&) override { return 1; }
    int send_udp(const std::string&) override { return 1; }

  private:
    std::string ip_;
    int port_;
  };

  std::unique_ptr<Sender> make_sender(const std::string& ip, int port)
  {
    return std::unique_ptr<Sender>(new DummySender(ip, port));
  }

} // namespace net

#else

// On Apple/Windows, a platform-specific implementation provides make_sender.

#endif
