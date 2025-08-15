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
  };

} // namespace net
