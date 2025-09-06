#pragma once

#include "service_lifecycle_listener.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <type_traits>
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

    void add_service(stored_service_t svc)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      services_.emplace_back(std::move(svc));
    }

    const std::vector<stored_service_t>& get_services() const
    {
      return services_;
    }

    // Thread-safe snapshot for iteration on background threads
    std::vector<stored_service_t> get_services_snapshot() const
    {
      std::lock_guard<std::mutex> lk(mtx_);
      return services_;
    }

    template <typename T> std::shared_ptr<T> get_service() const
    {
      static_assert(
          std::is_base_of<services::service_lifecycle_listener, T>::value,
          "T must derive from services::service_lifecycle_listener");

      std::lock_guard<std::mutex> lk(mtx_);
      for (const auto& svc : services_)
      {
        if (auto casted = std::dynamic_pointer_cast<T>(svc))
        {
          return casted;
        }
      }
      return nullptr;
    }

    void remove_service(stored_service_t svc)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      services_.erase(std::remove(services_.begin(), services_.end(), svc),
                      services_.end());
    }

    void clear_services()
    {
      std::lock_guard<std::mutex> lk(mtx_);
      services_.clear();
    }

  private:
    mutable std::mutex mtx_;
    std::vector<stored_service_t> services_;
  };
} // namespace services