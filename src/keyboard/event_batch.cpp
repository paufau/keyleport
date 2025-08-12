#include "event_batch.h"

#include <nlohmann/json.hpp>

namespace keyboard
{

  std::string EventBatch::encode() const
  {
    nlohmann::json j;
    j["events"] = nlohmann::json::array();
    for (const auto& e : events)
    {
      j["events"].push_back({{"type", static_cast<int>(e.type)},
                             {"action", static_cast<int>(e.action)},
                             {"code", static_cast<int>(e.code)},
                             {"dx", e.dx},
                             {"dy", e.dy}});
    }
    return j.dump();
  }

  EventBatch EventBatch::decode(const std::string& s)
  {
    EventBatch batch;
    auto j = nlohmann::json::parse(s, nullptr, false);
    if (!j.is_object())
    {
      return batch;
    }
    auto it = j.find("events");
    if (it == j.end() || !it->is_array())
    {
      return batch;
    }
    for (const auto& je : *it)
    {
      if (!je.is_object())
      {
        continue;
      }
      InputEvent e{};
      e.type = static_cast<InputEvent::Type>(je.value("type", 0));
      e.action = static_cast<InputEvent::Action>(je.value("action", 0));
      e.code = static_cast<uint16_t>(je.value("code", 0));
      e.dx = je.value("dx", 0);
      e.dy = je.value("dy", 0);
      // Back-compat: if payload uses legacy 'delta' for vertical scroll, map it to dy
      if (e.action == InputEvent::Action::Scroll && e.dy == 0)
      {
        e.dy = je.value("delta", 0);
      }
      batch.events.push_back(e);
    }
    return batch;
  }

} // namespace keyboard
