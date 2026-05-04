/**
 * D3D11 混合与透明示例
 *
 * 演示内容：
 *   - 创建混合状态 (BlendState) 使用 D3D11_BLEND_DESC
 *   - 通过 OMSetBlendState 启用 Alpha 混合
 *   - 渲染 3 个重叠的半透明四边形（红、绿、蓝），位于不同 Z 深度
 *   - Alpha 值随时间变化（正弦波），产生脉动透明效果
 *   - 对透明物体禁用深度写入，确保正确的混合顺序
 *
 * 参考：tutorial/native_win32/42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>

#include "../common/ComHelper.hpp"

using namespace DirectX;

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static const int WINDOW_WIDTH  = 800;
static const int WINDOW_HEIGHT = 600;

// 设备与上下文
static ComPtr<ID3D11Device>           g_device;
static ComPtr<ID3D11DeviceContext>    g_context;

// 交换链与渲染目标
static ComPtr<IDXGISwapChain>        g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_renderTargetView;

// 深度缓冲
static ComPtr<ID3D11Texture2D>        g_depthStencilBuffer;
static ComPtr<ID3D11DepthStencilView> g_depthStencilView;

// 深度模板状态 — 禁用深度写入（用于透明物体）
static ComPtr<ID3D11DepthStencilState> g_depthDisabledWriteState;
static ComPtr<ID3D11DepthStencilState> g_depthEnabledState;

// 混合状态
static ComPtr<ID3D11BlendState>       g_blendState;

// 管线资源
static ComPtr<ID3D11VertexShader>     g_vertexShader;
static ComPtr<ID3D11PixelShader>      g_pixelShader;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;
static ComPtr<ID3D11Buffer>           g_constantBuffer;

// ============================================================================
// 顶点结构体 — 位置 + 颜色
// ============================================================================

struct Vertex
{
    XMFLOAT4 position;  // 顶点位置（齐次坐标）
    XMFLOAT4 color;     // 顶点颜色（RGBA）
};

// ============================================================================
// 常量缓冲区结构体 — 与 HLSL 中的 cbuffer 对应
// ============================================================================

struct Constants
{
    XMMATRIX WorldViewProj; // 世界 x 观察 x 投影矩阵
    float    Alpha;         // 全局 Alpha 控制值
    float    Padding[3];    // 填充至 16 字节边界
};

// ============================================================================
// 编译着色器
// ============================================================================

static ComPtr<ID3DBlob> CompileShaderFromFile(
    const wchar_t* fileName, const char* entryPoint, const char* shaderModel)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        fileName,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        shaderModel,
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        blob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr, "着色器编译失败");
    }

    return blob;
}

// ============================================================================
// 创建三个半透明四边形的顶点数据
// ============================================================================

/**
 * 生成 3 个四边形（共 18 个顶点）。
 * 每个四边形位于不同的 Z 深度，颜色分别为红、绿、蓝。
 *
 * 四边形排列（从后到前）：
 *   - 红色四边形：z = 0.0
 *   - 绿色四边形：z = -0.5（偏左偏下）
 *   - 蓝色四边形：z = -1.0（偏右偏上）
 */
static void CreateQuadVertices(Vertex** ppVertices, UINT* pVertexCount)
{
    *pVertexCount = 18;  // 3 个四边形 x 2 三角形 x 3 顶点
    *ppVertices = new Vertex[18];

    Vertex* verts = *ppVertices;
    int idx = 0;

    // 定义四边形数据：位置偏移、Z 深度、颜色
    struct QuadDef
    {
        float offsetX, offsetY, offsetZ;  // 中心位置
        float size;                        // 边长
        XMFLOAT4 color;                    // 颜色
    };

    QuadDef quads[3] = {
        { -120.0f,    0.0f, 0.0f, 300.0f, { 1.0f, 0.0f, 0.0f, 0.6f } },  // 红色 — 最远
        {    0.0f,  -60.0f, 0.5f, 300.0f, { 0.0f, 1.0f, 0.0f, 0.6f } },  // 绿色 — 中间
        {  120.0f,   60.0f, 1.0f, 300.0f, { 0.0f, 0.0f, 1.0f, 0.6f } },  // 蓝色 — 最近
    };

    for (int q = 0; q < 3; ++q)
    {
        float cx = quads[q].offsetX;
        float cy = quads[q].offsetY;
        float cz = quads[q].offsetZ;
        float hs = quads[q].size * 0.5f;  // 半边长

        // 四边形的 4 个顶点
        XMFLOAT4 quadVerts[4] = {
            { cx - hs, cy - hs, cz, 1.0f },  // 左下
            { cx + hs, cy - hs, cz, 1.0f },  // 右下
            { cx + hs, cy + hs, cz, 1.0f },  // 右上
            { cx - hs, cy + hs, cz, 1.0f },  // 左上
        };

        // 三角形 1：0-1-2
        verts[idx].position = quadVerts[0]; verts[idx].color = quads[q].color; idx++;
        verts[idx].position = quadVerts[1]; verts[idx].color = quads[q].color; idx++;
        verts[idx].position = quadVerts[2]; verts[idx].color = quads[q].color; idx++;

        // 三角形 2：0-2-3
        verts[idx].position = quadVerts[0]; verts[idx].color = quads[q].color; idx++;
        verts[idx].position = quadVerts[2]; verts[idx].color = quads[q].color; idx++;
        verts[idx].position = quadVerts[3]; verts[idx].color = quads[q].color; idx++;
    }
}

// ============================================================================
// 初始化 D3D11 设备和交换链
// ============================================================================

static void InitDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount       = 1;
    swapChainDesc.BufferDesc.Width  = WINDOW_WIDTH;
    swapChainDesc.BufferDesc.Height = WINDOW_HEIGHT;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow      = hwnd;
    swapChainDesc.SampleDesc.Count  = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed          = TRUE;
    swapChainDesc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        g_swapChain.GetAddressOf(),
        g_device.GetAddressOf(),
        nullptr,
        g_context.GetAddressOf()
    );
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain 失败");

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "获取后缓冲失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTargetView.GetAddressOf());
    ThrowIfFailed(hr, "创建渲染目标视图失败");
}

// ============================================================================
// 创建深度缓冲和深度状态
// ============================================================================

static void CreateDepthStencil()
{
    // 创建深度模板纹理
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width            = WINDOW_WIDTH;
    depthDesc.Height           = WINDOW_HEIGHT;
    depthDesc.MipLevels        = 1;
    depthDesc.ArraySize        = 1;
    depthDesc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage            = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = g_device->CreateTexture2D(&depthDesc, nullptr, g_depthStencilBuffer.GetAddressOf());
    ThrowIfFailed(hr, "创建深度模板纹理失败");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = g_device->CreateDepthStencilView(g_depthStencilBuffer.Get(), &dsvDesc, g_depthStencilView.GetAddressOf());
    ThrowIfFailed(hr, "创建深度模板视图失败");

    // ---- 深度模板状态：深度测试启用，深度写入禁用 ----
    // 用于透明物体，确保它们不会遮挡后方已绘制的透明物体
    D3D11_DEPTH_STENCIL_DESC dsWriteDisabledDesc = {};
    dsWriteDisabledDesc.DepthEnable      = TRUE;
    dsWriteDisabledDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;  // 禁用深度写入
    dsWriteDisabledDesc.DepthFunc        = D3D11_COMPARISON_LESS;
    dsWriteDisabledDesc.StencilEnable    = FALSE;

    hr = g_device->CreateDepthStencilState(&dsWriteDisabledDesc, g_depthDisabledWriteState.GetAddressOf());
    ThrowIfFailed(hr, "创建禁用写入的深度模板状态失败");

    // ---- 深度模板状态：完全启用（用于不透明物体）----
    D3D11_DEPTH_STENCIL_DESC dsEnabledDesc = {};
    dsEnabledDesc.DepthEnable      = TRUE;
    dsEnabledDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
    dsEnabledDesc.DepthFunc        = D3D11_COMPARISON_LESS;
    dsEnabledDesc.StencilEnable    = FALSE;

    hr = g_device->CreateDepthStencilState(&dsEnabledDesc, g_depthEnabledState.GetAddressOf());
    ThrowIfFailed(hr, "创建深度模板状态失败");
}

// ============================================================================
// 创建 Alpha 混合状态
// ============================================================================

/**
 * 创建标准 Alpha 混合状态：
 *   - 源因子：SRC_ALPHA（源颜色的 Alpha 分量）
 *   - 目标因子：INV_SRC_ALPHA（1 - 源 Alpha）
 *   - 混合操作：ADD（标准加法混合）
 *
 * 最终颜色 = 源色 * SrcAlpha + 目标色 * (1 - SrcAlpha)
 */
static void CreateBlendState()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable  = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    // 仅配置第一个渲染目标的混合参数
    D3D11_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0];
    rtBlend.BlendEnable           = TRUE;
    rtBlend.SrcBlend              = D3D11_BLEND_SRC_ALPHA;        // 源混合因子
    rtBlend.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;    // 目标混合因子
    rtBlend.BlendOp               = D3D11_BLEND_OP_ADD;           // 颜色混合操作
    rtBlend.SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;        // Alpha 混合因子
    rtBlend.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;    // Alpha 目标因子
    rtBlend.BlendOpAlpha          = D3D11_BLEND_OP_ADD;           // Alpha 混合操作
    rtBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // 写入所有颜色通道

    HRESULT hr = g_device->CreateBlendState(&blendDesc, g_blendState.GetAddressOf());
    ThrowIfFailed(hr, "创建混合状态失败");
}

// ============================================================================
// 初始化着色器和管线状态
// ============================================================================

static void InitShadersAndPipeline()
{
    // 编译顶点着色器
    ComPtr<ID3DBlob> vsBlob = CompileShaderFromFile(
        L"shaders/06_blend_alpha/alpha.hlsl", "VSMain", "vs_5_0");
    HRESULT hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, g_vertexShader.GetAddressOf());
    ThrowIfFailed(hr, "创建顶点着色器失败");

    // 编译像素着色器
    ComPtr<ID3DBlob> psBlob = CompileShaderFromFile(
        L"shaders/06_blend_alpha/alpha.hlsl", "PSMain", "ps_5_0");
    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, g_pixelShader.GetAddressOf());
    ThrowIfFailed(hr, "创建像素着色器失败");

    // 定义输入布局 — POSITION(float4) + COLOR(float4)
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_device->CreateInputLayout(
        layoutDesc, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf());
    ThrowIfFailed(hr, "创建输入布局失败");

    // 关闭背面剔除（四边形顶点为 CCW 绕序）
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode              = D3D11_FILL_SOLID;
    rsDesc.CullMode              = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable       = TRUE;
    ComPtr<ID3D11RasterizerState> rsState;
    ThrowIfFailed(
        g_device->CreateRasterizerState(&rsDesc, rsState.GetAddressOf()),
        "创建光栅化状态失败");
    g_context->RSSetState(rsState.Get());
}

// ============================================================================
// 创建缓冲区
// ============================================================================

static void CreateBuffers()
{
    // ---- 顶点缓冲区 ----
    Vertex* quadVertices = nullptr;
    UINT vertexCount = 0;
    CreateQuadVertices(&quadVertices, &vertexCount);

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage          = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth      = sizeof(Vertex) * vertexCount;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = quadVertices;

    HRESULT hr = g_device->CreateBuffer(&vbDesc, &vbData, g_vertexBuffer.GetAddressOf());
    ThrowIfFailed(hr, "创建顶点缓冲区失败");
    delete[] quadVertices;

    // ---- 常量缓冲区 ----
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage          = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth      = sizeof(Constants);
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;

    hr = g_device->CreateBuffer(&cbDesc, nullptr, g_constantBuffer.GetAddressOf());
    ThrowIfFailed(hr, "创建常量缓冲区失败");
}

// ============================================================================
// 渲染帧
// ============================================================================

/**
 * 每帧渲染逻辑：
 * 1. 清除颜色和深度缓冲区
 * 2. 使用正交投影，计算 WorldViewProj 矩阵
 * 3. Alpha 值通过正弦波在 0.2 ~ 1.0 之间脉动
 * 4. 启用 Alpha 混合状态和禁用深度写入
 * 5. 按从后到前的顺序绘制三个半透明四边形
 * 6. 呈现
 */
static void RenderFrame()
{
    if (!g_context || !g_renderTargetView)
        return;

    // 清除渲染目标为深色背景
    float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);
    g_context->ClearDepthStencilView(g_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ---- 计算正交投影矩阵 ----
    XMMATRIX proj = XMMatrixOrthographicLH(
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.1f, 100.0f
    );

    // 观察矩阵：相机在 Z 轴负方向看向原点
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f),
        XMVectorSet(0.0f, 0.0f,  0.0f, 0.0f),
        XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
    );

    // 单位世界矩阵（四边形不需要旋转）
    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX worldViewProj = XMMatrixTranspose(world * view * proj);

    // ---- 计算脉动 Alpha 值 ----
    // 使用正弦波在 0.2 ~ 1.0 之间变化
    static DWORD startTime = GetTickCount();
    float time = (GetTickCount() - startTime) / 1000.0f;
    float alpha = 0.6f + 0.4f * sinf(time * 2.0f);  // 范围 [0.2, 1.0]

    // ---- 更新常量缓冲区 ----
    Constants consts = {};
    consts.WorldViewProj = worldViewProj;
    consts.Alpha         = alpha;
    consts.Padding[0]    = 0.0f;
    consts.Padding[1]    = 0.0f;
    consts.Padding[2]    = 0.0f;

    g_context->UpdateSubresource(g_constantBuffer.Get(), 0, nullptr, &consts, 0, 0);

    // ---- 设置混合状态 ----
    // 启用 Alpha 混合，混合因子为 (1,1,1,1)，采样掩码为 0xFFFFFFFF
    g_context->OMSetBlendState(g_blendState.Get(), nullptr, 0xFFFFFFFF);

    // ---- 设置深度状态（禁用深度写入）----
    // 透明物体不应写入深度缓冲，否则会阻止后方物体的渲染
    g_context->OMSetDepthStencilState(g_depthDisabledWriteState.Get(), 0);

    // ---- 设置渲染管线状态 ----
    g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), g_depthStencilView.Get());

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<float>(WINDOW_WIDTH);
    viewport.Height   = static_cast<float>(WINDOW_HEIGHT);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &viewport);

    g_context->IASetInputLayout(g_inputLayout.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->VSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
    g_context->PSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());

    // 绘制 3 个四边形（18 个顶点 = 6 个三角形）
    // 按从后到前顺序绘制，确保正确的 Alpha 混合
    g_context->Draw(18, 0);

    g_swapChain->Present(1, 0);
}

// ============================================================================
// 窗口过程
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        RenderFrame();
        ValidateRect(hwnd, nullptr);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// WinMain 入口点
// ============================================================================

int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"D3D11BlendAlphaClass";

    RegisterClassEx(&wc);

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0,
        L"D3D11BlendAlphaClass",
        L"D3D11 混合透明示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd)
    {
        MessageBoxA(nullptr, "创建窗口失败", "错误", MB_ICONERROR);
        return -1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---- 初始化 D3D11 资源 ----
    InitDeviceAndSwapChain(hwnd);
    CreateDepthStencil();
    CreateBlendState();
    InitShadersAndPipeline();
    CreateBuffers();

    // ---- 主消息循环 ----
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            RenderFrame();
        }
    }

    return static_cast<int>(msg.wParam);
}
