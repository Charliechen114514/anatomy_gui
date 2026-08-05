#include "mw/platform/windows_window_cb.h"
#include "mw/window_observer.h"
#include "mw/window_event.h"
#include <Windows.h>

namespace mw {

LRESULT CALLBACK WindowsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // 出生第一条消息：把 make() 经 lpParam 塞进来的 observer 存进 GWLP_USERDATA。
    // ⚠️ 必须 return，不能贯穿——否则会一路掉进下面 WM_DESTROY 直接退程序。
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // WM_DESTROY：退出消息循环。不需要 observer，放前面。
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    // 其余消息：取出"这扇窗的"observer，按消息造事件分发。
    auto* obs = reinterpret_cast<WindowsObserver*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_SIZE: {
            ResizeEvent e;
            e.newSize = {static_cast<int32_t>(LOWORD(lp)),
                         static_cast<int32_t>(HIWORD(lp))};
            if (obs) obs->onEvent(e);
            return 0;
        }
        // 后面阶段会加 WM_KEYDOWN / WM_POINTER → 各自造 KeyEvent / PointerEvent
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace mw
