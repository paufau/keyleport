#pragma once

#include "service_lifecycle_listener.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace services
{
  class services_repository
  {
  public:
    services_repository() {}
    ~services_repository() {}

    using stored_service_t =
        std::shared_ptr<services::service_lifecycle_listener>;

    void add_service(stored_service_t svc) { services_.emplace_back(svc); }

    const std::vector<stored_service_t>& get_services() const
    {
      return services_;
    }

    void remove_service(stored_service_t svc)
    {
      services_.erase(std::remove(services_.begin(), services_.end(), svc),
                      services_.end());
    }

    void clear_services() { services_.clear(); }

  private:
    std::vector<stored_service_t> services_;
  };
} // namespace services