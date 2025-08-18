#pragma once

#include "Message.h"

#include <chrono>
#include <stdint.h>
#include <string>

namespace net
{
  namespace p2p
  {

    struct Peer
    {
      std::string instance_id;
      uint64_t boot_id = 0;
      std::string device_name; // human-friendly device identifier
      std::string ip_address;
      uint16_t session_port = 0;
      State state = State::Idle;
      std::chrono::steady_clock::time_point last_heartbeat;
    };

  } // namespace p2p
} // namespace net
