#pragma once

#include "gui/framework/ui_scene.h"
#include "services/communication/communication_service.h"
#include "services/discovery/discovery_service.h"
#include "utils/event_emitter/event_emitter.h"

#include <cstdint>
#include <memory>
#include <thread>

class HomeScene : public gui::framework::UIScene
{
public:
  void didMount() override;
  void willUnmount() override;

private:
  void render() override;

  // Track the registered discovery service to remove it on unmount
  std::shared_ptr<services::discovery_service> discovery_service_;
  std::shared_ptr<services::communication_service> communication_service_;
  std::uint16_t communication_subscription_id_;
};
