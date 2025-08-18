#if defined(_WIN32)
#include "../listener.h"

#include <memory>
#include <utility>

namespace keyboard
{

  class WindowsListener : public Listener
  {
  public:
    void onEvent(EventHandler handler) override { handler_ = std::move(handler); }
    int run() override
    {
      // No-op stub listener for Windows to satisfy linkage
      // If needed, emit a synthetic event example:
      // if (handler_) { InputEvent ie{}; ie.type = InputEvent::Type::Key; ie.action = InputEvent::Action::Down; ie.code
      // = 0; ie.dx = 0; ie.dy = 0; handler_(ie); }
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

#endif
