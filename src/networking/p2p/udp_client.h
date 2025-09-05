#pragma once

#include "./message.h"
#include "./udp_client_configuration.h"

#include <enet/enet.h>

namespace p2p
{
  class udp_client
  {
  public:
    udp_client(udp_client_configuration config);
    ~udp_client();

    void send_reliable(message message);
    void send_unreliable(message message);

  private:
    udp_client_configuration config_;
    ENetHost* host_{nullptr};
    ENetPeer* peer_{nullptr};

    void send_impl(message message, bool is_reliable);
  };
} // namespace p2p