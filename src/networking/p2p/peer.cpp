#include "networking/p2p/peer.h"

#include "utils/device_name/device_name.h"

namespace p2p
{

  peer::peer(const std::string& ip_address) : ip_address_(ip_address)
  {
  }

  peer::~peer() = default;

  peer peer::self()
  {
    const std::string ip = "127.0.0.1";
    return peer{ip};
  }
  std::string peer::get_ip_address() const
  {
    return ip_address_;
  }
  void peer::set_ip_address(const std::string& ip_address)
  {
    ip_address_ = ip_address;
  }

} // namespace p2p
