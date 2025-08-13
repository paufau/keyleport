#ifdef _WIN32

#include "networking/server/sender.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
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

    int connect() override
    {
      ensure_wsa();
      if (tcp_sock_ == INVALID_SOCKET)
      {
        tcp_sock_ = connect_tcp(ip_, port_);
        if (tcp_sock_ == INVALID_SOCKET)
        {
          return 2;
        }
        // Reduce latency
        BOOL yes = 1;
        ::setsockopt(tcp_sock_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes));
      }
      if (udp_sock_ == INVALID_SOCKET)
      {
        udp_sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sock_ == INVALID_SOCKET)
        {
          disconnect();
          return 2;
        }
        int sndbuf = 1 << 20; // 1MiB
        ::setsockopt(udp_sock_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));
        DWORD timeoutMs = 100; // 100ms
        ::setsockopt(udp_sock_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
        {
          disconnect();
          return 2;
        }
        (void)::connect(udp_sock_, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(sizeof(addr)));
      }
      return 0;
    }

    int disconnect() override
    {
      if (tcp_sock_ != INVALID_SOCKET)
      {
        shutdown_send(tcp_sock_);
        close_socket(tcp_sock_);
        tcp_sock_ = INVALID_SOCKET;
      }
      if (udp_sock_ != INVALID_SOCKET)
      {
        close_socket(udp_sock_);
        udp_sock_ = INVALID_SOCKET;
      }
      return 0;
    }
    int send_tcp(const std::string& data) override
    {
      if (tcp_sock_ == INVALID_SOCKET)
      {
        int rc = connect();
        if (rc != 0)
        {
          return rc;
        }
      }
      // Frame: 4-byte big-endian length + payload
      std::lock_guard<std::mutex> lk(tcp_mu_);
      uint32_t nlen = htonl(static_cast<uint32_t>(data.size()));
      char hdr[4];
      std::memcpy(hdr, &nlen, sizeof(nlen));
      if (send_all(tcp_sock_, std::string(hdr, sizeof(hdr))) != 0 || send_all(tcp_sock_, data) != 0)
      {
        close_socket(tcp_sock_);
        tcp_sock_ = INVALID_SOCKET;
        if (connect() != 0)
        {
          return 3;
        }
        uint32_t nlen2 = htonl(static_cast<uint32_t>(data.size()));
        char hdr2[4];
        std::memcpy(hdr2, &nlen2, sizeof(nlen2));
        if (send_all(tcp_sock_, std::string(hdr2, sizeof(hdr2))) != 0 || send_all(tcp_sock_, data) != 0)
        {
          return 3;
        }
      }
      return 0;
    }

    int send_udp(const std::string& data) override
    {
      if (data.empty())
      {
        return 0;
      }
      ensure_wsa();
      if (udp_sock_ == INVALID_SOCKET)
      {
        int rc = connect();
        if (rc != 0)
        {
          return rc;
        }
      }
      int sent = ::send(udp_sock_, data.c_str(), static_cast<int>(data.size()), 0);
      if (sent == SOCKET_ERROR || sent != static_cast<int>(data.size()))
      {
        // attempt sendto fallback
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
        {
          return 2;
        }
        sent = ::sendto(udp_sock_, data.c_str(), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&addr),
                        static_cast<int>(sizeof(addr)));
        if (sent == SOCKET_ERROR || sent != static_cast<int>(data.size()))
        {
          std::cerr << "[udp send] failed on persistent socket, WSAGetLastError=" << WSAGetLastError() << std::endl;
          return 3;
        }
      }
      return 0;
    }

  private:
    std::string ip_;
    int port_;
    SocketHandle tcp_sock_ = INVALID_SOCKET;
    SocketHandle udp_sock_ = INVALID_SOCKET;
    std::mutex tcp_mu_;
  };

  std::unique_ptr<Sender> make_sender(const std::string& ip, int port)
  {
    return std::unique_ptr<Sender>(new CxxSender(ip, port));
  }

} // namespace net

#endif // _WIN32
