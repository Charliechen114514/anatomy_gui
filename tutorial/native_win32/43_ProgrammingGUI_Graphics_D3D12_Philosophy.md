# 通用GUI编程技术——图形渲染实战（四十三）——D3D12设计哲学：显式控制与性能解锁

> 上回我们聊了 D3D11 混合与透明——将 Alpha 混合、颜色混合、深度测试这些概念串起来，算是把 D3D11 的图形管线走到了一个比较完整的闭环。从这篇开始，我们要跨入一个全新的领域：Direct3D 12。这不是一次简单的版本升级，而是一场设计哲学的根本性变革。

## 前言：为什么要从 D3D11 跳到 D3D12

说实话，当我们第一次认真接触 D3D12 的时候，内心是抗拒的。

D3D11 用得好好的——API 简洁直观，驱动帮你兜底，内存管理不用操心，资源状态自动跟踪，同步机制基本透明。作为一个写业务逻辑和图形效果的工程师，D3D11 给你的体验是相当舒服的：你只需要关注"画什么"，不用操心"GPU 在底层怎么跑"。

但问题在于，这种"舒服"是有代价的。D3D11 的运行时和驱动层为了给你提供这种便利，在背后做了大量的工作：每次你提交一个 Draw Call，驱动都要检查资源状态、插入必要的屏障、管理内存驻留、处理隐式同步……这些操作全都发生在 CPU 端，而且对你的代码来说是完全不可控的。当你的场景复杂到一定程度，CPU 就会成为瓶颈——不是 GPU 算不过来，而是 CPU 来不及给 GPU 喂数据。

D3D12 的核心目标就是解决这个 CPU 驱动开销的问题。它把 D3D11 中由驱动自动管理的那些职责，全部移交到了应用程序端。你现在可以精确控制命令的录制和提交时机，可以显式管理资源的内存布局和状态转换，可以手动同步 CPU 和 GPU 的执行。代价是代码量翻了三五倍，收益是——如果你做得对——CPU 开销可以降到 D3D11 的十分之一甚至更低。

这不是一个轻松的选择，但如果你打算认真做高性能图形渲染，D3D12 是绕不过去的。

## 环境说明

我们接下来的所有 D3D12 实战都在以下环境中进行：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v143 工具集)
- **Windows SDK**: 10.0.26100 或更高版本
- **目标 API**: Direct3D 12 (Feature Level 11_0 起步，推荐 12_0+)
- **调试工具**: D3D12 调试层（Debug Layer），通过 `ID3D12Debug` 接口启用
- **图形硬件**: 任意支持 Feature Level 11_0 的 GPU（NVIDIA/AMD/Intel 均可，包括集成显卡）

D3D12 从 Windows 10 开始原生支持，Windows 11 上自然完全可用。如果你还在用 Windows 7，那确实需要做一些额外工作，但这个系列我们就不考虑这种老旧平台了。

## D3D11 vs D3D12：一场设计哲学的对比

在动手写代码之前，我们有必要先把 D3D11 和 D3D12 在设计层面的差异理清楚。这不是为了吓退你，而是为了让你在后续写代码的时候，能理解"为什么 D3D12 要这么做"。如果你只是机械地抄代码而不理解背后的原因，后面的坑会多到让你怀疑人生。

我们从五个维度来做对比：命令提交、资源状态管理、内存管理、描述符机制和同步模型。

### 命令提交：隐式 vs 显式

在 D3D11 中，你通过 `ID3D11DeviceContext` 直接调用绘制命令。你的每个 `Draw`、`DrawIndexed`、`ClearRenderTargetView` 调用都会被立即提交到驱动管线。你不需要关心命令在什么时候被真正发送到 GPU，也不需要关心命令之间的边界在哪里——驱动和运行时会帮你处理好这一切。这种模式叫做"立即模式"（Immediate Mode），它的优点是简单直观，缺点是 CPU 开销不可控。

D3D12 彻底抛弃了立即模式。你现在有三个层次的概念需要理解：`ID3D12CommandAllocator` 负责管理命令的底层内存池，`ID3D12GraphicsCommandList` 是你录制命令的接口，而 `ID3D12CommandQueue` 则是你把录制好的命令列表提交给 GPU 的通道。你需要先在一个命令列表上录好所有命令，调用 `Close()` 封闭它，然后通过命令队列的 `ExecuteCommandLists()` 提交执行。这个过程完全显式，你清楚知道命令在什么时候被提交，也清楚知道一批命令的边界在哪里。

这意味着你可以把命令的录制和提交解耦开来——在 GPU 执行上一帧命令的同时，CPU 就可以在另一个命令列表上录制下一帧的命令。这种并行是 D3D12 性能优势的核心来源之一。

### 资源状态：自动跟踪 vs 手动屏障

D3D11 的运行时会自动跟踪每个资源的状态。当一个纹理从"渲染目标"状态切换到"着色器资源"状态时，运行时会自动插入必要的 GPU 屏障。你完全不需要操心这件事——甚至不需要知道"资源状态"这个概念的存在。

D3D12 把这个跟踪责任完全交给了你。每个资源都有一个"当前状态"的概念，当资源需要在不同用途之间切换时（比如从渲染目标变成着器资源，或者从拷贝目标变成顶点缓冲），你必须在命令列表中显式插入 `ResourceBarrier` 调用来通知 GPU 这个状态变化。如果你忘记了，调试层会给你报错，渲染结果可能会不正确，严重时甚至会崩溃。

这是 D3D12 最容易被忽视的陷阱之一——初学者往往会忘记状态转换，然后花好几个小时排查为什么画面不对。

### 内存管理：驱动代管 vs 自主控制

在 D3D11 中，你调用 `CreateTexture2D` 或 `CreateBuffer`，驱动会自动选择合适的内存区域（系统内存还是显存），自动处理驻留管理，自动在需要的时候进行内存迁移。你创建了一个资源就完事了，不用管它在物理上是怎么存储的。

D3D12 把内存管理分成了三个层次。最简单的是 `CreateCommittedResource`——同时创建资源和底层堆，相当于 D3D11 的简化版本。进阶一点的是 `CreatePlacedResource`——你先手动创建一个堆（Heap），然后在堆中的指定偏移位置放置资源，这样可以多个资源共享同一个堆。最高级的是 `CreateReservedResource`——虚拟内存映射，按需提交物理页面，适用于超大纹理的流式加载。

不仅如此，D3D12 还有三种预定义的堆类型（UPLOAD、DEFAULT、READBACK），分别对应不同的 CPU-GPU 访问模式。理解这三种堆类型各自的用途和限制，是掌握 D3D12 内存管理的基础。

### 描述符机制：直接绑定 vs 间接引用

D3D11 中你通过 `IASetVertexBuffers`、`PSSetShaderResources` 这类函数直接把资源绑定到管线的各个插槽上。运行时在背后维护着一套绑定表，帮你处理资源到管线的映射。

D3D12 用"描述符"（Descriptor）替代了这套机制。描述符是一个轻量级的数据结构，它描述了 GPU 应该如何访问一个资源——比如一个顶点缓冲描述符会包含缓冲的地址、大小和步长信息。描述符被组织在"描述符堆"（Descriptor Heap）中，而着色器则通过根签名（Root Signature）来引用这些描述符。

你可以把描述符理解为 GPU 端的"指针"——它告诉 GPU 数据在哪里、格式是什么、如何访问。这种间接引用的方式虽然增加了一层抽象，但好处是 GPU 端的切换成本极低——换一个描述符只需要改一个指针，而不需要驱动去做一堆验证和状态更新。

### 同步模型：隐式同步 vs 围栏信号

D3D11 的同步几乎是隐式的。当你调用一个可能会与之前操作冲突的 API 时，运行时会自动插入等待。你不需要知道 GPU 是否完成了之前的工作——驱动会确保一切按正确顺序执行。

D3D12 没有"自动等待"这种东西。CPU 提交命令后立刻返回，GPU 在后台异步执行。如果你需要在 CPU 端等待 GPU 完成某个操作（比如回收命令分配器的内存），你必须使用围栏（Fence）机制：在 GPU 端通过 `Signal()` 打一个信号值，在 CPU 端通过 `SetEventOnCompletion()` 等待这个值到达。这种显式同步是 D3D12 性能优势的关键——CPU 永远不会被隐式等待阻塞——但也意味着你必须正确管理同步，否则要么死锁，要么访问已释放的资源导致崩溃。

### 差异对比总表

我们把上面的对比浓缩成一张表格，方便你在写代码的时候随时回来对照：

| 维度 | D3D11 | D3D12 |
|------|-------|-------|
| 命令提交 | 立即模式，ID3D11DeviceContext 直接调用 | 延迟模式，CommandList 录制 + CommandQueue 提交 |
| 资源状态 | 运行时自动跟踪和转换 | 应用程序手动插入 ResourceBarrier |
| 内存管理 | 驱动自动选择和迁移 | 三种堆类型 + 三种创建方式，手动控制 |
| 绑定模型 | 直接绑定到管线插槽 | 描述符 + 描述符堆 + 根签名间接引用 |
| 同步 | 隐式，驱动自动插入等待 | 显式围栏 + 事件，手动同步 CPU/GPU |
| CPU 开销 | 高（驱动验证 + 状态跟踪） | 低（应用层完全控制） |
| 代码复杂度 | 低 | 高（但可控） |
| 多线程录制 | 有限支持（Deferred Context） | 原生支持（多 CommandList 并行录制） |

看到这张表，你大概能理解为什么我说 D3D12 的学习曲线不在于渲染管线本身——管线的概念（顶点着色器、像素着色器、光栅化、混合）跟 D3D11 完全一样。真正的难点在于围绕管线的这套"基础设施"：命令管理、内存管理、状态管理和同步管理。

## D3D12CreateDevice：一切的起点

理论铺垫够了，现在我们开始写代码。跟 D3D11 一样，使用 D3D12 的第一步是创建设备对象。D3D12 的设备创建函数是 `D3D12CreateDevice`，它的声明如下：

```cpp
HRESULT D3D12CreateDevice(
    IUnknown          *pAdapter,          // DXGI 适配器，NULL 表示默认
    D3D_FEATURE_LEVEL FeatureLevel,      // 最低功能级别
    REFIID            riid,              // 接口 ID
    void              **ppDevice          // 输出设备指针
);
```

这个函数的签名跟 D3D11 的 `D3D11CreateDevice` 非常相似，但有一个重要的区别：D3D12 不支持 9_1 到 10_1 的功能级别，最低只支持 `D3D_FEATURE_LEVEL_11_0`。这很合理，因为 D3D12 本身就要求 GPU 至少支持 DirectX 11 级别的硬件特性。

在实际工程中，我们通常会先枚举 DXGI 适配器，检查硬件是否支持 D3D12，然后再创建设备。不过在学习和原型阶段，直接传 `NULL` 让系统选择默认适配器完全够用。

关于 `D3D12CreateDevice` 的完整文档，可以参考 [Microsoft Learn - D3D12CreateDevice function](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)。

## 最小 D3D12 骨架代码

接下来我们要写一个最小的 D3D12 程序骨架。它不会画出任何炫酷的东西——只是创建设备、命令队列、交换链、RTV 描述符堆和命令分配器，然后把窗口跑起来。但这些组件是所有 D3D12 程序的基础，后面所有文章都会在这个骨架上扩展。

我们一步一步来。

### 第一步：启用调试层

在开发阶段，强烈建议启用 D3D12 的调试层。调试层会在你犯错的时候给出详细的错误信息——比如你忘记插入资源屏障，或者你在 GPU 还在执行命令的时候重置了命令分配器，调试层都会立刻告诉你。没有调试层的 D3D12 开发就像闭着眼睛走雷区——能走过去纯属运气。

```cpp
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

void EnableDebugLayer()
{
    // 在 Debug 构建中启用调试层
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
}
```

`D3D12GetDebugInterface` 返回一个 `ID3D12Debug` 接口，调用 `EnableDebugLayer()` 后，所有后续的 D3D12 调用都会被调试层监控。注意这会带来显著的性能开销，所以只在 Debug 构建中启用。

### 第二步：创建设备

设备（Device）是 D3D12 中最核心的对象，几乎所有其他对象都通过设备来创建。

```cpp
ComPtr<ID3D12Device> device;

HRESULT hr = D3D12CreateDevice(
    nullptr,                        // 使用默认适配器
    D3D_FEATURE_LEVEL_11_0,         // 最低功能级别
    IID_PPV_ARGS(&device)
);

if (FAILED(hr))
{
    // 创建设备失败——可能是 GPU 不支持 D3D12
    MessageBox(nullptr, L"D3D12 设备创建失败", L"错误", MB_OK);
    return 0;
}
```

`IID_PPV_ARGS` 是一个宏，它展开后会自动填充 `riid` 和 `ppDevice` 两个参数。在 COM 编程中这是一个非常常用的模式，几乎所有 D3D12 的创建函数都会用到它。

关于 `ID3D12Device` 接口的完整方法列表，可以参考 [Microsoft Learn - ID3D12Device interface](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12device)。

### 第三步：创建命令队列

命令队列是 CPU 向 GPU 提交命令的通道。我们需要先创建它，因为创建交换链的时候会用到。

```cpp
ComPtr<ID3D12CommandQueue> commandQueue;

D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;    // 直接命令列表
queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;     // 无特殊标志

hr = device->CreateCommandQueue(
    &queueDesc,
    IID_PPV_ARGS(&commandQueue)
);

if (FAILED(hr))
{
    MessageBox(nullptr, L"命令队列创建失败", L"错误", MB_OK);
    return 0;
}
```

`D3D12_COMMAND_LIST_TYPE_DIRECT` 表示这是一个直接命令队列，GPU 驱动可以直接执行它收到的命令。这是最常用的类型，也是我们整个系列会一直用到的。其他类型还有 `BUNDLE`（命令包）、`COMPUTE`（计算）、`COPY`（拷贝）等，分别对应不同的使用场景。

关于命令队列的完整文档，可以参考 [Microsoft Learn - ID3D12CommandQueue interface](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12commandqueue)。

### 第四步：创建交换链

交换链负责管理后台缓冲区和前台缓冲区的交替。在 D3D12 中，交换链是通过 DXGI 创建的，而且跟 D3D11 有一个重要区别：创建交换链时传入的第一个参数不是设备指针，而是命令队列指针。

```cpp
ComPtr<IDXGISwapChain3> swapChain;

DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
swapChainDesc.Width = 0;                              // 使用窗口宽度
swapChainDesc.Height = 0;                             // 使用窗口高度
swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    // 像素格式
swapChainDesc.Stereo = FALSE;
swapChainDesc.SampleDesc.Count = 1;                   // 不使用多重采样
swapChainDesc.SampleDesc.Quality = 0;
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
swapChainDesc.BufferCount = 2;                        // 双缓冲
swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // 翻转模式
swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

ComPtr<IDXGIFactory4> factory;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

HWND hwnd = /* 你的窗口句柄 */;

ComPtr<IDXGISwapChain1> swapChain1;
hr = factory->CreateSwapChainForHwnd(
    commandQueue.Get(),       // 注意：传命令队列，不是设备！
    hwnd,
    &swapChainDesc,
    nullptr,
    nullptr,
    &swapChain1
);

if (SUCCEEDED(hr))
{
    swapChain1.As(&swapChain);
}
```

这里的 `DXGI_SWAP_EFFECT_FLIP_DISCARD` 是 D3D12 推荐的交换模式——它使用翻转模型（Flip Model），性能比传统的位块传输模式好得多。`BufferCount = 2` 表示双缓冲，这在大多数场景下已经足够。关于交换链创建的完整文档，可以参考 [Microsoft Learn - CreateSwapChainForHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)。

⚠️ 注意，D3D12 中创建交换链时传入的是命令队列（`ID3D12CommandQueue`），而不是设备对象。这是很多从 D3D11 转过来的开发者会犯的错误——在 D3D11 中确实传的是设备指针，但在 D3D12 中交换链需要直接与命令队列关联。

### 第五步：创建 RTV 描述符堆

渲染目标视图（Render Target View，RTV）是 D3D12 描述符机制的一个实例。交换链提供的后台缓冲区是普通的纹理资源，你需要通过 RTV 描述符告诉 GPU "这个纹理要作为渲染目标来使用"。而 RTV 描述符需要存放在一个描述符堆中。

```cpp
ComPtr<ID3D12DescriptorHeap> rtvHeap;

D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
rtvHeapDesc.NumDescriptors = 2;                              // 双缓冲，两个 RTV
rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;           // RTV 类型
rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

if (FAILED(hr))
{
    MessageBox(nullptr, L"RTV 描述符堆创建失败", L"错误", MB_OK);
    return 0;
}

// 获取描述符大小——不同 GPU 的描述符大小可能不同
UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV
);
```

`GetDescriptorHandleIncrementSize` 这个调用非常重要。在 D3D12 中，描述符的大小不是固定的——不同硬件、不同类型的描述符可能占用不同的字节数。你不能假设每个描述符占 32 字节或 64 字节，必须通过这个函数查询实际的增量大小，然后手动计算每个描述符的偏移位置。

这是 D3D12 设计哲学的一个缩影：不假设、不隐藏，把控制权交给你。

关于描述符堆的详细文档，可以参考 [Microsoft Learn - ID3D12Device::CreateDescriptorHeap](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap) 和 [Microsoft Learn - D3D12_DESCRIPTOR_HEAP_TYPE enumeration](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_descriptor_heap_type)。

### 第六步：创建后台缓冲区的 RTV

现在我们有了描述符堆，接下来需要为交换链的每个后台缓冲区创建渲染目标视图。

```cpp
ComPtr<ID3D12Resource> renderTargets[2];

// 获取描述符堆的起始句柄
CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
    rtvHeap->GetCPUDescriptorHandleForHeapStart()
);

for (UINT i = 0; i < 2; i++)
{
    // 获取第 i 个后台缓冲区
    hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));

    if (SUCCEEDED(hr))
    {
        // 为这个缓冲区创建 RTV
        device->CreateRenderTargetView(
            renderTargets[i].Get(),    // 目标资源
            nullptr,                   // 使用资源格式的默认描述
            rtvHandle                  // 描述符堆中的位置
        );
    }

    // 移动到下一个描述符位置
    rtvHandle.Offset(1, rtvDescriptorSize);
}
```

`CD3DX12_CPU_DESCRIPTOR_HANDLE` 是 D3D12 辅助库（`d3dx12.h`）提供的一个工具类，它简化了描述符句柄的偏移计算。`rtvHandle.Offset(1, rtvDescriptorSize)` 将句柄向后移动一个描述符的距离——这正是为什么我们之前要查询 `rtvDescriptorSize` 的原因。

### 第七步：创建命令分配器和命令列表

最后，我们创建命令分配器和命令列表。命令分配器管理命令的底层内存，命令列表则是录制命令的接口。

```cpp
ComPtr<ID3D12CommandAllocator> commandAllocator;

hr = device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(&commandAllocator)
);

if (FAILED(hr))
{
    MessageBox(nullptr, L"命令分配器创建失败", L"错误", MB_OK);
    return 0;
}

ComPtr<ID3D12GraphicsCommandList> commandList;

hr = device->CreateCommandList(
    0,                                      // 节点掩码（单 GPU 为 0）
    D3D12_COMMAND_LIST_TYPE_DIRECT,          // 命令列表类型
    commandAllocator.Get(),                  // 关联的命令分配器
    nullptr,                                // 初始管线状态（无）
    IID_PPV_ARGS(&commandList)
);

if (FAILED(hr))
{
    MessageBox(nullptr, L"命令列表创建失败", L"错误", MB_OK);
    return 0;
}

// 命令列表在创建时处于"打开"状态
// 在实际使用前需要先 Close，然后在渲染循环中 Reset
commandList->Close();
```

注意最后那个 `Close()` 调用。命令列表在通过 `CreateCommandList` 创建时处于"打开"状态——这意味着你可以立即开始录制命令。但在实际的渲染循环中，我们通常先调用 `Reset()` 重新初始化命令列表，然后录制，最后 `Close()` 封闭。所以在初始化阶段先把命令列表关掉是一个好习惯，确保它处于一致的状态。

关于命令分配器的文档，可以参考 [Microsoft Learn - ID3D12CommandAllocator](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12commandallocator)；命令列表的创建则参考 [Microsoft Learn - ID3D12Device::CreateCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommandlist)。

### 完整的初始化代码

把上面的步骤串起来，加上窗口创建和消息循环，就是一个完整的（虽然还不画任何东西的）D3D12 程序骨架：

```cpp
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// ========== 全局 D3D12 资源 ==========
ComPtr<ID3D12Device>              g_device;
ComPtr<ID3D12CommandQueue>        g_commandQueue;
ComPtr<IDXGISwapChain3>           g_swapChain;
ComPtr<ID3D12DescriptorHeap>      g_rtvHeap;
ComPtr<ID3D12CommandAllocator>    g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12Resource>            g_renderTargets[2];
UINT                              g_rtvDescriptorSize = 0;
UINT                              g_frameIndex = 0;

// ========== 函数声明 ==========
void EnableDebugLayer();
bool InitD3D12(HWND hwnd);
void Cleanup();

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 注册窗口类
    const wchar_t CLASS_NAME[] = L"D3D12Skeleton";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"D3D12 Skeleton",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 0;

    // 启用调试层（Debug 构建）
    EnableDebugLayer();

    // 初始化 D3D12
    if (!InitD3D12(hwnd))
    {
        MessageBox(nullptr, L"D3D12 初始化失败", L"错误", MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Cleanup();
    return 0;
}

void EnableDebugLayer()
{
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif
}

bool InitD3D12(HWND hwnd)
{
    HRESULT hr;

    // 1. 创建设备
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(&g_device));
    if (FAILED(hr)) return false;

    // 2. 创建命令队列
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = g_device->CreateCommandQueue(&queueDesc,
                                       IID_PPV_ARGS(&g_commandQueue));
    if (FAILED(hr)) return false;

    // 3. 创建交换链
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width            = 0;
    scDesc.Height           = 0;
    scDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.Stereo           = FALSE;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount      = 2;
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

    // 4. 创建 RTV 描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = g_device->CreateDescriptorHeap(&rtvHeapDesc,
                                         IID_PPV_ARGS(&g_rtvHeap));
    if (FAILED(hr)) return false;

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 5. 创建后台缓冲区 RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < 2; i++)
    {
        hr = g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
        if (FAILED(hr)) return false;

        g_device->CreateRenderTargetView(
            g_renderTargets[i].Get(), nullptr, rtvHandle);

        rtvHandle.Offset(1, g_rtvDescriptorSize);
    }

    // 6. 创建命令分配器和命令列表
    hr = g_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&g_commandAllocator));
    if (FAILED(hr)) return false;

    hr = g_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_commandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&g_commandList));
    if (FAILED(hr)) return false;

    g_commandList->Close();

    return true;
}

void Cleanup()
{
    // 等待 GPU 完成所有工作（在实际程序中应使用 Fence）
    // 这里简单处理，因为我们还没画任何东西
    g_renderTargets[0].Reset();
    g_renderTargets[1].Reset();
    g_commandList.Reset();
    g_commandAllocator.Reset();
    g_rtvHeap.Reset();
    g_swapChain.Reset();
    g_commandQueue.Reset();
    g_device.Reset();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这段代码看起来很长，但它做的事情其实很简单——创建了设备、命令队列、交换链、RTV 描述符堆、命令分配器和命令列表。每一步都是必要的，没有冗余。你可以编译运行它，会看到一个标准的 Windows 窗口，虽然里面什么都没画——窗口会显示你设置的背景色。

## 关于学习曲线的一个实话

如果你是第一次接触 D3D12，看到上面这么多代码只为了创建一个什么都不画的窗口，你的第一反应可能是：这也太复杂了吧。

这个反应完全正常。我自己第一次看到 D3D12 的初始化代码时也是这个感觉。

但我想强调一个观点：D3D12 的学习曲线不在于渲染管线本身。顶点着色器、像素着色器、光栅化、混合——这些概念跟 D3D11 完全一样，你之前学的东西全部都能用。真正的难点在于围绕管线的"基础设施"：命令的录制和提交机制、资源状态的显式转换、内存和堆的管理方式、CPU/GPU 之间的同步模型。

这些概念每一个单独拿出来都不难理解——但它们需要协同工作，缺一不可。你写一个清屏操作，就需要命令列表、命令分配器、资源屏障、RTV 描述符、围栏同步全部到位。少一个，程序要么不工作，要么调试层疯狂报错。

这就是为什么 D3D12 的入门门槛很高——不是因为某个概念特别深奥，而是因为你需要同时掌握多个概念才能写出最简单的程序。不过好消息是，一旦你跨过了这个门槛，后面的路会越走越宽——因为你已经理解了 GPU 编程的本质。

## 常见问题

### D3D12CreateDevice 返回 E_NOINTERFACE

这通常意味着你的系统不支持 D3D12。D3D12 需要 Windows 10 及以上版本。如果你确定系统版本没问题，检查一下是不是在传 `IID_PPV_ARGS` 的时候写错了接口类型。

### 交换链创建失败（DXGI_ERROR_INVALID_CALL）

在 D3D12 中，`CreateSwapChainForHwnd` 的第一个参数必须是 `ID3D12CommandQueue`，不能传设备。这是一个非常常见的错误，因为 D3D11 中确实传的是设备。

### 调试层报大量警告

如果你的 D3D12 调试层输出大量警告，不要慌。仔细阅读每一条警告信息，它们通常会精确地告诉你问题在哪里——比如"资源处于错误的状态"或者"命令分配器在被重置时仍有未完成的命令"。这些警告是帮助你学习的，不是阻碍你。

### CD3DX12_CPU_DESCRIPTOR_HANDLE 找不到

这个类型定义在 `d3dx12.h` 中，但这个头文件不属于 Windows SDK。你需要从微软的 GitHub 仓库下载它，或者直接手动计算描述符偏移。`d3dx12.h` 是一个纯头文件的辅助库，不对应任何 .lib 文件，下载后直接放到你的项目目录中 include 即可。

## 总结

这篇我们做了三件事。

第一，我们通过 D3D11 和 D3D12 的五个维度对比，理解了 D3D12 的设计哲学：把驱动层的控制权转移到应用层，用显式管理替代隐式管理，用更多的代码换取更低的 CPU 开销和更高的性能上限。

第二，我们了解了 `D3D12CreateDevice` 这个创建设备的入口函数，它是所有 D3D12 程序的起点。

第三，我们写了一个完整的 D3D12 骨架程序，包含了设备、命令队列、交换链、RTV 描述符堆、命令分配器和命令列表的创建。这个骨架是后续所有 D3D12 代码的基础。

但我们还缺一个关键环节——我们创建了命令队列和命令列表，但还没有真正用它们来执行任何命令。如何录制命令、如何提交命令、如何等待 GPU 完成？这些就是下一篇文章的内容：命令列表、队列与围栏的详细工作机制。

---

**参考资料**:
- [D3D12CreateDevice function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)
- [ID3D12Device interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12device)
- [ID3D12CommandQueue interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12commandqueue)
- [CreateSwapChainForHwnd - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)
- [ID3D12Device::CreateDescriptorHeap - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
- [Direct3D 12 Hello World Samples - Microsoft Learn](https://learn.microsoft.com/en-us/samples/microsoft/directx-graphics-samples/d3d12-hello-world-samples-win32/)
- [Microsoft DirectX-Graphics-Samples - GitHub](https://github.com/microsoft/DirectX-Graphics-Samples)

---

**练习**:

1. 编译运行上面的骨架代码，确认程序能正确创建窗口并且 D3D12 初始化成功。如果在 Debug 构建下运行，观察调试层是否有任何输出。

2. 阅读 [D3D12HelloWorld 示例](https://github.com/microsoft/DirectX-Graphics-Samples) 中的 HelloWindow 示例，对照本文的 D3D11 vs D3D12 差异对比表，指出 HelloWindow 代码中哪些地方体现了"显式命令提交"、"显式资源状态管理"和"显式同步"。

3. 修改骨架代码，把交换链的缓冲区数量从 2 改成 3（三缓冲），同时相应调整 RTV 描述符堆的 `NumDescriptors`。观察创建过程有什么变化。

4. 尝试在 `InitD3D12` 的每一步之间添加 `std::cout` 或 `OutputDebugString` 输出，构建一个初始化日志系统。思考：如果某一步失败，之前的资源需要怎么清理？
