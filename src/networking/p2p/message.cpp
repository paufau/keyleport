#include "networking/p2p/message.h"

namespace p2p
{

  message::message() = default;
  message::~message() = default;

  void message::set_payload(const std::string& payload)
  {
    payload_ = payload;
  }
  void message::set_from(const peer& from)
  {
    from_ = from;
  }
  void message::set_to(const peer& to)
  {
    to_ = to;
  }

  std::string message::get_payload() const
  {
    return payload_;
  }
  peer message::get_from() const
  {
    return from_;
  }
  peer message::get_to() const
  {
    return to_;
  }

} // namespace p2p
