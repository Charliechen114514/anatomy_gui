#include "platform/application_xcb.h"
#include "platform/xcb_helpers.h"
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <xcb/xcb.h>

namespace {

xcb_screen_t* get_default_screen(xcb_connection_t* connection) {
    // get the setups, and fetch the screen
    const xcb_setup_t* setup_handle = xcb_get_setup(connection);
    xcb_screen_iterator_t screen_it = xcb_setup_roots_iterator(setup_handle);
    return screen_it.data;
}

} // namespace

namespace anatomy_gui::gui {
XcbApplicationInternal::XcbApplicationInternal(int argc, char** argv) {
    // OK, ignore the argc and argv first
    server_connection =
        xcb_connect(xcb::default_server_name(), xcb::default_screen_index());

    if (const auto error_code = xcb_connection_has_error(server_connection)) {
        xcb_disconnect(server_connection);
        throw std::invalid_argument{
            "X Server Connection Error, please, check the code: " +
            std::to_string(error_code) + "\n"};
    };

    action_screen = get_default_screen(server_connection);
};

XcbApplicationInternal::~XcbApplicationInternal() {
    xcb_disconnect(server_connection);
}

window_handle_t XcbApplicationInternal::create_window_handle() {
    xcb_window_t window = xcb_generate_id(server_connection);

    uint32_t event_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
                          XCB_EVENT_MASK_KEY_RELEASE |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                          XCB_EVENT_MASK_FOCUS_CHANGE;
    uint32_t values[] = {event_mask};

    xcb_create_window(server_connection, XCB_COPY_FROM_PARENT, window,
                      action_screen->root, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, action_screen->root_visual,
                      XCB_CW_EVENT_MASK, values);

    return window;
}

int XcbApplicationInternal::execution() {
    xcb_generic_event_t* event;

    while ((event = xcb_wait_for_event(server_connection)) != nullptr) {
        const uint8_t response_type = event->response_type & ~0x80;
        free(event);
        if (response_type == XCB_KEY_PRESS) {
            return 0;
        }
    }

    return 0;
}

} // namespace anatomy_gui::gui