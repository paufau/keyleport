#include "DiscoveryServer.h"
#include "SessionServer.h"

#include <asio.hpp>
#include <iostream>
#include <random>

// Guard example with macro so it doesn't conflict with the app's main
#ifndef KEYLEPORT_P2P_EXAMPLE
int main();
#else
int p2p_example_main();
#endif

using namespace net::p2p;

static std::string gen_id()
{
  // Not a real UUID, but unique enough for demo
  std::random_device rd;
  std::mt19937_64 rng(rd());
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(rng), b = dist(rng);
  char buf[37];
  std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx", (unsigned)(a & 0xffffffff),
                (unsigned)((a >> 32) & 0xffff), (unsigned)(b & 0xffff), (unsigned)((b >> 16) & 0xffff),
                (unsigned long long)(b >> 32));
  return std::string(buf);
}

#ifdef KEYLEPORT_P2P_EXAMPLE
int p2p_example_main()
#else
int main()
#endif
{
  asio::io_context io;

  std::string instance_id = gen_id();
  static uint64_t boot_id = 1; // For demo purposes

  // Create TCP session server on ephemeral port
  SessionServer session_server(io, 0);
  uint16_t sess_port = session_server.port();

  // Create discovery server
  DiscoveryServer discovery(io, instance_id, boot_id, sess_port);
  discovery.set_state(State::Idle);

  // Accept connections: run handshake server-side
  session_server.async_accept(
      [&](Session::Ptr s)
      {
        std::cout << "Incoming TCP session from " << s->socket().remote_endpoint() << std::endl;
        discovery.set_state(State::Busy);
        s->start_server([&] { std::cout << "Handshake done (server)." << std::endl; });
      });

  // When peers update, try to connect to the first idle one (demo)
  discovery.set_on_peer_update(
      [&](const Peer& p)
      {
        std::cout << "Peer update: " << p.instance_id << " ip=" << p.ip_address << ":" << p.session_port
                  << " state=" << to_string(p.state) << std::endl;
        if (p.state == State::Idle)
        {
          // Try connect once
          auto sock = std::make_shared<asio::ip::tcp::socket>(io);
          asio::ip::tcp::endpoint ep(asio::ip::make_address(p.ip_address), p.session_port);
          sock->async_connect(ep,
                              [&, sock](std::error_code ec)
                              {
                                if (!ec)
                                {
                                  auto s = std::make_shared<Session>(std::move(*sock));
                                  discovery.set_state(State::Connecting);
                                  s->start_client(
                                      [&]
                                      {
                                        std::cout << "Handshake done (client)." << std::endl;
                                        discovery.set_state(State::Busy);
                                      });
                                }
                              });
        }
      });

  discovery.start();

  std::cout << "Instance " << instance_id << " boot=" << boot_id << " listening on TCP port " << sess_port << std::endl;
  std::cout << "Multicast discovery on " << MULTICAST_ADDR << ":" << DISCOVERY_PORT << std::endl;

  io.run();

  return 0;
}
