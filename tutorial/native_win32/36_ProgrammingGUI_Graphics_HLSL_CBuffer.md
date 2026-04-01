# 通用GUI编程技术——图形渲染实战（三十六）——Constant Buffer与数据传递：CPU-GPU通信通道

> 在上一篇文章中，我们聊了 HLSL 的编译和调试——运行时编译用 `D3DCompileFromFile`，离线编译用 `fxc` 或 `dxc`，调试用 PIX 或 RenderDoc。但当时我们故意回避了一个核心问题：CPU 端的 C++ 代码怎么把数据传递给 GPU 端的 Shader？Shader 里声明的 `g_WorldViewProj` 矩阵、`g_LightPos` 光源位置、`g_Time` 时间变量——这些"全局变量"的值从哪里来？
>
> 答案就是 Constant Buffer（常量缓冲区，简称 CBuffer）。它是 CPU 和 GPU 之间传递小量参数的标准通道。今天我们要深入理解 CBuffer 的内存对齐规则、数据更新方式，以及一个会让你调试到怀疑人生的 float3 对齐陷阱。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11（链接 `d3d11.lib`、`d3dcompiler.lib`）
- **前置知识**: 文章 34（HLSL 基础）、文章 35（HLSL 编译调试）

## Constant Buffer 是什么

在 HLSL 中，你可以用 `cbuffer` 关键字声明一组"常量"变量：

```hlsl
cbuffer PerFrameBuffer : register(b0) {
    matrix g_WorldViewProj;  // 世界-视图-投影矩阵
    float4  g_Color;         // 颜色
    float   g_Time;          // 时间
    float3  g_Padding;       // 对齐填充
};
```

这些变量看起来像是全局变量，但它们实际上存储在一块特殊的 GPU 缓冲区中——这就是 Constant Buffer。CPU 端通过 `ID3D11Buffer` 创建这块缓冲区并填充数据，然后绑定到渲染管线，GPU 端的 Shader 就能读取这些数据了。

`: register(b0)` 是寄存器绑定声明，表示这个 CBuffer 绑定到 slot 0。一个 Shader 最多可以使用 14 个 CBuffer（b0 到 b13），不过实际上你很少需要超过 3-4 个。

常见的 CBuffer 分组策略是按照更新频率来划分：`PerFrame`（每帧更新一次，如视图矩阵、光源方向）、`PerObject`（每个物体更新一次，如世界矩阵、物体颜色）、`Rarely`（很少更新，如屏幕分辨率、全局参数）。这种分组方式可以最小化数据传输量。

## 16 字节对齐规则

CBuffer 的内存布局有一套严格的对齐规则，理解它是最重要的一步。规则核心如下：

第一，CBuffer 的总大小必须是 16 字节的整数倍。如果你的 CBuffer 包含 13 个 float（52 字节），GPU 会自动补齐到 64 字节。

第二，每个变量按照其大小进行对齐。`float` 对齐到 4 字节边界，`float2` 对齐到 8 字节，`float3` 和 `float4` 都对齐到 16 字节边界。

第三，`matrix`（即 `float4x4`）占用 64 字节（4 × 4 × 4），对齐到 16 字节边界。HLSL 默认使用列主序（column-major），但 DirectXMath 使用行主序（row-major），所以 C++ 端传矩阵时需要转置。

第四，也是最容易踩坑的一点：`float3` 后面紧跟一个 `float` 时，`float3` 实际上占据 16 字节而不是 12 字节。这是因为 `float3` 的对齐要求是 16 字节，编译器会在后面插入 4 字节的 padding。

### 对齐陷阱示例

```hlsl
// ❌ 危险布局：float3 后面跟 float
cbuffer BadBuffer : register(b0) {
    float3 g_Position;  // offset 0, 占 16 字节 (12 + 4 padding)
    float  g_Scale;     // offset 16, 占 4 字节
};  // 总大小：20 字节，补齐到 32 字节
```

对应的 C++ 结构体必须完全匹配：

```cpp
// ✅ 正确：匹配 HLSL 的布局
struct BadBufferCPU {
    XMFLOAT3 position;    // offset 0, 占 12 字节
    float    padding;     // offset 12, 手动填充 4 字节
    float    scale;       // offset 16, 占 4 字节
    float    pad[3];      // offset 20, 补齐到 32 字节
};
```

或者更好的做法是在 HLSL 端避免这种布局，改为：

```hlsl
// ✅ 安全布局：用 float4 代替 float3 + float
cbuffer GoodBuffer : register(b0) {
    float4 g_PosAndScale;  // xyz = position, w = scale
};  // 总大小：16 字节
```

⚠️ 注意，如果你在 HLSL 中 `float3` 后面跟 `float`，而 C++ 端的对应结构体没有手动加 padding，GPU 读取到的 `g_Scale` 值会错位——它读到的是 padding 字节的值而不是你期望的值。这种 Bug 不会报错，不会崩溃，画面只是"看起来不对"，排查起来非常痛苦。

## 创建和更新 Constant Buffer

### 创建 CBuffer

```cpp
// 定义 C++ 端结构体（匹配 HLSL cbuffer 布局）
struct PerFrameCB {
    XMFLOAT4X4 worldViewProj;  // 64 字节
    XMFLOAT4   color;          // 16 字节
    float      time;           // 4 字节
    float      pad[3];         // 12 字节 padding，补齐到 96 字节
};
// static_assert 验证大小
static_assert(sizeof(PerFrameCB) % 16 == 0, "CBuffer 大小必须是 16 的倍数");

// 创建缓冲区
ID3D11Buffer* pConstantBuffer = NULL;
D3D11_BUFFER_DESC bd = {};
bd.ByteWidth = sizeof(PerFrameCB);
bd.Usage = D3D11_USAGE_DYNAMIC;              // 动态使用
bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;   // 绑定为 CBuffer
bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // CPU 可写

HRESULT hr = pDevice->CreateBuffer(&bd, NULL, &pConstantBuffer);
if (FAILED(hr)) {
    // 创建失败处理
}
```

关键参数解释：`D3D11_USAGE_DYNAMIC` 表示缓冲区内容会被频繁更新（每帧或每个物体），GPU 允许 CPU 写入。`D3D11_BIND_CONSTANT_BUFFER` 指定这是一个 CBuffer。`D3D11_CPU_ACCESS_WRITE` 允许 CPU 通过 `Map` 写入数据。

### 更新 CBuffer：Map/Unmap vs UpdateSubresource

D3D11 提供两种更新 CBuffer 数据的方式：

**方式一：Map + DISCARD（推荐用于频繁更新）**

```cpp
void UpdatePerFrameCB(ID3D11DeviceContext* pContext,
                      ID3D11Buffer* pCB,
                      const PerFrameCB& data)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = pContext->Map(pCB, 0,
        D3D11_MAP_WRITE_DISCARD,  // 关键参数！
        0, &mapped);

    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, &data, sizeof(PerFrameCB));
        pContext->Unmap(pCB, 0);
    }
}
```

`D3D11_MAP_WRITE_DISCARD` 是最重要的参数。它的语义是："我不关心缓冲区里原来的内容，给我一块新的内存区域写入。" GPU 内部会管理一个环形缓冲区（Ring Buffer），每次 `MAP_WRITE_DISCARD` 会给你一个新的槽位，GPU 可以继续读取旧的槽位而不被阻塞。这是实现 CPU-GPU 并行的标准模式。

⚠️ 注意，千万不要用 `D3D11_MAP_WRITE` 代替 `D3D11_MAP_WRITE_DISCARD`。`MAP_WRITE` 要求 GPU 完成当前帧对该缓冲区的所有读取操作后才能返回，这会导致 CPU 等待 GPU——也就是你拼命想避免的管线停顿（Pipeline Stall）。

**方式二：UpdateSubresource（适合不频繁的更新）**

```cpp
pContext->UpdateSubresource(pConstantBuffer, 0, NULL, &data, 0, 0);
```

`UpdateSubresource` 更简洁，一行代码搞定。但它内部会做一次额外的内存拷贝（CPU 端先拷贝到驱动管理的临时缓冲区，然后在合适的时机提交到 GPU）。对于每帧都更新的数据，这个额外拷贝是浪费的。`UpdateSubresource` 更适合初始化时或很少更新的场景。

## 绑定 CBuffer 到渲染管线

创建和更新 CBuffer 后，需要将它绑定到渲染管线的对应阶段：

```cpp
// 绑定到 Vertex Shader 的 slot 0
pContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);

// 绑定到 Pixel Shader 的 slot 0（可以是同一个，也可以是不同的）
pContext->PSSetConstantBuffers(0, 1, &pConstantBuffer);
```

第一个参数是 slot 编号，对应 HLSL 中的 `register(b0)`、`register(b1)` 等。VS 和 PS 有各自独立的 CBuffer slot，所以如果你在 VS 和 PS 中都需要同一个 CBuffer，需要分别绑定。

## 完整示例：通过 CBuffer 传递时间变量

下面是一个完整的可编译示例，通过 CBuffer 传递时间变量实现颜色随时间变化的动画：

```cpp
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// CBuffer 结构体
struct CBColor {
    XMFLOAT4 color;  // 16 字节
};
static_assert(sizeof(CBColor) % 16 == 0, "");

ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRTV = NULL;
ID3D11VertexShader* g_pVS = NULL;
ID3D11PixelShader* g_pPS = NULL;
ID3D11Buffer* g_pCB = NULL;

const char* g_psCode =
    "cbuffer CB : register(b0) {"
    "    float4 g_Color;"
    "};"
    "float4 PS_Main() : SV_TARGET {"
    "    return g_Color;"
    "}";

const char* g_vsCode =
    "float4 VS_Main(uint vid : SV_VertexID) : SV_POSITION {"
    "    return float4(-1+2*(vid&1), -1+2*(vid==0), 0, 1);"
    "}";

bool InitD3D(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = rc.right;
    scd.BufferDesc.Height = rc.bottom;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION, &scd,
        &g_pSwapChain, &g_pDevice, NULL, &g_pContext);

    ID3D11Texture2D* pBackBuf;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuf);
    g_pDevice->CreateRenderTargetView(pBackBuf, NULL, &g_pRTV);
    pBackBuf->Release();
    g_pContext->OMSetRenderTargets(1, &g_pRTV, NULL);

    D3D11_VIEWPORT vp = { 0, 0, (FLOAT)rc.right, (FLOAT)rc.bottom, 0, 1 };
    g_pContext->RSSetViewports(1, &vp);

    // 编译 Shader
    ID3DBlob* pVSBlob, *pPSBlob;
    D3DCompile(g_vsCode, strlen(g_vsCode), NULL, NULL, NULL, "VS_Main", "vs_5_0", 0, 0, &pVSBlob, NULL);
    D3DCompile(g_psCode, strlen(g_psCode), NULL, NULL, NULL, "PS_Main", "ps_5_0", 0, 0, &pPSBlob, NULL);

    g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVS);
    g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPS);
    pVSBlob->Release(); pPSBlob->Release();

    // 创建 CBuffer
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(CBColor);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_pDevice->CreateBuffer(&bd, NULL, &g_pCB);

    return true;
}

void Render(float time)
{
    // 更新 CBuffer：颜色随时间变化
    float r = 0.5f + 0.5f * sinf(time);
    float g = 0.5f + 0.5f * sinf(time + 2.094f);
    float b = 0.5f + 0.5f * sinf(time + 4.189f);

    CBColor cb = { XMFLOAT4(r, g, b, 1.0f) };
    D3D11_MAPPED_SUBRESOURCE ms;
    g_pContext->Map(g_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, &cb, sizeof(CBColor));
    g_pContext->Unmap(g_pCB, 0);

    // 绑定
    g_pContext->VSSetShader(g_pVS, NULL, 0);
    g_pContext->PSSetShader(g_pPS, NULL, 0);
    g_pContext->PSSetConstantBuffers(0, 1, &g_pCB);

    // 清屏 + 绘制
    g_pContext->ClearRenderTargetView(g_pRTV, XMFLOAT4(0, 0, 0, 1).m);
    g_pContext->Draw(3, 0);
    g_pSwapChain->Present(1, 0);
}
```

这个示例中，`Render` 函数每帧被调用一次。它先计算基于时间的三色正弦波值（产生缓慢变化的彩色效果），然后通过 `Map/Unmap` 更新 CBuffer，绑定到 PS，最后绘制一个全屏三角形。你会发现窗口的颜色在红绿蓝之间平滑变化。

## 常见问题与调试

### 问题1：CBuffer 值在 Shader 中全是零

检查你是否调用了 `VSSetConstantBuffers` 或 `PSSetConstantBuffers` 将缓冲区绑定到管线。创建 CBuffer 和填充数据只是第一步，不绑定的话 Shader 读到的就是默认值零。

### 问题2：float3 对齐导致数据错位

这是最常见的 CBuffer Bug。如果你的 HLSL 中有 `float3` 紧跟 `float`，C++ 结构体中必须在 `float3` 后面手动加一个 padding float。或者更好的做法是在 HLSL 中避免这种布局，改用 `float4`。

### 问题3：Map 返回 E_INVALIDARG

最常见的两个原因：一是 `ByteWidth` 不是 16 的倍数（CBuffer 大小必须对齐到 16 字节），二是 Buffer 的 Usage 不是 `DYNAMIC` 或 CPUAccessFlags 没有包含 `WRITE`。创建 CBuffer 时确保 `Usage = D3D11_USAGE_DYNAMIC`、`CPUAccessFlags = D3D11_CPU_ACCESS_WRITE`。

## 总结

Constant Buffer 是 CPU 和 GPU 之间传递参数的标准通道。理解它的关键在于三个要点：16 字节对齐规则（尤其是 float3 的陷阱）、`MAP_WRITE_DISCARD` 的正确使用（避免管线停顿）、以及 C++ 结构体必须和 HLSL cbuffer 布局精确匹配。

下一步，我们终于要搭建完整的 D3D11 渲染框架了。前面所有 HLSL 知识最终都要在 D3D11 的框架中运行。下一篇文章我们会从 `D3D11CreateDeviceAndSwapChain` 开始，搭建一个可复用的 D3D11 程序骨架——创建设备、管理交换链、处理窗口大小变化，所有 D3D11 程序都离不开这套初始化流程。

---

## 练习

1. 通过 Constant Buffer 传递时间变量，在 Pixel Shader 中实现彩虹色渐变（HSL → RGB 转换）。
2. 创建两个 CBuffer（PerFrame 和 PerObject），分别存储相机矩阵和物体颜色，在渲染循环中分别更新。
3. 故意制造一个 float3 对齐 Bug（不加 padding），用 Visual Studio Graphics Debugger 查看 CBuffer 中的实际值，观察数据错位。
4. 对比 `Map/WriteDiscard` 和 `UpdateSubresource` 的帧时间差异（用 `QueryPerformanceCounter` 测量），在更新频率为每帧 vs 每秒一次时分别测试。

---

**参考资料**:
- [ID3D11Device::CreateBuffer - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createbuffer)
- [ID3D11DeviceContext::Map - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)
- [Constant Buffers (DirectX HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d11-graphics-reference-resource-buffer-constant)
- [D3D11_BUFFER_DESC structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_buffer_desc)
- [Packing Rules for Constant Variables - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules)
