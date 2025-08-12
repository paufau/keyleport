// Only provide the dummy fallback on non-Apple, non-Windows platforms.
#if !defined(__APPLE__) && !defined(_WIN32)

#include "networking/server/receiver.h"

#include <iostream>
#include <memory>
#include <string>

namespace net
{

  class DummyReceiver : public Receiver
  {
  public:
    explicit DummyReceiver(int port) : port_(port) {}
    void onReceive(ReceiveHandler handler) override { handler_ = std::move(handler); }
    int run() override
    {
      std::cerr << "[receiver] cxx dummy: networking not supported on this platform (port " << port_ << ")\n";
      // No networking possible in this fallback. Return non-zero to indicate not available.
      return 1;
    }

  private:
    int port_;
    ReceiveHandler handler_;
  };

  std::unique_ptr<Receiver> make_receiver(int port)
  {
    return std::unique_ptr<Receiver>(new DummyReceiver(port));
  }

} // namespace net

#else

// On Apple/Windows, a platform-specific implementation provides make_receiver.

#endif
