#pragma once
#include "base/type.hpp"
namespace anatomy_gui::gui {
struct ApplicationInternal {
    [[nodiscard]] virtual window_handle_t create_window_handle() = 0;
    [[nodiscard]] virtual int execution() = 0;
};
} // namespace anatomy_gui::gui