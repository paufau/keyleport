#pragma once

#include "networking/p2p/peer.h"

namespace p2p
{
  class udp_client_configuration
  {
  public:
    udp_client_configuration();
    ~udp_client_configuration();

    int get_port() const;
    void set_port(int port);

    peer get_peer() const;
    void set_peer(const peer& p);

  private:
    int port_;
    peer peer_{};
  };
} // namespace p2p
