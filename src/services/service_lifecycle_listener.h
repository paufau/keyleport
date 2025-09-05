#pragma once

namespace services
{
  class service_lifecycle_listener
  {
  public:
    virtual ~service_lifecycle_listener() = default;

    virtual void init() = 0;
    virtual void update() = 0;
    virtual void cleanup() = 0;
  };
} // namespace services