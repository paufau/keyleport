#pragma once

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

  private:
    std::shared_ptr<p2p::udp_server> udp_server_;
    std::shared_ptr<p2p::udp_client> udp_client_;
  };
} // namespace services