namespace p2p
{
  class udp_server
  {
  public:
    udp_server();
    ~udp_server();

    bool start();
    void stop();

    private:
      // Private implementation details
    };
} // namespace p2p
