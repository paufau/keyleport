#pragma once

#include "gui/framework/ui_scene.h"
#include "services/discovery/discovery_service.h"

#include <cstdint>
#include <memory>
#include <thread>

class HomeScene : public gui::framework::UIScene
{
public:
  void didMount() override;
  void willUnmount() override;
  ~HomeScene() override;

private:
  void render() override;

  // Track the registered discovery service to remove it on unmount
  std::shared_ptr<services::discovery_service> discovery_service_;
};
