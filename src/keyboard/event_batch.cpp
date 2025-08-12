#include "event_batch.h"
#include <nlohmann/json.hpp>

namespace keyboard
{

  std::string EventBatch::encode() const
  {
    nlohmann::json j;
    j["events"] = nlohmann::json::array();
    for (const auto &e : events)
    {
      j["events"].push_back({{"type", static_cast<int>(e.type)},
                             {"action", static_cast<int>(e.action)},
                             {"code", static_cast<int>(e.code)},
                             {"x", e.x},
                             {"y", e.y},
                             {"dx", e.dx},
                             {"dy", e.dy},
                             {"delta", e.delta}});
    }
    return j.dump();
  }

  EventBatch EventBatch::decode(const std::string &s)
  {
    EventBatch batch;
    auto j = nlohmann::json::parse(s, nullptr, false);
    if (!j.is_object())
      return batch;
    auto it = j.find("events");
    if (it == j.end() || !it->is_array())
      return batch;
    for (const auto &je : *it)
    {
      if (!je.is_object())
        continue;
      InputEvent e{};
      e.type = static_cast<uint8_t>(je.value("type", 0));
      e.action = static_cast<uint8_t>(je.value("action", 0));
      e.code = static_cast<uint16_t>(je.value("code", 0));
      e.x = je.value("x", 0);
      e.y = je.value("y", 0);
      e.dx = je.value("dx", 0);
      e.dy = je.value("dy", 0);
      e.delta = je.value("delta", 0);
      batch.events.push_back(e);
    }
    return batch;
  }

} // namespace keyboard
