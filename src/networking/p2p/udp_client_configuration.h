#pragma once

namespace p2p
{
  class udp_client_configuration
  {
  public:
    udp_client_configuration();
    ~udp_client_configuration();

    int get_port() const;
    void set_port(int port);

  private:
    int port_;
  };
} // namespace p2p
