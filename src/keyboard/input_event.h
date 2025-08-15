#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

// Forward declare SDL event to avoid pulling SDL headers in users of this header
union SDL_Event;

namespace keyboard
{

  struct InputEvent
  {
    enum class Action : uint8_t
    {
      Down = 0,
      Up = 1,
      Move = 2,
      Scroll = 3
    };

    enum class Type : uint8_t
    {
      Key = 0,
      Mouse = 1
    };

    Type type;     // input source
    Action action; // key/mouse action
    uint16_t code; // keycode
    int32_t dx;    // relative movement x (for mouse move/scroll)
    int32_t dy;    // relative movement y (for mouse move/scroll)

    // Convert an SDL_Event to our InputEvent representation.
    // For unmapped events, returns a zero-initialized InputEvent.
    static InputEvent fromSDL(const SDL_Event& e);
  };

  // JSON converter for a single InputEvent
  struct InputEventJSONConverter
  {
    // Encode a single InputEvent into a compact JSON string.
    static inline std::string encode(const InputEvent& e)
    {
      nlohmann::json j{{"type", static_cast<int>(e.type)},
                       {"action", static_cast<int>(e.action)},
                       {"code", static_cast<int>(e.code)},
                       {"dx", e.dx},
                       {"dy", e.dy}};
      return j.dump();
    }

    // Decode a single InputEvent from a JSON string.
    static inline InputEvent decode(const std::string& s)
    {
      InputEvent e{};
      auto j = nlohmann::json::parse(s, nullptr, false);

      if (!j.is_object())
      {
        return e;
      }

      e.type = static_cast<InputEvent::Type>(j.value("type", 0));
      e.action = static_cast<InputEvent::Action>(j.value("action", 0));
      e.code = static_cast<uint16_t>(j.value("code", 0));
      e.dx = j.value("dx", 0);
      e.dy = j.value("dy", 0);

      return e;
    }
  };

} // namespace keyboard
