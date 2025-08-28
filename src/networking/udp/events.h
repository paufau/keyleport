// Common UDP event types used by udp_service and peer_connection

#pragma once

#include <string>

namespace net { namespace udp {

struct network_event_broadcast {
  std::string data;
  std::string from_address;
  int from_port{0};
};

struct network_event_connect {
  std::string address;
  int port{0};
};

struct network_event_disconnect {
  std::string address;
  int port{0};
};

struct network_event_data {
  std::string data;
  std::string from_address;
  int from_port{0};
};

}} // namespace net::udp
