#include "../device_name.h"

#include <sys/sysctl.h>
#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <string>

#include "utils/get_platform/platform.h"

namespace {

static std::string trim(const std::string &s) {
  const auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  const auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

static std::string get_username_fallback() {
  const char *login = getenv("USER");
  if (login && *login) return std::string(login);
  uid_t uid = getuid();
  struct passwd *pw = getpwuid(uid);
  if (pw && pw->pw_name) return std::string(pw->pw_name);
  return std::string();
}

static std::string get_model_macos() {
  char model[256];
  size_t size = sizeof(model);
  if (sysctlbyname("hw.model", model, &size, nullptr, 0) == 0) {
    return trim(std::string(model, size > 0 ? size - 1 : 0));
  }
  return std::string();
}

} // namespace

std::string get_device_name() {
  auto model = get_model_macos();
  if (!model.empty()) return model;
  auto username = get_username_fallback();
  if (!username.empty()) return username;
  auto platform = get_platform();
  if (!platform.empty()) return platform;
  return std::string();
}
