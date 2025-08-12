#include <string>
#include <cstring>
#include <iostream>
#include <memory>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "networking/server/receiver.h"

namespace net
{
  namespace
  {
    int listen_tcp(int port)
    {
      int sock = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sock < 0)
        return -1;
      int yes = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(port));
      if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
      {
        ::close(sock);
        return -2;
      }
      if (::listen(sock, 16) < 0)
      {
        ::close(sock);
        return -3;
      }
      return sock;
    }

    int recv_all(int sock, std::string &out)
    {
      char buf[4096];
      for (;;)
      {
        ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
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
      int sock = listen_tcp(port_);
      if (sock < 0)
        return 1;
      for (;;)
      {
        int cfd = ::accept(sock, nullptr, nullptr);
        if (cfd < 0)
          continue;
        std::string payload;
        if (recv_all(cfd, payload) == 0 && handler_)
          handler_(payload);
        ::shutdown(cfd, SHUT_RD);
        ::close(cfd);
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
