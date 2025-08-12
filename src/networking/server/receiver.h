#pragma once

#include <functional>
#include <string>

namespace net
{

  class Receiver
  {
  public:
    virtual ~Receiver() = default;
    using ReceiveHandler = std::function<void(const std::string&)>;
    virtual void onReceive(ReceiveHandler handler) = 0;
    virtual int  run() = 0;
  };

} // namespace net
