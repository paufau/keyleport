#include "networking/p2p/udp_client.h"

#include "networking/p2p/message.h"
#include "networking/p2p/peer.h"

#include <chrono>
#include <enet/enet.h>
#include <iostream>

namespace p2p
{

  namespace
  {
    // Connect to the target peer; returns connected ENetPeer* or nullptr on
    // failure
    ENetPeer* connect_peer(ENetHost* host, const std::string& ip, int port,
                           int timeout_ms = 250)
    {
      if (!host || ip.empty() || port <= 0)
      {
        return nullptr;
      }

      ENetAddress address{};
      address.port = static_cast<enet_uint16>(port);
      if (enet_address_set_host(&address, ip.c_str()) != 0)
      {
        return nullptr;
      }

      ENetPeer* peer =
          enet_host_connect(host, &address, /*channels*/ 2, /*data*/ 0);
      if (!peer)
      {
        return nullptr;
      }

      ENetEvent ev;
      int elapsed = 0;
      while (elapsed < timeout_ms)
      {
        int rc = enet_host_service(host, &ev, 50);
        elapsed += 50;
        if (rc > 0 && ev.type == ENET_EVENT_TYPE_CONNECT)
        {
          return peer;
        }
        if (rc > 0 && ev.type == ENET_EVENT_TYPE_DISCONNECT)
        {
          break;
        }
      }

      enet_peer_reset(peer);
      return nullptr;
    }

    void disconnect_peer(ENetHost* host, ENetPeer* peer)
    {
      if (!host || !peer)
      {
        return;
      }
      ENetEvent ev;
      enet_peer_disconnect(peer, 0);
      for (int i = 0; i < 5; ++i)
      {
        if (enet_host_service(host, &ev, 20) > 0 &&
            ev.type == ENET_EVENT_TYPE_DISCONNECT)
        {
          return;
        }
      }
      enet_peer_reset(peer);
    }
  } // namespace

  udp_client::udp_client(udp_client_configuration config)
      : config_(std::move(config))
  {
    // Initialize ENet (idempotent)
    enet_initialize();
    // Create a client host once
    host_ = enet_host_create(/*address*/ nullptr, /*peers*/ 1, /*channels*/ 2,
                             0, 0);
    if (!host_)
    {
      std::cerr << "[udp_client] Failed to create client host" << std::endl;
    }
  }

  udp_client::~udp_client()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (peer_)
    {
      enet_peer_disconnect(peer_, 0);
      ENetEvent ev;
      for (int i = 0; i < 3; ++i)
      {
        if (enet_host_service(host_, &ev, 20) > 0 &&
            ev.type == ENET_EVENT_TYPE_DISCONNECT)
        {
          break;
        }
      }
      enet_peer_reset(peer_);
      peer_ = nullptr;
    }
    if (host_)
    {
      enet_host_destroy(host_);
      host_ = nullptr;
    }
    // Do not deinitialize ENet globally here.
  }

  void udp_client::send_reliable(message msg)
  {
    send_impl(std::move(msg), true);
  }

  void udp_client::send_unreliable(message msg)
  {
    send_impl(std::move(msg), false);
  }

  unsigned long long udp_client::now_ms() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  bool udp_client::ensure_connected()
  {
    // Already connected
    if (peer_ && peer_->state == ENET_PEER_STATE_CONNECTED)
    {
      return true;
    }

    // If we have a stale peer pointer, reset it
    if (peer_ && peer_->state != ENET_PEER_STATE_CONNECTED)
    {
      enet_peer_reset(peer_);
      peer_ = nullptr;
    }

    unsigned long long now = now_ms();
    if (last_connect_attempt_ms_ != 0 &&
        static_cast<long long>(now - last_connect_attempt_ms_) <
            reconnect_interval_ms_)
    {
      return false; // too soon to retry
    }
    last_connect_attempt_ms_ = now;

    const std::string ip = config_.get_peer().get_ip_address();
    const int port = config_.get_port();
    if (ip.empty() || port <= 0)
    {
      std::cerr << "[udp_client] Cannot connect: invalid peer info"
                << std::endl;
      return false;
    }
    std::cout << "[udp_client] Attempting connect to " << ip << ':' << port
              << std::endl;
    peer_ = connect_peer(host_, ip, port, connect_timeout_ms_);
    if (!peer_ || peer_->state != ENET_PEER_STATE_CONNECTED)
    {
      std::cerr << "[udp_client] Connect attempt failed" << std::endl;
      if (peer_ && peer_->state != ENET_PEER_STATE_CONNECTED)
      {
        enet_peer_reset(peer_);
        peer_ = nullptr;
      }
      return false;
    }
    std::cout << "[udp_client] Connected" << std::endl;
    return true;
  }

  void udp_client::send_impl(message msg, bool is_reliable)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string payload = msg.get_payload();
    if (payload.empty())
    {
      std::cerr << "[udp_client] Attempt to send empty payload" << std::endl;
      return;
    }

    // Ensure host exists
    if (!host_)
    {
      host_ = enet_host_create(/*address*/ nullptr, /*peers*/ 1, /*channels*/ 2,
                               0, 0);
      if (!host_)
      {
        std::cerr << "[udp_client] Could not recreate host" << std::endl;
        return;
      }
    }

    // Lazy connect on first send only
    // Poll for any disconnect events quickly (non-blocking)
    if (host_)
    {
      ENetEvent ev;
      while (enet_host_service(host_, &ev, 0) > 0)
      {
        if (ev.type == ENET_EVENT_TYPE_DISCONNECT)
        {
          std::cerr << "[udp_client] Detected disconnect" << std::endl;
          if (peer_)
          {
            enet_peer_reset(peer_);
            peer_ = nullptr;
          }
        }
      }
    }

    if (!ensure_connected())
    {
      std::cerr << "[udp_client] Cannot send: not connected (will retry)"
                << std::endl;
      return;
    }

    const enet_uint32 flags = is_reliable
                                  ? ENET_PACKET_FLAG_RELIABLE
                                  : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
    ENetPacket* packet =
        enet_packet_create(payload.data(), payload.size(), flags);
    if (!packet)
    {
      std::cerr << "[udp_client] Failed to create ENet packet" << std::endl;
      return;
    }

    if (enet_peer_send(peer_, 0, packet) < 0)
    {
      enet_packet_destroy(packet);
      // keep state as-is; no reconnects here
      std::cerr << "[udp_client] enet_peer_send failed" << std::endl;
      return;
    }
    enet_host_flush(host_);
    std::cout << "[udp_client] Sent " << payload.size()
              << (is_reliable ? " reliable" : " unreliable") << " bytes"
              << std::endl;
  }

  void udp_client::flush_pending_messages()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (host_)
    {
      enet_host_flush(host_);
    }
  }

} // namespace p2p
