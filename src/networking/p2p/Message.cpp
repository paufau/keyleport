#include "Message.h"

#include <sstream>

namespace net
{
  namespace p2p
  {

    ::nlohmann::json to_json(const Message& m)
    {
      ::nlohmann::json j;
      j["proto"] = m.proto;
      j["type"] = m.type;
      j["from"] = m.from;
      j["boot"] = m.boot;
      if (!m.name.empty())
      {
        j["name"] = m.name;
      }
      if (!m.ip.empty())
      {
        j["ip"] = m.ip;
      }
      if (m.port)
      {
        j["port"] = m.port;
      }
      if (!m.state.empty())
      {
        j["state"] = m.state;
      }
      return j;
    }

    bool from_json(const ::nlohmann::json& j, Message& out)
    {
      try
      {
        out.proto = j.value("proto", std::string());
        out.type = j.value("type", std::string());
        out.from = j.value("from", std::string());
        out.boot = j.value("boot", 0ULL);
        out.name = j.value("name", std::string());
        out.ip = j.value("ip", std::string());
        out.port = static_cast<uint16_t>(j.value("port", 0));
        out.state = j.value("state", std::string());
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    bool parse_json(const std::string& s, Message& out)
    {
      try
      {
        auto j = ::nlohmann::json::parse(s);
        return from_json(j, out);
      }
      catch (...)
      {
        return false;
      }
    }

    std::string dump_json(const Message& m)
    {
      auto j = to_json(m);
      return j.dump();
    }

    Message make_announce(const std::string& instance_id, uint64_t boot_id, const std::string& ip,
                          uint16_t session_port, State state)
    {
      Message m;
      m.proto = PROTOCOL;
      m.type = "ANNOUNCE";
      m.from = instance_id;
      m.boot = boot_id;
      m.ip = ip;
      m.port = session_port;
      // Spec requires state to be "idle" or "busy" only in JSON.
      // Treat Connecting as Busy for availability purposes.
      m.state = (state == State::Busy || state == State::Connecting) ? "busy" : "idle";
      return m;
    }

    Message make_discover(const std::string& instance_id, uint64_t boot_id)
    {
      Message m;
      m.proto = PROTOCOL;
      m.type = "DISCOVER";
      m.from = instance_id;
      m.boot = boot_id;
      return m;
    }

    Message make_status(const std::string& instance_id, uint64_t boot_id, bool busy, const std::string& ip,
                        uint16_t session_port)
    {
      Message m;
      m.proto = PROTOCOL;
      m.type = busy ? "BUSY" : "FREE";
      m.from = instance_id;
      m.boot = boot_id;
      m.ip = ip;
      m.port = session_port;
      m.state = busy ? "busy" : "idle";
      return m;
    }

    Message make_goodbye(const std::string& instance_id, uint64_t boot_id, const std::string& ip, uint16_t session_port)
    {
      Message m;
      m.proto = PROTOCOL;
      m.type = "BYE";
      m.from = instance_id;
      m.boot = boot_id;
      m.ip = ip;
      m.port = session_port;
      return m;
    }

  } // namespace p2p
} // namespace net
