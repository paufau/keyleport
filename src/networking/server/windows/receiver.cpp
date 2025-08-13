#ifdef _WIN32

#include "networking/server/receiver.h"

#include "networking/discovery/discovery.h"

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
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

    SocketHandle listen_tcp(int port)
    {
      ensure_wsa();
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sock == INVALID_SOCKET)
      {
        return INVALID_SOCKET;
      }
      BOOL yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return INVALID_SOCKET;
      }
      if (::listen(sock, 16) < 0)
      {
        close_socket(sock);
        return INVALID_SOCKET;
      }
      return sock;
    }

    SocketHandle listen_udp(int port)
    {
      ensure_wsa();
      SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (sock == INVALID_SOCKET)
      {
        return INVALID_SOCKET;
      }
      BOOL yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
      // Note: Intentionally not using SIO_UDP_CONNRESET due to SDK compatibility variance.
      int rcvbuf = 1 << 20; // 1MiB
      ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
        return INVALID_SOCKET;
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
      // Start UDP discovery responder for clients with no configured IP
      auto responder = discovery::make_responder(port_, "keyleport");
      if (responder)
      {
        responder->start_async();
      }
      SocketHandle sock = listen_tcp(port_);
      SocketHandle udp = listen_udp(port_);
      if (sock == INVALID_SOCKET && udp == INVALID_SOCKET)
      {
        return 1;
      }

      for (;;)
      {
        fd_set readfds;
        FD_ZERO(&readfds);
        if (sock != INVALID_SOCKET)
        {
          FD_SET(sock, &readfds);
        }
        if (udp != INVALID_SOCKET)
        {
          FD_SET(udp, &readfds);
        }
        int nfds = 0; // ignored by winsock's select
        int ready = ::select(nfds, &readfds, nullptr, nullptr, nullptr);
        if (ready <= 0)
        {
          continue;
        }

        if (udp != INVALID_SOCKET && FD_ISSET(udp, &readfds))
        {
          char buf[65536];
          sockaddr_in from{};
          int addrlen = sizeof(from);
          int n = ::recvfrom(udp, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &addrlen);
          if (n > 0 && handler_)
          {
            handler_(std::string(buf, buf + n));
          }
        }

        if (sock != INVALID_SOCKET && FD_ISSET(sock, &readfds))
        {
          SocketHandle cfd = ::accept(sock, nullptr, nullptr);
          if (cfd != INVALID_SOCKET)
          {
            auto handler = handler_;
            std::thread(
                [cfd, handler]() mutable
                {
                  std::string buffer;
                  char tbuf[4096];
                  for (;;)
                  {
                    int n = ::recv(cfd, tbuf, sizeof(tbuf), 0);
                    if (n <= 0)
                    {
                      break;
                    }
                    buffer.append(tbuf, n);
                    for (;;)
                    {
                      if (buffer.size() < 4)
                      {
                        break;
                      }
                      uint32_t nlen_be = 0;
                      std::memcpy(&nlen_be, buffer.data(), 4);
                      uint32_t nlen = ntohl(nlen_be);
                      if (buffer.size() < 4u + nlen)
                      {
                        break;
                      }
                      std::string msg = buffer.substr(4, nlen);
                      buffer.erase(0, 4u + nlen);
                      if (handler)
                      {
                        handler(msg);
                      }
                    }
                  }
                  ::shutdown(cfd, SD_RECEIVE);
                  ::closesocket(cfd);
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

#endif // _WIN32