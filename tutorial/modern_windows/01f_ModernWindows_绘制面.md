# 01f · 绘制面 DrawingSurface——把 GPU 管线挂上合成树

> 窗立起来了（01d）、事件也通了（01e），但客户区还是空的——因为还没人往屏幕上画东西。这一篇终于上 GPU：`DrawingSurface` 把 D3D11 device、DXGI 交换链、Direct2D 1.1 上下文、DirectComposition 视觉树串成一条管线，把客户区涂成那帧深灰。这是 stage01 真正"看见画面"的一步，也是整条现代 Win32 渲染管线概念最密的一篇。
>
> 本篇就一个类（`DrawingSurface`），但它内部藏着四个图形对象和一条"窗口 → 设备 → 交换链 → D2D 上下文 → 合成视觉树 → 系统"的链路。我们按职责、再按这条链一步步拆。

## 这一篇结束你能看到什么

客户区不再是空的——出现一帧 **深灰（≈ `#1B1E22`）**。这一篇的交付物是"画面出现了"。resize 时的平滑跟随要等 01g（接 observer），本篇只先把**静态那一帧**画出来。

## 先把一个心智模型立住：合成器"每帧主动采样"

老式画法（GDI `WM_PAINT`、或 Direct2D 老的 `HwndRenderTarget`）是"应用画一帧、系统显示一帧"——应用不画，屏幕就不变。

咱们走 DirectComposition 这条路，模型变了：你把 GPU 渲染出来的内容挂到一棵**常驻的"合成视觉树"**上，之后**系统合成器每个刷新周期都主动去"看"这棵树、重新合成屏幕**——也就是说，**就算你的应用一行绘制代码都不跑，合成器仍在持续刷新**（它只是采到同一帧）。这个"谁负责刷新"从应用挪到了系统，是后面动画、特效、多层合成都能挂在这棵树上的根。理解了这点，下面每一步你才知道它在配什么。

## `DrawingSurface` 这个东西是干什么的

**画布总管**——独占整条 GPU 管线，对外只有一个职责：**把"画一帧"这句话变成屏幕上的像素。** 你给它一句"清成深灰"，它内部协调 device → 交换链 → D2D 上下文 → DComp 树把帧推上去；窗口缩放了，它负责重建缓冲。

| 能力 | 方法 | 干什么 |
|:---|:---|:---|
| 建整条管线 | `create(hwnd)` | D3D11 device → DXGI 交换链 → D2D 1.1 上下文 → DComp 视觉树，全挂上这扇窗 |
| 绑后缓冲 | `bindTarget()`（私有） | 把交换链后缓冲包成 D2D 位图目标（首帧 & resize 后都调） |
| 画一帧 | `render()` | `BeginDraw` → `Clear` 深灰 → `EndDraw` → `Present` |
| 改大小 | `resize(w,h)` | 先松手 → `ResizeBuffers` → 重新绑目标 |

## 接口（`include/mw/drawing_src.h`）

```cpp
#pragma once
#include "mw/platform/comptr.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>   // IDXGISwapChain1 / CreateSwapChainForComposition
#include <d2d1_1.h>    // ID2D1Factory1 / Device / DeviceContext / Bitmap1
#include <dcomp.h>     // IDComposition*
#include <cstdint>

namespace mw {
class DrawingSurface {
  public:
    DrawingSurface() = default;
    DrawingSurface(const DrawingSurface&) = delete;
    DrawingSurface& operator=(const DrawingSurface&) = delete;

    HRESULT create(HWND hwnd);
    HRESULT resize(std::uint32_t w, std::uint32_t h);
    void    render();

  private:
    HRESULT bindTarget();

    HWND hwnd_ = nullptr;
    ComPtr<ID3D11Device>         d3d_;
    ComPtr<IDXGIDevice>          dxgi_dev_;
    ComPtr<IDXGISwapChain1>      swap_chain_;
    ComPtr<ID2D1Factory1>        d2d_factory_;
    ComPtr<ID2D1Device>          d2d_device_;
    ComPtr<ID2D1DeviceContext>   d2d_ctx_;
    ComPtr<ID2D1Bitmap1>         target_bitmap_;
    ComPtr<IDCompositionDevice>  dcomp_dev_;
    ComPtr<IDCompositionTarget>  dcomp_target_;
    ComPtr<IDCompositionVisual>  dcomp_visual_;
};
} // namespace mw
```

这堆 `ComPtr<IXxx>` 成员就是管线的四段：D3D（`d3d_`/`dxgi_dev_`）、交换链（`swap_chain_`）、D2D（`d2d_*`/`d2d_ctx_`/`target_bitmap_`）、DComp（`dcomp_*`）。`create()` 把它们一个个连起来。

## 核心思路（`src/drawing_src.cpp`）

`create()` 的链路是 **device → 交换链 → D2D 上下文 → DComp 树**。我们按这段顺序拆它的非显然点——代码就按这些思路长出来。

**思路一：D3D11 device，为什么是 11 不是 12。** 都这年头了为啥还用 D3D11？因为 **Direct2D/DirectWrite 只和 D3D11 互操作**——它们内部直接构建在 D3D11 之上，`ID2D1Device` 必须由一个 D3D11 device 派生。上 D3D12 就得额外走一层翻译桥，得不偿失。建 device 时有个硬 flag 不能漏：`BGRA_SUPPORT`，因为 D2D 整套资源默认 BGRA 字节序，没它后面建 D2D device 直接 `E_INVALIDARG`。

```cpp
UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
ComPtr<ID3D11DeviceContext> immCtx;
D3D_FEATURE_LEVEL fl{};
HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                               nullptr, 0, D3D11_SDK_VERSION, &d3d_, &fl, &immCtx);
if (FAILED(hr)) return hr;
hr = d3d_.As(&dxgi_dev_);   // D3D11 device 同时也是个 DXGI device，QI 出来
```

`d3d_.As(&dxgi_dev_)` 用的是 COM 的 `QueryInterface`——一个 COM 对象能实现多个接口，`As` 就是"问它要另一个接口"。D3D11 device 背后也是个 DXGI device，我们要出这"另一面"给 D2D/DComp 认卡用。

**思路二：DXGI 交换链，为什么 `ForComposition`、为什么 `FLIP_DISCARD`。** 交换链是 GPU 渲染结果通往屏幕的"传送带"。两个选择很关键。第一，用 `CreateSwapChainForComposition`（不是 `ForHwnd`）——后者把交换链直接绑 HWND、走系统托管旧路径；前者不绑 HWND、纯粹一段 GPU 纹理流，后面由我们手动挂到 DComp 视觉树，这才是我们要的解耦。第二，`FLIP_DISCARD` 翻转模型（不是老的位块传送）——直接把后缓冲指针"翻"给合成器，没拷贝，效率高得多。

```cpp
// 从 device 往上拿到 DXGI factory（注意得从 device 链路拿，别凭空建，避免适配器不一致）
ComPtr<IDXGIAdapter> adapter;
ComPtr<IDXGIFactory> factoryBase;
dxgi_dev_->GetParent(IID_PPV_ARGS(&adapter));
adapter->GetParent(IID_PPV_ARGS(&factoryBase));
ComPtr<IDXGIFactory2> factory;
factoryBase.As(&factory);

RECT rc{}; GetClientRect(hwnd_, &rc);
DXGI_SWAP_CHAIN_DESC1 desc{};
desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;   // D2D 默认格式，8 位 BGRA
desc.SampleDesc  = {1, 0};                        // 翻转模型不支持 MSAA
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.BufferCount = 2;
desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
desc.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED; // 给后面透明/圆角留路
desc.Scaling     = DXGI_SCALING_STRETCH;
hr = factory->CreateSwapChainForComposition(d3d_.Get(), &desc, nullptr, &swap_chain_);
```

⚠️ `Format` 必须是 8 位 BGRA；`AlphaMode` 直接上 `PREMULTIPLIED`——这一篇画不透明深灰用不上 alpha，但下一站做圆角/半透明时省得回头重建交换链（格式一改，D2D 位图、DComp 提交全得跟着重来）。

**思路三：D2D 1.1，从 Factory1 到 Device 到 DeviceContext。** 注意接口名带 `1`：`ID2D1Factory1`/`ID2D1DeviceContext` 是 Direct2D 1.1 的东西。老的那套 `ID2D1HwndRenderTarget` 直接绑 HWND、内部用 GDI 风格重定向面；新的 device context **完全不绑 HWND**，它绑"任意一块 DXGI surface"——这种灵活性正是 DComp 架构要的。链路是 `Factory1 → Device（由 DXGI device 喂）→ DeviceContext`：

```cpp
D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
d2d_factory_->CreateDevice(dxgi_dev_.Get(), &d2d_device_);
d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_ctx_);
const FLOAT dpi = static_cast<FLOAT>(GetDpiForWindow(hwnd_));
d2d_ctx_->SetDpi(dpi, dpi);
hr = bindTarget();   // 把交换链后缓冲包成 D2D 位图目标（见下）
```

**`bindTarget()` 干的事**：交换链的后缓冲是一块 `IDXGISurface`，D2D 没法直接画上去——得包成 `ID2D1Bitmap1`，再 `SetTarget` 给 device context。这段会被反复调（首帧、每次 resize 后），所以独立成方法：

```cpp
HRESULT DrawingSurface::bindTarget() {
    d2d_ctx_->SetTarget(nullptr);
    target_bitmap_.Reset();
    ComPtr<IDXGISurface> surface;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return hr;
    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX = bp.dpiY = 96.0f;
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    hr = d2d_ctx_->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &target_bitmap_);
    if (FAILED(hr)) return hr;
    d2d_ctx_->SetTarget(target_bitmap_.Get());
    return S_OK;
}
```

`OPTIONS_TARGET | CANNOT_DRAW` 是"当渲染目标、不当可采样纹理"的标准组合（避免拿渲染目标当输入纹理造成反馈循环），别漏。

**思路四：DirectComposition，把交换链挂上视觉树。** 到这儿交换链还在 GPU 里空转，没人告诉系统显示它。DirectComposition 是那个"夹具"：建合成器 device → 用 HWND 建 target → 建 visual → 把交换链当 visual 内容 → visual 设为 target 根 → **`Commit`**。

```cpp
DCompositionCreateDevice(dxgi_dev_.Get(), IID_PPV_ARGS(&dcomp_dev_));
dcomp_dev_->CreateTargetForHwnd(hwnd_, /*topmost=*/TRUE, &dcomp_target_);
dcomp_dev_->CreateVisual(&dcomp_visual_);
dcomp_visual_->SetContent(swap_chain_.Get());   // 直接传交换链，DComp 原生认识
dcomp_target_->SetRoot(dcomp_visual_.Get());
return dcomp_dev_->Commit();   // ⚠️ 漏调 Commit = 黑屏
```

⚠️ 两处跟"直觉"对着干。一是 `SetContent` **直接传交换链**就对（`IDXGISwapChain`），DComp 内部原生支持，会自己建缓冲共享——别被网上老文章带去走 `CreateSurfaceFromHandle`（那是"共享纹理句柄"另一条手工路）。二是 `Commit` **不能漏**——DComp 是"保留模式 + 批提交"，前面 `SetContent`/`SetRoot` 只改了内存里的树，直到 `Commit` 才一次性提交给系统生效。漏了就是经典的"代码不报错、窗口黑屏"。

**思路五：`render()`——画一帧。** 现在管线全挂上了，画一帧就极朴素：开绘制 → 清成深灰 → 结束 → present。

```cpp
void DrawingSurface::render() {
    if (!d2d_ctx_) return;
    d2d_ctx_->BeginDraw();
    d2d_ctx_->Clear(D2D1::ColorF(0.106f, 0.117f, 0.133f, 1.0f)); // ≈ #1B1E22 深灰
    HRESULT hr = d2d_ctx_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED) {
        // TODO(后续器官)：设备丢失 → 重建整条管线。本阶段先记着不处理。
    }
    swap_chain_->Present(1, 0);   // 1 = 等刷新同步
}
```

`BeginDraw`/`EndDraw` 是 D2D 的硬框架（外面调绘制方法会静默失败）；`Present(1,0)` 的 `1` 是"等下一个刷新再翻"，60fps 对齐的标准用法。

**思路六：`resize()`——先松手再换缓冲。** 交换链改缓冲大小前，必须先 `SetTarget(nullptr)` + 释放 D2D 对后缓冲的引用，否则 `ResizeBuffers` 会因后缓冲还被占着而返回 `DXGI_ERROR_INVALID_CALL`（而且报错信息不提 D2D，坑）：

```cpp
HRESULT DrawingSurface::resize(std::uint32_t w, std::uint32_t h) {
    if (!swap_chain_ || w == 0 || h == 0) return S_OK; // 未 create / 尺寸 0，跳过
    d2d_ctx_->SetTarget(nullptr);
    target_bitmap_.Reset();
    HRESULT hr = swap_chain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return hr;
    return bindTarget();   // 重建位图目标 + SetTarget
}
```

开头那个 `if (!swap_chain_)` 不是多余的——**建窗期间 `WM_SIZE` 会早到**（窗还没 create 绘制面），那时 `resize` 被调进来得能优雅跳过，不能崩。01g 接 observer 时这条守卫就派上用场。

## 组装：main 里建绘制面、画首帧

01e 的 main 加两步——建 `DrawingSurface`、`create` 之后画第一帧：

```cpp
mw::DrawingSurface surface;
// ... 01d/01e 那套造窗 ...
auto win = std::move(*made);

if (FAILED(surface.create(win->native_handle()))) {
    MessageBoxW(nullptr, L"DrawingSurface::create 失败", L"绘制面失败", MB_ICONERROR);
    return 1;
}
ShowWindow(win->native_handle(), show);
surface.render();   // 首帧：客户区终于不是空的了——深灰
// ... 消息循环 ...
```

把 `drawing_src.cpp` 加进 `CMakeLists.txt`（`d3d11 dxgi d2d1 dcomp` 01d 已链接好）：

```cmake
add_executable(stage01_window_surface
    main.cpp
    src/window.cpp
    src/windows_factory.cpp
    src/drawing_src.cpp               # ← 01f 加
    src/platform/error.cpp
    src/platform/registry_window.cpp
    src/platform/windows_window_cb.cpp)
```

## 配置、构建、验证

```bash
cmake --build build --target stage01_window_surface
```

跑起来——客户区**终于有内容了**：一帧干净的深灰（`#1B1E22`）。从空窗到"看见画面"，这条 GPU 管线通了。

⚠️ 这阶段 resize 还没接绘制面（那是 01g），所以你拖动窗口时画面会短暂错位/拉伸——正常的，下一篇接上 observer 就平滑了。

## 踩坑清单

第一，**`D3D11_CREATE_DEVICE_BGRA_SUPPORT` 漏加** → 后面 `CreateDevice(D2D)` 返回 `E_INVALIDARG`，你会以为 D2D factory 建错了。

第二，**`Commit` 忘调** → 黑屏，代码不报错。改完 DComp 树就 `Commit`，肌肉记忆。

第三，**`resize` 前没 `SetTarget(nullptr)` + `Reset`** → `ResizeBuffers` 报 `DXGI_ERROR_INVALID_CALL`，还不提 D2D。

第四，**`SetContent` 别走 `CreateSurfaceFromHandle`**——直接传交换链。

第五，**设备丢失没处理**（`D2DERR_RECREATE_TARGET`）——本阶段是 TODO，驱动崩溃恢复/热插拔显示器/远程桌面断开会触发。后续器官系统处理。

## 收尾与下一篇

画面出来了——`DrawingSurface` 把 D3D11/DXGI/D2D1.1/DComp 串成一条链，GPU 画的那帧深灰挂上合成树、被系统每帧采样到屏幕。只剩最后一步：**resize 时自动重建缓冲 + 重画**，让窗口缩放平滑跟随。那就是 01g 的活——把 01e 的 observer 从"打印型"换成"驱动 DrawingSurface"，stage01 就完整了。我们 01g 见。

---

**相关资源**

- [Introducing Direct2D 1.1 — Kenny Kerr](https://learn.microsoft.com/en-us/archive/msdn-magazine/2013/may/windows-with-c-introducing-direct2d-1-1)（D2D 1.1 device/context 模型权威导读）
- [DXGI flip model — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)（翻转模型 vs 位块传送）
- [DirectComposition 基础概念 — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directcomp/basic-concepts)
- [CreateSwapChainForComposition — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1-2/nf-dxgi1-2-idxgifactory2-createswapchainforcomposition)
- [The Care and Feeding of Modern Swap Chains — Chuck Walbourn](https://walbourn.github.io/care-and-feeding-of-modern-swapchains/)
