#ifdef _WIN32

#include "networking/server/sender.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace net
{
  namespace
  {
    using SocketHandle = SOCKET;
    static void ensure_wsa()
    {
      static bool inited = false;
      if (!inited)
      {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
        {
          inited = true;
        }
      }
    }
    inline void close_socket(SocketHandle s)
    {
      ::closesocket(s);
    }
    inline int shutdown_send(SocketHandle s)
    {
      return ::shutdown(s, SD_SEND);
    }

    SocketHandle connect_tcp(const std::string& ip, int port)
    {
      ensure_wsa();
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sock == INVALID_SOCKET)
      {
        return INVALID_SOCKET;
      }
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
      {
        close_socket(sock);
        return INVALID_SOCKET;
      }
      if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return INVALID_SOCKET;
      }
      return sock;
    }

    int send_all(SocketHandle sock, const std::string& data)
    {
      const char* p = data.c_str();
      size_t remaining = data.size();
      while (remaining > 0)
      {
        int n = ::send(sock, p, static_cast<int>(remaining), 0);
        if (n <= 0)
        {
          return -1;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
      }
      return 0;
    }
  } // namespace

  class CxxSender : public Sender
  {
  public:
    CxxSender(std::string ip, int port) : ip_(std::move(ip)), port_(port) {}
    int run() override { return 0; }
    int send_tcp(const std::string& data) override
    {
      SocketHandle sock = connect_tcp(ip_, port_);
      if (sock == INVALID_SOCKET)
      {
        return 2;
      }
      if (send_all(sock, data) != 0)
      {
        close_socket(sock);
        return 3;
      }
      shutdown_send(sock);
      close_socket(sock);
      return 0;
    }

    int send_udp(const std::string& data) override
    {
      if (data.empty())
      {
        return 0;
      }
      ensure_wsa();
      SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (sock == INVALID_SOCKET)
      {
        return 2;
      }
      int sndbuf = 1 << 20; // 1MiB
      ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));
      DWORD timeoutMs = 100; // 100ms
      ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port_));
      if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
      {
        close_socket(sock);
        return 2;
      }

      int rc = ::connect(sock, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(sizeof(addr)));
      if (rc == 0)
      {
        int sent = ::send(sock, data.c_str(), static_cast<int>(data.size()), 0);
        if (sent == SOCKET_ERROR || sent != static_cast<int>(data.size()))
        {
          std::cerr << "[udp send] send failed, WSAGetLastError=" << WSAGetLastError() << std::endl;
          close_socket(sock);
          return 3;
        }
      }
      else
      {
        int sent = ::sendto(sock, data.c_str(), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&addr),
                            static_cast<int>(sizeof(addr)));
        if (sent == SOCKET_ERROR || sent != static_cast<int>(data.size()))
        {
          std::cerr << "[udp send] sendto failed, WSAGetLastError=" << WSAGetLastError() << std::endl;
          close_socket(sock);
          return 3;
        }
      }
      close_socket(sock);
      return 0;
    }

  private:
    std::string ip_;
    int port_;
  };

  std::unique_ptr<Sender> make_sender(const std::string& ip, int port)
  {
    return std::unique_ptr<Sender>(new CxxSender(ip, port));
  }

} // namespace net

#endif // _WIN32
