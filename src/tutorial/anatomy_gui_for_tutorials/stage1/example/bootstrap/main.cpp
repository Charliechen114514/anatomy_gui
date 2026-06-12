#include <iostream>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace {
const char* default_xserver_name() noexcept {
    return nullptr;
}

xcb_screen_t* get_default_screen(xcb_connection_t* connection) {
    // get the setups, and fetch the screen
    const xcb_setup_t* setup_handle = xcb_get_setup(connection);
    xcb_screen_iterator_t screen_it = xcb_setup_roots_iterator(setup_handle);
    return screen_it.data;
}

} // namespace

int main() {
    // 我们处理的是使用Linux默认的display-name,
    // 和使用的是默认屏幕，虽然这很不严谨，但是对于起步，这足够了
    xcb_connection_t* connection = xcb_connect(default_xserver_name(), nullptr);

    if (const auto error_code = xcb_connection_has_error(connection)) {
        std::cerr << "X Server Connection Error, please, check the code: "
                  << error_code << '\n';
        xcb_disconnect(connection);
        return error_code;
    }

    xcb_screen_t* default_screen = get_default_screen(connection);

    xcb_window_t window = xcb_generate_id(connection);

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window,
                      default_screen->root, 100, 100, 500, 500, 10,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      default_screen->root_visual, 0, NULL);
    xcb_map_window(connection, window);
    xcb_flush(connection);

    xcb_generic_event_t* event = nullptr;

    while ((event = xcb_wait_for_event(connection)) != nullptr) {
        const uint8_t response_type = event->response_type & ~0x80;

        if (response_type == XCB_KEY_PRESS) {
            free(event);
            break;
        }

        free(event);
    }

    // 释放链接，处理规范来
    xcb_disconnect(connection);
}
