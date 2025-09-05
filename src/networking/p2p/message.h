#pragma once

#include "networking/p2p/peer.h"

#include <string>

namespace p2p
{
  class message
  {
  public:
    message();
    ~message();

    void set_payload(const std::string& payload);
    void set_from(const peer& from);
    void set_to(const peer& to);

    std::string get_payload() const;
    peer get_from() const;
    peer get_to() const;

  private:
    std::string payload_;
    peer from_;
    peer to_;
  };
} // namespace p2p
