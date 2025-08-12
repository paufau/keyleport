#ifdef __APPLE__

#include "networking/server/sender.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <netinet/in.h>
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
    int send_tcp(const std::string& data) override
    {
      SocketHandle sock = connect_tcp(ip_, port_);
      if (sock < 0)
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
      SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (sock < 0)
      {
        return 2;
      }
      int sndbuf = 1 << 20; // 1MiB
      ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
      timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 100 * 1000; // 100ms
      ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port_));
      if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0)
      {
        close_socket(sock);
        return 2;
      }

      int rc = ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      if (rc == 0)
      {
        ssize_t sent = ::send(sock, data.c_str(), data.size(), 0);
        if (sent < 0 || static_cast<size_t>(sent) != data.size())
        {
          std::cerr << "[udp send] send failed" << std::endl;
          close_socket(sock);
          return 3;
        }
      }
      else
      {
        ssize_t sent = ::sendto(sock, data.c_str(), data.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (sent < 0 || static_cast<size_t>(sent) != data.size())
        {
          std::cerr << "[udp send] sendto failed" << std::endl;
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

#endif // __APPLE__
