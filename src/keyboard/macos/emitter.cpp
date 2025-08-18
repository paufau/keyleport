#if defined(__APPLE__)
#include "../emitter.h"

#include <iostream>
#include <memory>
#include <string>

namespace keyboard
{

  class MacEmitter : public Emitter
  {
  public:
    int emit(const InputEvent& event) override
    {
      std::cout << "[macos emitter] type=" << static_cast<int>(event.type)
                << " action=" << static_cast<int>(event.action) << " code=" << event.code << " dx=" << event.dx
                << " dy=" << event.dy << std::endl;
      return 0;
    }
  };

  std::unique_ptr<Emitter> make_macos_emitter()
  {
    return std::unique_ptr<Emitter>(new MacEmitter());
  }

} // namespace keyboard

#endif
