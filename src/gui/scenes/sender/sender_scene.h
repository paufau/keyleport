// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "flows/sender/sender.h"
#include "gui/framework/ui_scene.h"

class SenderScene : public gui::framework::UIScene
{
public:
  void didMount() override;
  void willUnmount() override;
  void handleInput(const gui::framework::UIInputEvent& event) override;

private:
  void render() override;
  bool is_mouse_contained_ = true;
  std::unique_ptr<flows::SenderFlow> flow_;
};
