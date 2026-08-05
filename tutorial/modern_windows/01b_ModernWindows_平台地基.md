# 01b · 平台地基——几何、错误、COM、HWND 的 RAII

> 01a 把解剖台搭好了，从这一篇起开始往 `stage01_window_surface/` 里落代码。但在写任何"窗口逻辑"之前，得先铺一层**平台胶水**：几何类型、错误模型、COM 指针、以及 HWND 的 RAII 包装。这一层没有任何 D3D，但后面所有器官都踩在它上面——它不稳，上面全晃。
>
> 本篇的四个小东西都很短，但 `unique_hwnd` 那张"句柄 deleter 对照表"值得专门记，是 Win32 编程最容易出血的地方。

## 这一篇结束你能看到什么

`stage01_window_surface/` 工程建起来，平台地基四个文件（`define.h` / `error.h`+`.cpp` / `comptr.h` / `window_handle.h`）编进一个能跑的 exe（main 暂时是空壳）。还没有窗——那是 01d 的事。这一篇的交付物是"地基就位、能编过"。

## 先把 stage01 工程挂上顶层

先在顶层 `CMakeLists.txt` 里把 stage01 串进来（01a 里预留的那行解开）：

```cmake
add_subdirectory(00_bootstrap)
add_subdirectory(stage01_window_surface)   # ← 01b 起加这一行
```

stage01 自己的 `CMakeLists.txt` 先放最简版本（main 空壳 + 平台层一个 `.cpp`），后面 01c–01g 会往里加源文件：

```cmake
# stage01_window_surface/CMakeLists.txt
add_executable(stage01_window_surface
    main.cpp
    src/platform/error.cpp)
target_include_directories(stage01_window_surface PRIVATE include)
set_target_properties(stage01_window_surface PROPERTIES WIN32_EXECUTABLE TRUE)
```

`WIN32_EXECUTABLE TRUE` 告诉链接器入口是 `wWinMain`（不是控制台 `main`）。`main.cpp` 这阶段先放个空壳，能编过就行：

```cpp
// stage01_window_surface/main.cpp —— 01b 阶段的空壳，01d 起往里填
#include <Windows.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) { return 0; }
```

目录结构现在长这样（本篇只加标★的四个文件）：

```
stage01_window_surface/
├─ CMakeLists.txt
├─ include/mw/
│  ├─ define.h            ★ 几何类型
│  ├─ window.h            （01d 才建）
│  └─ platform/
│     ├─ error.h          ★ 错误模型
│     ├─ comptr.h         ★ COM 指针别名
│     └─ window_handle.h  ★ HWND 的 RAII
├─ src/
│  └─ platform/error.cpp  ★ error_message() 实现
└─ main.cpp               （空壳）
```

下面一个文件一个文件讲。

## `define.h`——几何类型（Size_t / Point_t）

先把"尺寸"和"坐标"这两个全系列到处用的小东西钉死。用 Concepts 把模板参数约束成整数，免得有人塞个 `float` 进来：

```cpp
#pragma once
#include <concepts>

namespace mw {
template <typename Step = int32_t>
    requires std::integral<Step>
struct Point { Step x; Step y; };

template <typename Step = int32_t>
    requires std::integral<Step>
struct Size { Step width; Step height; };

using Point_t = Point<>;
using Size_t  = Size<>;     // 默认 int32_t
} // namespace mw
```

后面所有"宽高""坐标"都用 `Size_t` / `Point_t`，统一一套词。Concepts 那行 `requires std::integral<Step>` 是顺手用上 C++20——有人手滑传 `Size<float>` 直接编不过，比运行时才发现强。

## `comptr.h`——COM 指针别名

Windows 的图形 API（D3D/D2D/DComp）全是 COM 的——每个对象内部带引用计数，用完得手动 `Release()`。手动管在异常和提前 `return` 面前极其脆弱，所以用 WRL 的 `ComPtr`（RAII，析构自动 `Release`）。我们给它起个短别名：

```cpp
#pragma once
#include <wrl/client.h>

namespace mw {
template <typename Src> using ComPtr = Microsoft::WRL::ComPtr<Src>;
} // namespace mw
```

就一行 `using`，但它来自 Windows SDK 自带的 WRL，**不用装任何第三方依赖**。后面 `DrawingSurface`（01f）里那一堆 `ComPtr<ID3D11Device>` 全靠它。

## `error.h`——错误模型（Error / Result / from_hresult / from_win32）

这是本系列最重要的一层胶水。COM 和 DirectX 的 API 几乎全都返回一个 32 位的 `HRESULT` 错误码，Win32 那套则靠 `GetLastError()`——两套错误来源散落在代码里是 C 时代的混乱。我们用 `std::expected<T, Error>` 把它们统一收进一个词汇：

```cpp
#pragma once
#include <expected>
#include <string>
#ifndef _WIN32
#    error "error.h depends on FormatMessageW (Windows-only)."
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mw {
// 整个框架唯一的错误类型，三种失败味道：COM/DirectX 的 HRESULT、
// 经典 Win32 的 GetLastError 码、以及"不该发生"的内部 panic。
struct Error {
    enum class Kind { HResult, Win32, Panic } kind;
    unsigned long error_code;

    // "[Kind] 0xCODE: <OS 的本地化描述>"。实现放在 error.cpp，
    // 这样本头文件对 <Windows.h> 的依赖尽量轻。
    std::wstring error_message() const;
};

template <typename T> using Result = std::expected<T, Error>;

inline std::unexpected<Error> from_hresult(HRESULT hr) {
    return std::unexpected(Error{Error::Kind::HResult, static_cast<unsigned long>(hr)});
}
inline std::unexpected<Error> from_win32(DWORD e = GetLastError()) {
    return std::unexpected(Error{Error::Kind::Win32, static_cast<unsigned long>(e)});
}
} // namespace mw
```

为什么要 `std::expected` 而不是异常？因为 COM/Win32 的世界用异常是灾难（跨 ABI 边界、和 `HRESULT` 语义打架），而 `expected` 是值语义——调用点必须显式处理错误，错了一条链都看得见。后面你会反复看到这种写法：

```cpp
if (FAILED(hr)) return from_hresult(hr);   // 一行把 HRESULT 翻成 Error 往上传
```

`error_message()` 的实现放在 `error.cpp`，干的事是把错误码喂给 `FormatMessageW`——让 OS 把它存的本地化文案拿出来（比如 `0x80070057` → "参数错误"）：

```cpp
#include "mw/platform/error.h"
#include <format>

namespace mw {
namespace {
// 把一个数字状态码翻成 OS 里登记的本地化句子。
std::wstring system_message(unsigned long code) {
    constexpr DWORD kBufCch = 512;
    wchar_t buf[kBufCch] = {};
    const DWORD len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        buf, kBufCch, nullptr);
    if (len == 0) return {};                       // 这个码没登记描述
    std::wstring text(buf, len);
    // 系统消息自带尾部的 "\r\n"，削掉好拼成干净的一行
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        text.pop_back();
    return text;
}
} // namespace

std::wstring Error::error_message() const {
    const wchar_t* k = kind == Kind::HResult ? L"HRESULT"
                     : kind == Kind::Win32   ? L"Win32" : L"Panic";
    std::wstring detail = kind == Kind::Panic ? L"internal panic" : system_message(error_code);
    if (detail.empty()) detail = L"<no system description>";
    return std::format(L"[{}] 0x{:08X}: {}", k, error_code, detail);
}
} // namespace mw
```

这个 `error_message()` 在 stage01 里现在还看不出威力，但等 01d 建窗失败时弹个 `MessageBox`，那行可读的错误串就是它给的。

## `window_handle.h`——`unique_hwnd`（HWND 的 RAII，+ deleter 对照表）

最后一块、也是本篇最要紧的一块：HWND 的 RAII 包装。HWND 是 Windows 给每扇窗口发的"不透明编号"，它**不是指针**——不能 `delete`，关窗得调专门的 `DestroyWindow`。这种"创建用一套、释放用另一套"的句柄，正是 RAII 最该包的东西：

```cpp
#pragma once
#ifndef WIN32
#    error "Error including a Win32 header on a non-Win32 platform."
#else
#    include <Windows.h>

namespace mw::windows {
using native_window_handle_t = HWND;

class unique_hwnd {
  public:
    unique_hwnd(native_window_handle_t handle) : handle_(handle) {}
    ~unique_hwnd() { release_self(); }

    unique_hwnd(const unique_hwnd&)            = delete;
    unique_hwnd& operator=(const unique_hwnd&) = delete;
    unique_hwnd(unique_hwnd&& o) : handle_(o.handle_) { o.handle_ = nullptr; }
    unique_hwnd& operator=(unique_hwnd&& o) noexcept {
        if (this != &o) { release_self(); handle_ = o.handle_; o.handle_ = nullptr; }
        return *this;
    }

    [[nodiscard]] native_window_handle_t native_handle() const noexcept { return handle_; }
    void reset(native_window_handle_t handle) {
        if (handle == handle_) return;
        release_self();
        handle_ = handle;
    }
  private:
    void release_self() { if (handle_) DestroyWindow(handle_); }
    native_window_handle_t handle_{nullptr};
};
} // namespace mw::windows
#endif
```

它就是个"析构自动 `DestroyWindow`"的智能句柄，思路和 `std::unique_ptr` 一模一样（不可拷贝、可移动、析构释放）。

但这里要专门拎出来讲的，不是 `unique_hwnd` 本身，而是它背后的**一课**：**RAII 的 deleter 必须匹配句柄家族**。Windows 里不同句柄的释放函数完全不同，张冠李戴就是泄漏或 double-free。这张对照表你贴墙上——后面每接一个新句柄类型都要回头核对该走哪一行：

| 句柄类型 | 怎么来的 | 正确的释放 | 错配的后果 |
|---------|---------|-----------|-----------|
| `HWND` | `CreateWindowExW` | `DestroyWindow` | `CloseHandle` 关不掉，HWND 不归内核对象管 |
| `HBRUSH`/`HPEN`/`HFONT` | `CreateSolidBrush` 等 | `DeleteObject` | 泄漏 GDI 对象，吃满后整进程画不出来 |
| `HDC`（窗口/屏幕） | `GetDC` / `BeginPaint` | `ReleaseDC` / `EndPaint` | 两者释放路径分裂，错配泄漏或破坏 PAINTSTRUCT |
| `HDC`（自建） | `CreateCompatibleDC` | `DeleteDC` | 跟 `ReleaseDC` 是两套，错配 double-free |
| `HANDLE`（内核对象） | `CreateFileW` 等 | `CloseHandle` | 这才是 `CloseHandle` 的地盘 |

`unique_hwnd` 帮我们把 `HWND` 那一行做对了；后面遇到 `HBRUSH`、`HDC`，照葫芦画瓢各写一个 RAII（或者直接用微软的 wil 库，省事）。咱们 stage01 后面走 DComp 路线，几乎碰不到 GDI 句柄，所以暂时一个 `unique_hwnd` 就够。

## 配置、构建、验证

```bash
cmake --preset default
cmake --build build --target stage01_window_surface
```

能编过、链出 `stage01_window_surface.exe`（跑起来什么都不发生——main 是空壳，`wWinMain` 直接返回 0）就成。地基铺好了。

## 踩坑清单

第一，**`/utf-8` 漏了**——本篇代码里的中文注释会在某些机器上编成乱码。01a 在顶层 `add_compile_options(/utf-8)` 已经钉死，确认那条还在。

第二，**`error.h` 里 `#include <Windows.h>` 的位置**——它必须在内联函数 `from_hresult`/`from_win32` 用到 `HRESULT`/`DWORD` 之前，别手滑挪到后面。

第三，**RAII deleter 张冠李戴**——别用 `CloseHandle` 去"关" HWND，那是给内核对象（`CreateFile` 那类）准备的，HWND 走 `DestroyWindow`。

## 收尾与下一篇

平台地基铺好了：几何类型、错误模型、COM 指针、HWND 的 RAII。后面所有器官都踩在它上面。下一篇 **01c · 注册窗口类**——我们写第一个真正"碰 Win32 窗口"的代码：`registry_window`，把一类窗口登记进系统，把消息处理函数 `WndProc` 钉在"图纸"上。我们 01c 见。

---

**相关资源**

- [`std::expected` — cppreference](https://en.cppreference.com/w/cpp/utility/expected)（C++23 错误传递的标准方式）
- [`FormatMessageW` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessagew)（把错误码翻成本地化文案）
- [microsoft/wrl · ComPtr](https://learn.microsoft.com/en-us/cpp/windows/client-templates/comptr-class)（COM 对象的 RAII 智能指针）
