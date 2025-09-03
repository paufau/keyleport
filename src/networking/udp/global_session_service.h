// global_session_service: manages a single active session using udp_service
// Provides events on_session_start/on_session_end and simple idle/busy state.

#pragma once

#include "networking/udp/discovery_service.h"
#include "utils/event_emitter/event_emitter.h"
#include "networking/udp/peer_connection.h"
#include "networking/udp/udp_service.h"

#include <optional>
#include <string>

namespace net
{
  namespace udp
  {

    enum class session_state
    {
      idle,
      busy
    };

    class global_session_service
    {
    public:
      explicit global_session_service(udp_service& net);

      void connect_to_peer(const peer_info& peer);
      void disconnect();
      session_state get_current_state() const;

      event_emitter<peer_info> on_session_start;
      event_emitter<peer_info> on_session_end;

    private:
      udp_service& net_;
      std::optional<peer_info> current_;
      peer_connection_ptr current_conn_;
    };

  } // namespace udp
} // namespace net
