#pragma once

#include "./main_loop/main_loop.h"
#include <memory>

namespace services
{
  class service_locator
  {
  public:
    static service_locator& instance()
    {
      static service_locator instance;
      return instance;
    }

  std::unique_ptr<services::main_loop> main_loop;

  private:
    service_locator() {}
    ~service_locator() {}
  };
} // namespace service_locator