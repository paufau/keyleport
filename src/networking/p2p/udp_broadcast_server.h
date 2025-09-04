#pragma once

#include "./message.h"
#include "./udp_server_configuration.h"
#include "utils/event_emitter/event_emitter.h"

namespace p2p
{

  class udp_broadcast_server
  {
  public:
    udp_broadcast_server(udp_server_configuration config);
    ~udp_broadcast_server();

    void poll_events();

    utils::event_emitter<message> on_message;

  private:
    int sock_{-1};
    udp_server_configuration config_;
  };

} // namespace p2p