# 通用GUI编程技术——图形渲染实战（四十四）——D3D12命令列表、队列与围栏：GPU同步核心

> 上回我们把 D3D12 的设计哲学理了一遍，并且写出了最小骨架程序——创建了设备、命令队列、交换链、RTV 描述符堆、命令分配器和命令列表。但那个骨架只是把工具摆在桌面上，还没真正用起来。这篇我们就来深入命令的录制、提交和同步机制，这是 D3D12 运行时的核心骨架，理解了它，后面所有渲染操作才有根基。

## 前言：为什么命令管理这么重要

在 D3D11 中，命令管理几乎是不存在的概念——你调用 `Draw`，GPU 就执行；你调用 `ClearRenderTargetView`，画面就被清除。中间发生了什么、命令怎么到 GPU 的、执行完了没有——这些你都看不到也不用管。驱动帮你把一切打理得井井有条。

但 D3D12 不一样。命令的录制、提交、执行、同步，每一步都要求你亲自过问。这听起来很麻烦，但它给了你一个极其强大的能力：你可以精确控制 CPU 和 GPU 之间的协作关系。当 CPU 在录制第 N+1 帧的命令时，GPU 可以同时在执行第 N 帧的命令——这种 CPU/GPU 并行是 D3D12 性能优势的根本来源。

不过这个并行是有代价的。如果你在 GPU 还没执行完上一帧命令的时候，就去重写下一帧要用的命令内存，会发生什么？答案是：灾难性的数据竞争，轻则画面错误，重则驱动崩溃。所以同步机制——确保"CPU 不在 GPU 还在用的时候回收资源"——是 D3D12 编程中不可回避的核心话题。

## 环境说明

本篇延续上一篇的环境配置：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v143 工具集)
- **Windows SDK**: 10.0.26100 或更高版本
- **调试工具**: D3D12 调试层（务必在开发阶段启用）
- **依赖**: `d3d12.h`、`dxgi1_4.h`、`d3dx12.h`（辅助头文件）

如果你还没有准备好 D3D12 的开发环境，请先完成上篇的骨架代码，确保能成功编译运行。本篇的代码在上篇骨架的基础上扩展。

## CommandAllocator：命令的内存池

我们先从最底层开始。`ID3D12CommandAllocator` 是命令的内存分配器，它管理着一段用于存储 GPU 命令的内存空间。你可以把它理解为一个内存池——每次你在命令列表上录制一条命令（比如 `DrawInstanced`、`CopyBufferRegion`、`ResourceBarrier`），这条命令的数据就会被写入到命令分配器所管理的内存中。

创建命令分配器的方式很简单：

```cpp
ComPtr<ID3D12CommandAllocator> commandAllocator;

HRESULT hr = device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(&commandAllocator)
);
```

`D3D12_COMMAND_LIST_TYPE_DIRECT` 告诉设备我们要创建一个直接命令类型的分配器。这个类型决定了分配器中存储的命令最终由哪种命令队列来执行——直接命令由 3D 图形队列执行，计算命令由计算队列执行，拷贝命令由拷贝队列执行。在我们的系列中，绝大多数时候用的都是 `DIRECT` 类型。

命令分配器有一个非常关键的特性：它管理的内存是不可复用的。什么意思呢？当你录制了一批命令、提交给 GPU 执行后，分配器中那块内存并不会自动释放。你必须手动调用 `Reset()` 来告诉底层"这块内存可以回收利用了"。

```cpp
hr = commandAllocator->Reset();
```

但这里有一个致命的约束：**在调用 Reset 之前，必须确保 GPU 已经执行完所有使用该分配器录制的命令**。如果你在 GPU 还在执行命令的时候调用了 Reset，调试层会立刻报错，而且可能导致未定义行为。

根据 [Microsoft Learn - ID3D12CommandAllocator::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset) 的官方描述：你的应用程序调用 `Reset` 来复用与命令分配器关联的内存。从这次调用开始，运行时和驱动假设与该分配器关联的图形命令不再需要。

这就是为什么同步机制如此重要——我们需要一种方式来确认"GPU 已经执行完了"，然后才能安全地 Reset 命令分配器。这个机制就是后面要讲的围栏（Fence）。

### 命令分配器不能"复用"的含义

"不可复用"这个词可能会引起误解。准确地说，命令分配器是"可复用"的——你可以通过 `Reset()` 清空它，然后在下一帧重新用它来录制命令。但它不可"同时共享"——同一个分配器不能同时关联多个正在录制的命令列表。如果你想并行录制多个命令列表，每个命令列表必须有自己独立的分配器。

另外值得一提的是，一个命令分配器同一时间只能关联一个处于"打开"状态的命令列表。你不能让两个命令列表同时使用同一个分配器——这会导致命令内存交错，结果不可预测。

## GraphicsCommandList：命令的录制接口

`ID3D12GraphicsCommandList` 是你接触最多的接口。它负责录制所有的 GPU 命令：清屏、设置视口、设置管线状态、设置顶点缓冲、绘制、拷贝资源、插入屏障……所有的图形操作都是通过它录制的。

创建命令列表的代码如下：

```cpp
ComPtr<ID3D12GraphicsCommandList> commandList;

hr = device->CreateCommandList(
    0,                                  // 节点掩码
    D3D12_COMMAND_LIST_TYPE_DIRECT,      // 命令列表类型
    commandAllocator.Get(),              // 关联的命令分配器
    nullptr,                            // 初始管线状态
    IID_PPV_ARGS(&commandList)
);
```

创建后，命令列表处于"打开"状态，可以立即开始录制。当你录完所有命令后，调用 `Close()` 封闭它：

```cpp
commandList->Close();
```

根据 [Microsoft Learn - ID3D12GraphicsCommandList::Close](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close) 的描述，运行时会验证命令列表是否没有被重复关闭，同时检查录制过程中是否遇到了错误——如果有，Close 会返回对应的错误码。

关闭后的命令列表不能再录制任何命令，除非你对它调用 `Reset()`。这里有一个非常重要的细节：命令列表的 `Reset` 和命令分配器的 `Reset` 是两个不同的操作。命令列表的 `Reset` 只是把命令列表重新初始化到"打开"状态，让你可以重新录制命令，它几乎没有任何开销。而命令分配器的 `Reset` 则是真正回收底层命令内存，有前述的 GPU 完成约束。

```cpp
// 重置命令列表（几乎零开销）
commandList->Reset(commandAllocator.Get(), nullptr);
```

你可能会问：为什么要用 Reset 而不是每次都创建新的命令列表？答案很简单——对象创建是有开销的。命令列表本身是一个 COM 对象，创建和销毁都会涉及内核态调用。而 Reset 只是在用户态重置一些内部状态，速度快了几个数量级。所以正确的模式是：在初始化时创建命令列表，在每帧的开头 Reset 命令列表来重新录制。

### 命令录制的典型模式

一个典型的 D3D12 渲染帧的命令录制模式如下：

```cpp
// 1. 重置命令分配器（确保 GPU 已完成！）
commandAllocator->Reset();

// 2. 重置命令列表，重新开始录制
commandList->Reset(commandAllocator.Get(), nullptr);

// 3. 录制命令
// ... 设置视口、设置管线状态、绘制 ...

// 4. 封闭命令列表
commandList->Close();
```

这个模式会在每一帧中重复。步骤 1 是重量级操作（回收内存，需要 GPU 确认），步骤 2 是轻量级操作（重置状态），步骤 3 和 4 是正常的命令录制流程。

## CommandQueue：命令的提交通道

命令列表录制好之后，它只是躺在 CPU 端的内存里，GPU 还什么都不知道。你需要通过 `ID3D12CommandQueue` 把命令列表提交给 GPU 执行。

```cpp
ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
commandQueue->ExecuteCommandLists(
    1,                // 命令列表数量
    ppCommandLists    // 命令列表数组
);
```

`ExecuteCommandLists` 是一个异步操作——它把命令列表提交到 GPU 的执行队列后立刻返回，CPU 不会等待 GPU 执行完毕。根据 [Microsoft Learn - ID3D12CommandQueue::ExecuteCommandLists](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists) 的文档，当你一次提交多个命令列表时，驱动可能会将它们合并执行以提高效率。

这就是 D3D12 性能优势的关键——CPU 提交命令后可以立刻去做别的事情（比如录制下一帧的命令），GPU 在后台慢慢执行。但这也带来了同步的需求——你需要知道 GPU 什么时候执行完了，才能安全地回收资源。

### 执行顺序的保证

`ExecuteCommandLists` 对提交顺序有明确的保证：如果你在一次调用中提交了多个命令列表（比如 [A, B, C]），GPU 会按照 A -> B -> C 的顺序执行它们。如果你分三次调用，分别提交 A、B、C，在同一条命令队列上它们仍然会按照提交顺序执行。

但是，如果你有多条不同类型的命令队列（比如一条图形队列和一条计算队列），不同队列之间的执行顺序是不确定的。你需要通过同步原语来协调跨队列的执行顺序——这个话题我们在实际需要的时候再展开。

## Fence：CPU-GPU 同步的基石

围栏（Fence）是 D3D12 中 CPU 和 GPU 同步的核心机制。你可以把它理解为一个共享的计数器——GPU 端可以递增这个计数器（通过 `Signal`），CPU 端可以等待这个计数器到达某个期望的值（通过 `SetEventOnCompletion`）。

创建围栏的代码如下：

```cpp
ComPtr<ID3D12Fence> fence;

HRESULT hr = device->CreateFence(
    0,                                  // 初始值
    D3D12_FENCE_FLAG_NONE,              // 标志
    IID_PPV_ARGS(&fence)
);

UINT64 fenceValue = 0;  // 围栏的当前值
```

围栏的核心用法包含两个操作：`Signal` 和 `SetEventOnCompletion`。

### Signal：GPU 端打标记

当你提交了一批命令给 GPU 之后，可以在命令队列上调用 `Signal`，让 GPU 在执行完当前所有命令后，将围栏的值递增到指定的值。

```cpp
fenceValue++;
commandQueue->Signal(fence.Get(), fenceValue);
```

`Signal` 不等于"立刻执行"——它是在 GPU 的命令队列中插入一个特殊的标记。当 GPU 执行到这个标记时，它会把围栏的值设置为 `fenceValue`。所以如果在此之前你提交了一堆绘制命令，Signal 标记会在所有绘制命令执行完毕后才被处理。

### SetEventOnCompletion：CPU 端等待

当你需要在 CPU 端等待 GPU 完成时，需要用到 Windows 事件对象（Event）配合围栏的 `SetEventOnCompletion`。

```cpp
HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

// 等待 GPU 到达某个围栏值
void WaitForFence(ID3D12Fence* fence, UINT64 targetValue, HANDLE event)
{
    if (fence->GetCompletedValue() < targetValue)
    {
        fence->SetEventOnCompletion(targetValue, event);
        WaitForSingleObject(event, INFINITE);
    }
}
```

这里有一个重要的优化：我们先通过 `GetCompletedValue()` 检查 GPU 是否已经到达了目标值。如果已经到达，就不需要进入等待——这是一个零开销的快速路径。只有在 GPU 还没完成时，我们才设置事件并等待。

根据 [Microsoft Learn - ID3D12Fence::SetEventOnCompletion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion) 的文档，`SetEventOnCompletion` 指定了一个事件对象，当围栏到达指定值时该事件会被触发。这是一个高效的等待机制——CPU 线程在等待期间会被挂起，不会消耗 CPU 时间。

## 帧同步的标准模式

把上面的概念组合起来，我们就得到了 D3D12 帧同步的标准模式。这个模式几乎出现在所有 D3D12 程序中，是必须掌握的基础。

### 单帧同步的基本流程

一个完整的帧同步流程包含以下步骤：

```cpp
// ========== 渲染循环 ==========
void RenderFrame()
{
    // 1. 录制命令
    commandAllocator->Reset();   // 回收上一帧的命令内存
    commandList->Reset(commandAllocator.Get(), nullptr);

    // ... 录制所有渲染命令 ...

    commandList->Close();

    // 2. 提交命令
    ID3D12CommandList* ppCmdLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, ppCmdLists);

    // 3. 发出信号
    fenceValue++;
    commandQueue->Signal(fence.Get(), fenceValue);

    // 4. 提交交换链 Present
    swapChain->Present(1, 0);

    // 5. 等待 GPU 完成（简单实现）
    WaitForFence(fence.Get(), fenceValue, fenceEvent);
}
```

这个流程是正确的，但它有一个问题：每一帧结束时 CPU 都要等待 GPU 完成，这意味着 CPU 和 GPU 完全是串行工作的——GPU 执行第 N 帧的时候 CPU 在干等，CPU 录制第 N+1 帧的时候 GPU 在干等。性能和 D3D11 没什么区别。

### 多帧并行的正确做法

真正高效的做法是允许 CPU 领先 GPU 若干帧。CPU 不需要等 GPU 执行完当前帧才去录制下一帧——它可以提前录制好几帧的命令。但这里有一个关键约束：每一帧需要自己的命令分配器，因为命令分配器在被 Reset 之前必须确保 GPU 已经执行完了其中所有的命令。

```cpp
const UINT FRAME_COUNT = 2;  // 帧数 = 2 表示 CPU 最多领先 GPU 1 帧

struct FrameResources
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    UINT64 fenceValue = 0;  // 该帧提交时的围栏目标值
};

FrameResources frameResources[FRAME_COUNT];
UINT currentFrameIndex = 0;

void InitFrameResources()
{
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        g_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&frameResources[i].commandAllocator));
    }
}

void RenderFrame()
{
    UINT frameIndex = currentFrameIndex % FRAME_COUNT;

    // 等待该帧的 GPU 工作完成
    // （因为我们要复用这个帧的命令分配器）
    WaitForFence(fence.Get(),
                 frameResources[frameIndex].fenceValue,
                 fenceEvent);

    // 重置这个帧的命令分配器（现在安全了）
    frameResources[frameIndex].commandAllocator->Reset();

    // 重置命令列表
    commandList->Reset(
        frameResources[frameIndex].commandAllocator.Get(), nullptr);

    // ... 录制命令 ...
    commandList->Close();

    ID3D12CommandList* ppCmdLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, ppCmdLists);

    // 记录新的围栏值
    fenceValue++;
    frameResources[frameIndex].fenceValue = fenceValue;
    commandQueue->Signal(fence.Get(), fenceValue);

    swapChain->Present(1, 0);

    currentFrameIndex++;
}
```

这个模式的核心思想是：CPU 不等待"上一帧"完成，而是等待" FRAME_COUNT 帧前的那一帧"完成。如果 FRAME_COUNT = 2，CPU 最多领先 GPU 1 帧——当 CPU 准备录制第 3 帧时，它需要等待第 1 帧完成（因为第 1 帧和第 3 帧共享同一个命令分配器）。但如果 GPU 够快，第 1 帧早就完成了，CPU 根本不需要实际等待。

这就是 D3D12 的"多帧并行"——CPU 和 GPU 可以同时工作，吞吐量大幅提升。

⚠️ 注意一个常见的陷阱：很多初学者会忘记为每帧使用独立的命令分配器。他们只有一个分配器，在帧开始时 Reset 它——但如果 GPU 还在执行上一帧的命令，Reset 就会触发调试层错误。记住这个规则：**每一帧的命令分配器必须是独立的，不能跨帧共享。**

## 完整示例：清屏动画 + 正确帧同步

现在我们把上面所有的概念整合到一个完整的可运行程序中。这个程序会在黑色背景上画一个来回移动的彩色条纹——虽然很简单，但它展示了正确的命令录制、提交和同步流程。

```cpp
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cmath>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// ========== 帧同步常量 ==========
const UINT FRAME_COUNT = 2;

// ========== 全局资源 ==========
ComPtr<ID3D12Device>              g_device;
ComPtr<ID3D12CommandQueue>        g_commandQueue;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<IDXGISwapChain3>           g_swapChain;
ComPtr<ID3D12DescriptorHeap>      g_rtvHeap;
ComPtr<ID3D12Resource>            g_renderTargets[FRAME_COUNT];
ComPtr<ID3D12Fence>               g_fence;

// 每帧独立的资源
ComPtr<ID3D12CommandAllocator>    g_commandAllocators[FRAME_COUNT];
UINT64                            g_fenceValues[FRAME_COUNT] = {};
UINT64                            g_currentFenceValue = 0;

HANDLE                            g_fenceEvent = nullptr;
UINT                              g_rtvDescriptorSize = 0;
UINT                              g_frameIndex = 0;

// 动画状态
float g_hue = 0.0f;

// ========== 辅助函数 ==========
void WaitForGPU(UINT frameIndex)
{
    // 发出信号
    g_currentFenceValue++;
    g_fenceValues[frameIndex] = g_currentFenceValue;
    g_commandQueue->Signal(g_fence.Get(), g_currentFenceValue);

    // 在 CPU 端等待
    if (g_fence->GetCompletedValue() < g_fenceValues[frameIndex])
    {
        g_fence->SetEventOnCompletion(g_fenceValues[frameIndex], g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

// HSL 转 RGB（简化版）
float HSLToChannel(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

void HSLToRGB(float h, float s, float l, float& r, float& g, float& b)
{
    if (s == 0.0f)
    {
        r = g = b = l;
    }
    else
    {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = HSLToChannel(p, q, h + 1.0f / 3.0f);
        g = HSLToChannel(p, q, h);
        b = HSLToChannel(p, q, h - 1.0f / 3.0f);
    }
}

// ========== 初始化 ==========
bool InitD3D12(HWND hwnd)
{
    HRESULT hr;

    // 调试层
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    // 创建设备
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(&g_device));
    if (FAILED(hr)) return false;

    // 创建命令队列
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = g_device->CreateCommandQueue(&queueDesc,
                                       IID_PPV_ARGS(&g_commandQueue));
    if (FAILED(hr)) return false;

    // 创建交换链
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width            = 0;
    scDesc.Height           = 0;
    scDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount      = FRAME_COUNT;
    scDesc.Scaling          = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;

    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(), hwnd,
        &scDesc, nullptr, nullptr, &swapChain1);
    if (FAILED(hr)) return false;
    swapChain1.As(&g_swapChain);

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // RTV 描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr = g_device->CreateDescriptorHeap(&rtvHeapDesc,
                                         IID_PPV_ARGS(&g_rtvHeap));
    if (FAILED(hr)) return false;

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 创建 RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        hr = g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
        if (FAILED(hr)) return false;
        g_device->CreateRenderTargetView(
            g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, g_rtvDescriptorSize);
    }

    // 为每帧创建独立的命令分配器
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        hr = g_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_commandAllocators[i]));
        if (FAILED(hr)) return false;
        g_fenceValues[i] = 0;
    }

    // 创建命令列表
    hr = g_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_commandAllocators[0].Get(), nullptr,
        IID_PPV_ARGS(&g_commandList));
    if (FAILED(hr)) return false;
    g_commandList->Close();

    // 创建围栏和事件
    hr = g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                IID_PPV_ARGS(&g_fence));
    if (FAILED(hr)) return false;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) return false;

    return true;
}

// ========== 渲染 ==========
void Render()
{
    UINT frameIdx = g_frameIndex;

    // 等待该帧的 GPU 工作完成
    if (g_fence->GetCompletedValue() < g_fenceValues[frameIdx])
    {
        g_fence->SetEventOnCompletion(g_fenceValues[frameIdx], g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    // 重置该帧的命令分配器
    g_commandAllocators[frameIdx]->Reset();

    // 重置命令列表
    g_commandList->Reset(g_commandAllocators[frameIdx].Get(), nullptr);

    // 转换后台缓冲区状态：PRESENT -> RENDER_TARGET
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[frameIdx].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    g_commandList->ResourceBarrier(1, &barrier);

    // 获取当前帧的 RTV 句柄
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        g_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        frameIdx, g_rtvDescriptorSize);

    // 用 HSL 渐变色清屏
    float r, gg, b;
    HSLToRGB(g_hue, 0.7f, 0.4f, r, gg, b);
    float clearColor[] = { r, gg, b, 1.0f };
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 更新色相
    g_hue += 0.002f;
    if (g_hue > 1.0f) g_hue -= 1.0f;

    // 转换后台缓冲区状态：RENDER_TARGET -> PRESENT
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[frameIdx].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    g_commandList->ResourceBarrier(1, &barrier);

    // 封闭命令列表
    g_commandList->Close();

    // 提交命令
    ID3D12CommandList* ppCmdLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(1, ppCmdLists);

    // 更新围栏值
    g_currentFenceValue++;
    g_fenceValues[frameIdx] = g_currentFenceValue;
    g_commandQueue->Signal(g_fence.Get(), g_currentFenceValue);

    // Present
    g_swapChain->Present(1, 0);

    // 切换到下一帧
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}

// ========== 清理 ==========
void Cleanup()
{
    // 等待所有帧完成
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        WaitForGPU(i);
    }

    CloseHandle(g_fenceEvent);

    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        g_renderTargets[i].Reset();
        g_commandAllocators[i].Reset();
    }
    g_commandList.Reset();
    g_fence.Reset();
    g_rtvHeap.Reset();
    g_swapChain.Reset();
    g_commandQueue.Reset();
    g_device.Reset();
}

// ========== 窗口过程 ==========
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"D3D12CmdQueueDemo";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"D3D12 Command Queue Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    if (!InitD3D12(hwnd))
    {
        MessageBox(nullptr, L"初始化失败", L"错误", MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // 使用定时器驱动渲染循环（60fps）
    SetTimer(hwnd, 1, 16, nullptr);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Cleanup();
    return 0;
}
```

这段代码有几个值得注意的地方。首先，我们在 `Render()` 函数中看到了资源屏障（`ResourceBarrier`）的实际使用——在清屏之前，后台缓冲区需要从 `PRESENT` 状态转换到 `RENDER_TARGET` 状态；清屏之后，又需要从 `RENDER_TARGET` 转换回 `PRESENT` 状态。这就是上篇提到的"显式资源状态管理"在代码中的体现。

其次，我们使用了 `FRAME_COUNT = 2` 的双帧并行模式——两个命令分配器轮流使用，CPU 最多领先 GPU 1 帧。在渲染循环中，我们先检查当前帧索引对应的围栏值，如果 GPU 还没执行完，就等待。

第三，`g_swapChain->GetCurrentBackBufferIndex()` 在 Present 之后返回下一个要使用的后台缓冲区索引。由于我们使用的是 `DXGI_SWAP_EFFECT_FLIP_DISCARD`，Present 之后当前帧的后台缓冲区内容就不确定了——所以下一次使用它时不需要清屏之外的额外操作。

### WM_TIMER 驱动 vs WM_PAINT 驱动

在这个示例中，我同时使用了 WM_PAINT 和 SetTimer 两种方式来驱动渲染。WM_PAINT 负责实际的渲染操作，SetTimer 以约 60fps 的频率调用 InvalidateRect 来触发 WM_PAINT。这是一个简化的方案，在实际的游戏或高性能应用中，你通常会使用一个独立的渲染线程或者在消息循环中直接调用渲染函数。但对于学习和验证来说，这种方式足够了。

## 多帧并行的内存布局

为了帮助理解多帧并行的内存管理，我们可以画一个时间线来描述 CPU 和 GPU 的交互：

```
时间 ─────────────────────────────────────────────────────────►

CPU:  [录制帧0] [录制帧1] [等帧0] [录制帧2] [等帧1] [录制帧3] ...
GPU:            [执行帧0]         [执行帧1]         [执行帧2]  ...
                 ↑Signal(1)        ↑Signal(2)        ↑Signal(3)
```

在这个时间线中，CPU 可以在录制帧1的同时让 GPU 执行帧0（这就是并行的来源）。但当 CPU 要录制帧2时，它需要复用帧0的命令分配器——所以它必须等待帧0的 Signal(1) 到达，确认 GPU 已经执行完了帧0。

如果 GPU 执行速度跟得上 CPU（通常都是这样），那么"等帧0"这个等待几乎不会真正阻塞 CPU——因为 GPU 早就执行完了。但如果 GPU 执行特别慢（比如场景极其复杂），CPU 就不得不停下来等。这时候 `FRAME_COUNT` 的值就很重要了——如果你设成 3，CPU 就可以多领先 1 帧，缓冲余地更大。代价是延迟更高（用户输入到画面显示的延迟增加）和内存占用更多（需要更多的命令分配器和帧资源）。

## 常见问题

### 调试层报"Command allocator is being reset before previous executions have completed"

这是 D3D12 初学者最常遇到的错误，没有之一。它意味着你在 GPU 还在执行命令的时候调用了 `commandAllocator->Reset()`。

解决方案就是使用本文介绍的围栏同步机制：在 Reset 之前，先通过 Fence 确认 GPU 已经执行完了该分配器关联的所有命令。如果你用了多帧并行模式，确保每帧有独立的命令分配器，并且在 Reset 之前等待该帧对应的围栏值。

### 围栏值不增加 / 死锁

如果你发现程序卡死在 `WaitForSingleObject`，最可能的原因是你忘了调用 `commandQueue->Signal()`。围栏的值只有在 Signal 被执行后才会增加——如果你只是录制了命令但没有打 Signal 标记，CPU 就会永远等下去。

另一个可能的原因是围栏值搞混了。确保每个帧资源记录的 `fenceValue` 与 Signal 时使用的值一致。

### 资源屏障缺失导致画面闪烁

如果你发现清屏颜色不对，或者画面偶尔闪烁，可能是因为你忘记在清屏前插入 `PRESENT -> RENDER_TARGET` 的资源屏障。GPU 可能还在用旧的状态来解读后台缓冲区的用途，导致清屏操作没有正确执行。

### 为什么不直接在每帧结束时等 GPU

你可能会想，既然每一帧结束时都要等 GPU 完成，那为什么要搞多帧并行，直接每帧末尾等不就好了？

区别在于**等什么**。如果你在每帧末尾等 GPU 完成当前帧，CPU 和 GPU 是串行的——CPU 录制第 N 帧时 GPU 空闲，GPU 执行第 N 帧时 CPU 空闲。而多帧并行模式中，CPU 在录制第 N 帧的同时 GPU 在执行第 N-1 帧——CPU 等的不是"当前帧"完成，而是"几帧前"的某个帧完成。如果 GPU 够快，这个等待几乎不花时间。

## 总结

这篇我们深入了 D3D12 命令管理的三个核心组件。

`CommandAllocator` 是命令的内存池——它管理着命令数据的底层存储，通过 `Reset()` 回收内存。但 Reset 有一个硬约束：GPU 必须已经执行完所有使用该分配器的命令。这就是为什么多帧并行需要为每帧准备独立的分配器。

`GraphicsCommandList` 是命令的录制接口——你在它上面录制所有的渲染命令，通过 `Close()` 封闭，然后提交给命令队列。命令列表的 `Reset` 几乎零开销，可以在每帧重复使用同一个命令列表对象。

`CommandQueue` 和 `Fence` 构成了命令提交和同步的机制——命令队列负责把命令列表提交给 GPU 执行（异步），围栏负责让 CPU 知道 GPU 执行到了哪里。通过 `Signal` 在 GPU 端打标记，通过 `SetEventOnCompletion` 在 CPU 端等待，我们实现了安全的多帧并行。

我们还看到了 `ResourceBarrier` 的实际使用——后台缓冲区在 `PRESENT` 和 `RENDER_TARGET` 状态之间切换时，必须显式插入屏障。

下一篇我们要讨论的是资源与堆管理。命令系统告诉我们"怎么指挥 GPU"，而资源管理告诉我们"GPU 操作的数据在哪里、怎么从 CPU 传到 GPU"。这是 D3D12 内存模型的核心内容。

---

**参考资料**:
- [ID3D12CommandAllocator::Reset - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset)
- [ID3D12GraphicsCommandList::Close - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close)
- [ID3D12CommandQueue::ExecuteCommandLists - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)
- [ID3D12Fence::SetEventOnCompletion - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion)
- [Executing and Synchronizing Command Lists - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/executing-and-synchronizing-command-lists)
- [Creating and Recording Command Lists and Bundles - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles)
- [ID3D12Device::CreateCommandList - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommandlist)

---

**练习**:

1. 修改示例代码中的 `FRAME_COUNT` 从 2 改成 3。观察代码中有哪些地方需要同步修改。思考：三缓冲相比双缓冲的优势和劣势分别是什么？

2. 去掉渲染函数中的围栏等待（注释掉 `WaitForSingleObject` 相关代码），然后在 Debug 模式下运行。观察调试层的输出信息，理解为什么 Reset 命令分配器之前必须等待 GPU 完成。

3. 实现一个基于 `WM_TIMER` 的帧率计数器——在窗口标题栏显示当前 FPS。提示：使用 `GetTickCount64` 或 `QueryPerformanceCounter` 计算每秒渲染帧数。

4. 将清屏动画从简单的颜色渐变改成一个"呼吸灯"效果——屏幕颜色在亮和暗之间平滑过渡。提示：使用正弦函数控制亮度值。
