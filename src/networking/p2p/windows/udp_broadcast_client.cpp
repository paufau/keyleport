#ifdef _WIN32

#include "networking/p2p/udp_broadcast_client.h"

#include <string>
#include <utility>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace p2p
{

  udp_broadcast_client::udp_broadcast_client(udp_client_configuration config)
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
    ::setsockopt(sock_, SOL_SOCKET, SO_BROADCAST,
                 reinterpret_cast<const char*>(&on), sizeof(on));
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&on), sizeof(on));
  }

  udp_broadcast_client::~udp_broadcast_client()
  {
    if (sock_ >= 0)
    {
      ::closesocket(sock_);
      WSACleanup();
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

    (void)::sendto(sock_, message.data(), static_cast<int>(message.size()), 0,
                   reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  }

} // namespace p2p

#endif // _WIN32
