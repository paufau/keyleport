#include "../discovery.h"

#include "networking/server/server.h"
#include "utils/get_platform/platform.h"

#include <asio.hpp>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
      static const int kDiscoveryPort = 35353;               // dedicated UDP port for discovery
      static const char* kMulticastAddr = "239.255.255.250"; // well-known multicast for discovery

      inline void set_non_blocking(asio::ip::udp::socket& sock)
      {
        asio::error_code ec;
        sock.non_blocking(true, ec);
        (void)ec; // best-effort; safe to ignore here
      }

      struct ResponseInfo
      {
        int port = 0;
        std::string name;
        std::string platform;
      };

      inline ResponseInfo parse_response(const char* buf, size_t n)
      {
        ResponseInfo info{};
        if (!buf || n == 0)
        {
          return info;
        }

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

        const std::string p = parse_field("port=");
        if (!p.empty())
        {
          info.port = std::atoi(p.c_str());
        }
        info.name = parse_field("name=");
        info.platform = parse_field("platform=");
        return info;
      }

      inline std::unordered_set<std::string> collect_local_ipv4s(asio::io_context& io)
      {
        std::unordered_set<std::string> local_ips;
        try
        {
          asio::error_code ec;
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
        return local_ips;
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
        // Make socket non-blocking so we can check for shutdown flag regularly
        set_non_blocking(sock);
        // Also join a well-known multicast group to receive multicast probes
        asio::ip::address multicast_addr = asio::ip::make_address(kMulticastAddr, ec);
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
          size_t n = 0;
          ec = {};
          n = sock.receive_from(asio::buffer(buf, sizeof(buf)), from, 0, ec);
          if (ec)
          {
            if (ec == asio::error::would_block || ec == asio::error::try_again)
            {
              // No data ready; sleep briefly to avoid busy spin
              std::this_thread::sleep_for(std::chrono::milliseconds(10));
              continue;
            }
            // Other errors: just skip this iteration
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

      void onMessage(MessageHandler handler) override
      {
        std::lock_guard<std::mutex> lk(m_);
        msg_handler_ = std::move(handler);
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
        // Create networking server and start local receiver for inbound messages
        if (!server_)
        {
          server_ = net::make_server();
        }
        if (!receiver_)
        {
          try
          {
            receiver_ = server_->createReceiver(service_port_);
          }
          catch (const std::system_error& e)
          {
            std::cerr << "[discovery] Failed to bind local receiver on port " << service_port_ << ": " << e.what()
                      << ". Continuing without onMessage listener." << std::endl;
            receiver_.reset();
          }
          if (receiver_)
          {
            receiver_->onReceive(
                [this](const std::string& msg, const std::string& remote)
                {
                  MessageHandler cb;
                  {
                    std::lock_guard<std::mutex> lk(m_);
                    cb = msg_handler_;
                  }
                  if (cb)
                  {
                    // Populate candidate with remote ip if available
                    std::string ip = remote; // may be empty
                    entities::ConnectionCandidate cc(false, "", ip, std::to_string(service_port_));
                    cb(cc, msg);
                  }
                });
            // Launch receiver in a detached thread – it is designed to live for the process lifetime.
            recv_thr_ = std::thread([this]() { receiver_->run(); });
            recv_thr_.detach();
          }
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
        // Stop receiver (closes sockets and unblocks loops) to free the service port.
        if (receiver_)
        {
          receiver_->stop();
        }
        // Senders are cleaned up below.
        // Disconnect and clear all senders
        std::unordered_map<std::string, std::unique_ptr<net::Sender>> to_close;
        {
          std::lock_guard<std::mutex> lk(m_);
          to_close.swap(senders_);
        }
        for (auto& kv : to_close)
        {
          if (kv.second)
          {
            kv.second->disconnect();
          }
        }
        // Release receiver after stopping it so the port can be rebound later
        receiver_.reset();
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
        set_non_blocking(sock);

        const std::string probe = std::string(kProbePrefix) + " " + std::to_string(service_port_);
        asio::ip::udp::endpoint bcast(asio::ip::address_v4::broadcast(), static_cast<unsigned short>(kDiscoveryPort));
        asio::ip::udp::endpoint mcast(asio::ip::make_address_v4(kMulticastAddr),
                                      static_cast<unsigned short>(kDiscoveryPort));

        // Track last-seen timestamps per endpoint key ip:port
        struct SeenEntry
        {
          std::chrono::steady_clock::time_point last_seen;
          entities::ConnectionCandidate candidate{false, "", "", ""};
        };
        std::unordered_map<std::string, SeenEntry> seen;

        // Collect local IPs to suppress self-discovery
        auto local_ips = collect_local_ipv4s(io);

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
            const size_t len = (n < sizeof(buf)) ? n : sizeof(buf) - 1;
            buf[len] = '\0';
            ResponseInfo ri = parse_response(buf, len);
            int parsed_port = ri.port;
            std::string name = ri.name;
            std::string platform = ri.platform;

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
              // Create and connect a Sender for this server for future sendMessage calls
              ensure_sender_for(cc);
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
              // Remove associated sender if any
              remove_sender_for_key(k);
            }
          }
        }
      }

      bool is_running() const
      {
        std::lock_guard<std::mutex> lk(m_);
        return running_;
      }

      static std::string key_for(const entities::ConnectionCandidate& c) { return c.ip() + ":" + c.port(); }

      // Ensure a connected Sender exists for the given candidate
      void ensure_sender_for(const entities::ConnectionCandidate& c)
      {
        const std::string key = key_for(c);
        std::lock_guard<std::mutex> lk(m_);
        if (!server_)
        {
          server_ = net::make_server();
        }
        if (senders_.find(key) != senders_.end())
        {
          return;
        }
        auto s = server_->createSender(c.ip(), std::atoi(c.port().c_str()));
        if (s)
        {
          s->connect();
          senders_[key] = std::move(s);
        }
      }

      void remove_sender_for_key(const std::string& key)
      {
        std::lock_guard<std::mutex> lk(m_);
        auto it = senders_.find(key);
        if (it != senders_.end())
        {
          if (it->second)
          {
            it->second->disconnect();
          }
          senders_.erase(it);
        }
      }

      int sendMessage(const entities::ConnectionCandidate& target, const std::string& message) override
      {
        const std::string key = key_for(target);
        net::Sender* s = nullptr;
        {
          std::lock_guard<std::mutex> lk(m_);
          auto it = senders_.find(key);
          if (it != senders_.end())
          {
            s = it->second.get();
          }
        }
        if (!s)
        {
          // Create sender without holding the lock to avoid deadlock
          ensure_sender_for(target);
          std::lock_guard<std::mutex> lk(m_);
          auto it = senders_.find(key);
          if (it == senders_.end())
          {
            return 2;
          }
          s = it->second.get();
        }
        if (!s)
        {
          return 2;
        }
        return s->send_tcp(message);
      }

      mutable std::mutex m_;
      bool running_ = false;
      int service_port_ = 0;
      DiscoveredHandler handler_;
      LostHandler lost_handler_;
      MessageHandler msg_handler_;
      std::thread thr_;
      // Networking components for receiving and sending
      std::unique_ptr<net::Server> server_;
      std::unique_ptr<net::Receiver> receiver_;
      std::thread recv_thr_;
      std::unordered_map<std::string, std::unique_ptr<net::Sender>> senders_;
    };

    // Internal accessor to the singleton pointer
    static Discovery*& discovery_singleton_ptr()
    {
      static Discovery* inst = nullptr;
      return inst;
    }

    // Singleton accessor implementation
    Discovery& Discovery::instance()
    {
      Discovery*& inst = discovery_singleton_ptr();
      if (!inst)
      {
        inst = make_discovery().release();
      }
      return *inst;
    }

    void Discovery::destroy_instance()
    {
      Discovery*& inst = discovery_singleton_ptr();
      if (!inst)
      {
        return;
      }
      // Best-effort cleanup: stop discovery and disconnect senders.
      // Do NOT delete the instance because receiver threads are detached and have
      // no cooperative shutdown; deleting would risk use-after-free.
      inst->stop_discovery();
      inst = nullptr;
    }

    std::unique_ptr<Discovery> make_discovery()
    {
      return std::unique_ptr<Discovery>(new DiscoveryImpl());
    }
  } // namespace discovery
} // namespace net
