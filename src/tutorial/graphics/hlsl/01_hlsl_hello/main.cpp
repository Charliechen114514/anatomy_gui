/**
 * 01_hlsl_hello - HLSL 入门示例
 *
 * 本程序演示了 HLSL 着色器编程的最基本流程：
 * 1. D3DCompileFromFile    — 在运行时编译 .hlsl 文件（vs_5_0 / ps_5_0 目标）
 * 2. CreateVertexShader    — 从编译后的字节码创建顶点着色器
 * 3. CreatePixelShader     — 从编译后的字节码创建像素着色器
 * 4. CreateInputLayout     — 创建与 HLSL 结构体匹配的输入布局（POSITION + COLOR 语义）
 * 5. IASetVertexBuffers    — 绑定顶点缓冲区
 * 6. Draw                  — 绘制一个彩色三角形
 *
 * 着色器管线流程：
 *   [顶点数据] → 顶点着色器(变换) → 光栅化(插值) → 像素着色器(着色) → [帧缓冲]
 *
 * 参考: tutorial/native_win32/34_ProgrammingGUI_Graphics_HLSL_Basics.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cstdio>

// 引入公共 COM 辅助工具（ComPtr<T> 和 ThrowIfFailed）
#include "../common/ComHelper.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"HLSLHelloClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 顶点结构体 — 必须与 HLSL 中的 VS_INPUT 匹配
// ============================================================================

// 对应 HLSL 中的：
//   struct VS_INPUT {
//       float4 pos : POSITION;
//       float4 col : COLOR;
//   };
// 注意：HLSL 中 float4 对应 C++ 中的 4 个 float（16 字节）
struct Vertex
{
    float x, y, z, w;   // POSITION — 齐次坐标位置
    float r, g, b, a;   // COLOR    — 顶点颜色 (RGBA)
};

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static ComPtr<ID3D11Device>            g_device;
static ComPtr<ID3D11DeviceContext>     g_context;
static ComPtr<IDXGISwapChain>         g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_renderTarget;

// 着色器与管线资源
static ComPtr<ID3D11VertexShader>     g_vertexShader;    // 顶点着色器
static ComPtr<ID3D11PixelShader>      g_pixelShader;     // 像素着色器
static ComPtr<ID3D11InputLayout>      g_inputLayout;     // 输入布局
static ComPtr<ID3D11Buffer>           g_vertexBuffer;    // 顶点缓冲区

// ============================================================================
// D3D11 初始化
// ============================================================================

/**
 * 创建 D3D11 设备、设备上下文和交换链
 */
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

    // 创建渲染目标视图
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "CreateRenderTargetView 失败");

    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 着色器编译 — D3DCompileFromFile 运行时编译 .hlsl 文件
// ============================================================================

/**
 * 编译着色器文件
 *
 * D3DCompileFromFile 参数说明：
 * - pFileName:     .hlsl 文件路径
 * - pDefines:      预处理器宏定义（这里为 nullptr）
 * - pInclude:      包含处理接口（这里为 nullptr）
 * - pEntrypoint:   入口函数名（如 "VSMain"、"PSMain"）
 * - pTarget:       着色器模型目标（如 "vs_5_0"、"ps_5_0"）
 * - Flags1:        编译标志
 * - Flags2:        效果编译标志（通常为 0）
 * - ppCode:        [out] 编译后的字节码
 * - ppErrorMsgs:   [out] 编译错误信息
 */
static ComPtr<ID3DBlob> CompileShaderFromFile(
    const wchar_t* fileName,
    const char* entryPoint,
    const char* shaderModel)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        fileName,           // 着色器源文件路径
        nullptr,            // 无预处理器宏
        nullptr,            // 无自定义 include 处理
        entryPoint,         // 入口函数名
        shaderModel,        // 着色器模型目标
        0,                  // 编译标志（默认优化）
        0,                  // 效果标志
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    // 如果编译失败，显示错误信息
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            // errorBlob 包含编译器输出的详细错误信息
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
 * 初始化着色器管线
 *
 * 完整的着色器编译流程：
 *   .hlsl 文件 → D3DCompileFromFile → 字节码 Blob → CreateXxxShader → GPU 着色器对象
 *
 * 输入布局必须与 HLSL 中的 VS_INPUT 结构体完全对应：
 *   HLSL: float4 pos : POSITION  →  D3D: "POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT
 *   HLSL: float4 col : COLOR     →  D3D: "COLOR",    DXGI_FORMAT_R32G32B32A32_FLOAT
 */
static bool InitShaders()
{
    // ---- 第一步：编译顶点着色器 ----
    // 目标 "vs_5_0" 表示 Shader Model 5.0 的顶点着色器
    ComPtr<ID3DBlob> vsBlob = CompileShaderFromFile(
        L"shaders/01_hlsl_hello/vertex.hlsl",
        "VSMain",       // 入口函数名
        "vs_5_0"        // 顶点着色器 5.0
    );
    if (!vsBlob) return false;

    // 从编译后的字节码创建顶点着色器对象
    HRESULT hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),   // 字节码指针
        vsBlob->GetBufferSize(),      // 字节码大小
        nullptr,                      // 无类链接库
        g_vertexShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateVertexShader 失败");

    // ---- 第二步：定义输入布局 ----
    // 输入布局描述了顶点数据在内存中的排列方式
    // 每个元素对应 HLSL VS_INPUT 中的一个成员
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        // SemanticName  SemanticIndex  Format                     InputSlot  AlignedByteOffset  InputSlotClass          InstanceDataStepRate
        { "POSITION",    0,             DXGI_FORMAT_R32G32B32A32_FLOAT, 0,         0,                 D3D11_INPUT_PER_VERTEX_DATA, 0 },  // 顶点位置（偏移 0）
        { "COLOR",       0,             DXGI_FORMAT_R32G32B32A32_FLOAT, 0,         16,                D3D11_INPUT_PER_VERTEX_DATA, 0 },  // 顶点颜色（偏移 16 = 4*4 字节）
    };

    // 使用顶点着色器的字节码创建输入布局
    // 输入布局需要与着色器签名进行验证匹配
    hr = g_device->CreateInputLayout(
        layoutDesc,                        // 输入元素描述数组
        _countof(layoutDesc),              // 元素数量
        vsBlob->GetBufferPointer(),        // 顶点着色器字节码（用于签名验证）
        vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateInputLayout 失败");

    // ---- 第三步：编译像素着色器 ----
    // 目标 "ps_5_0" 表示 Shader Model 5.0 的像素着色器
    ComPtr<ID3DBlob> psBlob = CompileShaderFromFile(
        L"shaders/01_hlsl_hello/pixel.hlsl",
        "PSMain",       // 入口函数名
        "ps_5_0"        // 像素着色器 5.0
    );
    if (!psBlob) return false;

    // 从编译后的字节码创建像素着色器对象
    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        g_pixelShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreatePixelShader 失败");

    return true;
}

// ============================================================================
// 顶点数据初始化
// ============================================================================

/**
 * 创建一个彩色三角形的顶点缓冲区
 *
 * 三角形顶点在标准化设备坐标 (NDC) 中定义：
 *   X: [-1, 1] 从左到右
 *   Y: [-1, 1] 从下到上
 *   Z: [0, 1]  从近到远
 *
 * 三个顶点分别着红、绿、蓝色，光栅化器会自动在三角形内插值颜色
 */
static void InitVertexBuffer()
{
    // 定义三角形的三个顶点
    // 每个顶点包含位置 (x,y,z,w) 和颜色 (r,g,b,a)
    Vertex vertices[] = {
        // 位置 (x,    y,    z,   w)                      颜色 (r,   g,   b,   a)
        {  0.0f,  0.5f, 0.0f, 1.0f,    1.0f, 0.0f, 0.0f, 1.0f },  // 顶部 — 红色
        {  0.5f, -0.5f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f, 1.0f },  // 右下 — 绿色
        { -0.5f, -0.5f, 0.0f, 1.0f,    0.0f, 0.0f, 1.0f, 1.0f },  // 左下 — 蓝色
    };

    // 创建顶点缓冲区
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth      = sizeof(vertices);                      // 缓冲区大小（字节）
    bufferDesc.Usage          = D3D11_USAGE_IMMUTABLE;                 // 不可变（数据不会改变）
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;             // 绑定为顶点缓冲区
    bufferDesc.CPUAccessFlags = 0;                                     // CPU 不访问

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;                                       // 初始数据指针

    HRESULT hr = g_device->CreateBuffer(
        &bufferDesc,
        &initData,
        g_vertexBuffer.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateBuffer (顶点缓冲区) 失败");
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
 * 完整的渲染管线流程：
 *   1. 清除渲染目标
 *   2. 设置输入布局（告诉 GPU 如何解读顶点数据）
 *   3. 绑定顶点缓冲区
 *   4. 设置图元拓扑（三角形列表）
 *   5. 设置顶点/像素着色器
 *   6. 设置视口
 *   7. 调用 Draw 绘制
 *   8. Present 呈现
 */
static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 清除渲染目标为深灰色背景
    float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 设置输入布局 — 告诉 IA（输入装配器）如何解读顶点数据
    g_context->IASetInputLayout(g_inputLayout.Get());

    // 绑定顶点缓冲区到输入装配器
    UINT stride = sizeof(Vertex);   // 每个顶点的字节大小
    UINT offset = 0;
    g_context->IASetVertexBuffers(
        0,                              // 输入槽索引
        1,                              // 缓冲区数量
        g_vertexBuffer.GetAddressOf(),  // 缓冲区指针
        &stride,                        // 每个顶点的步长
        &offset                         // 起始偏移
    );

    // 设置图元拓扑 — 告诉 GPU 如何将顶点组合成图元
    // TRIANGLELIST: 每三个顶点构成一个三角形
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置着色器
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);   // 顶点着色器
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);    // 像素着色器

    // 设置视口（整个窗口区域）
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = (FLOAT)WINDOW_WIDTH;
    viewport.Height   = (FLOAT)WINDOW_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &viewport);

    // 绘制 — 提交 3 个顶点（一个三角形）
    g_context->Draw(3, 0);

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

    case WM_DESTROY:
    {
        // 释放所有 D3D11 资源（ComPtr 自动 Release）
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
 * 着色器编译流程图：
 *
 *   ┌─────────────┐    D3DCompileFromFile     ┌──────────────┐
 *   │ vertex.hlsl │ ──────────────────────→   │  字节码 Blob  │
 *   └─────────────┘    (入口: VSMain,         └──────┬───────┘
 *   ┌─────────────┐     目标: vs_5_0)                │
 *   │ pixel.hlsl  │ ──────────────────────→   ┌──────┴───────┐
 *   └─────────────┘    (入口: PSMain,         │ CreateXxxShader│
 *                       目标: ps_5_0)          └──────┬───────┘
 *                                                   │
 *                                    ┌──────────────┼──────────────┐
 *                                    ▼              ▼              ▼
 *                              ┌──────────┐  ┌──────────┐  ┌──────────┐
 *                              │ 顶点着色器 │  │ 像素着色器 │  │ 输入布局  │
 *                              └──────────┘  └──────────┘  └──────────┘
 *                                    │              │              │
 *                                    └──────┬───────┘              │
 *                                           ▼                      ▼
 *                                    ┌──────────────────────────────────┐
 *                                    │         渲染管线绑定              │
 *                                    │  IA → VS → 光栅化 → PS → OM     │
 *                                    └──────────────────────────────────┘
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
        L"HLSL 入门示例",
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

    // 编译着色器并创建管线资源
    if (!InitShaders())
    {
        // 着色器编译失败，错误信息已在 CompileShaderFromFile 中显示
        CoUninitialize();
        return 1;
    }

    // 创建顶点缓冲区
    InitVertexBuffer();

    // ========================================================================
    // 消息循环（PeekMessage 模式 — 空闲时持续渲染）
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
