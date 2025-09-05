#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace services
{
  struct typed_package
  {
  public:
    std::string __typename;
    std::string payload;

    inline std::string encode() const
    {
      nlohmann::json j{{"__typename", __typename}, {"payload", payload}};
      return j.dump();
    }

    static inline typed_package decode(const std::string& s)
    {
      typed_package p{};
      auto j = nlohmann::json::parse(s, nullptr, false);
      if (!j.is_object())
      {
        return p;
      }
      if (j.contains("__typename") && j["__typename"].is_string())
      {
        p.__typename = j.value("__typename", std::string{});
      }
      if (j.contains("payload") && j["payload"].is_string())
      {
        p.payload = j.value("payload", std::string{});
      }
      return p;
    }
  };
} // namespace services