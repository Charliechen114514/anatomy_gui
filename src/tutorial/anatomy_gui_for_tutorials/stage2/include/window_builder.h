#pragma once

#include "base/point.hpp"
#include "base/size.h"
#include "window.h"
#include <memory>
namespace anatomy_gui::gui {
class WindowBuilder {
  public:
    enum class WindowPolicy { ALIGN_CENTER };
    WindowBuilder(WindowPolicy p = WindowPolicy::ALIGN_CENTER);

    WindowBuilder& setSize(base::Size size);
    WindowBuilder& setPosition(base::Point tl);

    [[nodiscard]] std::unique_ptr<Window> create_window();

  private:
    WindowPolicy policy_;
    base::Size size_{0, 0};
    base::Point position_{0, 0};
};
} // namespace anatomy_gui::gui