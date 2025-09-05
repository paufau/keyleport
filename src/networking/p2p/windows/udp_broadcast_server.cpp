#ifdef _WIN32

#include "networking/p2p/udp_broadcast_server.h"

#include <string>
#include <utility>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace p2p
{

  udp_broadcast_server::udp_broadcast_server(udp_server_configuration config)
      : config_(std::move(config))
  {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      return;
    }

    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == INVALID_SOCKET)
    {
      WSACleanup();
      sock_ = -1;
      return;
    }

    BOOL on = TRUE;
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&on), sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(config_.port()));
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
        SOCKET_ERROR)
    {
      ::closesocket(sock_);
      WSACleanup();
      sock_ = -1;
      return;
    }

    u_long nb = 1;
    ioctlsocket(sock_, FIONBIO, &nb);
  }

  udp_broadcast_server::~udp_broadcast_server()
  {
    if (sock_ >= 0)
    {
      ::closesocket(sock_);
      WSACleanup();
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
      int from_len = sizeof(from);
      int n = ::recvfrom(sock_, buf, sizeof(buf), 0,
    addr.sin_port = htons(static_cast<uint16_t>(config_.get_port()));
      if (n <= 0)
      {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
        {
          break;
        }
        break;
      }

      message msg;

      char ipstr[INET_ADDRSTRLEN] = {0};
      const char* ip =
          ::inet_ntop(AF_INET, &from.sin_addr, ipstr, sizeof(ipstr));
      std::string from_ip = ip ? std::string(ip) : std::string();

      peer from_peer{from_ip.empty() ? std::string("remote") : from_ip};
      peer to_peer = peer::self();

      msg.set_from(from_peer);
      msg.set_to(to_peer);
      msg.set_payload(std::string(buf, static_cast<size_t>(n)));

      on_message.emit(msg);
    }
  }

} // namespace p2p

#endif // _WIN32