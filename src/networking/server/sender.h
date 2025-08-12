#pragma once
#include <string>

namespace net
{

  class Sender
  {
  public:
    virtual ~Sender() = default;
    virtual int run() = 0;
    virtual int send(const std::string &data) = 0;
  };

} // namespace net
