#pragma once

namespace p2p
{
  class udp_server_configuration
  {
  public:
    udp_server_configuration();
    ~udp_server_configuration();

    int get_port() const;
    void set_port(int port);

  private:
    int port_;
  };
} // namespace p2p
