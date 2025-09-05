#pragma once

#include "services/communication/typed_package.h"

#include <nlohmann/json.hpp>
#include <string>

namespace services
{

  struct become_receiver_package
  {
    static constexpr const char* __typename = "become_receiver";

    static bool is(services::typed_package pkg)
    {
      return pkg.__typename == __typename;
    }

    std::string ip_address;

    inline std::string encode() const
    {
      nlohmann::json j{{"ip_address", ip_address}};
      return j.dump();
    }

    static inline become_receiver_package decode(const std::string& s)
    {
      become_receiver_package p{};
      auto j = nlohmann::json::parse(s, nullptr, false);
      if (!j.is_object())
      {
        return p;
      }
      if (j.contains("ip_address") && j["ip_address"].is_string())
      {
        p.ip_address = j.value("ip_address", std::string{});
      }
      return p;
    }
  };

} // namespace services