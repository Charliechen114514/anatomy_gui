#pragma once
#include "base/point.hpp"
#include "base/size.h"

namespace anatomy_gui::gui {
struct WindowInternal {
    virtual ~WindowInternal() = default;
    virtual void show() = 0;
    virtual void setSize(base::Size size) = 0;
    virtual void setPosition(base::Point point) = 0;
};
} // namespace anatomy_gui::gui
