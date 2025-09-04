#include "networking/p2p/udp_server.h"

#include <enet/enet.h>
#include <iostream>

namespace p2p
{
  void udp_server::destroy_packet(struct _ENetPacket* packet)
  {
    if (packet)
    {
      enet_packet_destroy(packet);
    }
  }

  std::string udp_server::extract_ip(struct _ENetPeer* peer)
  {
    if (!peer)
    {
      return {};
    }

    char ipbuf[64] = {0};
    ENetAddress addr = peer->address;

    if (enet_address_get_host_ip(&addr, ipbuf, sizeof(ipbuf)) == 0)
    {
      return std::string(ipbuf);
    }

    return {};
  }

  udp_server::udp_server(udp_server_configuration config)
      : config_(std::move(config))
  {
    if (enet_initialize() != 0)
    {
      return;
    }
    enet_inited_ = true;

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(config_.get_port());
    host_ = enet_host_create(&address,
                             /* peer limit */ 32,
                             /* channel limit */ 2,
                             /* incoming bandwidth */ 0,
                             /* outgoing bandwidth */ 0);
  }

  udp_server::~udp_server()
  {
    if (host_)
    {
      enet_host_destroy(host_);
      host_ = nullptr;
    }
    if (enet_inited_)
    {
      enet_deinitialize();
      enet_inited_ = false;
    }
  }

  void udp_server::poll_events()
  {
    if (!host_)
    {
      return;
    }

    ENetEvent event;
    // Service with zero timeout to pump pending events without blocking long
    while (enet_host_service(host_, &event, 0) > 0)
    {
      if (event.type == ENET_EVENT_TYPE_RECEIVE)
      {
        std::string from_ip = extract_ip(event.peer);
        if (from_ip.empty())
        {
          std::cerr << "Failed to get from IP address" << std::endl;
          destroy_packet(event.packet);
          continue;
        }

        message msg;
        p2p::peer from_peer{from_ip};
        p2p::peer to_peer = p2p::peer::self();
        msg.set_from(from_peer);
        msg.set_to(to_peer);

        if (event.packet && event.packet->data && event.packet->dataLength > 0)
        {
          const char* data = reinterpret_cast<const char*>(event.packet->data);
          msg.set_payload(std::string(data, event.packet->dataLength));
        }

        on_message.emit(msg);
        destroy_packet(event.packet);
      }
    }
  }
} // namespace p2p
