#include "ui_dispatch.h"

#include <mutex>
#include <queue>

namespace gui
{
  namespace framework
  {

    namespace
    {
      std::mutex& mtx()
      {
        static std::mutex m;
        return m;
      }
      std::queue<std::function<void()>>& q()
      {
        static std::queue<std::function<void()>> qu;
        return qu;
      }
    } // namespace

    void post_to_ui(std::function<void()> fn)
    {
      if (!fn)
      {
        return;
      }
      std::lock_guard<std::mutex> lk(mtx());
      q().push(std::move(fn));
    }

    void process_ui_tasks()
    {
      // Process up to N tasks per frame to avoid long stalls/re-entrancy
      const int kMaxPerFrame = 8;
      for (int i = 0; i < kMaxPerFrame; ++i)
      {
        std::function<void()> fn;
        {
          std::lock_guard<std::mutex> lk(mtx());
          if (q().empty())
          {
            break;
          }
          fn = std::move(q().front());
          q().pop();
        }
        if (fn)
        {
          fn();
        }
      }
    }

  } // namespace framework
} // namespace gui
