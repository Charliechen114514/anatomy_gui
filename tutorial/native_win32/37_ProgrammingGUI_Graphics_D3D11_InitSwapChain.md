# 通用GUI编程技术——图形渲染实战（三十七）——D3D11初始化与SwapChain：从零搭建GPU渲染框架

> 前面我们用了好几篇文章来打基础——HLSL 语法、编译调试、Constant Buffer。如果你一路跟过来，应该已经理解了 Shader 编程的核心概念，也知道了 CPU 怎么通过 CBuffer 把数据传给 GPU。但所有那些 HLSL 代码和 CBuffer 示例，都是假设"有一个 D3D11 框架在运行"——而搭建这个框架，正是今天要讲的内容。
>
> D3D11 的初始化涉及三个核心对象：`ID3D11Device`（设备，负责创建资源）、`ID3D11DeviceContext`（设备上下文，负责提交命令）、`IDXGISwapChain`（交换链，负责呈现画面）。理解它们的关系是使用 D3D11 的前提，而正确处理窗口大小变化和设备丢失则是让程序稳定运行的关键。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11（链接 `d3d11.lib`、`dxgi.lib`、`d3dcompiler.lib`）
- **前置知识**: 文章 34-36（HLSL 基础 + 编译 + CBuffer）

## 核心对象三件套

D3D11 的所有操作都围绕三个核心对象展开，我们先把它们的关系搞清楚。

`ID3D11Device` 是 GPU 设备的逻辑抽象。它只负责一件事情——创建资源。创建顶点缓冲、创建纹理、创建 Shader、创建采样器——所有"创建"操作都通过 `Device` 完成。`Device` 是线程安全的，你可以从多个线程同时调用它的创建方法。

`ID3D11DeviceContext` 是命令提交通道。它负责把渲染命令（设置顶点缓冲、设置 Shader、设置 CBuffer、发出 Draw 调用）提交给 GPU 执行。每个 `DeviceContext` 对应一个命令队列，不能从多个线程同时使用同一个 `DeviceContext`。

`IDXGISwapChain` 是画面呈现管理器。它管理着一组（通常是 2 个或 3 个）后备缓冲区（Back Buffer），GPU 渲染到其中一个缓冲区，渲染完成后通过 `Present` 方法将这个缓冲区"交换"到前台显示。这就是"双缓冲"或"三缓冲"的硬件实现。

你可以把这个三件套类比为电影制作流程：`Device` 是道具部门（准备素材），`DeviceContext` 是导演（指挥拍摄），`SwapChain` 是放映机（把拍好的影片呈现给观众）。

## DXGI：DirectX Graphics Infrastructure

DXGI 是 Direct3D 底层的基础设施层，负责与 GPU 硬件和 Windows 窗口系统打交道。它独立于 D3D 版本——D3D10、D3D11、D3D12 都共用同一套 DXGI。

DXGI 的职责包括：枚举系统中的 GPU 适配器（`IDXGIAdapter`）、获取显示器输出（`IDXGIOutput`）、管理交换链（`IDXGISwapChain`）、处理全屏/窗口模式切换。大多数情况下你不需要直接操作 DXGI，它在你调用 `D3D11CreateDeviceAndSwapChain` 时被自动初始化。但在处理多显示器、多 GPU 或全屏模式切换等高级场景时，了解 DXGI 的存在会帮你理解问题的本质。

## 最小初始化代码

下面是一个可以直接编译运行的 D3D11 最小框架。它创建设备、创建交换链、设置渲染目标视图，然后在一个循环中不断清屏并呈现：

```cpp
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <directxmath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// 全局 D3D11 对象
ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRTV = NULL;

bool InitD3D(HWND hwnd, int width, int height)
{
    // 交换链描述
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;                                    // 双缓冲
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    // 32位色
    scd.BufferDesc.RefreshRate.Numerator = 60;              // 刷新率
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;     // 渲染目标
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;                                    // 窗口模式
    scd.SampleDesc.Count = 1;                               // 无多重采样
    scd.SampleDesc.Quality = 0;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;             // 丢弃旧缓冲
    scd.Flags = 0;

    // Feature levels：尝试从高到低
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    // 创建设备和交换链
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,                           // 默认适配器
        D3D_DRIVER_TYPE_HARDWARE,       // 硬件驱动
        NULL,                           // 无软件光栅化器
        0,                              // 无创建标志
        featureLevels,                  // Feature level 数组
        _countof(featureLevels),        // 数组长度
        D3D11_SDK_VERSION,
        &scd,                           // 交换链描述
        &g_pSwapChain,                  // 输出：交换链
        &g_pDevice,                     // 输出：设备
        &selectedFeatureLevel,          // 输出：实际获得的 Feature Level
        &g_pContext                     // 输出：设备上下文
    );
    if (FAILED(hr)) return false;

    // 创建渲染目标视图（RTV）
    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
    pBackBuffer->Release();  // GetBuffer 会增加引用计数，必须释放

    // 绑定渲染目标
    g_pContext->OMSetRenderTargets(1, &g_pRTV, NULL);

    // 设置视口
    D3D11_VIEWPORT vp = { 0, 0, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f };
    g_pContext->RSSetViewports(1, &vp);

    return true;
}
```

⚠️ 注意 `GetBuffer` 返回的后备缓冲区指针会增加引用计数。如果你忘记 `Release`，后备缓冲区永远不会被释放。这是 COM 引用计数的标准陷阱——`AddRef` 和 `Release` 必须成对出现。

### SwapChain 参数详解

`BufferCount = 2` 表示双缓冲（一个前台的正在显示，一个后台的正在渲染），这是最常见的配置。三缓冲（`BufferCount = 3`）可以进一步减少 VSync 等待时间，但会多占一个帧的显存。

`SwapEffect = DXGI_SWAP_EFFECT_DISCARD` 是最简单的交换模式——Present 后后台缓冲区内容变为未定义，你必须在每帧开始时重新清除或完整绘制。对于 Windows 10+，`DXGI_SWAP_EFFECT_FLIP_DISCARD` 是更现代的选择，它使用翻转模型（Flip Model），窗口合成效率更高，但要求 `BufferCount` 至少为 2。

`Windowed = TRUE` 表示窗口模式。如果你设为 `FALSE`，D3D11 会尝试进入独占全屏模式。但在实际开发中，建议始终以窗口模式初始化，然后通过 `SwapChain->SetFullscreenState` 切换全屏，这样更安全。

## 渲染循环

初始化完成后，渲染循环的核心就是三步：清屏、绘制、呈现。

```cpp
void Render()
{
    // 清屏为深蓝色
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    g_pContext->ClearRenderTargetView(g_pRTV, clearColor);

    // ... 在这里添加绘制命令 ...

    // 呈现（VSync = 1，同步到显示器刷新率）
    g_pSwapChain->Present(1, 0);
}
```

`Present` 的第一个参数是 SyncInterval——设为 1 表示同步到 VSync（每帧等待显示器刷新），设为 0 表示不同步（尽可能快地呈现，但会产生画面撕裂）。第二个参数是标志位，通常设为 0。

## 处理窗口大小变化：ResizeBuffers

当窗口大小变化时，后备缓冲区的尺寸也需要跟着变。这个过程有一个严格的操作顺序：

```cpp
case WM_SIZE:
{
    int newWidth = LOWORD(lParam);
    int newHeight = HIWORD(lParam);

    if (newWidth == 0 || newHeight == 0) return 0;

    if (g_pContext && g_pSwapChain)
    {
        // 第一步：解除 RTV 绑定
        g_pContext->OMSetRenderTargets(0, NULL, NULL);

        // 第二步：释放旧的 RTV
        if (g_pRTV) { g_pRTV->Release(); g_pRTV = NULL; }

        // 第三步：调整交换链缓冲区大小
        g_pSwapChain->ResizeBuffers(
            2,              // 缓冲区数量（必须与创建时一致或为 0）
            newWidth,
            newHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM,  // 格式（可以为 0 表示不变）
            0
        );

        // 第四步：重新创建 RTV
        ID3D11Texture2D* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
        pBackBuffer->Release();

        // 第五步：重新绑定 RTV
        g_pContext->OMSetRenderTargets(1, &g_pRTV, NULL);

        // 第六步：更新视口
        D3D11_VIEWPORT vp = { 0, 0, (FLOAT)newWidth, (FLOAT)newHeight, 0, 1 };
        g_pContext->RSSetViewports(1, &vp);
    }
    return 0;
}
```

⚠️ 注意，调用 `ResizeBuffers` 之前，你必须确保没有任何资源还在引用后备缓冲区。这就是为什么第一步要先解除 RTV 绑定（`OMSetRenderTargets(0, NULL, NULL)`），第二步释放旧的 RTV。如果你忘记了这一步，`ResizeBuffers` 会返回 `E_INVALIDARG` 或 `DXGI_ERROR_INVALID_CALL`，然后你的画面就消失了。

如果你还有深度缓冲区（Depth Buffer），也需要在 `ResizeBuffers` 之后重新创建，因为深度缓冲区的尺寸必须和渲染目标一致。

## 清理资源

程序退出时，按照创建的逆序释放所有资源：

```cpp
void Cleanup()
{
    // 先解除绑定
    if (g_pContext) g_pContext->OMSetRenderTargets(0, NULL, NULL);

    // 释放渲染目标视图
    if (g_pRTV) { g_pRTV->Release(); g_pRTV = NULL; }

    // 释放交换链
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }

    // 释放设备上下文
    if (g_pContext) { g_pContext->Release(); g_pContext = NULL; }

    // 最后释放设备
    if (g_pDevice) { g_pDevice->Release(); g_pDevice = NULL; }
}
```

释放顺序很重要。`SwapChain` 内部持有后备缓冲区的引用，如果先释放 `SwapChain` 再释放 `RTV`，可能会导致后备缓冲区被提前销毁。先释放 `RTV`（减少引用计数），再释放 `SwapChain`，是最安全的顺序。

## 常见问题与调试

### 问题1：D3D11CreateDeviceAndSwapChain 返回 E_FAIL

检查你的 Feature Level 数组是否包含了你的 GPU 支持的版本。集成显卡通常支持 D3D_FEATURE_LEVEL_11_0，但老旧的集成显卡可能只支持到 10_0。添加多个 Feature Level 到数组中，让 D3D11 自动选择最高的可用版本。

### 问题2：ResizeBuffers 返回 DXGI_ERROR_INVALID_CALL

99% 的原因是你在调用 `ResizeBuffers` 时，还有资源持有后备缓冲区的引用。最常见的遗漏是忘记释放深度缓冲区的 DSV（Depth Stencil View），或者某个 Shader Resource View 还在引用后备缓冲区。确保在 `ResizeBuffers` 前解除所有绑定并释放所有相关视图。

### 问题3：Present 后画面撕裂

画面撕裂（Tearing）是因为 `Present(0, 0)` 不同步到 VSync，GPU 和显示器刷新不同步。改为 `Present(1, 0)` 启用 VSync 即可消除撕裂，但会引入 1-2 帧的延迟。在 Windows 10+ 上，如果你使用 Flip Model 并设置了 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` 标志，可以启用"可变刷新率"（FreeSync/G-Sync），既无撕裂又无延迟。

## 总结

到这里，D3D11 的初始化框架就搭建完成了。三个核心对象（Device/Context/SwapChain）各司其职，`ResizeBuffers` 的五步操作顺序是处理窗口大小变化的标准流程。这套框架是你所有 D3D11 程序的基础——不管是简单的清屏还是复杂的 3D 渲染，初始化代码都是一样的。

下一步，我们要在 D3D11 框架上添加实际的几何体渲染。我们已经有了设备、上下文和交换链，但 GPU 还不知道要画什么。下一篇文章会讲解顶点缓冲（Vertex Buffer）和输入布局（Input Layout）——它们是把三角形数据从 CPU 内存搬到 GPU 内存，并告诉 GPU 如何解析这些数据的关键机制。

---

## 练习

1. 修改清屏颜色，实现每帧颜色在色相环上渐变的动画效果（在 `Render` 函数中根据时间计算 `clearColor`）。
2. 在 `WM_SIZE` 处理中添加深度缓冲区的重建逻辑（创建 `ID3D11DepthStencilView` 并绑定到 `OMSetRenderTargets`）。
3. 尝试将 `SwapEffect` 改为 `DXGI_SWAP_EFFECT_FLIP_DISCARD`，对比 `DISCARD` 模式的 CPU 占用率差异（用任务管理器观察）。
4. 用 `IDXGIAdapter::EnumOutputs` 枚举系统中的所有显示器，打印每个显示器的分辨率和刷新率。

---

**参考资料**:
- [D3D11CreateDeviceAndSwapChain - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdeviceandswapchain)
- [IDXGISwapChain::ResizeBuffers - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)
- [DXGI_SWAP_CHAIN_DESC structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc)
- [IDXGISwapChain::Present - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
- [D3D_FEATURE_LEVEL enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_feature_level)
