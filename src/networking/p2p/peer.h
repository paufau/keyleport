#pragma once

#include <string>

namespace p2p
{
  class peer
  {
  public:
    static peer self();

    peer() = default;
    peer(const std::string& ip_address);
    ~peer();

    std::string get_ip_address() const;

    void set_ip_address(const std::string& ip_address);

  private:
    std::string ip_address_{};
  };
} // namespace p2p