// peer_info: shared peer metadata and helper (de)serialization for discovery
// announces

#pragma once

#include <chrono>
#include <string>

namespace net
{
  namespace udp
  {

    enum class peer_state
    {
      unknown,
      discovered
    };

    struct peer_info
    {
      std::string device_id;
      std::string device_name; // human-friendly name
      std::string ip_address;
      int port{0};
      std::chrono::steady_clock::time_point last_seen;
      peer_state state{peer_state::unknown};
    };

    // Serialize a discovery announce payload. Note: ip_address/last_seen/state
    // are not broadcast.
    std::string peer_announce_to_json(const std::string& device_id,
                                      const std::string& device_name, int port);

    // Parse a discovery announce payload into peer_info, using sender
    // address/port. Returns false if parsing fails.
    bool peer_info_from_announce_json(const std::string& json_str,
                                      const std::string& from_address,
                                      int from_port, peer_info& out);

  } // namespace udp
} // namespace net
