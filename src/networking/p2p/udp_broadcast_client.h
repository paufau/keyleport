#pragma once

#include "./udp_client_configuration.h"

#include <string>

namespace p2p
{
  class udp_broadcast_client
  {
  public:
    udp_broadcast_client(udp_client_configuration config);
    ~udp_broadcast_client();

    void broadcast(const std::string& message);

  private:
    int sock_{-1};
    udp_client_configuration config_;
  };
} // namespace p2p