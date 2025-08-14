#pragma once

#include <string>

namespace entities
{
  class ConnectionCandidate
  {
  public:
    ConnectionCandidate(bool is_busy, std::string name, std::string ip, std::string port)
        : is_busy_(is_busy), name_(std::move(name)), ip_(std::move(ip)), port_(std::move(port))
    {
    }

    bool is_busy() const { return is_busy_; }
    const std::string& name() const { return name_; }
    const std::string& ip() const { return ip_; }
    const std::string& port() const { return port_; }

  private:
    bool is_busy_;
    std::string ip_;
    std::string port_;
    std::string name_;
    std::string platform_; // macos, windows, linux
  };
} // namespace entities