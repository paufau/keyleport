#include "args.h"

#include <iostream>

namespace cli
{

  static void set_error(Options &o, std::string msg)
  {
    o.valid = false;
    o.error = std::move(msg);
  }

  Options parse(int argc, char *argv[])
  {
    Options opt{};
    for (int i = 1; i < argc; ++i)
    {
      std::string arg = argv[i];
      if ((arg == "--mode" || arg == "-m") && i + 1 < argc)
      {
        opt.mode = argv[++i];
      }
      else if (arg == "--ip" && i + 1 < argc)
      {
        opt.ip = argv[++i];
      }
      else if ((arg == "--port" || arg == "-p") && i + 1 < argc)
      {
        try
        {
          opt.port = std::stoi(argv[++i]);
        }
        catch (...)
        {
          set_error(opt, "Invalid --port value");
          return opt;
        }
      }
      else if (arg == "--help" || arg == "-h")
      {
        opt.help = true;
      }
      else
      {
        set_error(opt, std::string("Unknown argument: ") + arg);
        return opt;
      }
    }
    return opt;
  }

  void print_usage(const char *program_name)
  {
    std::cout << "Usage: " << program_name << " --mode <sender|receiver> [--ip <addr>] [--port <port>]" << std::endl;
  }

} // namespace cli
