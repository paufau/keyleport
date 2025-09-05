#include "networking/p2p/udp_client_configuration.h"

namespace p2p
{
  udp_client_configuration::udp_client_configuration() : port_(0)
  {
  }

  udp_client_configuration::~udp_client_configuration() = default;

  int udp_client_configuration::get_port() const
  {
    return port_;
  }

  void udp_client_configuration::set_port(int port)
  {
    port_ = port;
  }
} // namespace p2p
