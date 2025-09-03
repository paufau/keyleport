#include "networking/udp/udp_service.h"

#include <cstring>
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace net
{
  namespace udp
  {

    static std::string make_key(const std::string& addr, int port)
    {
      return addr + ":" + std::to_string(port);
    }

    udp_service::udp_service() = default;

    udp_service::~udp_service()
    {
      stop();
    }

    bool udp_service::start(int listen_port, int broadcast_port)
    {
      if (running_.exchange(true))
      {
        return true;
      }

      listen_port_ = listen_port;
      if (broadcast_port > 0)
      {
        broadcast_port_ = broadcast_port;
      }
      else if (listen_port_ > 0)
      {
        broadcast_port_ = listen_port_ + 1;
      }
      else
      {
        broadcast_port_ = 33334; // default discovery port
      }

      if (enet_initialize() == 0)
      {
        enet_inited_ = true;
      }
      else
      {
        std::cerr << "ENet initialization failed" << std::endl;
        running_ = false;
        return false;
      }

#ifdef _WIN32
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
      {
        wsa_inited_ = true;
      }
      else
      {
        std::cerr << "WSAStartup failed" << std::endl;
      }
#endif

      ENetAddress address;
      address.host = ENET_HOST_ANY;
      address.port = static_cast<enet_uint16>(listen_port_);

      host_ = enet_host_create(&address, /* peers */ 32, /* channels */ 2, /* in bw */ 0, /* out bw */ 0);
      if (!host_)
      {
        std::cerr << "Failed to create ENet host" << std::endl;
        // We still allow discovery to run if broadcast sockets can bind.
      }
      else
      {
        // If we asked for an ephemeral port (0), query the assigned port
        ENetAddress bound = host_->address;
        listen_port_ = static_cast<int>(bound.port);
      }

      // create broadcast sockets (send + recv)
      bcast_send_sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
      if (bcast_send_sock_ != INVALID_SOCKET)
      {
#else
      if (bcast_send_sock_ >= 0)
      {
#endif
        int opt = 1;
        ::setsockopt(bcast_send_sock_, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));
#ifdef SO_REUSEADDR
        ::setsockopt(bcast_send_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#endif
#ifdef SO_REUSEPORT
        ::setsockopt(bcast_send_sock_, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt, sizeof(opt));
#endif
        // Bind send socket to INADDR_ANY to ensure consistent broadcast behavior on some OSes
        sockaddr_in saddr{};
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(0);
        saddr.sin_addr.s_addr = htonl(INADDR_ANY);
        ::bind(bcast_send_sock_, (sockaddr*)&saddr, sizeof(saddr));
      }

      bcast_recv_sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
      if (bcast_recv_sock_ != INVALID_SOCKET)
      {
#else
      if (bcast_recv_sock_ >= 0)
      {
#endif
        int opt = 1;
        ::setsockopt(bcast_recv_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
        ::setsockopt(bcast_recv_sock_, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt, sizeof(opt));
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(broadcast_port_));
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(bcast_recv_sock_, (sockaddr*)&addr, sizeof(addr)) != 0)
        {
          std::cerr << "Broadcast recv bind failed" << std::endl;
        }
      }

      if (host_)
      {
        service_thread_ = std::thread([this] { run_service_loop_(); });
      }
      broadcast_thread_ = std::thread([this] { run_broadcast_recv_loop_(); });
      // Prepare list of broadcast targets (limited + directed)
      compute_broadcast_targets_();
      return true;
    }

    void udp_service::stop()
    {
      if (!running_.exchange(false))
      {
        return;
      }
      if (service_thread_.joinable())
      {
        service_thread_.join();
      }
      if (broadcast_thread_.joinable())
      {
        broadcast_thread_.join();
      }

// close sockets
#ifdef _WIN32
      if (bcast_send_sock_ != INVALID_SOCKET)
      {
        ::closesocket(bcast_send_sock_);
        bcast_send_sock_ = INVALID_SOCKET;
      }
      if (bcast_recv_sock_ != INVALID_SOCKET)
      {
        ::closesocket(bcast_recv_sock_);
        bcast_recv_sock_ = INVALID_SOCKET;
      }
#else
      if (bcast_send_sock_ >= 0)
      {
        ::close(bcast_send_sock_);
        bcast_send_sock_ = -1;
      }
      if (bcast_recv_sock_ >= 0)
      {
        ::close(bcast_recv_sock_);
        bcast_recv_sock_ = -1;
      }
#endif

      {
        std::lock_guard<std::mutex> lock(host_mutex_);
        if (host_)
        {
          enet_host_destroy(host_);
          host_ = nullptr;
        }
      }
      if (enet_inited_)
      {
        enet_deinitialize();
        enet_inited_ = false;
      }

#ifdef _WIN32
      if (wsa_inited_)
      {
        WSACleanup();
        wsa_inited_ = false;
      }
#endif
    }

    void udp_service::run_service_loop_()
    {
      while (running_)
      {
        ENetEvent event;
        ENetHost* local_host = nullptr;
        {
          std::lock_guard<std::mutex> lock(host_mutex_);
          local_host = host_;
        }
        while (local_host && enet_host_service(local_host, &event, 10) > 0)
        {
          switch (event.type)
          {
          case ENET_EVENT_TYPE_CONNECT:
          {
            std::string addr = "unknown";
            if (event.peer->address.host != 0)
            {
              ENetAddress a = event.peer->address;
              char ip[64];
              enet_address_get_host_ip(&a, ip, sizeof(ip));
              addr = ip;
            }
            int port = event.peer->address.port;
            {
              std::lock_guard<std::mutex> lock(peers_mutex_);
              peers_[make_key(addr, port)] = event.peer;
              // ensure we have a wrapper for this peer
              if (peer_wrappers_.find(event.peer) == peer_wrappers_.end())
              {
                auto pc = std::make_shared<peer_connection>(this, event.peer);
                peer_wrappers_[event.peer] = pc;
                on_new_peer.emit(pc);
              }
            }
            on_connect.emit(network_event_connect{addr, port});
          }
          break;
          case ENET_EVENT_TYPE_RECEIVE:
          {
            std::string from = "unknown";
            if (event.peer)
            {
              ENetAddress a = event.peer->address;
              char ip[64];
              enet_address_get_host_ip(&a, ip, sizeof(ip));
              from = ip;
            }
            int port = event.peer ? event.peer->address.port : 0;
            std::string data(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
            if (event.peer)
            {
              std::shared_ptr<peer_connection> pc;
              {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                auto itp = peer_wrappers_.find(event.peer);
                if (itp != peer_wrappers_.end())
                {
                  pc = itp->second;
                }
                else
                {
                  // create wrapper lazily if missing
                  auto created = std::make_shared<peer_connection>(this, event.peer);
                  peer_wrappers_[event.peer] = created;
                  on_new_peer.emit(created);
                  pc = created;
                }
              }
              if (pc)
              {
                pc->on_receive_data.emit(network_event_data{data, from, port});
              }
            }
            enet_packet_destroy(event.packet);
          }
          break;
          case ENET_EVENT_TYPE_DISCONNECT:
          {
            std::string addr = "unknown";
            if (event.peer)
            {
              ENetAddress a = event.peer->address;
              char ip[64];
              enet_address_get_host_ip(&a, ip, sizeof(ip));
              addr = ip;
            }
            int port = event.peer ? event.peer->address.port : 0;
            {
              std::lock_guard<std::mutex> lock(peers_mutex_);
              peers_.erase(make_key(addr, port));
              peer_wrappers_.erase(event.peer);
            }
            on_disconnect.emit(network_event_disconnect{addr, port});
          }
          break;
          default:
            break;
          }
        }
      }
    }

    void udp_service::run_broadcast_recv_loop_()
    {
      std::vector<char> buf(2048);
      while (running_)
      {
        if (bcast_recv_sock_ < 0)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          continue;
        }
        sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        int flags = 0;
#ifndef _WIN32
        flags = MSG_DONTWAIT;
#endif
        int n =
            ::recvfrom(bcast_recv_sock_, buf.data(), static_cast<int>(buf.size()), flags, (sockaddr*)&from, &fromlen);
        if (n > 0)
        {
          std::string payload(buf.data(), buf.data() + n);
          if (payload.rfind("BCAST:", 0) == 0)
          {
            char ip[INET_ADDRSTRLEN]{};
            ::inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
            on_receive_broadcast.emit(network_event_broadcast{payload.substr(6), ip, ntohs(from.sin_port)});
          }
        }
        else
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }

    void udp_service::broadcast(const std::string& data)
    {
#ifdef _WIN32
      if (!running_ || bcast_send_sock_ == INVALID_SOCKET)
      {
        return;
      }
#else
      if (!running_ || bcast_send_sock_ < 0)
      {
        return;
      }
#endif
      std::string payload = std::string("BCAST:") + data;
      if (bcast_targets_.empty())
      {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(broadcast_port_));
        addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        ::sendto(bcast_send_sock_, payload.data(), static_cast<int>(payload.size()), 0, (sockaddr*)&addr, sizeof(addr));
        return;
      }
      for (auto target : bcast_targets_)
      {
        target.sin_port = htons(static_cast<uint16_t>(broadcast_port_));
        ::sendto(bcast_send_sock_, payload.data(), static_cast<int>(payload.size()), 0,
                 reinterpret_cast<const sockaddr*>(&target), sizeof(target));
      }
    }

    void udp_service::compute_broadcast_targets_()
    {
      bcast_targets_.clear();
      // Always include limited broadcast
      sockaddr_in lb{};
      lb.sin_family = AF_INET;
      lb.sin_addr.s_addr = htonl(INADDR_BROADCAST);
      bcast_targets_.push_back(lb);
#ifndef _WIN32
      // Add directed broadcasts per non-loopback IPv4 interface
      ifaddrs* ifaddr = nullptr;
      if (getifaddrs(&ifaddr) == 0 && ifaddr)
      {
        for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
          if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
          {
            continue;
          }
          if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_BROADCAST))
          {
            continue;
          }
          auto* a = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
          auto* m = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
          if (!a || !m)
          {
            continue;
          }
          uint32_t ip = ntohl(a->sin_addr.s_addr);
          uint32_t mask = ntohl(m->sin_addr.s_addr);
          uint32_t bcast = (ip & mask) | (~mask);
          sockaddr_in b{};
          b.sin_family = AF_INET;
          b.sin_addr.s_addr = htonl(bcast);
          bcast_targets_.push_back(b);
        }
        freeifaddrs(ifaddr);
      }
#endif
    }

    peer_connection_ptr udp_service::connect_to(const std::string& address, int port)
    {
      if (!host_)
      {
        return nullptr;
      }
      ENetAddress addr;
      enet_address_set_host(&addr, address.c_str());
      addr.port = static_cast<enet_uint16>(port);
      ENetPeer* peer = enet_host_connect(host_, &addr, 2, 0);
      if (!peer)
      {
        return nullptr;
      }
      {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[make_key(address, port)] = peer;
        if (peer_wrappers_.find(peer) == peer_wrappers_.end())
        {
          peer_wrappers_[peer] = std::make_shared<peer_connection>(this, peer);
        }
      }
      on_connect.emit(network_event_connect{address, port});
      return peer_wrappers_[peer];
    }

    void udp_service::flush()
    {
      std::lock_guard<std::mutex> lock(host_mutex_);
      if (host_)
      {
        enet_host_flush(host_);
      }
    }

    void udp_service::send_reliable_packet(_ENetPeer* peer, const std::string& data)
    {
      if (!peer)
      {
        return;
      }
      ENetPacket* pkt = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
      {
        std::lock_guard<std::mutex> lock(host_mutex_);
        enet_peer_send(reinterpret_cast<ENetPeer*>(peer), 0, pkt);
        if (host_)
        {
          enet_host_flush(host_);
        }
      }
    }

    void udp_service::send_unreliable_packet(_ENetPeer* peer, const std::string& data)
    {
      if (!peer)
      {
        return;
      }
      ENetPacket* pkt = enet_packet_create(data.data(), data.size(), 0);
      {
        std::lock_guard<std::mutex> lock(host_mutex_);
        enet_peer_send(reinterpret_cast<ENetPeer*>(peer), 1, pkt);
        if (host_)
        {
          enet_host_flush(host_);
        }
      }
    }

    void udp_service::disconnect_peer(_ENetPeer* peer)
    {
      if (!peer)
      {
        return;
      }
      std::lock_guard<std::mutex> lock(host_mutex_);
      enet_peer_disconnect(reinterpret_cast<ENetPeer*>(peer), 0);
    }

  } // namespace udp
} // namespace net
