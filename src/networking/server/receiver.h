#pragma once

#include <functional>
#include <string>

namespace net
{

  class Receiver
  {
  public:
    virtual ~Receiver() = default;
    // Handler receives payload and remote address (ip or ip:port when available)
    using ReceiveHandler = std::function<void(const std::string&, const std::string&)>;
    virtual void onReceive(ReceiveHandler handler) = 0;
    virtual int run() = 0;
    // Request the receiver to stop and unblock any blocking calls.
    virtual void stop() = 0;
  };

} // namespace net
