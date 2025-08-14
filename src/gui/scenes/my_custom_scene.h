// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "gui/framework/ui_scene.h"

class MyCustomScene : public gui::framework::UIScene
{
public:
  void didMount() override {}
  void willUnmount() override {}

private:
  unsigned long long frame_count_ = 0;
  void render() override;
};
