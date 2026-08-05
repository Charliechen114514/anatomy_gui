# 01e · 事件层——让窗口会"喊"resize

> 01d 那扇窗能拖、能调大小了，但你 resize 它的时候，`WindowsWindowProc` 收到 `WM_SIZE` 却什么都不干——我们还没处理。这一篇给它接上**事件层**：把 Win32 的原始消息翻译成框架的 `IEvent`（先是 `ResizeEvent`），统一派给 `WindowsObserver`。这不是 stage01 必须的（窗口不画也能 resize），但它是后续所有交互（键盘、鼠标、IME）的模板，值得现在就把骨架搭对。
>
> 本篇先不让事件"干实事"（干实事是 01g 的事，那时它驱动 DrawingSurface 重建缓冲），只让它**喊出来**——用一个打印型 observer 验证"消息→事件→observer"这条链通了。

## 这一篇结束你能看到什么

resize 窗口时，事件链跑通：`WindowsWindowProc` 把 `WM_SIZE` 翻译成 `ResizeEvent`、派给 observer，observer（一个打印型实现）把新尺寸**喊出来**。因为 stage01 是 Windows 子系统程序（没有控制台），我们用 `OutputDebugStringW` 喊——你在 VS Code 里 `F5` 调试运行，就能在"输出"面板看到那一行行 `[obs] resize → 960x600`。这一篇的交付物是"事件链验证通过"。

## 为什么要搞"事件层"这一层抽象

你可能会问：WndProc 收到 `WM_SIZE`，直接调 `DrawingSurface::resize` 不就完了，干嘛绕一道"事件"？两个理由。

第一，**解耦**。WndProc 是 platform 层、不该直接认识 `DrawingSurface`（那是 01f 才有的器官）。让它派一个抽象的"事件"出去，谁关心谁接（observer），WndProc 就不用知道下游是谁。

第二，**扩展**。现在只有 resize，但后面键盘（`WM_KEYDOWN`）、指针（`WM_POINTER`）、IME 全都要走这条路。如果每加一种消息就给 observer 加一个方法、给 WndProc 加一段硬编码分发，很快就乱。搞一层 `IEvent` + `onEvent`，新消息只要"造一种新事件、在 WndProc 里多一个 case"，observer 侧按 `event.type` 分派——结构统一、加起来便宜。

## 类一：`IEvent` / `ResizeEvent`（`include/mw/window_event.h`）

事件基类 + 第一个具体事件。基类用一个 `Type` 枚举标记"我是哪种事件"（枚举标签、不是虚函数分发，简单够用）：

```cpp
#pragma once
#include "mw/define.h"

namespace mw {
// 事件基类：靠 type 区分种类（现在只有 Resize，以后会有 Key / Pointer ...）。
struct IEvent {
    enum class Type { Resize } type;
    IEvent(Type t) : type(t) {}
};

// "窗口尺寸变了"。newSize 是新尺寸；oldSize 暂时没人填（需要的话后续补）。
struct ResizeEvent : IEvent {
    ResizeEvent() : IEvent(IEvent::Type::Resize) {}
    Size_t oldSize{};
    Size_t newSize{};
};
} // namespace mw
```

**思路：为什么用枚举标签、而不是虚函数多态？** 因为现在事件种类少、分发逻辑简单——observer 收到 `onEvent(e)` 后 `switch (e.type)` 一下，按需 `static_cast` 到具体子类取数据，比搞一套虚函数接口（`virtual void dispatch(Observer&)` 之类）轻得多。等以后事件种类多了、逻辑复杂了，再升级成 visitor 或虚函数也不迟——这层抽象现在以最小成本把"扩展点"留出来就行。

## 类二：`WindowsObserver`（`include/mw/window_observer.h`）

谁想接窗口事件，谁实现这个接口。它就一个方法 `onEvent`，所有事件统一从这儿进：

```cpp
#pragma once
#include "mw/window_event.h"

namespace mw {
// 窗口事件的接收者。WndProc 把消息翻成 IEvent 子类，统一从 onEvent 进来，
// 里面按 event.type 分派。具体谁来实现？stage01 里先来个"打印型"的验证链路（本篇），
// 01g 换成"驱动 DrawingSurface resize"的真 observer。
class WindowsObserver {
  public:
    virtual ~WindowsObserver() = default;
    virtual void onEvent(const IEvent& event) = 0;
};
} // namespace mw
```

`virtual ... = 0` 是纯虚——这是个接口，必须由具体类实现。`virtual ~WindowsObserver() = default` 是给多态基类配个虚析构（标准动作，免得子类析构走错）。

## 类三：给 `WindowsWindowProc` 加 WM_SIZE 分发

01d 那版 proc 只处理出生（`WM_NCCREATE`）和销毁（`WM_DESTROY`）。现在加一段：取出 observer、把 `WM_SIZE` 翻成 `ResizeEvent`、派给 observer。

```cpp
#include "mw/platform/windows_window_cb.h"
#include "mw/window_observer.h"
#include "mw/window_event.h"
#include <Windows.h>

namespace mw {

LRESULT CALLBACK WindowsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // 出生：存 observer 进 GWLP_USERDATA（01d 已有，就地 return 不贯穿）
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    // 销毁：退出消息循环（01d 已有）
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    // 01e 新增：其余消息，取出 observer，按消息造事件分发
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
```

**思路一：`WM_SIZE` 的新尺寸藏在 `LPARAM` 里。** Win32 把客户区的新宽高塞进 `LPARAM`——低字（`LOWORD`）是宽、高字（`HIWORD`）是高。我们把它俩掏出来填进 `ResizeEvent::newSize`。

**思路二：observer 可能为空。** 01d 的 main 传的是 nullptr observer；01e 这篇 main 才传真的。所以 `if (obs)` 守一道——没 observer 就不派（也安全，比如某些早期消息 observer 还没就位）。01g 接上真 observer 后这里就有活干了。

**思路三：扩展点长这样。** 以后加键盘：在 `window_event.h` 加个 `KeyEvent : IEvent { Type::Key, ... }`，在枚举里加 `Key`，在 proc 这个 `switch` 里加一个 `case WM_KEYDOWN`，造 `KeyEvent` 派给 observer。observer 侧 `switch(event.type)` 多一个 `case Key`。**没有任何结构变动，纯增量**——这就是事件层的价值。

## 组装：一个打印型 observer 验证链路

在 main 里写一个最小的 observer，收到 resize 就 `OutputDebugStringW` 喊出来：

```cpp
#include "mw/windows_factory.h"
#include "mw/window.h"
#include "mw/window_event.h"
#include "mw/window_observer.h"
#include <Windows.h>
#include <cstdio>

namespace {
// 打印型 observer：只为验证"消息→事件→observer"链路通不通。01g 换成驱动 DrawingSurface 的真货。
class PrintObserver : public mw::WindowsObserver {
    void onEvent(const mw::IEvent& e) override {
        if (e.type != mw::IEvent::Type::Resize) return;
        const auto& r = static_cast<const mw::ResizeEvent&>(e);
        wchar_t buf[128];
        swprintf(buf, 128, L"[obs] resize → %dx%d\n", r.newSize.width, r.newSize.height);
        OutputDebugStringW(buf);
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
    PrintObserver observer;                         // ← 真的 observer 了
    mw::WindowFactory factory;
    auto made = factory.setTitle(L"器官一·窗口与绘制面")
                       .setSize({960, 600})
                       .setPosition({CW_USEDEFAULT, 0})
                       .setObserver(&observer)      // ← 传进去（经 lpParam → GWLP_USERDATA）
                       .make();
    if (!made) {
        MessageBoxW(nullptr, made.error().error_message().c_str(), L"建窗失败", MB_ICONERROR);
        return 1;
    }
    auto win = std::move(*made);

    ShowWindow(win->native_handle(), show);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
```

注意 `observer` 必须比 `Window` 活得久——它是个栈上局部变量，生命周期一直撑到 `wWinMain` 结束（消息循环退出后才析构），而 WndProc 在这期间随时可能 `obs->onEvent(...)`，所以安全。

## 验证：`OutputDebugStringW` 怎么看

stage01 是 Windows 子程序，没有控制台，`printf` 看不到。`OutputDebugStringW` 的输出走调试通道：

- **VS Code 里 `F5` 调试运行** → 底部"输出"面板（或"调试控制台"）能看到 `[obs] resize → ...`。
- 或用微软的 **DebugView** 工具（独立小工具），能抓到所有进程的 `OutputDebugString`。

跑起来，拖动窗口边框改大小——每改一次，输出里就多一行 `[obs] resize → <新宽>x<新高>`。**事件链通了**：消息 → `WindowsWindowProc` → `ResizeEvent` → `observer->onEvent`。

## 踩坑清单

第一，**observer 比 Window 先死** → UAF。务必让 observer 的生命周期覆盖整个消息循环（放 `wWinMain` 栈上、或堆上活到 quit 后）。

第二，**`WM_SIZE` 的尺寸在 `LPARAM`、不是 `WPARAM`**。低字宽、高字高，别取反。

第三，**看不到 `OutputDebugString` 输出**——要么没在调试模式下跑（`F5` 不是 `Ctrl+F5`），要么没用 DebugView。它不是 `printf`，不走 stdout。

第四，**`IEvent` 的 `Type` 枚举忘加新值**——以后加 `KeyEvent` 时，枚举里没加 `Key`、`onEvent` 里 `switch` 没加 `case Key`，事件就被静默忽略了。

## 收尾与下一篇

事件层搭好了——WndProc 把消息翻译成事件、派给 observer，结构统一、扩展点是纯增量。打印型 observer 验证了链路通。但窗口现在还是**空的**（没画面），resize 喊出来的尺寸也还干不了实事。下一篇 **01f · 绘制面 DrawingSurface**——我们终于上 GPU：D3D11 device + DXGI 交换链 + D2D 1.1 + DirectComposition，把客户区涂成那帧深灰。我们 01f 见。

---

**相关资源**

- [`WM_SIZE` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-size)（新尺寸在 LPARAM 的低字/高字）
- [`OutputDebugStringW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-outputdebugstringw)（GUI 程序的调试输出通道）
- [DebugView — Microsoft Sysinternals](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview)（抓所有进程的 OutputDebugString）
