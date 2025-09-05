#pragma once

#include "services/communication/typed_package.h"

#include <iostream>
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

    std::string device_id;

    inline std::string encode() const
    {
      nlohmann::json j{{"device_id", device_id}};
      return j.dump();
    }

    static inline become_receiver_package decode(const std::string& s)
    {
      become_receiver_package p{};
      auto j = nlohmann::json::parse(s, nullptr, false);
      if (!j.is_object())
      {
        std::cerr
            << "[become_receiver_package] Decode failed: not a JSON object"
            << std::endl;
        return p;
      }
      if (j.contains("device_id") && j["device_id"].is_string())
      {
        p.device_id = j.value("device_id", std::string{});
        std::cout << "[become_receiver_package] Decoded device_id='"
                  << p.device_id << "'" << std::endl;
      }
      return p;
    }
  };

} // namespace services