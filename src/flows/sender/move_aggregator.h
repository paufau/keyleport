// Simple aggregator for coalescing mouse movement deltas
#pragma once

#include <atomic>
#include <mutex>

namespace flows
{

  struct MoveAggregator
  {
    std::atomic<bool> running{true};
    std::mutex m;
    int agg_dx{0};
    int agg_dy{0};

    void add(int dx, int dy)
    {
      if (dx == 0 && dy == 0)
      {
        return;
      }
      std::lock_guard<std::mutex> lock(m);
      agg_dx += dx;
      agg_dy += dy;
    }

    void take(int& dx, int& dy)
    {
      std::lock_guard<std::mutex> lock(m);
      dx = agg_dx;
      dy = agg_dy;
      agg_dx = 0;
      agg_dy = 0;
    }
  };

} // namespace flows
