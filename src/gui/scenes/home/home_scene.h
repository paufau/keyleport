#pragma once

#include "gui/framework/ui_scene.h"

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

  // Subscriptions to app_net events to clean up on unmount
  std::uint64_t discovery_sub_id_{0};
  std::uint64_t lose_sub_id_{0};
  std::uint64_t session_start_sub_id_{0};
};
