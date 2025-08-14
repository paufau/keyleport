#include "../discovery.h"

#include "utils/get_platform/platform.h"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
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
        // Also join a well-known multicast group to receive multicast probes
        asio::ip::address multicast_addr = asio::ip::make_address("239.255.255.250", ec);
        if (!ec)
        {
          asio::ip::multicast::join_group join(multicast_addr.to_v4());
          sock.set_option(join, ec);
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

      void onLost(LostHandler handler) override
      {
        std::lock_guard<std::mutex> lk(m_);
        lost_handler_ = std::move(handler);
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
        asio::ip::udp::endpoint mcast(asio::ip::make_address_v4("239.255.255.250"),
                                      static_cast<unsigned short>(kDiscoveryPort));

        // Track last-seen timestamps per endpoint key ip:port
        struct SeenEntry
        {
          std::chrono::steady_clock::time_point last_seen;
          entities::ConnectionCandidate candidate{false, "", "", ""};
        };
        std::unordered_map<std::string, SeenEntry> seen;

        // Collect local IPs to suppress self-discovery
        std::unordered_set<std::string> local_ips;
        try
        {
          asio::ip::tcp::resolver res(io);
          auto hostname = asio::ip::host_name(ec);
          if (!ec)
          {
            asio::ip::tcp::resolver::results_type results = res.resolve(hostname, "0", ec);
            if (!ec)
            {
              for (const auto& e : results)
              {
                auto a = e.endpoint().address();
                if (a.is_v4())
                {
                  local_ips.insert(a.to_string());
                }
              }
            }
          }
          // Also try a trick to get primary outbound IP
          asio::ip::udp::socket tmp(io);
          tmp.open(asio::ip::udp::v4());
          tmp.connect(asio::ip::udp::endpoint(asio::ip::make_address("8.8.8.8"), 53), ec);
          if (!ec)
          {
            auto a = tmp.local_endpoint().address();
            if (a.is_v4())
            {
              local_ips.insert(a.to_string());
            }
          }
        }
        catch (...)
        {
        }

        while (is_running())
        {
          // periodically re-broadcast
          sock.send_to(asio::buffer(probe.data(), probe.size()), bcast, 0, ec);
          // also send via multicast for networks that filter broadcast
          sock.send_to(asio::buffer(probe.data(), probe.size()), mcast, 0, ec);

          const auto start = std::chrono::steady_clock::now();
          while (is_running() && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500))
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
            // Skip self
            if (local_ips.find(ip) != local_ips.end())
            {
              continue;
            }
            const std::string key = ip + ":" + std::to_string(parsed_port);

            // Update last seen and notify discovery if it's a new endpoint
            bool is_new = (seen.find(key) == seen.end());
            auto now = std::chrono::steady_clock::now();
            entities::ConnectionCandidate cc(/*is_busy=*/false, name, ip, std::to_string(parsed_port));
            auto& entry = seen[key];
            if (is_new)
            {
              entry.candidate = cc;
            }
            entry.last_seen = now;

            if (is_new)
            {
              DiscoveredHandler cb;
              {
                std::lock_guard<std::mutex> lk(m_);
                cb = handler_;
              }
              if (cb)
              {
                cb(cc);
              }
            }
          }

          // After the receive window, evict and notify lost endpoints
          const auto now = std::chrono::steady_clock::now();
          const auto lost_timeout = std::chrono::seconds(2); // not seen for >2s considered lost

          LostHandler lostCb;
          {
            std::lock_guard<std::mutex> lk(m_);
            lostCb = lost_handler_;
          }

          if (lostCb)
          {
            std::vector<entities::ConnectionCandidate> to_remove;
            for (const auto& kv : seen)
            {
              if (now - kv.second.last_seen > lost_timeout)
              {
                to_remove.push_back(kv.second.candidate);
              }
            }
            for (const auto& cand : to_remove)
            {
              const std::string k = cand.ip() + ":" + cand.port();
              seen.erase(k);
              lostCb(cand);
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
      LostHandler lost_handler_;
      std::thread thr_;
      std::unique_ptr<DiscoveryResponder> responder_;
    };

    std::unique_ptr<Discovery> make_discovery()
    {
      return std::unique_ptr<Discovery>(new DiscoveryImpl());
    }
  } // namespace discovery
} // namespace net
