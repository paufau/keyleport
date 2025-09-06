// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "gui/framework/ui_scene.h"
#include "keyboard/keyboard.h"

#include <memory>

class ReceiverScene : public gui::framework::UIScene
{
public:
  void didMount() override;
  void willUnmount() override;

private:
  void render() override;
  // Keyboard injection components
  std::unique_ptr<keyboard::Keyboard> kb_;
  std::unique_ptr<keyboard::Emitter> emitter_;
  uint16_t communication_subscription_id_;
};
