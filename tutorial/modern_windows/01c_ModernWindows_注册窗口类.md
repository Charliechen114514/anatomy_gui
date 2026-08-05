# 01c · 注册窗口类——把 WndProc 钉在图纸上

> 01b 的地基是"类型和胶水"，从这一篇起我们开始碰真正的 Win32 窗口。但 Win32 造一扇窗是**两步**：先 `RegisterClassExW` 登记"有这么一类窗口"、把它的消息交给哪个函数；再 `CreateWindowExW` 按这张图纸造实例。本篇只做第一步——登记。造窗是 01d 的事。
>
> 这一篇短，但藏着一个关键设计：怎么做到"全程注册一次、还不出现 `static`"。

## 这一篇结束你能看到什么

`registry_window` 就位——一个 `register_window_class(WNDPROC)` 函数，能幂等地把框架主窗口类登记进系统。编过、但还看不到窗（造窗的 `CreateWindowExW` 在 01d）。这一篇的交付物是"图纸画好了"。

## 先把概念理顺：图纸、WndProc、按类共享

Win32 的窗口**不是面向对象的**——它先让你登记一份"窗口类"（结构体 `WNDCLASSEXW`，注意它不是 C++ class，是一份模板），里面最关键的一项叫 `lpfnWndProc`：**告诉系统"这类窗口的消息来了，交给这个函数处理"**。那个函数就叫 **WndProc**。

这张"图纸"一个进程登记一次就行；后面想造几扇窗，就 `CreateWindowExW` 几次，每次都按这张图纸造——**所有用同一张图纸造出来的窗口，共用同一个 WndProc**。这是故意的（一个函数，省得到处复制），但带来一个后续要解决的问题（一个 WndProc 怎么区分"消息给哪扇窗"），那是 01d 的话题。本篇只管"把图纸登记好、把 WndProc 钉上去"。

## `registry_window` 这个东西是干什么的

它就干一件事：**用框架主窗口类的名字、把调用方给的 WndProc 登记进系统，且全程只登记一次。** 对外只露一个函数 + 那个类名常量。

| 能力 | 对外接口 | 干什么 |
|:---|:---|:---|
| 登记窗口类 | `register_window_class(WNDPROC proc)` | 把 `proc` 填进图纸的 `lpfnWndProc`，调 `RegisterClassExW`；已登记过就跳过 |
| 类名常量 | `kMainWindowClassName` | 图纸的名字，注册和造窗都靠它对上（`CreateWindowExW` 按名找图纸） |

### 接口（`include/mw/platform/registry_window.h`）

```cpp
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
```

注意 `kMainWindowClassName` 是 `inline constexpr`——头文件里的常量，多个翻译单元引用也不冲突（C++17 inline 变量）。registry 和 01d 的 factory 都用它，单一来源，不会写串。

### 核心思路（`src/platform/registry_window.cpp`）

```cpp
#include "mw/platform/registry_window.h"

namespace mw {

void register_window_class(WNDPROC proc) {
    // 先问 OS：这个类名注册过了吗？
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
```

这里有三个非显然的点，一个一个讲。

**思路一：怎么做到"全程只注册一次"，还一个 `static` 都不写？** 正常做法是用 `std::call_once` 或一个 `static bool` 标志——但 `static` 是隐式全局态，不踏实。这里有个更干净的招：**"注册过了吗"这件事，OS 自己就替你记着**——每进程有一张"已注册窗口类表"。我们用 `GetClassInfoExW(类名)` 问一句"这个类名在不在表里"，在就跳过，不在才 `RegisterClassExW`。整个函数**无状态**，连 `call_once` 都省了。

**思路二：`proc` 为什么是参数，而不是 registry 自己写死一个 WndProc？** 因为 WndProc（01d 的 `WindowsWindowProc`）住在 `mw` 层、要路由到 `WindowsObserver`（框架类型）；而 `registry_window` 在 `platform` 层，**不该反过来依赖 `mw`**（层级倒挂）。所以让调用方（01d 的 factory）把 `&WindowsWindowProc` 当参数传进来——registry 只管"给我哪个 proc 我就钉哪个"，不认识 observer 为何物。平台层保持纯机制、可复用。

**思路三：`cbSize` 必须填，`hbrBackground`/`style` 故意不填。** `cbSize = sizeof(wc)` 是系统判断结构体版本的依据，**漏了直接失败**。`hbrBackground`（GDI 画背景用的刷子）和 `style`（CS_HREDRAW 之类）都不填——咱们走 DComp 路线（`WS_EX_NOREDIRECTIONBITMAP` 焊死 GDI），不靠 GDI 画背景，填了反而多余。

## 接进工程

`src/platform/registry_window.cpp` 加进 stage01 的 `CMakeLists.txt`：

```cmake
add_executable(stage01_window_surface
    main.cpp
    src/platform/error.cpp
    src/platform/registry_window.cpp)   # ← 01c 加
```

## 配置、构建、验证

```bash
cmake --build build --target stage01_window_surface
```

编过即可——`register_window_class` 这阶段还没人调用（01d 的 factory 才会调它），所以这只是"把它编译进库里备好"。

## 踩坑清单

第一，**`cbSize` 忘填必挂**——`RegisterClassExW` 靠它判断版本，漏了直接返回失败。

第二，**类名写串**——`registry_window.cpp` 里的 `kMainWindowClassName` 和 01d `CreateWindowExW` 用的类名必须一字不差。咱们把名字放在 `registry_window.h` 里当 `inline constexpr`，registry 和 factory 都引用它，从根上避免各写一份。

第三，**别在 registry 里 `#include` 框架头**——`registry_window` 是平台层，保持只认 `WNDPROC` 这个类型，别让它依赖 `mw/window.h` 或 `mw/window_observer.h`。

## 收尾与下一篇

图纸画好了、WndProc 也准备好了（虽然 01c 还没写 WndProc 本体——那是 01d 的第一块）。下一篇 **01d · 把窗立起来**：我们写 `WindowsWindowProc`（那个真正的 WndProc）、`Window`（HWND 的 C++ 化身）、`WindowFactory`（按图纸造窗的 builder），最后跑出**第一扇可见的窗**（空白客户区，因为还没画）。我们 01d 见。

---

**相关资源**

- [`RegisterClassExW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw)
- [`GetClassInfoExW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclassinfoexw)（查询某类名是否已注册）
- [`WNDCLASSEXW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassexw)
