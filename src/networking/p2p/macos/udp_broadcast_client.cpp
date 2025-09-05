#ifdef __APPLE__

#include "networking/p2p/udp_broadcast_client.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace p2p
{

  udp_broadcast_client::udp_broadcast_client(udp_client_configuration config)
      : config_(std::move(config))
  {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0)
    {
      return;
    }

    int on = 1;
    ::setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif

    int flags = ::fcntl(sock_, F_GETFL, 0);
    if (flags >= 0)
    {
      ::fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    }
  }

  udp_broadcast_client::~udp_broadcast_client()
  {
    if (sock_ >= 0)
    {
      ::close(sock_);
      sock_ = -1;
    }
  }

  void udp_broadcast_client::broadcast(const std::string& message)
  {
    if (sock_ < 0 || message.empty())
    {
      return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(config_.get_port()));
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    (void)::sendto(sock_, message.data(), message.size(), 0,
                   reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  }

} // namespace p2p

#endif // __APPLE__
