#pragma once
#include "mw/window_event.h"

namespace mw {
// 窗口事件的接收者。WndProc 把消息翻成 IEvent 子类（ResizeEvent / 以后的
// KeyEvent…），统一从 onEvent 进来，里面按 event.type 分派。
class WindowsObserver {
  public:
    virtual ~WindowsObserver() = default;
    virtual void onEvent(const IEvent& event) = 0;
};
} // namespace mw
