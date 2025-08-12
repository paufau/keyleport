#pragma once

#include "receiver.h"
#include "sender.h"

#include <memory>
#include <string>

namespace net
{

  class Server
  {
  public:
    virtual ~Server() = default;
    virtual std::unique_ptr<Sender>   createSender(const std::string& ip, int port) = 0;
    virtual std::unique_ptr<Receiver> createReceiver(int port) = 0;
  };

  std::unique_ptr<Server> make_server();

} // namespace net
