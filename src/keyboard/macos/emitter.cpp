#include <memory>
#include <string>

#include "../emitter.h"

namespace keyboard
{

  class MacEmitter : public Emitter
  {
  public:
    int emit(const InputEvent &event) override
    {
      (void)event;
      return 0;
    }
  };

  std::unique_ptr<Emitter> make_macos_emitter()
  {
    return std::unique_ptr<Emitter>(new MacEmitter());
  }

} // namespace keyboard
