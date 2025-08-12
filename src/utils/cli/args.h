#pragma once

#include <string>

namespace cli
{

  struct Options
  {
    std::string mode;         // "sender" or "receiver"
    std::string ip;           // required if mode == sender
    int         port = 0;     // defaulted to 8080 in parse() if <= 0
    bool        help = false; // --help or -h
    bool        valid = true; // false if parsing error
    std::string error;        // optional error message
  };

  Options parse(int argc, char* argv[]);
  void    print_usage(const char* program_name);

} // namespace cli
