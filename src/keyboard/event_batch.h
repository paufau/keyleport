#pragma once

#include "input_event.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace keyboard
{

  struct EventBatch
  {
    std::vector<InputEvent> events;

    std::string encode() const;

    static EventBatch decode(const std::string& json);

    // Convenience vector-like API for seamless adoption
    inline bool   empty() const noexcept { return events.empty(); }
    inline size_t size() const noexcept { return events.size(); }
    inline void   clear() noexcept { events.clear(); }
    inline void   push_back(const InputEvent& event) { events.push_back(event); }
  };

} // namespace keyboard
