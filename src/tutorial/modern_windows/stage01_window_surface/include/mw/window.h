#pragma once
#include "define.h"
#include "platform/window_handle.h"
#include <memory>

namespace mw {
// Yeah, a window maker
struct WindowFactory;

class WindowsObserver;

class Window {
  public:
    friend struct WindowFactory;

    Size_t size() const noexcept;
    Point_t position() const noexcept;

    mw::windows::native_window_handle_t native_handle() const {
        if (native_handle_) {
            return native_handle_->native_handle();
        }

        return {};
    }

  private:
    Window() = delete;
    Window(std::unique_ptr<mw::windows::unique_hwnd> window);

    

  private:
    std::unique_ptr<mw::windows::unique_hwnd> native_handle_;
};

} // namespace mw
