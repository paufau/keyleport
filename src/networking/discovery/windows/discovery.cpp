#ifdef _WIN32

#include "networking/discovery/discovery.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace net
{
  namespace discovery
  {
    namespace
    {
      static const char* kProbePrefix = "KP_DISCOVER_V1?";
      static const char* kRespPrefix = "KP_DISCOVER_V1!";
      static const int kDiscoveryPort = 35353; // dedicated UDP port for discovery

      void ensure_wsa()
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

      SOCKET make_udp_socket()
      {
        ensure_wsa();
        SOCKET s = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (s == INVALID_SOCKET)
        {
          return INVALID_SOCKET;
        }
        BOOL yes = 1;
        ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
        return s;
      }
    } // namespace

    class Responder : public DiscoveryResponder
    {
    public:
      Responder(int servicePort, std::string name) : port_(servicePort), name_(std::move(name)) {}
      ~Responder()
      {
        running_.store(false);
        if (thr_.joinable())
        {
          thr_.join();
        }
        if (sock_ != INVALID_SOCKET)
        {
          ::closesocket(sock_);
        }
      }
      void start_async() override
      {
        if (started_)
        {
          return;
        }
        started_ = true;
        running_.store(true);
        thr_ = std::thread([this] { loop(); });
      }

    private:
      void loop()
      {
        sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ == INVALID_SOCKET)
        {
          return;
        }
        BOOL yes = 1;
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        // Bind to dedicated discovery port, not the service port, to avoid stealing service UDP packets
        addr.sin_port = htons(static_cast<uint16_t>(kDiscoveryPort));
        if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
          return;
        }
        char buf[1024];
        while (running_.load())
        {
          fd_set rfds;
          FD_ZERO(&rfds);
          FD_SET(sock_, &rfds);
          timeval tv{1, 0};
          int r = ::select(0, &rfds, nullptr, nullptr, &tv);
          if (r <= 0)
          {
            continue;
          }
          sockaddr_in from{};
          int fl = sizeof(from);
          int n = ::recvfrom(sock_, buf, sizeof(buf) - 1, 0, reinterpret_cast<sockaddr*>(&from), &fl);
          if (n <= 0)
          {
            continue;
          }
          buf[n] = 0;
          if (std::strncmp(buf, kProbePrefix, std::strlen(kProbePrefix)) != 0)
          {
            continue;
          }
          // Respond with service info
          char ipbuf[64];
          const char* src = ::inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));
          if (!src)
          {
            std::strcpy(ipbuf, "0.0.0.0");
          }
          char out[256];
          std::snprintf(out, sizeof(out), "%s ip=%s;port=%d;name=%s;platform=windows", kRespPrefix, ipbuf, port_,
                        name_.c_str());
          ::sendto(sock_, out, static_cast<int>(std::strlen(out)), 0, reinterpret_cast<sockaddr*>(&from), fl);
        }
      }

      int port_;
      std::string name_;
      std::atomic<bool> running_{false};
      std::thread thr_;
      bool started_ = false;
      SOCKET sock_ = INVALID_SOCKET;
    };

    std::unique_ptr<DiscoveryResponder> make_responder(int servicePort, const std::string& name)
    {
      return std::unique_ptr<DiscoveryResponder>(new Responder(servicePort, name));
    }

    std::vector<ServerInfo> discover(int servicePort, int timeoutMs)
    {
      std::vector<ServerInfo> found;
      SOCKET s = make_udp_socket();
      if (s == INVALID_SOCKET)
      {
        return found;
      }

      // Set recv timeout
      DWORD timeout = static_cast<DWORD>(timeoutMs);
      ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

      const std::string probe = std::string(kProbePrefix) + " " + std::to_string(servicePort);
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      // Send probes to discovery port instead of service port
      addr.sin_port = htons(static_cast<uint16_t>(kDiscoveryPort));
      InetPtonA(AF_INET, "255.255.255.255", &addr.sin_addr);
      ::sendto(s, probe.c_str(), static_cast<int>(probe.size()), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

      // Collect replies (single socket, timeout governed by SO_RCVTIMEO)
      for (;;)
      {
        char buf[512];
        sockaddr_in from{};
        int fl = sizeof(from);
        int n = ::recvfrom(s, buf, sizeof(buf) - 1, 0, reinterpret_cast<sockaddr*>(&from), &fl);
        if (n <= 0)
        {
          break;
        }
        buf[n] = 0;
        if (std::strncmp(buf, kRespPrefix, std::strlen(kRespPrefix)) != 0)
        {
          continue;
        }
        ServerInfo si{};
        char ipbuf[64];
        const char* src = ::inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));
        si.ip = src ? src : std::string();
        si.port = servicePort;
        si.platform = "windows";
        // naive parse for name
        const char* namePos = std::strstr(buf, "name=");
        if (namePos)
        {
          namePos += 5;
          const char* end = std::strchr(namePos, ';');
          if (!end)
          {
            end = buf + std::strlen(buf);
          }
          si.name.assign(namePos, end);
        }
        found.push_back(std::move(si));
      }
      ::closesocket(s);
      return found;
    }
  } // namespace discovery
} // namespace net

#endif // _WIN32
