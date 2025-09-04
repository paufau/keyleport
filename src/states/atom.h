// Generic observable state container
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
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

    // Get current value
    // Non-const accessor to allow mutation of the contained value.
    // Note: requires the Atom to have been assigned a value via set() or
    // constructed with an initial value.
    T& get() { return *value_; }
    // Const accessor retained for read-only use sites.
    const T& get() const { return *value_; }

    // Set value (lvalue/rvalue) and notify all subscribers
    void set(const T& v)
    {
      value_.reset(new T(v));
      notify_();
    }
    void set(T&& v)
    {
      value_.reset(new T(std::move(v)));
      notify_();
    }

    // Subscribe to value changes; returns an id for optional unsubscription
    std::size_t subscribe(Callback cb)
    {
      const std::size_t id = ++next_id_;
      listeners_.push_back(Listener{id, std::move(cb)});
      return id;
    }

    // Optional: remove a listener by id
    void unsubscribe(std::size_t id)
    {
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
    void clear_listeners() { listeners_.clear(); }

  private:
    void notify_()
    {
      if (!value_)
      {
        return; // nothing to notify yet
      }
      for (const auto& l : listeners_)
      {
        if (l.cb)
        {
          l.cb(*value_);
        }
      }
    }

    struct Listener
    {
      std::size_t id;
      Callback cb;
    };

    std::unique_ptr<T> value_; // defer construction until explicitly set
    std::vector<Listener> listeners_{};
    std::size_t next_id_ = 0;
  };

} // namespace states
