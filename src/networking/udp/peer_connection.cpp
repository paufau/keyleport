#include "networking/udp/peer_connection.h"
#include "networking/udp/udp_service.h"

#include <enet/enet.h>

namespace net { namespace udp {

peer_connection::peer_connection(udp_service* svc, _ENetPeer* peer)
  : service_(svc), peer_(peer) {}

void peer_connection::send_reliable(const std::string& data)
{
  if (!peer_) return;
  ENetPacket* pkt = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(reinterpret_cast<ENetPeer*>(peer_), /* channel */ 0, pkt);
  if (service_) service_->flush();
}

void peer_connection::send_unreliable(const std::string& data)
{
  if (!peer_) return;
  ENetPacket* pkt = enet_packet_create(data.data(), data.size(), 0);
  enet_peer_send(reinterpret_cast<ENetPeer*>(peer_), /* channel */ 1, pkt);
  if (service_) service_->flush();
}

void peer_connection::disconnect()
{
  if (!peer_) return;
  enet_peer_disconnect(reinterpret_cast<ENetPeer*>(peer_), 0);
}

std::string peer_connection::address() const
{
  if (!peer_) return {};
  ENetAddress a = reinterpret_cast<ENetPeer*>(peer_)->address;
  char ip[64] = {0};
  enet_address_get_host_ip(&a, ip, sizeof(ip));
  return std::string(ip);
}

int peer_connection::port() const
{
  if (!peer_) return 0;
  return reinterpret_cast<ENetPeer*>(peer_)->address.port;
}

bool peer_connection::is_connected() const
{
  if (!peer_) return false;
  return reinterpret_cast<ENetPeer*>(peer_)->state == ENET_PEER_STATE_CONNECTED;
}

}} // namespace net::udp
