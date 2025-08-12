#ifdef __APPLE__

#include "networking/server/receiver.h"

#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
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

    SocketHandle listen_tcp(int port)
    {
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sock < 0)
      {
        return -1;
      }
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return -2;
      }
      if (::listen(sock, 16) < 0)
      {
        close_socket(sock);
        return -3;
      }
      return sock;
    }

    SocketHandle listen_udp(int port)
    {
      SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (sock < 0)
      {
        return -1;
      }
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      int rcvbuf = 1 << 20;
      ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return -2;
      }
      return sock;
    }
  } // namespace

  class CxxReceiver : public Receiver
  {
  public:
    explicit CxxReceiver(int port) : port_(port) {}
    void onReceive(ReceiveHandler handler) override { handler_ = std::move(handler); }
    int run() override
    {
      SocketHandle sock = listen_tcp(port_);
      SocketHandle udp = listen_udp(port_);
      if (sock < 0 && udp < 0)
      {
        return 1;
      }

      for (;;)
      {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;
        if (sock >= 0)
        {
          FD_SET(sock, &readfds);
          if (sock > maxfd)
          {
            maxfd = sock;
          }
        }
        if (udp >= 0)
        {
          FD_SET(udp, &readfds);
          if (udp > maxfd)
          {
            maxfd = udp;
          }
        }
        int ready = ::select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (ready <= 0)
        {
          continue;
        }

        if (udp >= 0 && FD_ISSET(udp, &readfds))
        {
          char buf[65536];
          sockaddr_in from{};
          socklen_t addrlen = sizeof(from);
          ssize_t n = ::recvfrom(udp, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &addrlen);
          if (n > 0 && handler_)
          {
            handler_(std::string(buf, static_cast<size_t>(n)));
          }
        }

        if (sock >= 0 && FD_ISSET(sock, &readfds))
        {
          SocketHandle cfd = ::accept(sock, nullptr, nullptr);
          if (cfd >= 0)
          {
            auto handler = handler_;
            std::thread(
                [cfd, handler]() mutable
                {
                  std::string payload;
                  char tbuf[4096];
                  for (;;)
                  {
                    ssize_t n = ::recv(cfd, tbuf, sizeof(tbuf), 0);
                    if (n < 0)
                    {
                      break;
                    }
                    if (n == 0)
                    {
                      break;
                    }
                    payload.append(tbuf, static_cast<size_t>(n));
                  }
                  if (handler)
                  {
                    handler(payload);
                  }
                  ::shutdown(cfd, SHUT_RD);
                  ::close(cfd);
                })
                .detach();
          }
        }
      }
      return 0;
    }

  private:
    int port_;
    ReceiveHandler handler_;
  };

  std::unique_ptr<Receiver> make_receiver(int port)
  {
    return std::unique_ptr<Receiver>(new CxxReceiver(port));
  }

} // namespace net

#endif // __APPLE__