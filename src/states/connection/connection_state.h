#include "../../entities/connection_candidate.h"
#include "../atom.h"

#include <memory>
#include <string>

namespace states
{
  class ConnectionState
  {
  public:
    Atom<std::shared_ptr<entities::ConnectionCandidate>> connected_device;
    Atom<std::vector<entities::ConnectionCandidate>> available_devices;

    // Add your state management code here
    bool is_connected() const { return is_connected_; }
    void set_connected(bool connected) { is_connected_ = connected; }

    void init()
    {
      connected_device.set(nullptr);
      available_devices.set(std::vector<entities::ConnectionCandidate>{});
    }

  private:
    bool is_connected_ = false;
  };
} // namespace states