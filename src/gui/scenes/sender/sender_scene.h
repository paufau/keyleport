// Simple scene that renders a centered "Hello world!" message
#pragma once

#include "gui/framework/ui_scene.h"

class SenderScene : public gui::framework::UIScene
{
public:
  void didMount() override {}
  void willUnmount() override {}

private:
  void render() override;
};
