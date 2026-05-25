#pragma once
#include "window_handle.h"
#include "base/type.hpp"
#include <xcb/xcb.h>

namespace anatomy_gui::gui {
struct XcbWindowInternal : public WindowInternal {
  public:
    XcbWindowInternal(xcb_connection_t* conn, window_handle_t handle);

    void show() override;
    void setSize(base::Size size) override;
    void setPosition(base::Point point) override;

  private:
    xcb_connection_t* connection_;
    window_handle_t handle_;
};
} // namespace anatomy_gui::gui
