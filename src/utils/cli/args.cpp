#include "args.h"

#include <iostream>

namespace cli
{

  static void set_error(Options& o, std::string msg)
  {
    o.valid = false;
    o.error = std::move(msg);
  }

  Options parse(int argc, char* argv[])
  {
    Options opt{};
#ifdef _WIN32
    // On Windows, default to receiver on port 8080 when no args are provided
    if (argc <= 1)
    {
      opt.mode = "receiver";
      opt.port = 8080;
      return opt;
    }
#endif
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
    // Post-parse normalization/validation
    if (!opt.help)
    {
      if (opt.mode != "sender" && opt.mode != "receiver")
      {
        set_error(opt, "--mode must be 'sender' or 'receiver'");
      }
      if (opt.port <= 0)
      {
        opt.port = 8080; // default
      }
    }
    return opt;
  }

  void print_usage(const char* program_name)
  {
    std::cout << "Usage: " << program_name
              << " --mode <sender|receiver> [--ip <addr>] [--port <port>]"
              << std::endl;
  }

} // namespace cli
