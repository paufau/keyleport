#include <string>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
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

    SocketHandle listen_udp(int port)
    {
#ifdef _WIN32
      ensure_wsa();
#endif
      SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
      if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;
      BOOL yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));
      // Disable UDP connection reset behavior to avoid WSAECONNRESET when ICMP unreachable is received
      DWORD bytesReturned = 0;
      BOOL newBehavior = FALSE;
      ::WSAIoctl(sock, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), NULL, 0, &bytesReturned, NULL, NULL);
      // Increase receive buffer
      int rcvbuf = 1 << 20; // 1MiB
      ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcvbuf), sizeof(rcvbuf));
#else
      if (sock < 0)
        return -1;
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      int rcvbuf = 1 << 20; // 1MiB
      ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
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
      SocketHandle udp = listen_udp(port_);
#ifdef _WIN32
      if (sock == INVALID_SOCKET && udp == INVALID_SOCKET)
        return 1;
#else
      if (sock < 0 && udp < 0)
        return 1;
#endif
      for (;;)
      {
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        if (sock != INVALID_SOCKET)
          FD_SET(sock, &readfds);
        if (udp != INVALID_SOCKET)
          FD_SET(udp, &readfds);
        int nfds = 0; // ignored by winsock select
        int ready = ::select(nfds, &readfds, nullptr, nullptr, nullptr);
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;
        if (sock >= 0)
        {
          FD_SET(sock, &readfds);
          if (sock > maxfd)
            maxfd = sock;
        }
        if (udp >= 0)
        {
          FD_SET(udp, &readfds);
          if (udp > maxfd)
            maxfd = udp;
        }
        int ready = ::select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
#endif
        if (ready <= 0)
          continue;

        // UDP datagrams
#ifdef _WIN32
        if (udp != INVALID_SOCKET && FD_ISSET(udp, &readfds))
#else
        if (udp >= 0 && FD_ISSET(udp, &readfds))
#endif
        {
          char buf[65536];
#ifdef _WIN32
          int addrlen = 0;
#else
          socklen_t addrlen = 0;
#endif
          sockaddr_in from{};
          addrlen = sizeof(from);
#ifdef _WIN32
          int n = ::recvfrom(udp, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&from), &addrlen);
          if (n > 0)
          {
            // Optional: log bytes received for debugging
            // std::cerr << "[udp recv] " << n << " bytes" << std::endl;
            if (handler_)
              handler_(std::string(buf, buf + n));
          }
#else
          ssize_t n = ::recvfrom(udp, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&from), &addrlen);
          if (n > 0)
          {
            // Optional: log bytes received for debugging
            // std::cerr << "[udp recv] " << n << " bytes" << std::endl;
            if (handler_)
              handler_(std::string(buf, static_cast<size_t>(n)));
          }
#endif
        }

        // New TCP connection
#ifdef _WIN32
        if (sock != INVALID_SOCKET && FD_ISSET(sock, &readfds))
#else
        if (sock >= 0 && FD_ISSET(sock, &readfds))
#endif
        {
          SocketHandle cfd = ::accept(sock, nullptr, nullptr);
#ifdef _WIN32
          if (cfd == INVALID_SOCKET)
          { /* ignore */
          }
          else
          {
            auto handler = handler_;
            std::thread([cfd, handler]() mutable
                        {
        std::string payload;
        // reuse recv_all logic locally
        auto recv_all_thread = [](SocketHandle s, std::string &out) -> int {
    char tbuf[4096];
    for(;;){ int n = ::recv(s, tbuf, sizeof(tbuf), 0); if (n < 0) return -1; if (n == 0) break; out.append(tbuf, n);} return 0; };
        if (recv_all_thread(cfd, payload) == 0 && handler) handler(payload);
        ::shutdown(cfd, SD_RECEIVE);
        ::closesocket(cfd); })
                .detach();
          }
#else
          if (cfd < 0)
          { /* ignore */
          }
          else
          {
            auto handler = handler_;
            std::thread([cfd, handler]() mutable
                        {
        std::string payload;
        auto recv_all_thread = [](SocketHandle s, std::string &out) -> int {
    char tbuf[4096];
    for(;;){ ssize_t n = ::recv(s, tbuf, sizeof(tbuf), 0); if (n < 0) return -1; if (n == 0) break; out.append(tbuf, static_cast<size_t>(n)); } return 0; };
        if (recv_all_thread(cfd, payload) == 0 && handler) handler(payload);
        ::shutdown(cfd, SHUT_RD);
        ::close(cfd); })
                .detach();
          }
#endif
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
