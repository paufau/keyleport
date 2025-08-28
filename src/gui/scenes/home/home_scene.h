#pragma once

#include "gui/framework/ui_scene.h"
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
};
