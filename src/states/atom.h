// Generic observable state container
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace states
{

  template <typename T> class Atom
  {
  public:
    using Callback = std::function<void(const T&)>;

    Atom() = default;
    explicit Atom(T initial) : value_(new T(std::move(initial))) {}

    // Thread-safe snapshot getter (by value). If not set yet, returns
    // default-constructed T.
    T value() const
    {
      std::lock_guard<std::mutex> lk(mtx_);
      return value_ ? *value_ : T{};
    }

    // Set value (lvalue/rvalue) and notify all subscribers
    void set(const T& v)
    {
      {
        std::lock_guard<std::mutex> lk(mtx_);
        value_.reset(new T(v));
      }
      notify_();
    }
    void set(T&& v)
    {
      {
        std::lock_guard<std::mutex> lk(mtx_);
        value_.reset(new T(std::move(v)));
      }
      notify_();
    }

    // Subscribe to value changes; returns an id for optional unsubscription
    std::size_t subscribe(Callback cb)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      const std::size_t id = ++next_id_;
      listeners_.push_back(Listener{id, std::move(cb)});
      return id;
    }

    // Optional: remove a listener by id
    void unsubscribe(std::size_t id)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      for (auto it = listeners_.begin(); it != listeners_.end(); ++it)
      {
        if (it->id == id)
        {
          listeners_.erase(it);
          break;
        }
      }
    }

    // Remove all listeners
    void clear_listeners()
    {
      std::lock_guard<std::mutex> lk(mtx_);
      listeners_.clear();
    }

  private:
    void notify_()
    {
      // Copy snapshot under lock then invoke callbacks without lock
      std::vector<Listener> ls;
      T snapshot{};
      {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!value_)
        {
          return;
        }
        ls = listeners_;
        snapshot = *value_;
      }
      for (const auto& l : ls)
      {
        if (l.cb)
        {
          l.cb(snapshot);
        }
      }
    }

    struct Listener
    {
      std::size_t id;
      Callback cb;
    };

    mutable std::mutex mtx_;
    std::unique_ptr<T> value_; // defer construction until explicitly set
    std::vector<Listener> listeners_{};
    std::size_t next_id_ = 0;
  };

} // namespace states
