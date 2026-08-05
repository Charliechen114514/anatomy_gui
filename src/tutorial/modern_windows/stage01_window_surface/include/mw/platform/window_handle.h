#pragma once

#ifndef WIN32
#    error "Error including a Win32 headers in a none Win32 Platforms"
#else
#    include <Windows.h>

namespace mw::windows {

using native_window_handle_t = HWND;

class unique_hwnd {
  public:
    unique_hwnd(native_window_handle_t handle) : handle_(handle) {}

    [[nodiscard("You should get the windows with a var")]]
    native_window_handle_t native_handle() const noexcept {
        return handle_;
    }

    void reset(native_window_handle_t handle) {
        if (handle == handle_) {
            return; // already self
        }

        release_self();
        handle_ = handle;
    }

    unique_hwnd(const unique_hwnd& other) = delete;
    unique_hwnd& operator=(const unique_hwnd& other) = delete;

    unique_hwnd(unique_hwnd&& other_one) : handle_(other_one.handle_) {
        other_one.handle_ = nullptr;
    }

    unique_hwnd& operator=(unique_hwnd&& o) noexcept {
        if (this != &o) {
            release_self();
            handle_ = o.handle_;
            o.handle_ = nullptr;
        }
        return *this;
    }

  private:
    void release_self() {
        if (handle_) {
            // destroy a valid windows
            DestroyWindow(handle_);
        }
    }
    native_window_handle_t handle_{nullptr};
};
} // namespace mw::windows

#endif
