#pragma once
#include "application_handle.h"
#include "base/type.hpp"

#include <algorithm>
#include <vector>
#include <xcb/xcb.h>

namespace anatomy_gui::gui {
struct XcbApplicationInternal : public ApplicationInternal {
  public:
    XcbApplicationInternal(int argc, char** argv);
    ~XcbApplicationInternal();
    [[nodiscard]] window_handle_t create_window_handle() override;
    [[nodiscard]] int execution() override; // Event Poll IMPL

    xcb_connection_t* get_connection() const { return server_connection; }
    void remove_a_native_window(window_handle_t w) {
        auto it = std::remove(windows.begin(), windows.end(), w);
        windows.erase(it, windows.end());
    }

  private:
    std::vector<window_handle_t> windows;
    xcb_screen_t* action_screen; // screen we work
    xcb_connection_t*
        server_connection{}; // Oh, we actually dont expose it, so take it easy!
};
} // namespace anatomy_gui::gui