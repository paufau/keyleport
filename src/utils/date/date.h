#pragma once

#include <chrono>
#include <cstdint>

namespace utils
{
  namespace date
  {
    inline uint64_t now()
    {
      return static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count());
    }
  } // namespace date
} // namespace utils
