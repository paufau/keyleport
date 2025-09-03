// event_emitter: a small thread-safe pub/sub utility for arbitrary event payload types
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace net
{
  namespace udp
  {

    template <typename EventT> class event_emitter
    {
    public:
      using callback_t = std::function<void(const EventT&)>;
      using subscription_id = std::uint64_t;

      subscription_id subscribe(callback_t cb)
      {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto id = ++next_id_;
        subscribers_.push_back(subscription_entry{id, std::move(cb)});
        return id;
      }
      void emit(const EventT& ev)
      {
        // copy under lock, invoke without lock to avoid deadlocks
        std::vector<callback_t> snapshot;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          snapshot.reserve(subscribers_.size());
          for (const auto& s : subscribers_)
          {
            snapshot.push_back(s.callback);
          }
        }
        for (auto& cb : snapshot)
        {
          if (cb)
          {
            cb(ev);
          }
        }
      }
      void unsubscribe(subscription_id id)
      {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it)
        {
          if (it->id == id)
          {
            subscribers_.erase(it);
            break;
          }
        }
      }
      void clear_subscribers()
      {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
      }

    private:
      struct subscription_entry
      {
        subscription_id id{0};
        callback_t callback;
      };

      std::mutex mutex_;
      std::vector<subscription_entry> subscribers_;
      subscription_id next_id_{0};
    };

  } // namespace udp
} // namespace net
