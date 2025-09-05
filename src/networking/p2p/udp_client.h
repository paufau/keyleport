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

    // Reconnect support
    unsigned long long last_connect_attempt_ms_{0};
    int reconnect_interval_ms_{1000}; // 1s between attempts
    int connect_timeout_ms_{500};

    bool ensure_connected();
    unsigned long long now_ms() const;

    void send_impl(message message, bool is_reliable);
  };
} // namespace p2p