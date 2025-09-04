#ifdef __APPLE__

#include "networking/p2p/udp_broadcast_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace p2p
{

  udp_broadcast_server::udp_broadcast_server(udp_server_configuration config)
      : config_(std::move(config))
  {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0)
    {
      return;
    }

    int on = 1;
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#ifdef SO_REUSEPORT
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(config_.get_port()));
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
      ::close(sock_);
      sock_ = -1;
      return;
    }

    int flags = ::fcntl(sock_, F_GETFL, 0);
    if (flags >= 0)
    {
      ::fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    }
  }

  udp_broadcast_server::~udp_broadcast_server()
  {
    if (sock_ >= 0)
    {
      ::close(sock_);
      sock_ = -1;
    }
  }

  void udp_broadcast_server::poll_events()
  {
    if (sock_ < 0)
    {
      return;
    }

    for (int i = 0; i < 8; ++i)
    {
      char buf[1500];
      sockaddr_in from{};
      socklen_t from_len = sizeof(from);
      ssize_t n = ::recvfrom(sock_, buf, sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&from), &from_len);
      if (n <= 0)
      {
        break;
      }

      message msg;

      char ipstr[INET_ADDRSTRLEN] = {0};
      const char* ip =
          ::inet_ntop(AF_INET, &from.sin_addr, ipstr, sizeof(ipstr));
      std::string from_ip = ip ? std::string(ip) : std::string();

      if (from_ip.empty())
      {
        std::cerr << "Failed to get sender IP address" << std::endl;
        continue;
      }

      peer from_peer{from_ip};
      peer to_peer = peer::self();

      msg.set_from(from_peer);
      msg.set_to(to_peer);
      msg.set_payload(std::string(buf, static_cast<size_t>(n)));

      on_message.emit(msg);
    }
  }

} // namespace p2p

#endif // __APPLE__