#include <iostream>
#include <string>
#include "utils/get_platform/platform.h"
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"
#include "keyboard/input_event.h"
#include <sstream>

int main(int argc, char *argv[])
{
  cli::Options opt = cli::parse(argc, argv);
  if (opt.help)
  {
    cli::print_usage(argv[0]);
    return 0;
  }
  if (!opt.valid)
  {
    std::cout << opt.error << std::endl;
    cli::print_usage(argv[0]);
    return 1;
  }

  if (opt.mode != "sender" && opt.mode != "receiver")
  {
    cli::print_usage(argv[0]);
    return 1;
  }
  if (opt.port <= 0)
    opt.port = 8080; // default

  auto server = net::make_server();
  auto kb = keyboard::make_keyboard();

  if (opt.mode == "receiver")
  {
    // Create keyboard emitter and feed decoded events into it

    auto emitter = kb->createEmitter();
    keyboard::Emitter *emitter_ptr = emitter.get();

    auto r = server->createReceiver(opt.port);
    r->onReceive([emitter_ptr](const std::string &payload)
                 {
  if (payload.empty()) return; 
  // Debug: show received payload
  std::cerr << "[receiver] payload: " << payload << std::endl;
    keyboard::InputEvent ev = keyboard::InputEventJSONConverter::decode(payload);
    std::cerr << "[receiver] decoded type=" << static_cast<int>(ev.type)
              << " action=" << static_cast<int>(ev.action)
              << " code=" << ev.code
              << " dx=" << ev.dx
              << " dy=" << ev.dy << std::endl;
    emitter_ptr->emit(ev); });
    return r->run();
  }
  if (opt.ip.empty())
    return 2;
  auto s = server->createSender(opt.ip, opt.port);

  auto listener = kb->createListener();

  net::Sender *sender_ptr = s.get();
  listener->onEvent([sender_ptr](const keyboard::InputEvent &ev)
                    {
    const bool use_udp = (ev.type == keyboard::InputEvent::Type::Mouse && (ev.action == keyboard::InputEvent::Action::Move || ev.action == keyboard::InputEvent::Action::Scroll));
    const std::string payload = keyboard::InputEventJSONConverter::encode(ev);
    // Debug: show send path and payload
    std::cerr << "[sender] via " << (use_udp ? "UDP" : "TCP") << ": " << payload << std::endl;
    if (use_udp) {
      sender_ptr->send_udp(payload);
    } else {
      sender_ptr->send_tcp(payload);
    } });

  return listener->run();
}