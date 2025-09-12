#pragma once

#include "./message.h"
#include "./udp_client_configuration.h"

#include <enet/enet.h>
#include <mutex>

namespace p2p
{
  class udp_client
  {
  public:
    udp_client(udp_client_configuration config);
    ~udp_client();

    void flush_pending_messages();

    void send_reliable(message message);
    void send_unreliable(message message);

  private:
    // Channel layout: keep unreliable traffic separate from reliable to
    // minimize head-of-line blocking when losses occur on the reliable path.
    static constexpr enet_uint8 kChannelUnreliable = 0;
    static constexpr enet_uint8 kChannelReliable = 1;

    udp_client_configuration config_;
    ENetHost* host_{nullptr};
    ENetPeer* peer_{nullptr};
    std::mutex mutex_; // guards host_/peer_ and all ENet calls

    // Reconnect support
    unsigned long long last_connect_attempt_ms_{0};
    int reconnect_interval_ms_{1000}; // 1s between attempts
    int connect_timeout_ms_{500};

    // Precondition: mutex_ is held by caller
    bool ensure_connected();
    unsigned long long now_ms() const;

    void send_impl(message message, bool is_reliable);

    // Precondition: mutex_ is held by caller; services ENet events with a
    // small budget to advance acks/timeouts and detect disconnects.
    void flush_service_events();
  };
} // namespace p2p