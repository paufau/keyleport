#include "../discovery.h"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
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
        asio::io_context io;
        asio::ip::udp::socket sock(io);
        asio::error_code ec;
        sock.open(asio::ip::udp::v4(), ec);
        if (ec)
        {
          return;
        }
        sock.set_option(asio::socket_base::reuse_address(true), ec);
        sock.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), static_cast<unsigned short>(kDiscoveryPort)), ec);
        if (ec)
        {
          return;
        }
        for (;;)
        {
          if (!running_.load())
          {
            break;
          }
          char buf[1024];
          asio::ip::udp::endpoint from;
          size_t n = sock.receive_from(asio::buffer(buf, sizeof(buf)), from, 0, ec);
          if (ec)
          {
            continue;
          }
          if (n == 0)
          {
            continue;
          }
          if (std::strncmp(buf, kProbePrefix, std::strlen(kProbePrefix)) != 0)
          {
            continue;
          }
          char out[256];
          std::snprintf(out, sizeof(out), "%s ip=%s;port=%d;name=%s;platform=cxx", kRespPrefix,
                        from.address().to_string().c_str(), port_, name_.c_str());
          sock.send_to(asio::buffer(out, std::strlen(out)), from, 0, ec);
        }
      }

      int port_;
      std::string name_;
      std::atomic<bool> running_{false};
      std::thread thr_;
      bool started_ = false;
    };

    std::unique_ptr<DiscoveryResponder> make_responder(int servicePort, const std::string& name)
    {
      return std::unique_ptr<DiscoveryResponder>(new Responder(servicePort, name));
    }

    std::vector<ServerInfo> discover(int servicePort, int timeoutMs)
    {
      std::vector<ServerInfo> found;
      asio::io_context io;
      asio::ip::udp::socket sock(io);
      asio::error_code ec;
      sock.open(asio::ip::udp::v4(), ec);
      if (ec)
      {
        return found;
      }
      sock.set_option(asio::socket_base::broadcast(true), ec);
      sock.set_option(asio::socket_base::reuse_address(true), ec);
      // Non-blocking receive loop with time budget
      sock.non_blocking(true, ec);

      const std::string probe = std::string(kProbePrefix) + " " + std::to_string(servicePort);
      asio::ip::udp::endpoint bcast(asio::ip::address_v4::broadcast(), static_cast<unsigned short>(kDiscoveryPort));
      sock.send_to(asio::buffer(probe.data(), probe.size()), bcast, 0, ec);

      using clock = std::chrono::steady_clock;
      auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
      while (clock::now() < deadline)
      {
        char buf[512];
        asio::ip::udp::endpoint from;
        size_t n = 0;
        ec = {};
        n = sock.receive_from(asio::buffer(buf, sizeof(buf)), from, 0, ec);
        if (ec)
        {
          if (ec == asio::error::would_block || ec == asio::error::try_again)
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
          }
          break;
        }
        if (n == 0)
        {
          continue;
        }
        if (std::strncmp(buf, kRespPrefix, std::strlen(kRespPrefix)) != 0)
        {
          continue;
        }
        ServerInfo si{};
        si.ip = from.address().to_string();
        si.port = servicePort;
        si.platform = "cxx";
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
      return found;
    }
  } // namespace discovery
} // namespace net
