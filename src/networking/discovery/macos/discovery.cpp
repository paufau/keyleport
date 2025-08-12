#ifdef __APPLE__

#include "networking/discovery/discovery.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <ifaddrs.h>
#include <memory>
#include <mutex>
#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace net
{
  namespace discovery
  {
    namespace
    {
      static const char* kProbePrefix = "KP_DISCOVER_V1?";
      static const char* kRespPrefix = "KP_DISCOVER_V1!";
      static const int kDiscoveryPort = 35353; // dedicated UDP port for discovery

      std::string get_broadcast_ip_for(const std::string& ip, const std::string& netmask)
      {
        in_addr ipaddr{}, maskaddr{};
        if (::inet_aton(ip.c_str(), &ipaddr) == 0 || ::inet_aton(netmask.c_str(), &maskaddr) == 0)
        {
          return {};
        }
        in_addr bcast{};
        bcast.s_addr = ipaddr.s_addr | ~maskaddr.s_addr;
        return std::string(::inet_ntoa(bcast));
      }

      std::vector<std::string> enumerate_broadcasts()
      {
        std::vector<std::string> res;
        ifaddrs* ifa = nullptr;
        if (::getifaddrs(&ifa) != 0 || !ifa)
        {
          return res;
        }
        for (ifaddrs* p = ifa; p; p = p->ifa_next)
        {
          if (!p->ifa_addr || !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK))
          {
            continue;
          }
          if (p->ifa_addr->sa_family != AF_INET)
          {
            continue;
          }
          auto* sa = reinterpret_cast<sockaddr_in*>(p->ifa_addr);
          auto* nm = reinterpret_cast<sockaddr_in*>(p->ifa_netmask);
          if (!sa || !nm)
          {
            continue;
          }
          std::string ip = ::inet_ntoa(sa->sin_addr);
          std::string mask = ::inet_ntoa(nm->sin_addr);
          auto b = get_broadcast_ip_for(ip, mask);
          if (!b.empty())
          {
            res.push_back(b);
          }
        }
        ::freeifaddrs(ifa);
        // Always add limited broadcast as a fallback
        res.push_back("255.255.255.255");
        return res;
      }

      int make_udp_socket()
      {
        int s = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
        {
          return -1;
        }
        int yes = 1;
        ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
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
        if (sock_ >= 0)
        {
          ::close(sock_);
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
        if (sock_ < 0)
        {
          return;
        }
        int yes = 1;
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
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
          int r = ::select(sock_ + 1, &rfds, nullptr, nullptr, &tv);
          if (r <= 0)
          {
            continue;
          }
          sockaddr_in from{};
          socklen_t fl = sizeof(from);
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
          std::snprintf(ipbuf, sizeof(ipbuf), "%s", ::inet_ntoa(from.sin_addr));
          char out[256];
          std::snprintf(out, sizeof(out), "%s ip=%s;port=%d;name=%s;platform=macos", kRespPrefix, ipbuf, port_,
                        name_.c_str());
          ::sendto(sock_, out, std::strlen(out), 0, reinterpret_cast<sockaddr*>(&from), fl);
        }
      }

      int port_;
      std::string name_;
      std::atomic<bool> running_{false};
      std::thread thr_;
      bool started_ = false;
      int sock_ = -1;
    };

    std::unique_ptr<DiscoveryResponder> make_responder(int servicePort, const std::string& name)
    {
      return std::unique_ptr<DiscoveryResponder>(new Responder(servicePort, name));
    }

    std::vector<ServerInfo> discover(int servicePort, int timeoutMs)
    {
      std::vector<ServerInfo> found;
      int s = make_udp_socket();
      if (s < 0)
      {
        return found;
      }
      // Set recv timeout
      timeval tv{};
      tv.tv_sec = timeoutMs / 1000;
      tv.tv_usec = (timeoutMs % 1000) * 1000;
      ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

      auto broadcasts = enumerate_broadcasts();
      const std::string probe = std::string(kProbePrefix) + " " + std::to_string(servicePort);

      for (const auto& b : broadcasts)
      {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        // Send probes to discovery port
        addr.sin_port = htons(static_cast<uint16_t>(kDiscoveryPort));
        ::inet_pton(AF_INET, b.c_str(), &addr.sin_addr);
        ::sendto(s, probe.c_str(), static_cast<int>(probe.size()), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      }

      // Collect replies
      for (;;)
      {
        char buf[512];
        sockaddr_in from{};
        socklen_t fl = sizeof(from);
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
        si.ip = ::inet_ntoa(from.sin_addr);
        si.port = servicePort;
        si.platform = "macos";
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
      ::close(s);
      return found;
    }
  } // namespace discovery
} // namespace net

#endif // __APPLE__
