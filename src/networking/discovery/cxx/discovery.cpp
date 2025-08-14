#include "../discovery.h"

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
        // Make socket non-blocking so we can check for shutdown flag regularly
        sock.non_blocking(true, ec);
        if (ec)
        {
          // If non-blocking fails, continue anyway but the thread may hang on shutdown
          // (best-effort; most platforms support this)
          ec = {};
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

        // Close all TCP sessions
        close_all_sessions();
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

              // Start TCP session to receive messages from this server
              start_session_for(cc);
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
              close_session_for_key(k);
            }
          }
        }
      }

      bool is_running() const
      {
        std::lock_guard<std::mutex> lk(m_);
        return running_;
      }

      // Session management
      struct Session
      {
        std::shared_ptr<asio::io_context> io;
        std::shared_ptr<asio::ip::tcp::socket> sock;
        std::thread thr;
        entities::ConnectionCandidate candidate{false, "", "", ""};
      };

      static std::string key_for(const entities::ConnectionCandidate& c) { return c.ip() + ":" + c.port(); }

      void start_session_for(const entities::ConnectionCandidate& c)
      {
        const std::string key = key_for(c);
        std::lock_guard<std::mutex> lk(m_);
        if (sessions_.count(key))
        {
          return; // already started
        }
        auto io = std::make_shared<asio::io_context>();
        auto sock = std::make_shared<asio::ip::tcp::socket>(*io);
        // Initialize session entry without copying std::thread
        Session& entry = sessions_[key];
        entry.io = io;
        entry.sock = sock;
        entry.candidate = c;
        entry.thr = std::thread([this, key]() { this->session_thread(key); });
      }

      void session_thread(std::string key)
      {
        std::shared_ptr<asio::io_context> io;
        std::shared_ptr<asio::ip::tcp::socket> sock;
        entities::ConnectionCandidate cand{false, "", "", ""};
        {
          std::lock_guard<std::mutex> lk(m_);
          auto it = sessions_.find(key);
          if (it == sessions_.end())
          {
            return;
          }
          io = it->second.io;
          sock = it->second.sock;
          cand = it->second.candidate;
        }

        asio::error_code ec;
        // Connect
        asio::ip::tcp::resolver res(*io);
        auto eps = res.resolve(cand.ip(), cand.port(), ec);
        if (ec)
        {
          return;
        }
        asio::connect(*sock, eps, ec);
        if (ec)
        {
          return;
        }
        asio::ip::tcp::no_delay nd(true);
        sock->set_option(nd, ec);

        std::vector<char> buffer;
        buffer.reserve(4096);
        for (;;)
        {
          char temp[4096];
          size_t n = sock->read_some(asio::buffer(temp, sizeof(temp)), ec);
          if (ec)
          {
            break;
          }
          buffer.insert(buffer.end(), temp, temp + n);
          // parse frames: 4-byte big-endian length + payload
          for (;;)
          {
            if (buffer.size() < 4)
            {
              break;
            }
            uint32_t nlen_be = 0;
            std::memcpy(&nlen_be, buffer.data(), 4);
            uint32_t nlen = ntohl(nlen_be);
            if (buffer.size() < 4u + nlen)
            {
              break;
            }
            std::string msg(buffer.begin() + 4, buffer.begin() + 4 + nlen);
            buffer.erase(buffer.begin(), buffer.begin() + 4 + nlen);
            MessageHandler cb;
            {
              std::lock_guard<std::mutex> lk2(m_);
              cb = msg_handler_;
            }
            if (cb)
            {
              cb(cand, msg);
            }
          }
        }
        // On error/exit: close and remove session
        close_session_for_key(key);
      }

      void close_session_for_key(const std::string& key)
      {
        Session toClose;
        {
          std::lock_guard<std::mutex> lk(m_);
          auto it = sessions_.find(key);
          if (it == sessions_.end())
          {
            return;
          }
          toClose = std::move(it->second);
          sessions_.erase(it);
        }
        asio::error_code ec;
        if (toClose.sock)
        {
          toClose.sock->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
          toClose.sock->close(ec);
        }
        if (toClose.thr.joinable())
        {
          if (std::this_thread::get_id() == toClose.thr.get_id())
          {
            // Can't join self; detach to avoid std::terminate on destruction
            toClose.thr.detach();
          }
          else
          {
            toClose.thr.join();
          }
        }
      }

      void close_all_sessions()
      {
        std::vector<std::string> keys;
        {
          std::lock_guard<std::mutex> lk(m_);
          for (const auto& kv : sessions_)
          {
            keys.push_back(kv.first);
          }
        }
        for (const auto& k : keys)
        {
          close_session_for_key(k);
        }
      }

      int sendMessage(const entities::ConnectionCandidate& target, const std::string& message) override
      {
        const std::string key = key_for(target);
        std::shared_ptr<asio::ip::tcp::socket> sock;
        {
          std::lock_guard<std::mutex> lk(m_);
          auto it = sessions_.find(key);
          if (it == sessions_.end())
          {
            // No session yet: try a one-off synchronous connect-and-send
            asio::io_context io;
            asio::error_code ec;
            asio::ip::tcp::resolver res(io);
            auto eps = res.resolve(target.ip(), target.port(), ec);
            if (ec)
            {
              return 2;
            }
            asio::ip::tcp::socket tmp(io);
            asio::connect(tmp, eps, ec);
            if (ec)
            {
              return 2;
            }
            asio::ip::tcp::no_delay nd(true);
            tmp.set_option(nd, ec);
            uint32_t nlen = htonl(static_cast<uint32_t>(message.size()));
            char hdr[4];
            std::memcpy(hdr, &nlen, 4);
            asio::write(tmp, asio::buffer(hdr, 4), ec);
            if (ec)
            {
              return 2;
            }
            asio::write(tmp, asio::buffer(message.data(), message.size()), ec);
            if (ec)
            {
              return 2;
            }
            // Fire-and-forget: close connection; also spin up a session for future messages
            start_session_for(target);
            return 0;
          }
          sock = it->second.sock;
        }
        if (!sock || !sock->is_open())
        {
          // Try the same one-off fallback when socket isn't open
          asio::io_context io;
          asio::error_code ec;
          asio::ip::tcp::resolver res(io);
          auto eps = res.resolve(target.ip(), target.port(), ec);
          if (ec)
          {
            return 2;
          }
          asio::ip::tcp::socket tmp(io);
          asio::connect(tmp, eps, ec);
          if (ec)
          {
            return 2;
          }
          asio::ip::tcp::no_delay nd(true);
          tmp.set_option(nd, ec);
          uint32_t nlen = htonl(static_cast<uint32_t>(message.size()));
          char hdr[4];
          std::memcpy(hdr, &nlen, 4);
          asio::write(tmp, asio::buffer(hdr, 4), ec);
          if (ec)
          {
            return 2;
          }
          asio::write(tmp, asio::buffer(message.data(), message.size()), ec);
          if (ec)
          {
            return 2;
          }
          start_session_for(target);
          return 0;
        }
        // frame and send
        uint32_t nlen = htonl(static_cast<uint32_t>(message.size()));
        char hdr[4];
        std::memcpy(hdr, &nlen, 4);
        asio::error_code ec;
        asio::write(*sock, asio::buffer(hdr, 4), ec);
        if (ec)
        {
          return 2;
        }
        asio::write(*sock, asio::buffer(message.data(), message.size()), ec);
        if (ec)
        {
          return 2;
        }
        return 0;
      }

      mutable std::mutex m_;
      bool running_ = false;
      int service_port_ = 0;
      DiscoveredHandler handler_;
      LostHandler lost_handler_;
      MessageHandler msg_handler_;
      std::thread thr_;
      std::unique_ptr<DiscoveryResponder> responder_;
      std::unordered_map<std::string, Session> sessions_;
    };

    // Singleton accessor implementation
    Discovery& Discovery::instance()
    {
      static std::unique_ptr<Discovery> inst = make_discovery();
      return *inst.get();
    }

    std::unique_ptr<Discovery> make_discovery()
    {
      return std::unique_ptr<Discovery>(new DiscoveryImpl());
    }
  } // namespace discovery
} // namespace net
