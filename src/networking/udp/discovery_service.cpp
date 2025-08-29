#include "networking/udp/discovery_service.h"

#include "networking/udp/peer_info.h"
#include "utils/device_name/device_name.h"

#include <iostream>

namespace net
{
  namespace udp
  {

    discovery_service::discovery_service(udp_service& net) : net_(net)
    {
    }

    discovery_service::~discovery_service()
    {
      stop_discovery();
    }

    void discovery_service::start_discovery(int listen_port, std::chrono::seconds lose_after)
    {
      if (running_.exchange(true))
      {
        return;
      }
      listen_port_ = listen_port;
      lose_after_ = lose_after;

      // Subscribe to broadcasts and data to update last_seen
      net_.on_receive_broadcast.subscribe(
          [this](const network_event_broadcast& ev)
          {
            peer_info info;
            if (!peer_info_from_announce_json(ev.data, ev.from_address, ev.from_port, info))
            {
              std::cerr << "discovery_service parse error" << std::endl;
              return;
            }
            const auto id = info.device_id;
            bool is_new = false;
            {
              std::lock_guard<std::mutex> lock(mutex_);
              auto it = peers_.find(id);
              if (it == peers_.end())
              {
                peers_[id] = info;
                is_new = true;
              }
              else
              {
                it->second.last_seen = info.last_seen;
                it->second.ip_address = info.ip_address;
                it->second.port = info.port;
              }
            }
            if (is_new)
            {
              on_discovery.emit(info);
            }
          });

      // Periodic broadcaster thread
      broadcast_thread_ = std::thread([this] { loop_broadcast_(); });
      // Expiration thread
      expire_thread_ = std::thread([this] { loop_expire_(); });
    }

    void discovery_service::stop_discovery()
    {
      if (!running_.exchange(false))
      {
        return;
      }
      if (broadcast_thread_.joinable())
      {
        broadcast_thread_.join();
      }
      if (expire_thread_.joinable())
      {
        expire_thread_.join();
      }
    }

    std::vector<peer_info> discovery_service::get_known_peers()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      std::vector<peer_info> out;
      out.reserve(peers_.size());
      for (const auto& kv : peers_)
      {
        out.push_back(kv.second);
      }
      return out;
    }

    void discovery_service::loop_broadcast_()
    {
      // For the prototype, generate a pseudo device_id.
      std::string device_id = "device-" + std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFF);
      const std::string device_name = get_device_name();
      auto payload = peer_announce_to_json(device_id, device_name, listen_port_);

      while (running_)
      {
        net_.broadcast(payload);
        std::this_thread::sleep_for(std::chrono::milliseconds(750));
      }
    }

    void discovery_service::loop_expire_()
    {
      while (running_)
      {
        auto now = std::chrono::steady_clock::now();
        std::vector<peer_info> lost;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          for (auto it = peers_.begin(); it != peers_.end();)
          {
            if (now - it->second.last_seen > lose_after_)
            {
              lost.push_back(it->second);
              it = peers_.erase(it);
            }
            else
            {
              ++it;
            }
          }
        }
        for (auto& p : lost)
        {
          on_lose.emit(p);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    }

  } // namespace udp
} // namespace net
