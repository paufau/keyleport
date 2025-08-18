#pragma once

#include <nlohmann/json.hpp>
#include <stdint.h>
#include <string>

namespace net
{
  namespace p2p
  {

    // Protocol constants
    static const char* const PROTOCOL = "p2p-demo/1";
    static const char* const MULTICAST_ADDR = "239.255.0.1";
    static const uint16_t DISCOVERY_PORT = 38001;

    enum class State
    {
      Idle,
      Connecting,
      Busy
    };

    inline const char* to_string(State s)
    {
      switch (s)
      {
      case State::Idle:
        return "idle";
      case State::Connecting:
        return "connecting";
      case State::Busy:
        return "busy";
      }
      return "idle";
    }

    inline State state_from_string(const std::string& s)
    {
      if (s == "idle")
      {
        return State::Idle;
      }
      if (s == "connecting")
      {
        return State::Connecting;
      }
      if (s == "busy")
      {
        return State::Busy;
      }
      return State::Idle;
    }

    struct Message
    {
      std::string proto; // "p2p-demo/1"
      std::string type;  // ANNOUNCE | DISCOVER | BUSY | FREE
      std::string from;  // instance_id
      uint64_t boot = 0; // boot_id
      std::string name;  // device name (optional)
      std::string ip;    // sender ip
      uint16_t port = 0; // TCP session port
      std::string state; // idle|connecting|busy (in ANNOUNCE)
    };

    // Serialization helpers
    ::nlohmann::json to_json(const Message& m);
    bool from_json(const ::nlohmann::json& j, Message& out);
    bool parse_json(const std::string& s, Message& out);
    std::string dump_json(const Message& m);

    // Factory helpers for common messages
    Message make_announce(const std::string& instance_id, uint64_t boot_id, const std::string& ip,
                          uint16_t session_port, State state);
    Message make_discover(const std::string& instance_id, uint64_t boot_id);
    Message make_status(const std::string& instance_id, uint64_t boot_id, bool busy, const std::string& ip,
                        uint16_t session_port);
    Message make_goodbye(const std::string& instance_id, uint64_t boot_id, const std::string& ip,
                         uint16_t session_port);

  } // namespace p2p
} // namespace net
