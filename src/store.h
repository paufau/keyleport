// Global singleton containing states

#include "states/connection/connection_state.h"

#include <vector>

namespace store
{
  // Global instance accessor
  inline states::ConnectionState& connection_state()
  {
    static states::ConnectionState instance;
    return instance;
  }

  inline void init()
  {
    // Ensure atoms are initialized to valid values before first get() usage
    connection_state().available_devices.set(std::vector<entities::ConnectionCandidate>{});
  }
} // namespace store