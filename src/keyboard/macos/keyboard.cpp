#include "../keyboard.h"

#include <memory>

namespace keyboard
{

  std::unique_ptr<Listener> make_macos_listener();
  std::unique_ptr<Emitter> make_macos_emitter();

  class MacKeyboard : public Keyboard
  {
  public:
    std::unique_ptr<Listener> createListener() override { return make_macos_listener(); }
    std::unique_ptr<Emitter> createEmitter() override { return make_macos_emitter(); }
  };

  std::unique_ptr<Keyboard> make_keyboard()
  {
    return std::unique_ptr<Keyboard>(new MacKeyboard());
  }

} // namespace keyboard
