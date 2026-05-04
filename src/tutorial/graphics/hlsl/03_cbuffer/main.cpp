/**
 * 03_cbuffer - HLSL Constant Buffer 示例
 *
 * 本程序演示了 HLSL 常量缓冲区（Constant Buffer）的完整使用流程：
 * 1. D3D11_BUFFER_DESC      — 配置常量缓冲区（DYNAMIC 用法，CPU_ACCESS_WRITE）
 * 2. CreateBuffer            — 创建常量缓冲区（16 字节对齐）
 * 3. Map/Unmap               — 动态更新常量缓冲区数据（D3D11_MAP_WRITE_DISCARD）
 * 4. VSSetConstantBuffers    — 将常量缓冲区绑定到顶点着色器
 * 5. PSSetConstantBuffers    — 将常量缓冲区绑定到像素着色器
 * 6. UpdateSubresource       — 替代 Map/Unmap 的另一种更新方式
 *
 * 常量缓冲区是 CPU 向 GPU 着色器传递数据的标准方式：
 *   CPU 更新数据 → Map/Unmap → 常量缓冲区 → GPU 着色器读取
 *
 * 本示例渲染一个全屏四边形，其颜色基于时间值通过 cbuffer 传递给像素着色器，
 * 像素着色器使用 sin 函数计算动画颜色。
 *
 * 参考: tutorial/native_win32/36_ProgrammingGUI_Graphics_HLSL_CBuffer.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cstdio>

// 引入公共 COM 辅助工具
#include "../common/ComHelper.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"HLSLCBufferClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 顶点结构体 — 全屏四边形只需位置，颜色由 cbuffer 驱动
// ============================================================================

struct Vertex
{
    float x, y, z, w;   // POSITION
};

// ============================================================================
// 常量缓冲区结构体 — 必须与 HLSL 中的 cbuffer 匹配，且 16 字节对齐
// ============================================================================

// 对应 HLSL 中的：
//   cbuffer TimeColorBuffer : register(b0)
//   {
//       float4 TimeColor;
//       float  TimeValue;
//       float3 Padding;
//   };
//
// 重要：HLSL 常量缓冲区要求 16 字节对齐！
// 每个成员的偏移量必须是 16 字节的倍数，或者打包在一个 16 字节组内。
// 这里 float4(16字节) + float(4字节) + float3(12字节) = 32 字节 = 2 * 16
struct alignas(16) CBuffer
{
    float timeColor[4];   // TimeColor — 动画颜色（float4，16 字节）
    float timeValue;      // TimeValue  — 经过的秒数（float，4 字节）
    float padding[3];     // Padding    — 填充至 16 字节边界（3 * float = 12 字节）
};

static_assert(sizeof(CBuffer) == 32, "CBuffer 大小必须为 32 字节（16 字节对齐）");

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static ComPtr<ID3D11Device>            g_device;
static ComPtr<ID3D11DeviceContext>     g_context;
static ComPtr<IDXGISwapChain>         g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_renderTarget;

static ComPtr<ID3D11VertexShader>     g_vertexShader;
static ComPtr<ID3D11PixelShader>      g_pixelShader;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;
static ComPtr<ID3D11Buffer>           g_constantBuffer;   // 常量缓冲区

// 计时器
static DWORD g_startTime = 0;

// 更新方式标志：true = Map/Unmap, false = UpdateSubresource
static bool g_useMapUnmap = true;

// ============================================================================
// D3D11 初始化
// ============================================================================

static void InitD3D11(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount       = 1;
    swapChainDesc.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.SampleDesc.Count   = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.OutputWindow       = hwnd;
    swapChainDesc.Windowed           = TRUE;
    swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, driverType, nullptr, 0,
        featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &g_swapChain,
        &g_device,
        &selectedFeatureLevel,
        &g_context
    );
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain 失败");

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "CreateRenderTargetView 失败");

    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 着色器编译
// ============================================================================

/**
 * 编译着色器文件
 *
 * 本示例使用单个 cbuffer.hlsl 文件包含顶点和像素着色器
 * 通过不同的入口函数名区分：VSMain（顶点）和 PSMain（像素）
 */
static ComPtr<ID3DBlob> CompileShaderFromFile(
    const wchar_t* fileName,
    const char* entryPoint,
    const char* shaderModel)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        fileName,
        nullptr, nullptr,
        entryPoint,
        shaderModel,
        0, 0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());

            char msg[512];
            snprintf(msg, sizeof(msg),
                "着色器编译失败!\n\n文件: %ls\n入口: %s\n目标: %s\n\n错误:\n%s",
                fileName, entryPoint, shaderModel,
                (const char*)errorBlob->GetBufferPointer());
            MessageBoxA(nullptr, msg, "HLSL 编译错误", MB_ICONERROR | MB_OK);
        }
        else
        {
            char msg[256];
            snprintf(msg, sizeof(msg),
                "无法打开着色器文件: %ls\n请确认 shaders 目录位于可执行文件同级目录下。",
                fileName);
            MessageBoxA(nullptr, msg, "文件错误", MB_ICONERROR | MB_OK);
        }
        PostQuitMessage(1);
        return nullptr;
    }

    return shaderBlob;
}

// ============================================================================
// 着色器与管线初始化
// ============================================================================

/**
 * 初始化着色器和常量缓冲区
 *
 * 常量缓冲区创建要点：
 * - ByteWidth 必须是 16 的倍数（HLSL 常量缓冲区要求）
 * - Usage = D3D11_USAGE_DYNAMIC — 允许 CPU 每帧更新数据
 * - CPUAccessFlags = D3D11_CPU_ACCESS_WRITE — CPU 需要写入权限
 * - BindFlags = D3D11_BIND_CONSTANT_BUFFER — 绑定为常量缓冲区
 */
static bool InitShaders()
{
    // ---- 编译顶点着色器 ----
    ComPtr<ID3DBlob> vsBlob = CompileShaderFromFile(
        L"shaders/03_cbuffer/cbuffer.hlsl",
        "VSMain",
        "vs_5_0"
    );
    if (!vsBlob) return false;

    HRESULT hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        g_vertexShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateVertexShader 失败");

    // ---- 定义输入布局（只需要 POSITION，无 COLOR） ----
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_device->CreateInputLayout(
        layoutDesc, _countof(layoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateInputLayout 失败");

    // ---- 编译像素着色器 ----
    ComPtr<ID3DBlob> psBlob = CompileShaderFromFile(
        L"shaders/03_cbuffer/cbuffer.hlsl",
        "PSMain",
        "ps_5_0"
    );
    if (!psBlob) return false;

    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        g_pixelShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreatePixelShader 失败");

    // ---- 创建常量缓冲区 ----
    //
    // D3D11_BUFFER_DESC 关键参数：
    //   ByteWidth      — 缓冲区大小（字节），必须是 16 的倍数
    //   Usage          — D3D11_USAGE_DYNAMIC 表示数据会频繁更新
    //   BindFlags      — D3D11_BIND_CONSTANT_BUFFER 表示这是常量缓冲区
    //   CPUAccessFlags — D3D11_CPU_ACCESS_WRITE 允许 CPU 写入
    //
    // 为什么用 DYNAMIC 而不是 DEFAULT？
    //   DEFAULT + UpdateSubresource: CPU 通过临时拷贝更新（适合不频繁更新）
    //   DYNAMIC + Map/Unmap:         CPU 直接映射内存更新（适合每帧更新，性能更好）
    D3D11_BUFFER_DESC cbufferDesc = {};
    cbufferDesc.ByteWidth      = sizeof(CBuffer);                       // 32 字节（16 字节对齐）
    cbufferDesc.Usage          = D3D11_USAGE_DYNAMIC;                   // 动态用法（频繁更新）
    cbufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;            // 常量缓冲区
    cbufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;                // CPU 写入权限
    cbufferDesc.MiscFlags      = 0;
    cbufferDesc.StructureByteStride = 0;

    // 创建空的常量缓冲区（数据在每帧渲染时更新）
    hr = g_device->CreateBuffer(
        &cbufferDesc,
        nullptr,                   // 无初始数据
        g_constantBuffer.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateBuffer (常量缓冲区) 失败");

    return true;
}

// ============================================================================
// 全屏四边形顶点数据初始化
// ============================================================================

/**
 * 创建全屏四边形的顶点缓冲区
 *
 * 使用两个三角形组成一个覆盖整个屏幕的四边形
 * 顶点坐标在 NDC 空间中：X: [-1, 1], Y: [-1, 1]
 */
static void InitVertexBuffer()
{
    // 全屏四边形 = 两个三角形
    Vertex vertices[] = {
        // 第一个三角形（上半部分 + 右下角）
        { -1.0f,  1.0f, 0.0f, 1.0f },   // 左上
        {  1.0f,  1.0f, 0.0f, 1.0f },   // 右上
        {  1.0f, -1.0f, 0.0f, 1.0f },   // 右下

        // 第二个三角形（左下角 + 上半部分）
        { -1.0f,  1.0f, 0.0f, 1.0f },   // 左上
        {  1.0f, -1.0f, 0.0f, 1.0f },   // 右下
        { -1.0f, -1.0f, 0.0f, 1.0f },   // 左下
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth      = sizeof(vertices);
    bufferDesc.Usage          = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    HRESULT hr = g_device->CreateBuffer(
        &bufferDesc, &initData, g_vertexBuffer.GetAddressOf());
    ThrowIfFailed(hr, "CreateBuffer (顶点缓冲区) 失败");
}

// ============================================================================
// 更新常量缓冲区
// ============================================================================

/**
 * 方式一：Map/Unmap 更新常量缓冲区（推荐用于每帧更新）
 *
 * 工作原理：
 * 1. Map — 获取指向 GPU 缓冲区内存的指针（使用 DISCARD 丢弃旧数据）
 * 2. 写入数据到映射的内存
 * 3. Unmap — 通知 GPU 数据已更新，可以读取
 *
 * D3D11_MAP_WRITE_DISCARD 的含义：
 *   丢弃旧的缓冲区内容，返回一个新的内存指针
 *   这样 GPU 可以继续读取上一帧的数据，同时 CPU 写入新数据
 *   避免了 CPU-GPU 同步等待
 */
static void UpdateConstantBuffer_MapUnmap(float timeValue)
{
    // 填充常量缓冲区数据
    CBuffer cbufferData = {};
    cbufferData.timeValue = timeValue;
    // timeColor 在本示例中未使用，但保留结构完整性
    cbufferData.timeColor[0] = 0.0f;
    cbufferData.timeColor[1] = 0.0f;
    cbufferData.timeColor[2] = 0.0f;
    cbufferData.timeColor[3] = 1.0f;
    cbufferData.padding[0]   = 0.0f;
    cbufferData.padding[1]   = 0.0f;
    cbufferData.padding[2]   = 0.0f;

    // 映射常量缓冲区，获取可写的内存指针
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_context->Map(
        g_constantBuffer.Get(),          // 要映射的缓冲区
        0,                               // 子资源索引
        D3D11_MAP_WRITE_DISCARD,         // 映射类型：丢弃旧数据，获取新指针
        0,                               // 映射标志
        &mappedResource                  // [out] 映射后的内存指针
    );
    ThrowIfFailed(hr, "Map (常量缓冲区) 失败");

    // 将数据复制到映射的内存
    memcpy(mappedResource.pData, &cbufferData, sizeof(CBuffer));

    // 解除映射，通知 GPU 数据已更新
    g_context->Unmap(g_constantBuffer.Get(), 0);
}

/**
 * 方式二：UpdateSubresource 更新常量缓冲区（适合不频繁更新）
 *
 * 工作原理：
 *   CPU 将数据拷贝到一个临时位置，GPU 在下次使用时自动读取
 *   比 Map/Unmap 多一次内存拷贝，但不需要 DYNAMIC 用法
 *
 * 适用场景：
 *   - 数据不频繁更新（如每几帧更新一次）
 *   - 缓冲区使用 D3D11_USAGE_DEFAULT 而非 DYNAMIC
 *
 * 注意：本示例的常量缓冲区使用 DYNAMIC 用法，
 *       但 UpdateSubresource 仍然可以使用（只是不是最优选择）
 */
static void UpdateConstantBuffer_UpdateSubresource(float timeValue)
{
    CBuffer cbufferData = {};
    cbufferData.timeValue = timeValue;
    cbufferData.timeColor[0] = 0.0f;
    cbufferData.timeColor[1] = 0.0f;
    cbufferData.timeColor[2] = 0.0f;
    cbufferData.timeColor[3] = 1.0f;
    cbufferData.padding[0]   = 0.0f;
    cbufferData.padding[1]   = 0.0f;
    cbufferData.padding[2]   = 0.0f;

    // UpdateSubresource 直接将数据复制到缓冲区
    g_context->UpdateSubresource(
        g_constantBuffer.Get(),          // 目标缓冲区
        0,                               // 子资源索引
        nullptr,                         // 目标盒（nullptr = 整个资源）
        &cbufferData,                    // 源数据指针
        sizeof(CBuffer),                 // 源数据行间距（2D/3D 纹理用）
        0                                // 源数据深度间距（3D 纹理用）
    );
}

// ============================================================================
// 窗口大小调整
// ============================================================================

static void OnResize(UINT width, UINT height)
{
    if (width == 0 || height == 0 || !g_swapChain)
        return;

    g_renderTarget.Reset();

    HRESULT hr = g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    ThrowIfFailed(hr, "ResizeBuffers 失败");

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "Resize 后 GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "Resize 后 CreateRenderTargetView 失败");

    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 渲染
// ============================================================================

/**
 * 每帧渲染函数
 *
 * 常量缓冲区更新流程：
 *   CPU 计算时间值 → 更新 CBuffer → Map/Unmap 或 UpdateSubresource →
 *   绑定到着色器 → GPU 着色器读取 cbuffer 数据 → 计算颜色
 */
static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 计算经过的时间（秒）
    float timeValue = (GetTickCount() - g_startTime) / 1000.0f;

    // ---- 更新常量缓冲区 ----
    // 使用 Map/Unmap 方式（推荐用于每帧更新）
    if (g_useMapUnmap)
    {
        UpdateConstantBuffer_MapUnmap(timeValue);
    }
    else
    {
        UpdateConstantBuffer_UpdateSubresource(timeValue);
    }

    // 清除渲染目标
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 设置输入布局
    g_context->IASetInputLayout(g_inputLayout.Get());

    // 绑定顶点缓冲区
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);

    // 设置图元拓扑
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ---- 绑定常量缓冲区到着色器 ----
    // VSSetConstantBuffers: 绑定到顶点着色器的寄存器 b0
    // PSSetConstantBuffers: 绑定到像素着色器的寄存器 b0
    // 对应 HLSL: cbuffer TimeColorBuffer : register(b0)
    g_context->VSSetConstantBuffers(
        0,                              // 起始寄存器索引（b0）
        1,                              // 绑定数量
        g_constantBuffer.GetAddressOf()
    );
    g_context->PSSetConstantBuffers(
        0,                              // 起始寄存器索引（b0）
        1,                              // 绑定数量
        g_constantBuffer.GetAddressOf()
    );

    // 设置着色器
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

    // 设置视口
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = (FLOAT)WINDOW_WIDTH;
    viewport.Height   = (FLOAT)WINDOW_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &viewport);

    // 绘制全屏四边形（6 个顶点 = 2 个三角形）
    g_context->Draw(6, 0);

    // 呈现
    HRESULT hr = g_swapChain->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        MessageBox(nullptr,
            L"GPU 设备已被移除或重置。\n程序即将退出。",
            L"设备错误", MB_ICONERROR | MB_OK);
        PostQuitMessage(1);
    }
}

// ============================================================================
// 窗口过程
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
    {
        UINT width  = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        OnResize(width, height);
        return 0;
    }

    // 按空格键切换 Map/Unmap 和 UpdateSubresource 两种更新方式
    case WM_KEYDOWN:
    {
        if (wParam == VK_SPACE)
        {
            g_useMapUnmap = !g_useMapUnmap;
            const char* method = g_useMapUnmap ? "Map/Unmap" : "UpdateSubresource";
            char msg[128];
            snprintf(msg, sizeof(msg), "常量缓冲区更新方式已切换为: %s", method);
            OutputDebugStringA(msg);
            OutputDebugStringA("\n");

            // 设置窗口标题以显示当前方式
            const wchar_t* title = g_useMapUnmap
                ? L"HLSL Constant Buffer 示例 [Map/Unmap]"
                : L"HLSL Constant Buffer 示例 [UpdateSubresource]";
            SetWindowText(hwnd, title);
        }
        return 0;
    }

    case WM_DESTROY:
    {
        g_constantBuffer.Reset();
        g_vertexBuffer.Reset();
        g_inputLayout.Reset();
        g_pixelShader.Reset();
        g_vertexShader.Reset();
        g_renderTarget.Reset();
        g_context.Reset();
        g_swapChain.Reset();
        g_device.Reset();

        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

static ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 程序入口
// ============================================================================

/**
 * WinMain — 程序入口点
 *
 * 常量缓冲区数据流图：
 *
 *   ┌─────────────┐
 *   │  CPU 端数据  │
 *   │ (CBuffer)   │
 *   │  timeValue  │
 *   └──────┬──────┘
 *          │
 *     ┌────┴─────┐
 *     │ 更新方式  │
 *     └────┬─────┘
 *          │
 *   ┌──────┴──────────────────────────┐
 *   │                                  │
 *   ▼                                  ▼
 * ┌─────────────────┐    ┌──────────────────────┐
 * │   Map/Unmap     │    │  UpdateSubresource   │
 * │ (DYNAMIC 用法)  │    │  (DEFAULT 用法)      │
 * │                 │    │                      │
 * │ 1. Map(DISCARD) │    │ CPU → 临时拷贝 → GPU │
 * │ 2. memcpy       │    │                      │
 * │ 3. Unmap        │    │ 适合不频繁更新       │
 * │                 │    │                      │
 * │ 适合每帧更新    │    │                      │
 * └────────┬────────┘    └──────────┬───────────┘
 *          │                        │
 *          └───────────┬────────────┘
 *                      ▼
 *          ┌───────────────────────┐
 *          │  GPU 常量缓冲区       │
 *          │  register(b0)         │
 *          └───────────┬───────────┘
 *                      │
 *          ┌───────────┴───────────┐
 *          ▼                       ▼
 *   ┌─────────────┐        ┌─────────────┐
 *   │  顶点着色器  │        │  像素着色器  │
 *   │  VSMain     │        │  PSMain     │
 *   │  读取 cbuffer│       │  读取 TimeValue│
 *   └─────────────┘        │  计算动画颜色 │
 *                          └─────────────┘
 *
 * 按空格键可切换 Map/Unmap 和 UpdateSubresource 两种更新方式
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ThrowIfFailed(hr, "CoInitializeEx 失败");

    // 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"HLSL Constant Buffer 示例 [Map/Unmap]",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 初始化 D3D11
    InitD3D11(hwnd);

    // 初始化着色器和常量缓冲区
    if (!InitShaders())
    {
        CoUninitialize();
        return 1;
    }

    // 创建全屏四边形顶点缓冲区
    InitVertexBuffer();

    // 记录起始时间
    g_startTime = GetTickCount();

    // ========================================================================
    // 消息循环
    // ========================================================================
    MSG msg = {};
    bool running = true;

    while (running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (running)
        {
            Render();
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}
