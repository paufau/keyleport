#pragma once
#include <string>

namespace net
{

  class Sender
  {
  public:
    virtual ~Sender() = default;
    virtual int run() = 0;
    // TCP and UDP explicit send methods
    virtual int send_tcp(const std::string& data) = 0;
    virtual int send_udp(const std::string& data) = 0;
  };

} // namespace net
