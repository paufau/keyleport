#include "platform.h"

std::string get_platform()
{
#ifdef _WIN32
  return "windows";
#elif __APPLE__
  return "macos";
#else
  return "linux";
#endif
}
