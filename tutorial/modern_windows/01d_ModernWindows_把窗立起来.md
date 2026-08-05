# 01d · 把窗立起来——从一扇空白窗到 GWLP_USERDATA

> 01c 画好了图纸（窗口类）、把 WndProc 钉了上去。这一篇按图纸**造一扇**：`CreateWindowExW`。但 Win32 窗口不是面向对象的——系统派消息时**只认 HWND、只调 WndProc**，它不认识我们的 `Window` 对象。所以本篇真正的难点不是"调一个 API 造窗"，而是**怎么把"句柄世界"接回"C++ 对象世界"**——靠的就是 `GWLP_USERDATA` 那套。
>
> 本篇是 stage01 里概念最密的一篇：三个类（`WindowsWindowProc` / `Window` / `WindowFactory`）+ 一个 Win32 不可绕开的把戏。

## 这一篇结束你能看到什么

**第一扇可见的窗**——960×600、标题"器官一·窗口与绘制面"。客户区是空的（还没画，那是 01f 的事），但窗真的立起来了、能拖动能关。这一篇的交付物就是"造窗这条链全闭合"。

## 先把两个 API 一次性理顺

造一扇窗 = 两个系统调用（01c 讲过，这里再压成两句）：

- **`RegisterClassExW`**（01c 已做）—— 登记"有这么一类窗口"，把 WndProc 钉在图纸的 `lpfnWndProc` 上。
- **`CreateWindowExW`**（本篇）—— 按图纸的**类名**造一扇实例，返回 HWND。

还有一个**反直觉但极关键**的点：`CreateWindowExW` 看着像"造完才返回"，其实**它在执行过程中会同步地给 WndProc 派消息**（`WM_NCCREATE`、`WM_CREATE` 等）——也就是说 HWND 还没交到你手里，WndProc 就已经在被调了。这带来本篇代码的写顺序：**必须先写好 WndProc、再调 `CreateWindowExW`**。

## 类一：`WindowsWindowProc`——WndProc 本体（住 platform 层）

### 这个东西是干什么的

它是**平台层的窗口过程**：系统把每条消息派给它，它负责"把 Win32 消息翻译成我们框架能理解的东西"。这一篇它的职责还很简单——出生时存一个指针、销毁时退出循环；01e 会给它加"把消息翻成事件、派给 observer"。

| 能力 | 干什么 |
|:---|:---|
| `WM_NCCREATE` | 出生第一条消息：把造窗时塞进来的"私货"指针存进 `GWLP_USERDATA` |
| `WM_DESTROY` | 窗口被销毁：`PostQuitMessage` 让消息循环退出 |
| 其余 | 交给 `DefWindowProcW` 系统默认处理 |

### 接口（`include/mw/platform/windows_window_cb.h`）

```cpp
#pragma once
#ifndef WIN32
#    error "windows_window_cb.h depends on Win32 (WNDPROC)."
#else
#    include <Windows.h>
namespace mw {
// 平台层的窗口过程。它认 WindowsObserver（mw 层），所以住在 mw 命名空间、
// 由 windows_factory 注册。（01d 这版只处理出生和销毁；01e 加事件分发。）
LRESULT CALLBACK WindowsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
} // namespace mw
#endif
```

### 核心思路（`src/platform/windows_window_cb.cpp`）

```cpp
#include "mw/platform/windows_window_cb.h"
#include <Windows.h>

namespace mw {

LRESULT CALLBACK WindowsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        // 出生第一条消息：把造窗时经 lpParam 塞进来的指针存进 GWLP_USERDATA。
        // ⚠️ 必须 return，不能贯穿——否则会一路掉进下面 WM_DESTROY 直接退程序。
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace mw
```

**思路一：`WM_NCCREATE` 是干什么的、为什么要在这儿存指针。** 这是窗口收到的**第一条**消息，在 `CreateWindowExW` 还没返回时就已经派发。我们在这里把造窗时塞进来的"私货"指针（01d 这版还没真用，留空；01e 塞 observer）存进 `GWLP_USERDATA`——这是每扇窗口**各自自带的一个指针槽**。之后任何消息进来，WndProc 都能从 HWND 的这个槽里取回"这扇窗对应的那个东西"。这就是把"句柄世界"接回"对象世界"的缝。

**思路二：贯穿 bug（教学点）。** 你可能手滑把 `WM_NCCREATE` 的 `return` 漏掉，写成"处理完接着往下走"——那它会**贯穿到 `WM_DESTROY`**，于是窗口刚出生就 `PostQuitMessage`，程序秒退。这种 bug 静默又致命，养成规矩：`switch` 的每个 `case` 要么 `return` 要么 `break`，`WM_NCCREATE` 这里尤其要**就地 return**。

**思路三：为什么 WndProc 是个 platform 自由函数，不是 `Window` 的成员？** 因为它要认识 `WindowsObserver`（框架类型，01e 接上），属于 `mw` 层的事；但它操作的 `HWND`/`GWLP_USERDATA` 是 Win32 机制。放在 `mw` 命名空间下、`platform/` 目录里的自由函数，层级最干净：platform 提供纯机制，WndProc 做翻译。而且——01c 讲过，WndProc 是**按类**的，一整个类共享一个，写成静态成员也行、自由函数也行，自由函数更轻。

## 类二：`Window`——一扇窗的 C++ 化身（被动持有 HWND）

### 这个东西是干什么的

`Window` 就是"一扇窗"在 C++ 里对应的对象，但它**很被动**——只持有那扇窗的 HWND（包在 `unique_hwnd` 里），并提供查尺寸/位置的接口。它**不管"怎么画"**（那是 01f 的 `DrawingSurface`），也**不直接处理消息**（那是 `WindowsWindowProc`）。它就是个"HWND 的 C++ 持有者"。

| 能力 | 方法 | 干什么 |
|:---|:---|:---|
| 拿窗口句柄 | `native_handle()` | 把 HWND 给 DrawingSurface / 消息分发用 |
| 查客户区尺寸 | `size()` | `GetClientRect` |
| 查窗口位置 | `position()` | `GetWindowRect` |

### 接口（`include/mw/window.h`）

```cpp
#pragma once
#include "define.h"
#include "platform/window_handle.h"
#include <memory>

namespace mw {
struct WindowFactory;   // fwd
class WindowsObserver;  // fwd

class Window {
  public:
    friend struct WindowFactory;        // 只有 factory 能构造 Window

    Size_t   size() const noexcept;     // 客户区尺寸
    Point_t  position() const noexcept; // 屏幕坐标位置

    mw::windows::native_window_handle_t native_handle() const {
        return native_handle_ ? native_handle_->native_handle() : mw::windows::native_window_handle_t{};
    }

  private:
    Window() = delete;                  // 不允许默认构造
    Window(std::unique_ptr<mw::windows::unique_hwnd> window);   // 只让 factory 调
    std::unique_ptr<mw::windows::unique_hwnd> native_handle_;
};
} // namespace mw
```

**思路：为什么 `Window` 这么被动？** 因为职责要单一。窗口类管"持有 HWND + 查几何"，消息处理交给 WndProc（平台），绘制交给 DrawingSurface（01f），事件回调交给 observer（01e）。把职责摊开，每个类一目了然——这正是这具骨架要教的东西。`friend WindowFactory` 是因为 `Window` 的构造函数私有了（强制"只能通过 factory 造窗"），靠友元放 factory 进来。

## 类三：`WindowFactory`——builder + 造窗

### 这个东西是干什么的

造窗有一堆参数（标题、尺寸、位置、还要触发注册、还要塞"私货"），用**builder 模式**串起来最顺：链式 `setTitle().setSize()...make()`。`make()` 干三件事——注册窗口类（01c 的函数）、`CreateWindowExW` 真造、把 HWND 包成 `Window`。

| 能力 | 方法 | 干什么 |
|:---|:---|:---|
| 设标题 | `setTitle(view)` | builder |
| 设尺寸 | `setSize(Size_t)` | builder |
| 设位置 | `setPosition(Point_t)` | builder |
| 设 observer | `setObserver(WindowsObserver*)` | builder（01e 才用，01d 留空） |
| 造 | `make()` | 注册 → `CreateWindowExW`（observer 当私货） → 包成 `Window` |

### 接口（`include/mw/windows_factory.h`）

```cpp
#pragma once
#include "define.h"
#include "platform/error.h"
#include "window.h"
#include <memory>
#include <string>
#include <string_view>

namespace mw {
class WindowsObserver; // fwd

struct WindowFactory {
    Result<std::unique_ptr<Window>> make();

    WindowFactory& setTitle(std::wstring_view window_title);
    WindowFactory& setSize(Size_t window_size);
    WindowFactory& setPosition(Point_t position);
    WindowFactory& setObserver(WindowsObserver* observer);

  private:
    std::wstring     title_    = L"ModernWindows";
    Size_t           size_     {960, 600};
    Point_t          pos_      {0, 0};
    WindowsObserver* observer_ = nullptr;
};
} // namespace mw
```

注意 builder 字段都是普通成员（不是 `static`），`make()` 用它们。`observer_` 默认 nullptr——01d 这篇 main 不调 `setObserver`，所以造窗时塞的"私货"是空；01e 才接真 observer。

### 核心思路（`src/windows_factory.cpp`）

```cpp
#include "mw/windows_factory.h"
#include "mw/platform/error.h"
#include "mw/platform/registry_window.h"
#include "mw/platform/windows_window_cb.h"
#include "mw/window.h"
#include <Windows.h>

namespace mw {

WindowFactory& WindowFactory::setTitle(std::wstring_view t)  { title_ = t;    return *this; }
WindowFactory& WindowFactory::setSize(Size_t s)              { size_ = s;     return *this; }
WindowFactory& WindowFactory::setPosition(Point_t p)         { pos_  = p;     return *this; }
WindowFactory& WindowFactory::setObserver(WindowsObserver* o){ observer_ = o; return *this; }

Result<std::unique_ptr<Window>> WindowFactory::make() {
    // 1) 注册窗口类（01c，幂等），把 WindowsWindowProc 钉在图纸上
    register_window_class(&WindowsWindowProc);

    // 2) 按图纸造一扇：WS_EX_NOREDIRECTIONBITMAP（焊死 GDI 的命门）+ observer 当 lpParam 私货
    HWND raw = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP,
        kMainWindowClassName,                       // 必须和注册时的类名一致
        title_.c_str(),
        WS_OVERLAPPEDWINDOW,
        pos_.x, pos_.y, size_.width, size_.height,
        nullptr, nullptr, GetModuleHandleW(nullptr),
        reinterpret_cast<LPVOID>(observer_));       // ← 进 WM_NCCREATE 的 lpCreateParams
    if (!raw) return from_win32();

    // 3) 包成 unique_hwnd → Window
    auto handle = std::make_unique<mw::windows::unique_hwnd>(raw);
    return std::unique_ptr<Window>(new Window(std::move(handle)));
}

} // namespace mw
```

**思路一：`WS_EX_NOREDIRECTIONBITMAP` 是整条渲染管线的命门。** 加它等于跟系统说"别给这扇窗准备 GDI 的重定向面了，我自己用 DirectComposition 提供内容"。代价是**这扇窗上 GDI 彻底画不出来**（01f 会展开）。这就是为什么咱们全程走 D3D/D2D、不碰 GDI。

**思路二：`lpParam` 是"私货"——HWND 世界和对象世界的唯一通道。** `CreateWindowExW` 的最后一个参数 `lpParam`，会被系统塞进 `WM_NCCREATE` 消息的 `CREATESTRUCTW.lpCreateParams` 里。我们把 `observer_` 指针塞进去，WndProc 在 `WM_NCCREATE` 一收到就把它存进 `GWLP_USERDATA`——之后任何消息都能从 `GWLP_USERDATA` 取回它。01d 这版 `observer_` 是 nullptr（还没真 observer），但这条"通道"先铺好，01e 直接用。

**思路三：造窗时 `Window` 对象还不存在，所以 `lpParam` 塞的是 observer、不是 `Window*`。** 这是经典的"先有鸡还是先有蛋"：`CreateWindowExW` 执行期间 WndProc 已经在跑，可我们正要拿它返回的 HWND 去**构造** `Window`——`Window` 这一刻还没诞生，没法把 `this` 塞进 `lpParam`。所以塞的是**早已存在的 observer**（它的生命周期由调用方管，比窗长），等窗造完再包成 `Window`。

**思路四：为什么返回 `Result<std::unique_ptr<Window>>`。** 注册和造窗都会失败，用 01b 的 `Result`/`from_win32` 把失败往上冒（值语义，不抛异常）。`unique_ptr` 是因为 `Window` 的构造函数私有（友元 factory 才能调），用 `unique_ptr` 把堆上的 `Window` 所有权交出去。

## 类四：`Window` 的实现（`src/window.cpp`）

构造函数 + 两个几何查询，都很直白：

```cpp
#include "mw/window.h"
#include <Windows.h>

namespace mw {
Window::Window(std::unique_ptr<mw::windows::unique_hwnd> window)
    : native_handle_(std::move(window)) {}

Size_t Window::size() const noexcept {
    RECT r{};
    if (native_handle_) GetClientRect(native_handle_->native_handle(), &r);
    return {static_cast<int32_t>(r.right), static_cast<int32_t>(r.bottom)};
}

Point_t Window::position() const noexcept {
    RECT r{};
    if (native_handle_) GetWindowRect(native_handle_->native_handle(), &r);
    return {static_cast<int32_t>(r.left), static_cast<int32_t>(r.top)};
}
} // namespace mw
```

## 组装：`main.cpp`（最小版，造一扇空窗）

把 factory 用起来，造窗、显示、跑消息循环。observer 这阶段不传（留空）：

```cpp
#include "mw/windows_factory.h"
#include "mw/window.h"
#include <Windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
    mw::WindowFactory factory;
    auto made = factory.setTitle(L"器官一·窗口与绘制面")
                       .setSize({960, 600})
                       .setPosition({CW_USEDEFAULT, 0})
                       .make();                 // observer 留空（nullptr）
    if (!made) {
        MessageBoxW(nullptr, made.error().error_message().c_str(), L"建窗失败", MB_ICONERROR);
        return 1;
    }
    auto win = std::move(*made);

    ShowWindow(win->native_handle(), show);

    // 消息循环——线程级的泵。stage02 会把它提炼进 Application + 协程恢复。
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
```

注意 `make()` 返回的是 `Result<unique_ptr<Window>>`，所以先 `if (!made)` 查错（错误串靠 01b 的 `error_message()`），再 `std::move(*made)` 把 `Window` 取出来。

## 接进工程 + 跑

`CMakeLists.txt` 把新的 `.cpp` 都加上：

```cmake
add_executable(stage01_window_surface
    main.cpp
    src/window.cpp
    src/windows_factory.cpp
    src/platform/error.cpp
    src/platform/registry_window.cpp
    src/platform/windows_window_cb.cpp)
target_include_directories(stage01_window_surface PRIVATE include)
target_link_libraries(stage01_window_surface PRIVATE d3d11 dxgi d2d1 dcomp)  # 01f 用
set_target_properties(stage01_window_surface PROPERTIES WIN32_EXECUTABLE TRUE)
```

```bash
cmake --build build --target stage01_window_surface
```

跑起来——**第一扇窗出现了**：960×600、标题"器官一·窗口与绘制面"。客户区是空的（没画），但能拖、能调整大小、能关。从"图纸"到"一扇真窗"，这条链通了。

## 踩坑清单

第一，**`WM_NCCREATE` 没 `return` → 贯穿到 `WM_DESTROY` → 秒退**。每个 `case` 都要 `return` 或 `break`。

第二，**忘加 `WS_EX_NOREDIRECTIONBITMAP`**。后面 01f 接 DComp 时内容会被一张占位的 GDI 重定向面挡住，画面诡异。

第三，**`kMainWindowClassName` 和 `CreateWindowExW` 的类名对不上**——`CreateWindowExW` 找不到图纸，直接失败。两边都引用 `registry_window.h` 里那个常量，别各写一份。

第四，**`make()` 忘了返回 `Result`**——造窗失败就没法优雅上报。全程 `Result<...>` + `from_win32()`。

## 收尾与下一篇

第一扇窗立起来了。`WindowsWindowProc` 的 `GWLP_USERDATA` 把"句柄世界"接回了"对象世界"，`WindowFactory` 把注册+造窗+包窗串成一条链。但窗口现在还"不会说话"——你 resize 它，`WindowsWindowProc` 收到 `WM_SIZE` 却什么都不干（我们还没处理）。下一篇 **01e · 事件层**：我们引入 `IEvent`/`ResizeEvent` + `WindowsObserver::onEvent`，让 WndProc 把 `WM_SIZE` 翻译成 `ResizeEvent` 派给 observer，先让 resize 在控制台"喊出来"，验证事件链通了。我们 01e 见。

---

**相关资源**

- [`CreateWindowExW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw)
- [`WM_NCCREATE` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccreate)（窗口收到的第一条消息）
- [`GWLP_USERDATA` / `SetWindowLongPtrW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw)（每窗口自带的"指针槽"）
- [`WS_EX_NOREDIRECTIONBITMAP` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)（焊死 GDI 出口的命门）
