#include "platform/window.h"

namespace {

// The class name must be unique inside the process. One class serves every
// Window instance.
constexpr wchar_t kWindowClassName[] = L"AnatomyGuiWindow";

}  // namespace

bool Window::create(HINSTANCE instance, const wchar_t* title, int width, int height) {
    WNDCLASSEXW window_class{};
    window_class.cbSize        = sizeof(window_class);
    window_class.lpfnWndProc   = &Window::window_proc;
    window_class.hInstance     = instance;
    window_class.lpszClassName = kWindowClassName;
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    // The class brush stays null: the paint handler owns the client area, and
    // letting the system erase first is what makes a resize flash.
    window_class.hbrBackground = nullptr;

    // Registering the same class twice fails. A second window is a supported
    // case, so treat "already there" as success.
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // The last argument is the only channel that reaches the window procedure
    // before CreateWindowExW returns, so the object travels through it.
    HWND hwnd = CreateWindowExW(0, kWindowClassName, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                                instance, this);
    if (hwnd == nullptr) {
        return false;
    }

    hwnd_ = hwnd;
    return true;
}

void Window::invalidate() {
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void Window::show(int show_command) const {
    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
}

// NOLINTBEGIN(performance-no-int-to-ptr)
LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // WM_NCCREATE is the only message that carries the creation parameters, and
    // it arrives before CreateWindowExW returns. Stash the object pointer here;
    // every later message reads it back.
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self   = static_cast<Window*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
        return self->handle_message(msg, wparam, lparam);
    }

    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->handle_message(msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
// NOLINTEND(performance-no-int-to-ptr)

LRESULT Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        // A destroyed window is not an exiting process: drop WM_QUIT in the
        // queue and let the message loop end on its own terms.
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }
}
