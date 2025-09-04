#if !defined(_WIN32) && !defined(__APPLE__)
#include "../device_name.h"

#include "utils/get_platform/platform.h"

#include <fstream>
#include <pwd.h>
#include <string>
#include <unistd.h>

namespace
{

  static std::string trim(const std::string& s)
  {
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
      return "";
    }
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
  }

  static std::string read_first_line(const char* path)
  {
    std::ifstream f(path);
    if (!f.is_open())
    {
      return std::string();
    }
    std::string line;
    std::getline(f, line);
    return trim(line);
  }

  static std::string get_username_fallback()
  {
    const char* login = getenv("USER");
    if (login && *login)
    {
      return std::string(login);
    }
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (pw && pw->pw_name)
    {
      return std::string(pw->pw_name);
    }
    return std::string();
  }

  static std::string get_model_linux()
  {
    // Prefer DMI product name
    std::string name =
        read_first_line("/sys/devices/virtual/dmi/id/product_name");
    if (!name.empty())
    {
      std::string version =
          read_first_line("/sys/devices/virtual/dmi/id/product_version");
      if (!version.empty() && version != "None")
      {
        name += " " + version;
      }
      return name;
    }
    // Fallback to hostname
    name = read_first_line("/etc/hostname");
    return name;
  }

} // namespace

std::string get_device_name()
{
  auto model = get_model_linux();
  if (!model.empty())
  {
    return model;
  }
  auto username = get_username_fallback();
  if (!username.empty())
  {
    return username;
  }
  auto platform = get_platform();
  if (!platform.empty())
  {
    return platform;
  }
  return std::string();
}

#endif // generic cxx (non-Windows, non-macOS)
