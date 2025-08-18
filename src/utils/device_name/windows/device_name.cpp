#include "../device_name.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Lmcons.h>
#include <string>

#include "utils/get_platform/platform.h"

namespace {

static std::string get_username_fallback() {
  char username[UNLEN + 1];
  DWORD size = UNLEN + 1;
  if (GetUserNameA(username, &size)) {
    return std::string(username);
  }
  return std::string();
}

static std::string get_model_windows() {
  // Lightweight proxy: computer name. Real model would require WMI.
  char name[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  if (GetComputerNameA(name, &size)) {
    return std::string(name);
  }
  return std::string();
}

} // namespace

std::string get_device_name() {
  auto model = get_model_windows();
  if (!model.empty()) return model;
  auto username = get_username_fallback();
  if (!username.empty()) return username;
  auto platform = get_platform();
  if (!platform.empty()) return platform;
  return std::string();
}
