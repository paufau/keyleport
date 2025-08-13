#pragma once
#include <string>

namespace net
{

  class Sender
  {
  public:
    virtual ~Sender() = default;
    // Establish and tear down any underlying resources (sockets, threads, etc.)
    // Returns 0 on success; non-zero on failure.
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int run() = 0;
    // TCP and UDP explicit send methods
    virtual int send_tcp(const std::string& data) = 0;
    virtual int send_udp(const std::string& data) = 0;
  };

} // namespace net
