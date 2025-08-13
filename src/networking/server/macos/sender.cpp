#ifdef __APPLE__

#include "networking/server/sender.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace net
{
  namespace
  {
    using SocketHandle = int;
    inline void close_socket(SocketHandle s)
    {
      ::close(s);
    }
    inline int shutdown_send(SocketHandle s)
    {
      return ::shutdown(s, SHUT_WR);
    }

    SocketHandle connect_tcp(const std::string& ip, int port)
    {
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sock < 0)
      {
        return -1;
      }
      // Avoid SIGPIPE on send (macOS-specific)
      int set = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
      // Enable keepalive for long-lived connections
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
      // Reduce latency
      ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
      {
        close_socket(sock);
        return -2;
      }
      if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return -3;
      }
      return sock;
    }

    int send_all(SocketHandle sock, const std::string& data)
    {
      const char* p = data.c_str();
      size_t remaining = data.size();
      while (remaining > 0)
      {
        ssize_t n = ::send(sock, p, remaining, 0);
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
      // TCP
      if (tcp_sock_ < 0)
      {
        tcp_sock_ = connect_tcp(ip_, port_);
        if (tcp_sock_ < 0)
        {
          tcp_sock_ = -1;
          return 2;
        }
      }
      // UDP (connected UDP to set default peer)
      if (udp_sock_ < 0)
      {
        udp_sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sock_ < 0)
        {
          disconnect();
          return 2;
        }
        int sndbuf = 1 << 20; // 1MiB
        ::setsockopt(udp_sock_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000; // 100ms
        ::setsockopt(udp_sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
        {
          disconnect();
          return 2;
        }
        // connect UDP to set default destination (optional; sendto fallback available)
        (void)::connect(udp_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      }
      return 0;
    }

    int disconnect() override
    {
      if (tcp_sock_ >= 0)
      {
        shutdown_send(tcp_sock_);
        close_socket(tcp_sock_);
        tcp_sock_ = -1;
      }
      if (udp_sock_ >= 0)
      {
        close_socket(udp_sock_);
        udp_sock_ = -1;
      }
      return 0;
    }
    int send_tcp(const std::string& data) override
    {
      if (tcp_sock_ < 0)
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
      // Send header then payload
      if (send_all(tcp_sock_, std::string(hdr, sizeof(hdr))) != 0 || send_all(tcp_sock_, data) != 0)
      {
        // try one reconnect
        close_socket(tcp_sock_);
        tcp_sock_ = -1;
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
      if (udp_sock_ < 0)
      {
        int rc = connect();
        if (rc != 0)
        {
          return rc;
        }
      }
      ssize_t sent = ::send(udp_sock_, data.c_str(), data.size(), 0);
      if (sent < 0 || static_cast<size_t>(sent) != data.size())
      {
        // fallback: attempt to sendto using destination
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
        {
          return 2;
        }
        sent = ::sendto(udp_sock_, data.c_str(), data.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (sent < 0 || static_cast<size_t>(sent) != data.size())
        {
          std::cerr << "[udp send] failed on persistent socket" << std::endl;
          return 3;
        }
      }
      return 0;
    }

  private:
    std::string ip_;
    int port_;
    SocketHandle tcp_sock_ = -1;
    SocketHandle udp_sock_ = -1;
    std::mutex tcp_mu_;
  };

  std::unique_ptr<Sender> make_sender(const std::string& ip, int port)
  {
    return std::unique_ptr<Sender>(new CxxSender(ip, port));
  }

} // namespace net

#endif // __APPLE__
