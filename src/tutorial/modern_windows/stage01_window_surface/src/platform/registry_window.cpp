#include "mw/platform/registry_window.h"

namespace mw {

void register_window_class(WNDPROC proc) {
    // 先问 OS：这个类名注册过了吗？OS 每进程自带一张"已注册窗口类表"，
    // 它替我们记着"有没有注册"这件事——所以我们自己一个 static 都不用。
    WNDCLASSEXW probe{};
    probe.cbSize = sizeof(probe);
    if (GetClassInfoExW(GetModuleHandleW(nullptr), kMainWindowClassName, &probe)) {
        return; // 已注册，跳过
    }

    // 没注册，画图纸：把 proc 钉在 lpfnWndProc 上。
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = proc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kMainWindowClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    // （hbrBackground/style 都不填——咱们是 DComp 窗，不靠 GDI 画背景）
    RegisterClassExW(&wc);
}

} // namespace mw
