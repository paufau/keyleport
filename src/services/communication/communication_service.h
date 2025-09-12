#pragma once

#include "./typed_package.h"
#include "networking/p2p/peer.h"
#include "networking/p2p/udp_client.h"
#include "networking/p2p/udp_server.h"
#include "services/service_lifecycle_listener.h"

#include <memory>

namespace services
{
  class communication_service : public service_lifecycle_listener
  {
  public:
    communication_service();
    ~communication_service();

    void init() override;
    void update() override;
    void cleanup() override;

    void pin_connection(p2p::peer target_peer);
    void unpin_connection();

    void send_package_reliable(const typed_package& package);
    void send_package_unreliable(const typed_package& package);

    utils::event_emitter<services::typed_package> on_package;
    utils::event_emitter<void> on_disconnect;

  private:
    std::shared_ptr<p2p::udp_server> udp_server_;
    std::shared_ptr<p2p::udp_client> udp_client_;
    std::shared_ptr<p2p::peer> pinned_peer_;

    int default_communication_port_ = 8801;
  };
} // namespace services