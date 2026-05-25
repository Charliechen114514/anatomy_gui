#pragma once

namespace anatomy_gui::gui::xcb {
const char* default_server_name() noexcept; // nullptr as well
int* default_screen_index() noexcept;       // in xcb, defaulty is nullptr
} // namespace anatomy_gui::gui::xcb
