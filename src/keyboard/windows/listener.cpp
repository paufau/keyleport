#include <memory>
#include <utility>
#include "../listener.h"

namespace keyboard
{

  class WindowsListener : public Listener
  {
  public:
    void onEvent(EventHandler handler) override { handler_ = std::move(handler); }
    int run() override
    {
      // No-op stub listener for Windows to satisfy linkage
      return 0;
    }

  private:
    EventHandler handler_;
  };

  std::unique_ptr<Listener> make_windows_listener()
  {
    return std::unique_ptr<Listener>(new WindowsListener());
  }

} // namespace keyboard
