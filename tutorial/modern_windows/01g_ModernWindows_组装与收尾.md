# 01g · 组装与收尾——让 resize 平滑跟随，stage01 收官

> 01f 画面出来了，但 resize 还没接绘制面，拖窗口时画面会错位。这一篇是 stage01 的最后一步：把 01e 的 observer 从"打印型"换成"驱动 DrawingSurface"——resize 时自动重建缓冲 + 重画一帧。接完，stage01 的完整交付物就齐了：一扇深灰的窗、拖动平滑跟随、整条现代 Win32 渲染管线跑通。
>
> 本篇还有一个收尾动作：**commit**。stage01 是个天然里程碑，把它固化成一个 git 提交，再进 stage02。

## 这一篇结束你能看到什么

**stage01 的完整交付物**：960×600 深灰窗，**拖动 resize 平滑跟随、不闪白、不拉伸**——observer 收到 resize 事件 → `DrawingSurface::resize` 重建缓冲 → `render` 重画。然后一个 git commit 把它钉住。

## 最后一块拼图：`SurfaceObserver`

01e 的 `PrintObserver` 只是验证链路通。现在换个真能干活的——收到 resize 就调绘制面的 `resize` + `render`：

```cpp
#include "mw/windows_factory.h"
#include "mw/window.h"
#include "mw/drawing_src.h"
#include "mw/window_event.h"
#include "mw/window_observer.h"
#include <Windows.h>
#include <cstdint>

namespace {
// 把窗口 resize 事件接到 DrawingSurface：重建缓冲 + 重画一帧。
class SurfaceObserver : public mw::WindowsObserver {
    mw::DrawingSurface* surface_;
  public:
    explicit SurfaceObserver(mw::DrawingSurface* s) : surface_(s) {}
    void onEvent(const mw::IEvent& e) override {
        if (e.type != mw::IEvent::Type::Resize) return;
        const auto& r = static_cast<const mw::ResizeEvent&>(e);
        if (SUCCEEDED(surface_->resize(static_cast<std::uint32_t>(r.newSize.width),
                                       static_cast<std::uint32_t>(r.newSize.height)))) {
            surface_->render();   // 缓冲重建完，立刻补一帧
        }
    }
};
} // namespace
```

它持有一个 `DrawingSurface*`（不拥有，只引用），收到 `ResizeEvent` 就把新尺寸喂给 `surface_->resize`，成功后再 `render` 补一帧。就这么简单——01e 把链路铺好了，这儿只是把"打印"换成"干活"。

## 完整 main：注意那个"先有鸡还是先有蛋"

把所有东西组装起来。这里有个必须想清楚的顺序问题：

```cpp
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
    mw::DrawingSurface surface;        // ① 先有 surface 对象（还没 create，是空的）
    SurfaceObserver    observer(&surface);  // ② observer 持 surface*

    mw::WindowFactory factory;
    auto made = factory.setTitle(L"器官一·窗口与绘制面")
                       .setSize({960, 600})
                       .setPosition({CW_USEDEFAULT, 0})
                       .setObserver(&observer)   // ③ observer 当私货塞进造窗调用
                       .make();
    if (!made) {
        MessageBoxW(nullptr, made.error().error_message().c_str(), L"建窗失败", MB_ICONERROR);
        return 1;
    }
    auto win = std::move(*made);

    // ④ 有 HWND 了，才初始化绘制面
    if (FAILED(surface.create(win->native_handle()))) {
        MessageBoxW(nullptr, L"DrawingSurface::create 失败", L"绘制面失败", MB_ICONERROR);
        return 1;
    }

    ShowWindow(win->native_handle(), show);
    surface.render();   // ⑤ 首帧深灰

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
```

**那个鸡生蛋问题**：`DrawingSurface::create` 要 HWND，HWND 来自 `Window`，`Window` 由 `factory.make()` 造，而 `make()` 又要 observer（当 lpParam）。observer 持的是 `surface*`——可这时 `surface` 还没 `create`（HWND 还没有）。怎么破？

破法就两步：① `surface` 对象**先存在**（空的、没 create），observer 持它的指针；② `make()` 造窗期间 `WM_SIZE` 会**早到**，那时 observer 收到 resize → 调 `surface_->resize`——但 `resize` 开头有 `if (!swap_chain_) return S_OK;` 这道守卫（01f 留的），`surface` 还没 `create`，直接跳过，不崩。等 `make()` 返回、`surface.create(hwnd)` 跑完，后续的 resize 才真正重建缓冲。**先有对象、后填内容**，靠那道守卫兜住中间那段空窗期。

## 配置、构建、运行

`CMakeLists.txt` 到这就齐了（01f 已加 `drawing_src.cpp`，本篇 main 用同一个文件）：

```bash
cmake --build build --target stage01_window_surface
```

跑起来——**stage01 的完整交付物**：

- 一扇 960×600、标题"器官一·窗口与绘制面"的窗
- 客户区是那帧深灰（`#1B1E22`）
- **拖动边角 resize：背景平滑跟随、不闪一下白、不拉伸糊掉**——flip model + DComp 合成，observer 驱动重建+重画，三者合力

从序章的"为什么造框架"，到一扇真能看见、能平滑 resize 的 GPU 之窗——器官一（窗口与绘制面）完整地立住了。

## 收尾：commit 这个里程碑

整个 `src/tutorial/modern_windows/` 到目前为止还没一个 commit。stage01 是个天然快照点，把它钉住：

```bash
cd <repo>
git add src/tutorial/modern_windows/00_bootstrap \
        src/tutorial/modern_windows/CMakeLists.txt \
        src/tutorial/modern_windows/CMakePresets.json \
        src/tutorial/modern_windows/.vscode \
        src/tutorial/modern_windows/stage01_window_surface
git status   # 确认要提交的内容
git commit -m "feat(modern_windows): stage01 —— 窗口与绘制面

- 工具链：CMake + Ninja + clangd（cmake-vs.cmd 注入 vcvars64 + 真 ninja）
- 平台层：error/Result、ComPtr、unique_hwnd、registry_window、WindowsWindowProc
- 框架：Window（被动 HWND 持有者）、WindowFactory（builder+make）、
  IEvent/ResizeEvent、WindowsObserver（事件层）
- 绘制面：DrawingSurface（D3D11 + DXGI flip + D2D 1.1 + DirectComposition）
- 交付物：一扇深灰的窗，resize 平滑跟随"
```

（commit 信息你按自己的习惯调；重点是给 stage01 一个明确的快照点，后面 stage02 出问题随时能 diff 回来。）

## stage01 全景回顾

七篇走完，回头看我们到底造了什么：

| 篇 | 器官/层 | 核心产物 |
|:--|:--|:--|
| 01a | 工具链骨架 | CMake+Ninja+clangd，`00_bootstrap` 自检 |
| 01b | 平台地基（L0） | `define`/`error`/`comptr`/`unique_hwnd`（+ deleter 对照表） |
| 01c | 注册窗口类 | `registry_window`（GetClassInfoExW 幂等、零 static） |
| 01d | 把窗立起来 | `WindowsWindowProc` + `Window` + `WindowFactory`，第一扇空窗 |
| 01e | 事件层 | `IEvent`/`ResizeEvent` + `WindowsObserver::onEvent`，链路验证 |
| 01f | 绘制面 | `DrawingSurface`，深灰帧 |
| 01g | 组装收尾 | `SurfaceObserver` + 完整 main，resize 平滑 + commit |

整条链闭合了：`WindowFactory::make`（注册 → `CreateWindowExW` 带 observer）→ `DrawingSurface::create`（D3D/DXGI/D2D/DComp）→ `render` 画灰帧 → 运行时 `WM_SIZE → ResizeEvent → observer → surface.resize+render`。`WS_EX_NOREDIRECTIONBITMAP` 焊死 GDI、合成器每帧主动采样、事件层解耦扩展——这就是现代 Win32 GUI 的地基。

## 下一站：stage02（器官二·消息循环与协程）

stage01 的消息循环还是最原始的 `while(GetMessage)`——它只会被动等系统消息，没法做定时刷新、没法等多个事件源、更没法和协程配合。stage02 我们就爆改它：

- 把它升级成**多源调度器**（`MsgWaitForMultipleObjectsEx`，能同时盯原生消息 + 投递任务 + 协程恢复 + 句柄等待）
- 把 **C++20 协程**焊上去——`coroutine_handle::address()` 经 `PostMessage` 投给 message-only HWND，在 UI 线程恢复，让框架第一次"会喘气"

那是一个全新的器官（事件循环 + 异步），也是这系列相对 stage01 的第一条 headline。先歇口气，stage01 这座山翻过去了——我们 stage02 见。

---

**相关资源**

- [DirectComposition · composition 视觉树](https://learn.microsoft.com/en-us/windows/win32/directcomp/basic-concepts)
- [DXGI flip model — 为什么 resize 不闪白](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
- [Git · 提交](https://git-scm.com/docs/git-commit)（给里程碑打快照）
