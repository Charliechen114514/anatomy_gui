#pragma once

#ifndef WIN32
#    error "windows_window_cb.h depends on Win32 (WNDPROC)."
#else
#    include <Windows.h>

namespace mw {
// 平台层的窗口过程：把 Win32 消息翻译成 IEvent，路由给挂在这扇窗上的 observer。
// 它认 WindowsObserver（mw 层），所以住在 mw 命名空间、由 windows_factory 注册。
LRESULT CALLBACK WindowsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
} // namespace mw

#endif
