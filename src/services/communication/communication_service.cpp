#include "./communication_service.h"

#include <iostream>

namespace services
{
  communication_service::communication_service() = default;

  communication_service::~communication_service() = default;

  void communication_service::init()
  {
    p2p::udp_server_configuration server_config;
    server_config.set_port(default_communication_port_);

    udp_server_ = std::make_shared<p2p::udp_server>(server_config);

    udp_server_->on_message.subscribe(
        [this](const p2p::message& msg)
        {
          if (pinned_peer_ &&
              msg.get_from().get_ip_address() != pinned_peer_->get_ip_address())
          {
            return;
          }

          typed_package package = typed_package::decode(msg.get_payload());
          on_package.emit(package);
        });
  }

  void communication_service::update()
  {
    udp_server_->poll_events();
  }

  void communication_service::cleanup()
  {
    if (udp_server_)
    {
      udp_server_.reset();
    }
    if (udp_client_)
    {
      udp_client_.reset();
    }
  }

  void communication_service::pin_connection(p2p::peer target_peer)
  {
    pinned_peer_ = std::make_shared<p2p::peer>(target_peer);

    p2p::udp_client_configuration config;
    config.set_port(default_communication_port_);
    config.set_peer(*pinned_peer_);

    udp_client_ = std::make_shared<p2p::udp_client>(config);
  }

  void communication_service::unpin_connection()
  {
    pinned_peer_ = nullptr;

    if (udp_client_)
    {
      udp_client_.reset();
    }
  }

  void
  communication_service::send_package_reliable(const typed_package& package)
  {
    if (!udp_client_ || !pinned_peer_)
    {
      std::cerr << "Unable to send reliable package: No UDP client or pinned "
                   "peer available."
                << std::endl;
      return;
    }

    p2p::message msg;
    msg.set_from(p2p::peer::self());
    msg.set_to(*pinned_peer_);
    msg.set_payload(package.encode());

    udp_client_->send_reliable(msg);
  }

  void
  communication_service::send_package_unreliable(const typed_package& package)
  {
    if (!udp_client_ || !pinned_peer_)
    {
      std::cerr << "Unable to send reliable package: No UDP client or pinned "
                   "peer available."
                << std::endl;
      return;
    }

    p2p::message msg;
    msg.set_from(p2p::peer::self());
    msg.set_to(*pinned_peer_);
    msg.set_payload(package.encode());

    udp_client_->send_unreliable(msg);
  }
} // namespace services