#include "mw/window.h"
#include <Windows.h>

namespace mw {

Window::Window(std::unique_ptr<mw::windows::unique_hwnd> window)
    : native_handle_(std::move(window)) {}

Size_t Window::size() const noexcept {
    RECT r{};
    if (native_handle_) GetClientRect(native_handle_->native_handle(), &r);
    return {static_cast<int32_t>(r.right), static_cast<int32_t>(r.bottom)};
}

Point_t Window::position() const noexcept {
    RECT r{};
    if (native_handle_) GetWindowRect(native_handle_->native_handle(), &r);
    return {static_cast<int32_t>(r.left), static_cast<int32_t>(r.top)};
}

} // namespace mw
