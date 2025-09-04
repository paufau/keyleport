#include "networking/p2p/udp_server_configuration.h"

namespace p2p
{
  udp_server_configuration::udp_server_configuration() : port_(0)
  {
  }

  udp_server_configuration::~udp_server_configuration() = default;

  int udp_server_configuration::get_port() const
  {
    return port_;
  }

  void udp_server_configuration::set_port(int port)
  {
    port_ = port;
  }
} // namespace p2p
