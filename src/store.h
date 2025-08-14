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
    connection_state().init();
  }
} // namespace store