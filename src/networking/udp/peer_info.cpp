#include "networking/udp/peer_info.h"

#include <nlohmann/json.hpp>

namespace net
{
  namespace udp
  {

    using json = nlohmann::json;

    std::string peer_announce_to_json(const std::string& device_id,
                                      const std::string& device_name, int port)
    {
      json j{{"device_id", device_id},
             {"device_name", device_name},
             {"port", port}};
      return j.dump();
    }

    bool peer_info_from_announce_json(const std::string& json_str,
                                      const std::string& from_address,
                                      int from_port, peer_info& out)
    {
      try
      {
        auto j = json::parse(json_str);
        out.device_id = j.value("device_id", std::string("unknown"));
        out.device_name = j.value("device_name", std::string(""));
        out.ip_address = from_address;
        // Prefer the announced ENet port; fallback to UDP source port if
        // missing (legacy)
        out.port = j.value("port", from_port);
        out.last_seen = std::chrono::steady_clock::now();
        out.state = peer_state::discovered;
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

  } // namespace udp
} // namespace net
