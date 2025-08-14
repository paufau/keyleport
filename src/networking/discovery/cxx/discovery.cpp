#include "../discovery.h"

#include "utils/get_platform/platform.h"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
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
          // Respond with our own advertised info (do not echo sender IP)
          char out[256];
          std::snprintf(out, sizeof(out), "%s port=%d;name=%s;platform=%s", kRespPrefix, port_, name_.c_str(),
                        get_platform().c_str());
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

    // Async discovery client implementation
    class DiscoveryImpl : public Discovery
    {
    public:
      DiscoveryImpl() = default;
      ~DiscoveryImpl() override { stop_discovery(); }

      void onDiscovered(DiscoveredHandler handler) override
      {
        std::lock_guard<std::mutex> lk(m_);
        handler_ = std::move(handler);
      }

      void start_discovery(int servicePort) override
      {
        std::lock_guard<std::mutex> lk(m_);
        if (running_)
        {
          return;
        }
        running_ = true;
        service_port_ = servicePort;
        // Also act as a responder so that multiple nodes running discovery can find each other
        responder_ = make_responder(service_port_, "keyleport");
        if (responder_)
        {
          responder_->start_async();
        }
        thr_ = std::thread([this] { this->loop(); });
      }

      void stop_discovery() override
      {
        {
          std::lock_guard<std::mutex> lk(m_);
          running_ = false;
        }
        if (thr_.joinable())
        {
          thr_.join();
        }
        // Stop responder by destroying it
        responder_.reset();
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
        sock.set_option(asio::socket_base::broadcast(true), ec);
        sock.set_option(asio::socket_base::reuse_address(true), ec);
        sock.non_blocking(true, ec);

        const std::string probe = std::string(kProbePrefix) + " " + std::to_string(service_port_);
        asio::ip::udp::endpoint bcast(asio::ip::address_v4::broadcast(), static_cast<unsigned short>(kDiscoveryPort));

        // to de-duplicate endpoints (ip:port)
        std::unordered_set<std::string> seen;

        while (is_running())
        {
          // periodically re-broadcast
          sock.send_to(asio::buffer(probe.data(), probe.size()), bcast, 0, ec);

          const auto start = std::chrono::steady_clock::now();
          while (is_running() && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(300))
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

            // Parse response fields: port, name, platform
            int parsed_port = 0;
            std::string name = "";
            std::string platform = "";

            auto parse_field = [&](const char* key) -> std::string
            {
              const char* pos = std::strstr(buf, key);
              if (!pos)
              {
                return {};
              }
              pos += std::strlen(key);
              const char* end = std::strchr(pos, ';');
              if (!end)
              {
                end = buf + std::strlen(buf);
              }
              return std::string(pos, end);
            };

            {
              const std::string p = parse_field("port=");
              if (!p.empty())
              {
                parsed_port = std::atoi(p.c_str());
              }
              name = parse_field("name=");
              platform = parse_field("platform=");
            }

            if (parsed_port <= 0)
            {
              // fallback to our service port if not provided
              parsed_port = service_port_;
            }

            const std::string ip = from.address().to_string();
            const std::string key = ip + ":" + std::to_string(parsed_port);
            if (!seen.insert(key).second)
            {
              continue; // already reported
            }

            DiscoveredHandler cb;
            {
              std::lock_guard<std::mutex> lk(m_);
              cb = handler_;
            }
            if (cb)
            {
              entities::ConnectionCandidate cc(/*is_busy=*/false, name, ip, std::to_string(parsed_port));
              cb(cc);
            }
          }
        }
      }

      bool is_running() const
      {
        std::lock_guard<std::mutex> lk(m_);
        return running_;
      }

      mutable std::mutex m_;
      bool running_ = false;
      int service_port_ = 0;
      DiscoveredHandler handler_;
      std::thread thr_;
      std::unique_ptr<DiscoveryResponder> responder_;
    };

    std::unique_ptr<Discovery> make_discovery()
    {
      return std::unique_ptr<Discovery>(new DiscoveryImpl());
    }
  } // namespace discovery
} // namespace net
