#pragma once

#include <functional>

namespace gui
{
  namespace framework
  {

    // Post a function to be executed on the UI thread (drained from the window
    // frame loop)
    void post_to_ui(std::function<void()> fn);

    // Called by the window each frame on the UI thread to run queued tasks
    void process_ui_tasks();

  } // namespace framework
} // namespace gui
