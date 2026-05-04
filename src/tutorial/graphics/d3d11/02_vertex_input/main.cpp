/**
 * 02_vertex_input - D3D11 顶点缓冲与输入布局
 *
 * 本程序在 01_init_swapchain 基础上，演示如何使用 D3D11 绘制几何图形：
 * 1. 定义顶点结构体 { float x, y, z; float r, g, b, a; }
 * 2. CreateBuffer (D3D11_BIND_VERTEX_BUFFER) — 创建顶点缓冲
 * 3. IASetVertexBuffers + IASetPrimitiveTopology — 绑定顶点缓冲并设置图元拓扑
 * 4. D3DCompileFromFile — 运行时编译 HLSL 着色器
 * 5. CreateVertexShader + CreatePixelShader — 创建着色器对象
 * 6. CreateInputLayout — 根据着色器输入签名创建输入布局（POSITION + COLOR 语义）
 * 7. VSSetShader + PSSetShader + Draw(3, 0) — 设置着色器并绘制
 *
 * 渲染结果：一个彩色三角形，三个顶点分别为红、绿、蓝，
 * GPU 自动在三角形内部进行颜色插值，形成渐变效果。
 *
 * 参考: tutorial/native_win32/38_ProgrammingGUI_Graphics_D3D11_VertexInput.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

// 引入公共 COM 辅助工具（ComPtr<T> 和 ThrowIfFailed）
#include "../common/ComHelper.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D3D11VertexInputClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 顶点结构体定义
// ============================================================================

/**
 * 简单顶点结构体：位置 + 颜色
 *
 * - x, y, z: 顶点在裁剪空间中的位置（-1 到 1 为可见范围）
 * - r, g, b, a: 顶点颜色（红绿蓝 + 透明度，范围 0.0 ~ 1.0）
 *
 * 注意：裁剪空间中 z=0 表示近裁剪面，这里所有顶点 z=0。
 */
struct Vertex
{
    float x, y, z;     // 位置 (POSITION 语义)
    float r, g, b, a;  // 颜色 (COLOR 语义)
};

// 三个顶点构成一个三角形
// 顶点 0 (顶部) = 红色，顶点 1 (左下) = 绿色，顶点 2 (右下) = 蓝色
static const Vertex TRIANGLE_VERTICES[] =
{
    {  0.0f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f, 1.0f },  // 顶部 — 红色
    {  0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f, 1.0f },  // 右下 — 绿色
    { -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f, 1.0f },  // 左下 — 蓝色
};

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static ComPtr<ID3D11Device>            g_device;        // D3D11 设备
static ComPtr<ID3D11DeviceContext>     g_context;       // 设备上下文
static ComPtr<IDXGISwapChain>         g_swapChain;     // 交换链
static ComPtr<ID3D11RenderTargetView>  g_renderTarget;  // 渲染目标视图

// 本例新增资源
static ComPtr<ID3D11VertexShader>     g_vertexShader;  // 顶点着色器
static ComPtr<ID3D11PixelShader>      g_pixelShader;   // 像素着色器
static ComPtr<ID3D11InputLayout>      g_inputLayout;   // 输入布局
static ComPtr<ID3D11Buffer>           g_vertexBuffer;  // 顶点缓冲

// ============================================================================
// D3D11 初始化（设备 + 交换链 + 渲染目标）
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
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, driverType, nullptr, 0,
        featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc, &g_swapChain, &g_device, &selectedFeatureLevel, &g_context
    );
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain 失败");

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "CreateRenderTargetView 失败");

    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);

    // 设置视口（Viewport）— 定义渲染区域为整个窗口客户区
    RECT rc;
    GetClientRect(hwnd, &rc);
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = static_cast<float>(rc.right);
    vp.Height   = static_cast<float>(rc.bottom);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &vp);
}

// ============================================================================
// 着色器编译与创建
// ============================================================================

/**
 * 编译着色器文件并创建顶点/像素着色器及输入布局
 *
 * D3DCompileFromFile 可以在运行时编译 HLSL 源文件：
 * - vs_5_0 / ps_5_0 为着色器模型版本
 * - D3DCOMPILE_ENABLE_STRICTNESS 启用严格编译模式
 *
 * CreateInputLayout 根据着色器的输入签名（D3D11_INPUT_ELEMENT_DESC 数组）
 * 告诉 GPU 如何从顶点缓冲中提取数据
 */
static void InitShaders()
{
    // --- 编译顶点着色器 ---
    ComPtr<ID3DBlob> vsBlob;       // 编译后的顶点着色器字节码
    ComPtr<ID3DBlob> errorBlob;    // 编译错误信息

    // 着色器文件路径：相对于可执行文件所在目录
    const wchar_t* shaderPath = L"shaders/02_vertex_input/simple.hlsl";

    HRESULT hr = D3DCompileFromFile(
        shaderPath,                     // HLSL 源文件路径
        nullptr,                        // 不使用宏定义
        nullptr,                        // 不使用 include 处理器
        "VSMain",                       // 顶点着色器入口点名称
        "vs_5_0",                       // 顶点着色器目标版本
        D3DCOMPILE_ENABLE_STRICTNESS,   // 编译标志
        0,                              // 不使用效果标志
        &vsBlob,                        // [out] 编译后的字节码
        &errorBlob                      // [out] 编译错误信息
    );

    // 如果编译失败，输出错误信息
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            MessageBoxA(nullptr, (const char*)errorBlob->GetBufferPointer(),
                        "着色器编译错误", MB_ICONERROR);
        }
        ThrowIfFailed(hr, "D3DCompileFromFile (顶点着色器) 失败");
    }

    // 创建顶点着色器对象
    hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),     // 字节码指针
        vsBlob->GetBufferSize(),        // 字节码大小
        nullptr,                        // 不使用类链接
        &g_vertexShader                 // [out] 顶点着色器
    );
    ThrowIfFailed(hr, "CreateVertexShader 失败");

    // --- 定义输入布局 ---
    // 描述顶点数据在内存中的排列方式
    // 每个元素对应着色器中的一个输入语义
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        {
            "POSITION",                     // 语义名称 — 对应 HLSL 中的 : POSITION
            0,                              // 语义索引（POSITION0 中的 0）
            DXGI_FORMAT_R32G32B32_FLOAT,    // 数据格式：3 个 float（x, y, z）
            0,                              // 输入槽索引（顶点缓冲绑定到哪个槽）
            0,                              // 字节偏移量（位置在顶点结构体开头，偏移为 0）
            D3D11_INPUT_PER_VERTEX_DATA,    // 输入分类（逐顶点数据）
            0                               // 实例数据步长（仅用于实例化，这里为 0）
        },
        {
            "COLOR",                        // 语义名称 — 对应 HLSL 中的 : COLOR
            0,                              // 语义索引
            DXGI_FORMAT_R32G32B32A32_FLOAT, // 数据格式：4 个 float（r, g, b, a）
            0,                              // 输入槽索引（同一个顶点缓冲）
            12,                             // 字节偏移量：跳过 3 个 float (3 * 4 = 12 字节)
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
    };

    // 使用着色器字节码中的签名信息创建输入布局
    // 这确保了 C++ 端的顶点数据布局与 HLSL 着色器的输入签名匹配
    hr = g_device->CreateInputLayout(
        layoutDesc,                      // 输入元素描述数组
        _countof(layoutDesc),            // 元素数量
        vsBlob->GetBufferPointer(),      // 顶点着色器字节码（用于验证签名匹配）
        vsBlob->GetBufferSize(),
        &g_inputLayout                   // [out] 输入布局
    );
    ThrowIfFailed(hr, "CreateInputLayout 失败");

    // --- 编译像素着色器 ---
    ComPtr<ID3DBlob> psBlob;

    hr = D3DCompileFromFile(
        shaderPath,
        nullptr, nullptr,
        "PSMain",                       // 像素着色器入口点名称
        "ps_5_0",                       // 像素着色器目标版本
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            MessageBoxA(nullptr, (const char*)errorBlob->GetBufferPointer(),
                        "着色器编译错误", MB_ICONERROR);
        }
        ThrowIfFailed(hr, "D3DCompileFromFile (像素着色器) 失败");
    }

    // 创建像素着色器对象
    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &g_pixelShader
    );
    ThrowIfFailed(hr, "CreatePixelShader 失败");
}

// ============================================================================
// 创建顶点缓冲
// ============================================================================

/**
 * 创建顶点缓冲并将三角形顶点数据上传到 GPU
 *
 * D3D11_BUFFER_DESC 描述缓冲区的属性：
 * - ByteWidth = 缓冲区大小（字节）
 * - BindFlags = D3D11_BIND_VERTEX_BUFFER 表示此缓冲区用作顶点缓冲
 * - CPUAccessFlags = 0 表示 GPU 独占访问（CPU 不需要读写）
 *
 * D3D11_SUBRESOURCE_DATA 指定初始化数据（三角形顶点数组）
 */
static void InitVertexBuffer()
{
    // 顶点缓冲描述
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth      = sizeof(TRIANGLE_VERTICES);        // 缓冲区大小（字节）
    bufferDesc.Usage          = D3D11_USAGE_DEFAULT;              // GPU 读写
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;        // 绑定为顶点缓冲
    bufferDesc.CPUAccessFlags = 0;                                // CPU 不访问
    bufferDesc.MiscFlags      = 0;

    // 初始化数据：将顶点数组的内容复制到缓冲区
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = TRIANGLE_VERTICES;                        // 指向 CPU 端数据
    initData.SysMemPitch      = 0;  // 仅用于 2D/3D 纹理
    initData.SysMemSlicePitch = 0;  // 仅用于 3D 纹理

    HRESULT hr = g_device->CreateBuffer(
        &bufferDesc,      // 缓冲区描述
        &initData,        // 初始化数据
        &g_vertexBuffer   // [out] 顶点缓冲
    );
    ThrowIfFailed(hr, "CreateBuffer (顶点缓冲) 失败");
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

    // 更新视口大小
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = static_cast<float>(width);
    vp.Height   = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &vp);
}

// ============================================================================
// 渲染
// ============================================================================

/**
 * 每帧渲染函数
 *
 * 渲染管线流程：
 * 1. 清除后台缓冲
 * 2. 设置输入布局（告诉 GPU 如何解析顶点数据）
 * 3. 绑定顶点缓冲（提供实际数据）
 * 4. 设置图元拓扑（告诉 GPU 顶点组成三角形）
 * 5. 设置顶点着色器和像素着色器
 * 6. Draw(3, 0) — 绘制 3 个顶点，从索引 0 开始
 * 7. Present — 呈现到屏幕
 */
static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 清除为深灰色背景
    float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 设置输入布局 — 告诉 IA（输入装配器）如何解析顶点数据
    g_context->IASetInputLayout(g_inputLayout.Get());

    // 绑定顶点缓冲到输入装配器
    UINT stride = sizeof(Vertex);  // 每个顶点的大小（字节）
    UINT offset = 0;               // 缓冲区中的起始偏移
    g_context->IASetVertexBuffers(
        0,                          // 输入槽索引（对应输入布局中的 InputSlot）
        1,                          // 绑定的缓冲区数量
        g_vertexBuffer.GetAddressOf(), // 缓冲区指针数组
        &stride,                    // 每个顶点的步幅（字节）
        &offset                     // 起始偏移（字节）
    );

    // 设置图元拓扑 — 三角形列表
    // 每 3 个顶点构成一个独立三角形
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置着色器
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);  // 顶点着色器
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);   // 像素着色器

    // 发出绘制命令：绘制 3 个顶点，从顶点 0 开始
    // GPU 会调用顶点着色器 3 次（每个顶点一次），然后进行光栅化和像素着色
    g_context->Draw(3, 0);

    // 呈现（垂直同步）
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
        // 逆序释放资源（ComPtr 自动调用 Release）
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

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"D3D11 顶点缓冲示例",
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

    // 初始化 D3D11 基础设施
    InitD3D11(hwnd);

    // 初始化着色器和输入布局
    InitShaders();

    // 创建顶点缓冲
    InitVertexBuffer();

    // 消息循环（PeekMessage 模式 — 空闲时持续渲染）
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
