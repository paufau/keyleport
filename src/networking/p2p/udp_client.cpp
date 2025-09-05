#include "networking/p2p/udp_client.h"

#include "networking/p2p/message.h"
#include "networking/p2p/peer.h"

#include <enet/enet.h>

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
  }

  udp_client::~udp_client()
  {
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

  void udp_client::send_impl(message msg, bool is_reliable)
  {
    const std::string payload = msg.get_payload();
    if (payload.empty())
    {
      return;
    }

    // Ensure host exists
    if (!host_)
    {
      host_ = enet_host_create(/*address*/ nullptr, /*peers*/ 1, /*channels*/ 2,
                               0, 0);
      if (!host_)
      {
        return;
      }
    }

    // Lazy connect on first send only
    if (!peer_)
    {
      const std::string ip = config_.get_peer().get_ip_address();
      const int port = config_.get_port();
      if (ip.empty() || port <= 0)
      {
        return;
      }
      peer_ = connect_peer(host_, ip, port);
    }

    // Do not attempt reconnects if we already have a peer but it's not
    // connected
    if (!peer_ || peer_->state != ENET_PEER_STATE_CONNECTED)
    {
      return;
    }

    const enet_uint32 flags = is_reliable
                                  ? ENET_PACKET_FLAG_RELIABLE
                                  : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
    ENetPacket* packet =
        enet_packet_create(payload.data(), payload.size(), flags);
    if (!packet)
    {
      return;
    }

    if (enet_peer_send(peer_, 0, packet) < 0)
    {
      enet_packet_destroy(packet);
      // keep state as-is; no reconnects here
      return;
    }
    enet_host_flush(host_);
  }

} // namespace p2p
