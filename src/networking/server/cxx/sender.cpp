#include <string>
#include <cstring>
#include <iostream>
#include <memory>
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

#include "networking/server/sender.h"

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
    inline int shutdown_send(SocketHandle s) { return ::shutdown(s, SD_SEND); }
#else
    using SocketHandle = int;
    inline void ensure_wsa() {}
    inline void close_socket(SocketHandle s) { ::close(s); }
    inline int shutdown_send(SocketHandle s) { return ::shutdown(s, SHUT_WR); }
#endif

    SocketHandle connect_tcp(const std::string &ip, int port)
    {
#ifdef _WIN32
      ensure_wsa();
#endif
      SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
      if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;
#else
      if (sock < 0)
        return -1;
#endif
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
      {
        close_socket(sock);
#ifdef _WIN32
        return INVALID_SOCKET;
#else
        return -2;
#endif
      }
      if (::connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
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

    int send_all(SocketHandle sock, const std::string &data)
    {
      const char *p = data.c_str();
      size_t remaining = data.size();
      while (remaining > 0)
      {
#ifdef _WIN32
        int n = ::send(sock, p, static_cast<int>(remaining), 0);
#else
        ssize_t n = ::send(sock, p, remaining, 0);
#endif
        if (n <= 0)
          return -1;
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
    int send(const std::string &data) override
    {
      SocketHandle sock = connect_tcp(ip_, port_);
#ifdef _WIN32
      if (sock == INVALID_SOCKET)
        return 2;
#else
      if (sock < 0)
        return 2;
#endif
      if (send_all(sock, data) != 0)
      {
        close_socket(sock);
        return 3;
      }
      shutdown_send(sock);
      close_socket(sock);
      return 0;
    }

  private:
    std::string ip_;
    int port_;
  };

  std::unique_ptr<Sender> make_sender(const std::string &ip, int port)
  {
    return std::unique_ptr<Sender>(new CxxSender(ip, port));
  }

} // namespace net
