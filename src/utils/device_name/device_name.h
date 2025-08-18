#pragma once

#include <string>

// Returns a human-friendly device identifier.
// Preference order:
// 1) Device model (e.g., Mac14,7 or a DMI product name on Linux)
// 2) Current user's login name
// 3) Platform name (e.g., macos/windows/linux)
// If all are unavailable, returns an empty string.
std::string get_device_name();
