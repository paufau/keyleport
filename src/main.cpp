#include "services/main_loop/main_loop.h"
#include "services/service_locator.h"
#include "store.h"

int main(int argc, char* argv[])
{
  store::init();

  services::service_locator::instance().main_loop =
      std::make_unique<services::main_loop>();

  auto& main_loop = *services::service_locator::instance().main_loop;
  main_loop.init();
  main_loop.run();
  main_loop.cleanup();
}