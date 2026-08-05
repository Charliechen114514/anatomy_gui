#include "mw/windows_factory.h"
#include "mw/platform/error.h"
#include "mw/platform/registry_window.h"
#include "mw/platform/windows_window_cb.h"
#include "mw/window.h"
#include <Windows.h>

namespace mw {

WindowFactory& WindowFactory::setTitle(std::wstring_view t) {
    title_ = t;
    return *this;
}
WindowFactory& WindowFactory::setSize(Size_t s) {
    size_ = s;
    return *this;
}
WindowFactory& WindowFactory::setPosition(Point_t p) {
    pos_ = p;
    return *this;
}
WindowFactory& WindowFactory::setObserver(WindowsObserver* o) {
    observer_ = o;
    return *this;
}

Result<std::unique_ptr<Window>> WindowFactory::make() {
    // 1) 注册窗口类（幂等），把 Window::window_proc 钉在图纸上
    register_window_class(&WindowsWindowProc);

    // 2) 按图纸造一扇：WS_EX_NOREDIRECTIONBITMAP + observer 当 lpParam 私货
    HWND raw =
        CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP,
                        kMainWindowClassName, // 必须和注册时的类名一致
                        title_.c_str(), WS_OVERLAPPEDWINDOW, pos_.x, pos_.y, size_.width,
                        size_.height, nullptr, nullptr, GetModuleHandleW(nullptr),
                        reinterpret_cast<LPVOID>(observer_)); // ← 进 WM_NCCREATE 的 lpCreateParams
    if (!raw) {
        return from_win32();
    }

    // 3) 包成 unique_hwnd → Window
    auto handle = std::make_unique<mw::windows::unique_hwnd>(raw);
    return std::unique_ptr<Window>(new Window(std::move(handle)));
}

} // namespace mw
