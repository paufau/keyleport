// peer_connection: represents a single connected peer over ENet.
// Provides per-peer reliable/unreliable send and disconnect APIs.

#pragma once

#include "utils/event_emitter/event_emitter.h"
#include "networking/udp/events.h"

#include <memory>
#include <string>

struct _ENetPeer;

namespace net
{
  namespace udp
  {

    class udp_service;

    class peer_connection : public std::enable_shared_from_this<peer_connection>
    {
    public:
      // Non-copyable
      peer_connection(const peer_connection&) = delete;
      peer_connection& operator=(const peer_connection&) = delete;

      // Constructed by udp_service
      peer_connection(udp_service* svc, _ENetPeer* peer);

      // Per-peer send
      void send_reliable(const std::string& data);
      void send_unreliable(const std::string& data);

      // Disconnect only this peer
      void disconnect();

      // Info
      std::string address() const;
      int port() const;

      // State
      bool is_connected() const;

      // Events
      event_emitter<network_event_data> on_receive_data;

    private:
      friend class udp_service;

      udp_service* service_{nullptr};
      _ENetPeer* peer_{nullptr};
    };

    using peer_connection_ptr = std::shared_ptr<peer_connection>;

  } // namespace udp
} // namespace net
