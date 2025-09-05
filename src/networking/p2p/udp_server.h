#pragma once

#include "./message.h"
#include "./udp_server_configuration.h"
#include "utils/event_emitter/event_emitter.h"

#include <enet/enet.h>

namespace p2p
{
  class udp_server
  {
  public:
    udp_server(udp_server_configuration config);
    ~udp_server();

    void poll_events();

    utils::event_emitter<message> on_message;

  private:
    udp_server_configuration config_;
    ENetHost* host_{nullptr};
    bool enet_inited_{false};

    void destroy_packet(ENetPacket* packet);
    std::string extract_ip(ENetPeer* peer);
  };
} // namespace p2p
