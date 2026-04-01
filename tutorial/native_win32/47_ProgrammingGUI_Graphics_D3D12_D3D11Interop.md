# 通用GUI编程技术——图形渲染实战（四十七）——D3D12与D3D11互操作及选型建议

> 上一篇我们拆解了 D3D12 的描述符堆和根签名——描述符是 GPU 端的"指针"，根签名定义了 Shader 如何找到这些指针，两者配合构成了 D3D12 资源绑定模型的完整图景。到此为止，我们已经掌握了 D3D12 的核心基础设施：命令系统、资源管理、描述符绑定。但在实际项目中，你几乎不可能只使用 D3D12——你可能需要用 Direct2D 画 UI 文字，或者在一个已有的 D3D11 项目中逐步引入 D3D12。微软提供了 D3D11On12 互操作层来支持这种"混合使用"的场景，今天我们就来拆解它。

## 前言：为什么需要互操作

说句实话，如果你做一个纯 3D 游戏引擎，D3D12 完全够用，不需要任何互操作。但在 GUI 编程的场景下，情况不太一样。你可能已经有一个基于 D3D11 或 Direct2D 的 UI 渲染框架（文字渲染、矢量图形、控件绘制），现在想引入 D3D12 来处理一些高性能的 3D 渲染任务（比如在 D3D12 中渲染一个 3D 预览窗口），同时保留原有的 2D UI 不变。

或者更常见的场景：你想在 D3D12 的渲染帧上叠加 Direct2D 绘制的 UI 元素（文字、按钮、进度条等）。Direct2D 在 Windows 上是通过 D3D11 的设备来创建渲染目标的，它不能直接在 D3D12 的命令队列上工作。所以你需要一个桥接层——D3D11On12 就是这个桥接。

根据 [Microsoft Learn - D3D11On12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-11-on-12) 的官方描述，D3D11On12 互操作层允许你在 D3D12 的命令队列上使用 D3D11 的接口和运行时。这意味着你可以在同一个渲染管线中混用 D3D12 和 D3D11 的 API 调用，而不需要维护两套完全独立的渲染设备。

## 环境说明

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v143 工具集)
- **Windows SDK**: 10.0.26100 或更高版本
- **依赖**: `d3d12.h`、`d3d11.h`、`d2d1.h`、`dwrite.h`
- **链接库**: `d3d12.lib`、`d3d11.lib`、`d2d1.lib`、`dwrite.lib`
- **前置知识**: 文章 44（命令队列）、文章 46（描述符堆与根签名）

## D3D11On12 的架构原理

在深入代码之前，我们先理解一下 D3D11On12 在底层是怎么工作的。

当你通过 `D3D11On12CreateDevice` 创建了一个互操作设备后，你得到了一个 D3D11 的设备对象（`ID3D11Device`）和一个 D3D11 的设备上下文（`ID3D11DeviceContext`）。这些 D3D11 对象在底层并不是独立的——它们实际上是 D3D12 设备和命令队列的"代理"。当你通过 D3D11 的接口提交渲染命令时，D3D11On12 层会把这些命令翻译成 D3D12 的命令，然后在你的 D3D12 命令队列上执行。

你可以把这个过程类比为"翻译官"——D3D11On12 把 D3D11 的 API 调用翻译成 D3D12 的命令，然后通过你的 D3D12 命令队列提交给 GPU。这样，D3D12 和 D3D11 的命令就共享同一条 GPU 执行通道，资源的共享也变得自然而然。

但这个翻译是有开销的。D3D11On12 层需要维护 D3D11 的状态跟踪、资源管理等功能，这些在原生 D3D12 中是不需要的。所以互操作不适合高性能的渲染路径——如果你需要对每帧数百万个三角形做 D3D12 渲染，然后又通过 D3D11On12 层做额外的处理，翻译开销可能成为瓶颈。

## 第一步——创建互操作设备

创建互操作设备的核心函数是 `D3D11On12CreateDevice`。根据 [Microsoft Learn - D3D11On12CreateDevice](https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nf-d3d11on12-d3d11on12createdevice) 的文档，它的参数列表如下：

```cpp
#include <d3d11on12.h>
#pragma comment(lib, "d3d11.lib")

// 假设你已经有了 D3D12 设备和命令队列
// ComPtr<ID3D12Device> g_d3d12Device;
// ComPtr<ID3D12CommandQueue> g_commandQueue;

ComPtr<ID3D11Device> g_d3d11Device;
ComPtr<ID3D11DeviceContext> g_d3d11Context;
ComPtr<ID3D11On12Device> g_d3d11On12Device;

HRESULT hr = D3D11On12CreateDevice(
    g_d3d12Device.Get(),           // D3D12 设备
    D3D11_CREATE_DEVICE_FLAG_NONE, // D3D11 创建标志
    nullptr,                       // Feature Levels（NULL = 默认）
    0,                             // Feature Levels 数量
    reinterpret_cast<IUnknown**>(&g_commandQueue),  // 命令队列数组
    1,                             // 命令队列数量
    0,                             // 节点掩码（单 GPU = 0）
    &g_d3d11Device,                // 输出 D3D11 设备
    &g_d3d11Context,               // 输出 D3D11 设备上下文
    nullptr                        // 返回的 Feature Level（可选）
);

if (FAILED(hr))
{
    // 互操作设备创建失败
    return false;
}

// 查询 D3D11On12 接口
g_d3d11Device.As(&g_d3d11On12Device);
```

`D3D11On12CreateDevice` 的关键参数是第一个（D3D12 设备）和第五个（命令队列数组）。它们建立了 D3D11 到 D3D12 的桥接关系——所有通过这个 D3D11 设备提交的命令，最终都会被翻译后在指定的 D3D12 命令队列上执行。

创建成功后，我们通过 `As`（COM 的 `QueryInterface`）获取了 `ID3D11On12Device` 接口。这个接口提供了 D3D11 和 D3D12 资源之间的"包装"和"解包"操作——后面马上会用到。

## 第二步——包装 D3D12 资源给 D3D11 使用

D3D12 的渲染目标（交换链的后台缓冲区）是 `ID3D12Resource` 对象，D3D11 不认识它。我们需要通过 `ID3D11On12Device::CreateWrappedResource` 把 D3D12 资源"包装"成 D3D11 可以使用的资源：

```cpp
// 为每个后台缓冲区创建 D3D11 包装资源
ComPtr<ID3D11Resource> g_wrappedResources[2];

for (UINT i = 0; i < 2; i++)
{
    D3D11_RESOURCE_FLAGS resourceFlags = {};
    resourceFlags.BindFlags = D3D11_BIND_RENDER_TARGET;

    hr = g_d3d11On12Device->CreateWrappedResource(
        g_renderTargets[i].Get(),       // D3D12 资源
        &resourceFlags,                  // D3D11 资源标志
        D3D12_RESOURCE_STATE_RENDER_TARGET,  // D3D12 输入状态
        D3D12_RESOURCE_STATE_PRESENT,        // D3D12 输出状态
        nullptr,
        IID_PPV_ARGS(&g_wrappedResources[i])
    );

    if (FAILED(hr))
    {
        // 包装资源创建失败
        return false;
    }
}
```

`CreateWrappedResource` 的几个关键参数值得展开说说。`D3D11_RESOURCE_FLAGS` 定义了这个资源在 D3D11 中的用途——`D3D11_BIND_RENDER_TARGET` 表示它作为 D3D11 的渲染目标使用。第三个参数（`D3D12_RESOURCE_STATE_RENDER_TARGET`）是 D3D11 获得资源控制权时，D3D12 端资源应该处于的状态。第四个参数（`D3D12_RESOURCE_STATE_PRESENT`）是 D3D11 释放资源控制权后，D3D12 端资源应该转换到的状态。

这两个状态参数本质上是在告诉 D3D11On12 层："当 D3D11 要用这个资源时，自动把它转到 RENDER_TARGET 状态；当 D3D11 用完了，自动把它转回 PRESENT 状态。"

## 第三步——创建 D2D 渲染目标

有了包装后的 D3D11 资源，我们就可以按照标准的 D2D + D3D11 集成流程来创建 Direct2D 的渲染目标了。首先从 D3D11 设备创建 D2D 设备：

```cpp
#include <d2d1_3.h>
#pragma comment(lib, "d2d1.lib")

ComPtr<ID2D1Factory3> g_d2dFactory;
ComPtr<ID2D1Device2> g_d2dDevice;
ComPtr<ID2D1DeviceContext2> g_d2dContext;

// 创建 D2D 工厂
D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);

// 从 D3D11 设备获取 DXGI 设备，创建 D2D 设备
ComPtr<IDXGIDevice> dxgiDevice;
g_d3d11Device.As(&dxgiDevice);

g_d2dFactory->CreateDevice(dxgiDevice.Get(), &g_d2dDevice);
g_d2dDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_d2dContext);
```

然后为每个后台缓冲区创建 D2D 的位图渲染目标：

```cpp
ComPtr<ID2D1Bitmap1> g_d2dRenderTargets[2];

D2D1_BITMAP_PROPERTIES1 bitmapProps = {};
bitmapProps.bitmapOptions =
    D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
bitmapProps.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
bitmapProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

for (UINT i = 0; i < 2; i++)
{
    ComPtr<IDXGISurface> surface;
    g_wrappedResources[i].As(&surface);

    g_d2dContext->CreateBitmapFromDxgiSurface(
        surface.Get(), &bitmapProps, &g_d2dRenderTargets[i]);
}
```

到这里，我们已经建立了一条完整的桥接链路：D3D12 资源 → D3D11 包装资源 → DXGI Surface → D2D 位图渲染目标。D2D 可以直接在 D3D12 的后台缓冲区上绘制 2D 图形了。

## 第四步——D2D 绘制 UI 的完整流程

渲染一帧的流程分为三个阶段：先用 D3D12 渲染 3D 场景，然后用 D2D 绘制 2D UI，最后 Present。

关键操作是 `AcquireWrappedResources` 和 `ReleaseWrappedResources`——它们分别获取和释放 D3D11 对 D3D12 资源的控制权：

```cpp
void RenderFrame()
{
    UINT frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // ========== 阶段 1：D3D12 渲染 3D 场景 ==========
    // ... 录制 D3D12 命令（渲染 3D 场景到后台缓冲区） ...
    // ... ResourceBarrier: RENDER_TARGET → PRESENT（给 D2D 用）...

    // 等待 D3D12 命令完成
    WaitForGPU();

    // ========== 阶段 2：D2D 绘制 UI ==========
    // 获取包装资源的控制权（自动将资源转到 RENDER_TARGET 状态）
    ID3D11Resource* ppResources[] = { g_wrappedResources[frameIndex].Get() };
    g_d3d11On12Device->AcquireWrappedResources(ppResources, 1);

    // 设置 D2D 渲染目标
    g_d2dContext->SetTarget(g_d2dRenderTargets[frameIndex].Get());

    // 开始 D2D 绘制
    g_d2dContext->BeginDraw();

    // 绘制 UI 元素（文字、按钮、进度条等）
    g_d2dContext->Clear(D2D1::ColorF(0, 0));  // 清除为全透明（保留 D3D12 的渲染结果）

    // 示例：绘制一段文字
    ComPtr<ID2D1SolidColorBrush> pBrush;
    g_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), &pBrush);

    ComPtr<IDWriteTextFormat> pTextFormat;
    // ... 创建文字格式 ...

    g_d2dContext->DrawTextW(
        L"D3D12 + D2D Interop",
        18,
        pTextFormat.Get(),
        D2D1::RectF(10, 10, 400, 50),
        pBrush.Get()
    );

    // 结束 D2D 绘制
    g_d2dContext->EndDraw();

    // 释放包装资源的控制权（自动将资源转回 PRESENT 状态）
    g_d3d11On12Device->ReleaseWrappedResources(ppResources, 1);

    // 刷新 D3D11 命令（确保 D2D 的绘制被提交到 D3D12 命令队列）
    g_d3d11Context->Flush();

    // ========== 阶段 3：Present ==========
    g_swapChain->Present(1, 0);
}
```

这段代码中有几个关键步骤值得仔细理解。

`AcquireWrappedResources` 把 D3D12 资源的控制权交给 D3D11。它会自动在 D3D12 命令队列上插入一个资源屏障，把资源从我们在 `CreateWrappedResource` 时指定的"输出状态"（`PRESENT`）转换到"输入状态"（`RENDER_TARGET`）。这样 D2D 就可以在资源上绘制了。

`ReleaseWrappedResources` 把控制权还回 D3D12。它会在 D3D12 命令队列上插入反向的屏障，把资源从 `RENDER_TARGET` 转回 `PRESENT`。

`g_d3d11Context->Flush()` 确保 D3D11 端所有待处理的命令都被提交到 D3D12 命令队列。没有这一步，D2D 的绘制可能还留在 D3D11 的内部缓冲区中，Present 时看不到。

⚠️ 这里有一个非常容易踩的坑：`AcquireWrappedResources` 和 `ReleaseWrappedResources` 必须成对调用。如果你只 Acquire 了但忘了 Release，资源就永远处于 D3D11 控制下的 RENDER_TARGET 状态，D3D12 在 Present 时会因为状态不对而出问题。反过来，如果你 Release 了但没 Acquire，D3D11 会在没有控制权的情况下尝试操作资源，导致未定义行为。

## 互操作的同步问题

上面的流程中有一个隐含的同步问题：D3D12 渲染和 D2D 渲染之间必须正确同步。D2D 必须等 D3D12 渲染完成后才能开始在同一个后台缓冲区上绘制。

在我们的简化示例中，通过 `WaitForGPU()` 来同步——CPU 等待 GPU 执行完 D3D12 的命令后，再开始 D2D 绘制。这是一个安全但不太高效的方案，因为它引入了一次 CPU-GPU 同步。

在实际项目中，更高效的做法是使用 Fence 来做更细粒度的同步——D3D12 渲染完成后 Signal 一个 Fence 值，D2D 绘制前等待这个 Fence 值。或者更简单地，利用 D3D11On12 内部的隐式同步机制（`AcquireWrappedResources` 内部会自动等待关联的 D3D12 命令完成），但要注意这可能导致 CPU 侧的额外等待。

## 选型建议：D3D11 vs D3D12

讨论了这么多互操作的细节，一个自然的问题是：我到底应该用 D3D11 还是 D3D12？

### 应用/工具类项目 → D3D11 足够

如果你的项目是一个工具软件、编辑器界面、数据可视化应用，D3D11（配合 Direct2D）完全足够。这类项目的渲染负载通常不高（几十到几百个 Draw Call），D3D11 的驱动开销不会成为瓶颈。D3D11 的 API 更简洁，开发效率更高，调试也更方便。

典型场景包括：图片编辑器的预览窗口、CAD 软件的 2D 视图、音频可视化工具、简单的图表应用。

### 引擎/高性能渲染 → D3D12

如果你在开发一个游戏引擎、实时渲染引擎、或者需要处理数万 Draw Call 的高性能应用，D3D12 的显式控制可以带来显著的 CPU 端性能提升。多线程命令录制、显式资源状态管理、手动同步控制——这些特性在 Draw Call 数量极大的时候会体现出真正的价值。

典型场景包括：游戏引擎、GPU 粒子系统、大规模场景渲染、需要 Compute Shader 做复杂计算的应用。

### 学习/教学 → D3D11 先行

如果你是在学习图形编程，建议从 D3D11 开始。D3D11 的 API 更接近"图形管线"本身——你关注的是顶点着色器、像素着色器、纹理采样、混合这些渲染概念。D3D12 的额外复杂度（命令管理、资源状态、描述符）会分散你对渲染核心概念的理解。

掌握了 D3D11 的渲染管线后，再学 D3D12 就只需要理解"基础设施的变化"——渲染概念是一样的，只是管理方式从隐式变成了显式。

### 迁移/混合 → 互操作

如果你有一个已有的 D3D11 项目想逐步迁移到 D3D12，或者需要在 D3D12 项目中保留 D3D11/D2D 的 UI 渲染，D3D11On12 互操作层就是你的过渡方案。它允许你在同一个渲染管线中混用两种 API，按模块逐步迁移，而不是一次性重写。

## 常见问题

### D3D11On12CreateDevice 失败

最常见的失败原因是 D3D12 设备不支持互操作。确保你的 D3D12 设备是在支持 D3D12 的 GPU 上创建的（不是 WARP 软件光栅化器），并且 Windows 版本支持 D3D11On12（Windows 10 1607 及以上）。

### D2D 绘制内容看不到

检查你是否在 D2D 绘制前调用了 `AcquireWrappedResources`，绘制后调用了 `ReleaseWrappedResources`，以及最后是否调用了 `g_d3d11Context->Flush()`。三个调用缺一不可。

### 画面闪烁或撕裂

可能是 D3D12 渲染和 D2D 绘制之间的同步不正确。确保 D3D12 渲染完成后（Fence 信号到达）再开始 D2D 绘制。如果你省略了同步步骤，D2D 可能会在 D3D12 还在写入后台缓冲区的时候就开始绘制，导致画面混乱。

### 性能比纯 D3D11 还差

这通常是因为互操作的同步开销过大。每次 `AcquireWrappedResources` 和 `ReleaseWrappedResources` 都涉及 CPU-GPU 同步，如果频繁调用（比如每一帧都创建新的包装资源），开销会累积。建议在初始化时创建好所有包装资源，在渲染循环中只做 Acquire/Release 操作。

## 总结

这篇我们拆解了 D3D12 与 D3D11 互操作的完整机制。

D3D11On12 互操作层本质上是一个"翻译官"——它把 D3D11 的 API 调用翻译成 D3D12 的命令，通过 D3D12 命令队列提交给 GPU。核心流程包括：通过 `D3D11On12CreateDevice` 创建互操作设备，通过 `CreateWrappedResource` 把 D3D12 资源包装成 D3D11 可用的资源，在渲染时通过 `AcquireWrappedResources`/`ReleaseWrappedResources` 管理资源的控制权切换。

我们还讨论了 D3D11 和 D3D12 的选型建议——应用/工具用 D3D11，引擎/高性能用 D3D12，学习用 D3D11 先行，迁移/混合用互操作。选型没有绝对的对错，关键是根据项目需求做出合理的权衡。

到此为止，我们的 D3D12 部分告一段落。接下来，我们把视线从 GPU 加速的 3D 渲染拉回到 Win32 的控件世界——下一篇要聊的是 Owner-Draw 控件：如何利用 `WM_DRAWITEM` 消息让系统控件（ListBox、ComboBox、Button）焕然一新。

---

## 练习

1. 创建一个 D3D12 项目，使用 D3D11On12 互操作层在 D3D12 渲染的 3D 场景上叠加 D2D 绘制的文字和半透明矩形 UI。确保 Acquire/Release 配对正确。

2. 研究 ImGui 的 D3D11 和 D3D12 后端实现（[GitHub - ocornut/imgui](https://github.com/ocornut/imgui)）。阅读 `imgui_impl_dx11.cpp` 和 `imgui_impl_dx12.cpp` 的源码，写一段文字总结两者在资源管理和渲染流程上的主要差异。

3. 实验互操作的性能影响：分别测量纯 D3D12 渲染和 D3D12+D2D 互操作渲染的帧时间，对比引入互操作后的额外开销。提示：使用 `QueryPerformanceCounter` 测量每帧耗时。

4. 尝试在互操作场景中正确使用 Fence 进行同步，替代简单的 `WaitForGPU` 方案。思考：在什么情况下 Fence 同步比直接等待更高效？

---

**参考资料**:
- [Direct3D 11 on 12 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-11-on-12)
- [D3D11On12CreateDevice function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nf-d3d11on12-d3d11on12createdevice)
- [ID3D11On12Device interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11on12/nn-d3d11on12-id3d11on12device)
- [D2D1DeviceContext - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/nn-d2d1_1-id2d1devicecontext)
- [D3D12 and D2D interop sample - Microsoft Learn](https://learn.microsoft.com/en-us/samples/microsoft/directx-graphics-samples/d3d1211on12win32/)
- [Working with Direct3D 11 and Direct2D - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/devices-and-device-contexts)
