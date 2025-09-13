#if defined(_WIN32)
#include "../keyboard.h"

#include <memory>

namespace keyboard
{

  std::unique_ptr<Emitter> make_windows_emitter();

  class WindowsKeyboard : public Keyboard
  {
  public:
    std::unique_ptr<Emitter> createEmitter() override
    {
      return make_windows_emitter();
    }
  };

  std::unique_ptr<Keyboard> make_keyboard()
  {
    return std::unique_ptr<Keyboard>(new WindowsKeyboard());
  }

} // namespace keyboard
#endif
