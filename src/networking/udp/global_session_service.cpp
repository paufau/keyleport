#include "networking/udp/global_session_service.h"

namespace net { namespace udp {

global_session_service::global_session_service(udp_service& net)
  : net_(net)
{
}

void global_session_service::connect_to_peer(const peer_info& peer)
{
  if (current_.has_value())
    disconnect();
  current_ = peer;
  current_conn_ = net_.connect_to(peer.ip_address, peer.port);
  on_session_start.emit(peer);
}

void global_session_service::disconnect()
{
  if (!current_.has_value()) return;
  if (current_conn_) current_conn_->disconnect();
  on_session_end.emit(*current_);
  current_.reset();
  current_conn_.reset();
}

session_state global_session_service::get_current_state() const
{
  return current_.has_value() ? session_state::busy : session_state::idle;
}

}} // namespace net::udp
