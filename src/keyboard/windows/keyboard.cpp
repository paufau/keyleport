#include <memory>
#include "../keyboard.h"

namespace keyboard
{

  std::unique_ptr<Listener> make_windows_listener();
  std::unique_ptr<Emitter> make_windows_emitter();

  class WindowsKeyboard : public Keyboard
  {
  public:
    std::unique_ptr<Listener> createListener() override { return make_windows_listener(); }
    std::unique_ptr<Emitter> createEmitter() override { return make_windows_emitter(); }
  };

  std::unique_ptr<Keyboard> make_keyboard()
  {
    return std::unique_ptr<Keyboard>(new WindowsKeyboard());
  }

} // namespace keyboard
