#include "my_custom_scene.h"

#include <cstdio>
#include <imgui.h>

void MyCustomScene::render()
{
  // Get viewport size
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 display = io.DisplaySize;

  // Create an invisible window covering the screen and center text
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(display);
  ImGui::Begin("__root__", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

  // Advance frame counter
  ++frame_count_;
  char buf[128];
  snprintf(buf, sizeof(buf), "Hello world! %llu", frame_count_);
  ImVec2 text_size = ImGui::CalcTextSize(buf);
  ImVec2 pos;
  pos.x = (display.x - text_size.x) * 0.5f;
  pos.y = (display.y - text_size.y) * 0.5f;
  ImGui::SetCursorPos(pos);
  ImGui::TextUnformatted(buf);

  ImGui::End();
}
