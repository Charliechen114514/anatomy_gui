#pragma once

#ifndef WIN32
#    error "registry_window.h depends on Win32 (RegisterClassExW / GetClassInfoExW)."
#else
#    include <Windows.h>

namespace mw {
// 框架主窗口类的名字。registry 注册时用它，factory 造窗时也用它——
// 两边引用同一个符号，避免各写一份字符串对不上。
inline constexpr const wchar_t* kMainWindowClassName = L"MwMainWindow";

// 注册框架的主窗口类，幂等：用 GetClassInfoExW 问 OS 这个类名注册过没有，
// 注册过就直接返回——全程无 static、无全局状态。proc 是这类窗口的消息处理函数。
void register_window_class(WNDPROC proc);
} // namespace mw

#endif
