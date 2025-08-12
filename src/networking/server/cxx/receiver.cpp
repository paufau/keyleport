#include <string>
#include <cstring>
#include <iostream>
#include <memory>
#include <functional>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "networking/server/receiver.h"

namespace net
{
  namespace
  {

#ifdef _WIN32
    using SocketHandle = SOCKET;
    static void ensure_wsa()
    {
      static bool inited = false;
      if (!inited)
      {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
          inited = true;
      }
    }
    inline void close_socket(SocketHandle s) { ::closesocket(s); }
    inline int shutdown_read(SocketHandle s) { return ::shutdown(s, SD_RECEIVE); }
#else
    using SocketHandle = int;
    inline void ensure_wsa() {}
    inline void close_socket(SocketHandle s) { ::close(s); }
    inline int shutdown_read(SocketHandle s) { return ::shutdown(s, SHUT_RD); }
#endif

    SocketHandle listen_tcp(int port)
    {
#ifdef _WIN32
      ensure_wsa();
#endif
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
      if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;
      BOOL yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));
#else
      if (sock < 0)
        return -1;
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
      {
        close_socket(sock);
#ifdef _WIN32
        return INVALID_SOCKET;
#else
        return -2;
#endif
      }
      if (::listen(sock, 16) < 0)
      {
        close_socket(sock);
#ifdef _WIN32
        return INVALID_SOCKET;
#else
        return -3;
#endif
      }
      return sock;
    }

    int recv_all(SocketHandle sock, std::string &out)
    {
      char buf[4096];
      for (;;)
      {
#ifdef _WIN32
        int n = ::recv(sock, buf, sizeof(buf), 0);
#else
        ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
#endif
        if (n < 0)
          return -1;
        if (n == 0)
          break;
        out.append(buf, static_cast<size_t>(n));
      }
      return 0;
    }
  }

  class CxxReceiver : public Receiver
  {
  public:
    explicit CxxReceiver(int port) : port_(port) {}
    void onReceive(ReceiveHandler handler) override { handler_ = std::move(handler); }
    int run() override
    {
      SocketHandle sock = listen_tcp(port_);
#ifdef _WIN32
      if (sock == INVALID_SOCKET)
        return 1;
#else
      if (sock < 0)
        return 1;
#endif
      for (;;)
      {
        SocketHandle cfd = ::accept(sock, nullptr, nullptr);
#ifdef _WIN32
        if (cfd == INVALID_SOCKET)
          continue;
#else
        if (cfd < 0)
          continue;
#endif
        std::string payload;
        if (recv_all(cfd, payload) == 0 && handler_)
          handler_(payload);
        shutdown_read(cfd);
        close_socket(cfd);
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
