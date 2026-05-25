#include "platform/window_xcb.h"
#include <xcb/xcb.h>

namespace anatomy_gui::gui {

XcbWindowInternal::XcbWindowInternal(xcb_connection_t* conn,
                                     window_handle_t handle)
    : connection_(conn), handle_(handle) {}

void XcbWindowInternal::show() {
    xcb_map_window(connection_, handle_);
    xcb_flush(connection_);
}

void XcbWindowInternal::setSize(base::Size size) {
    const uint32_t values[] = {size.width, size.height};
    xcb_configure_window(connection_, handle_,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    xcb_flush(connection_);
}

void XcbWindowInternal::setPosition(base::Point point) {
    const uint32_t values[] = {point.x(), point.y()};
    xcb_configure_window(connection_, handle_,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(connection_);
}

} // namespace anatomy_gui::gui
