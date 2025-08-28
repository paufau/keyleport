// Simple reusable event_emitter template to decouple components via events.
// Provides subscribe() and emit() for arbitrary event payload types.

#pragma once

#include <functional>
#include <mutex>
#include <vector>
#include <cstdint>

namespace net { namespace udp {

template <typename event_t>
class event_emitter {
public:
  using callback_t = std::function<void(const event_t&)>;
  using subscription_id = std::uint64_t;

  struct subscription_entry {
    subscription_id id{0};
    callback_t callback;
  };

  // subscribe: thread-safe registration of a listener. Returns a subscription id for later unsubscription.
  subscription_id subscribe(callback_t cb)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto id = ++next_id_;
    subscribers_.push_back(subscription_entry{ id, std::move(cb) });
    return id;
  }

  // emit: call all listeners with the given event
  void emit(const event_t& ev)
  {
    // copy under lock, invoke without lock to avoid deadlocks
    std::vector<callback_t> snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot.reserve(subscribers_.size());
      for (const auto& s : subscribers_) {
        snapshot.push_back(s.callback);
      }
    }
    for (auto& cb : snapshot)
    {
      if (cb)
        cb(ev);
    }
  }

  // unsubscribe: remove a previously registered listener by id
  void unsubscribe(subscription_id id)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
      if (it->id == id) {
        subscribers_.erase(it);
        break;
      }
    }
  }

  // clear all subscribers
  void clear_subscribers()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
  }

private:
  std::mutex mutex_;
  std::vector<subscription_entry> subscribers_;
  subscription_id next_id_{0};
};

}} // namespace net::udp
