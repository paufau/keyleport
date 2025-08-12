#include <iostream>
#include <string>
#include "utils/get_platform/platform.h"
#include "utils/cli/args.h"
#include "networking/server/server.h"
#include "keyboard/keyboard.h"
#include "keyboard/event_batch.h"
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
      const keyboard::EventBatch batch = keyboard::EventBatch::decode(payload);
      if (batch.empty()) return;
      for (const auto &ev : batch.events) {
        emitter_ptr->emit(ev);
      } });
    return r->run();
  }
  if (opt.ip.empty())
    return 2;
  auto s = server->createSender(opt.ip, opt.port);

  auto listener = kb->createListener();

  net::Sender *sender_ptr = s.get();
  listener->onEvent([sender_ptr](const keyboard::EventBatch &batch)
                    {
    if (!batch.empty())
    {
      const std::string payload = batch.encode();
      sender_ptr->send(payload);
    } });

  return listener->run();
}