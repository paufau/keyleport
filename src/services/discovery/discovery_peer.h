#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace services
{
  enum class discovery_peer_state
  {
    idle,
    busy
  };

  struct discovery_peer
  {
    std::string device_id;
    std::string device_name;
    std::string ip_address;
    std::string platform; // macos, windows, linux
    discovery_peer_state state = discovery_peer_state::idle;

    // Encode this discovery_peer into a compact JSON string
    inline std::string encode() const
    {
      nlohmann::json j{{"device_id", device_id},
                       {"device_name", device_name},
                       {"ip_address", ip_address},
                       {"platform", platform},
                       {"state", static_cast<int>(state)}};
      return j.dump();
    }

    static inline discovery_peer decode(const std::string& s)
    {
      discovery_peer p{};
      auto j = nlohmann::json::parse(s, nullptr, false);
      if (!j.is_object())
      {
        return p;
      }

      p.device_id = j.value("device_id", std::string{});
      p.device_name = j.value("device_name", std::string{});
      p.ip_address = j.value("ip_address", std::string{});
      p.platform = j.value("platform", std::string{});

      // Only accept numeric state; ignore non-numeric values
      if (j.contains("state") && j["state"].is_number_integer())
      {
        int sv = j.value("state", 0);
        if (sv == static_cast<int>(discovery_peer_state::idle) ||
            sv == static_cast<int>(discovery_peer_state::busy))
        {
          p.state = static_cast<discovery_peer_state>(sv);
        }
        else
        {
          p.state = discovery_peer_state::idle;
        }
      }

      return p;
    }
  };
} // namespace services
