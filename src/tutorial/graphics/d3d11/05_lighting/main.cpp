/**
 * D3D11 光照模型示例
 *
 * 演示内容：
 *   - 顶点结构包含位置 + 法线 + 颜色
 *   - 在常量缓冲区中传入方向光方向 (float3 LightDir)
 *   - 顶点着色器中计算漫反射 + 环境光光照
 *   - 旋转立方体，每个面具有独立法线和不同颜色
 *   - 光源方向缓慢旋转，展示动态光照效果
 *
 * 参考：tutorial/native_win32/41_ProgrammingGUI_Graphics_D3D11_Lighting.md
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

// 管线资源
static ComPtr<ID3D11VertexShader>     g_vertexShader;
static ComPtr<ID3D11PixelShader>      g_pixelShader;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;
static ComPtr<ID3D11Buffer>           g_constantBuffer;

// ============================================================================
// 顶点结构体 — 位置 + 法线 + 颜色
// ============================================================================

struct Vertex
{
    XMFLOAT4 position;  // 顶点位置（齐次坐标）
    XMFLOAT3 normal;    // 法线方向
    XMFLOAT4 color;     // 顶点颜色（RGBA）
};

// ============================================================================
// 常量缓冲区结构体 — 与 HLSL 中的 cbuffer 对应
// ============================================================================

struct Constants
{
    XMMATRIX World;         // 世界矩阵（用于法线变换）
    XMMATRIX WorldViewProj; // 世界 x 观察 x 投影矩阵
    XMFLOAT3 LightDir;      // 光源方向（归一化）
    float    Padding;       // 填充至 16 字节边界
};

// ============================================================================
// 编译着色器
// ============================================================================

/**
 * 从文件编译 HLSL 着色器。
 * 使用 d3dcompiler 在运行时编译 .hlsl 文件。
 */
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
// 创建带法线的立方体顶点数据
// ============================================================================

/**
 * 生成一个单位立方体的 24 个独立顶点（每面 4 个顶点，共 6 面）。
 * 每个面使用独立的法线方向，确保平面着色的正确性。
 * 最终绘制 36 个顶点（通过索引列表展开为 12 个三角形）。
 *
 * 立方体以原点为中心，边长为 1（范围 [-0.5, 0.5]）。
 */
static void CreateLitCubeVertices(Vertex** ppVertices, UINT* pVertexCount)
{
    *pVertexCount = 36;  // 6 面 x 2 三角形 x 3 顶点
    *ppVertices = new Vertex[36];

    // 定义每个面的颜色
    XMFLOAT4 faceColors[6] = {
        { 1.0f, 0.2f, 0.2f, 1.0f },  // 前面 — 红色
        { 0.2f, 1.0f, 0.2f, 1.0f },  // 后面 — 绿色
        { 0.2f, 0.2f, 1.0f, 1.0f },  // 上面 — 蓝色
        { 1.0f, 1.0f, 0.2f, 1.0f },  // 下面 — 黄色
        { 1.0f, 0.2f, 1.0f, 1.0f },  // 右面 — 品红
        { 0.2f, 1.0f, 1.0f, 1.0f },  // 左面 — 青色
    };

    // 定义每个面的法线方向
    XMFLOAT3 faceNormals[6] = {
        {  0.0f,  0.0f,  1.0f },  // 前面
        {  0.0f,  0.0f, -1.0f },  // 后面
        {  0.0f,  1.0f,  0.0f },  // 上面
        {  0.0f, -1.0f,  0.0f },  // 下面
        {  1.0f,  0.0f,  0.0f },  // 右面
        { -1.0f,  0.0f,  0.0f },  // 左面
    };

    // 每个面的 4 个顶点位置
    struct Face { XMFLOAT4 v[4]; };
    Face faces[6] = {
        // 前面 (z = +0.5)
        { { {-0.5f,-0.5f, 0.5f, 1.0f}, { 0.5f,-0.5f, 0.5f, 1.0f}, { 0.5f, 0.5f, 0.5f, 1.0f}, {-0.5f, 0.5f, 0.5f, 1.0f} } },
        // 后面 (z = -0.5)
        { { { 0.5f,-0.5f,-0.5f, 1.0f}, {-0.5f,-0.5f,-0.5f, 1.0f}, {-0.5f, 0.5f,-0.5f, 1.0f}, { 0.5f, 0.5f,-0.5f, 1.0f} } },
        // 上面 (y = +0.5)
        { { {-0.5f, 0.5f, 0.5f, 1.0f}, { 0.5f, 0.5f, 0.5f, 1.0f}, { 0.5f, 0.5f,-0.5f, 1.0f}, {-0.5f, 0.5f,-0.5f, 1.0f} } },
        // 下面 (y = -0.5)
        { { {-0.5f,-0.5f,-0.5f, 1.0f}, { 0.5f,-0.5f,-0.5f, 1.0f}, { 0.5f,-0.5f, 0.5f, 1.0f}, {-0.5f,-0.5f, 0.5f, 1.0f} } },
        // 右面 (x = +0.5)
        { { { 0.5f,-0.5f, 0.5f, 1.0f}, { 0.5f,-0.5f,-0.5f, 1.0f}, { 0.5f, 0.5f,-0.5f, 1.0f}, { 0.5f, 0.5f, 0.5f, 1.0f} } },
        // 左面 (x = -0.5)
        { { {-0.5f,-0.5f,-0.5f, 1.0f}, {-0.5f,-0.5f, 0.5f, 1.0f}, {-0.5f, 0.5f, 0.5f, 1.0f}, {-0.5f, 0.5f,-0.5f, 1.0f} } },
    };

    // 每个面的三角形索引：0-1-2, 0-2-3
    int faceIndices[6] = { 0, 1, 2, 0, 2, 3 };

    Vertex* verts = *ppVertices;
    for (int f = 0; f < 6; ++f)
    {
        for (int t = 0; t < 6; ++t)
        {
            int idx = f * 6 + t;
            verts[idx].position = faces[f].v[faceIndices[t]];
            verts[idx].normal   = faceNormals[f];
            verts[idx].color    = faceColors[f];
        }
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

    // 获取后缓冲并创建渲染目标视图
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "获取后缓冲失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTargetView.GetAddressOf());
    ThrowIfFailed(hr, "创建渲染目标视图失败");
}

// ============================================================================
// 创建深度缓冲
// ============================================================================

static void CreateDepthStencil()
{
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
}

// ============================================================================
// 初始化着色器和管线状态
// ============================================================================

static void InitShadersAndPipeline()
{
    // 编译顶点着色器
    ComPtr<ID3DBlob> vsBlob = CompileShaderFromFile(
        L"shaders/05_lighting/lighting.hlsl", "VSMain", "vs_5_0");
    HRESULT hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, g_vertexShader.GetAddressOf());
    ThrowIfFailed(hr, "创建顶点着色器失败");

    // 编译像素着色器
    ComPtr<ID3DBlob> psBlob = CompileShaderFromFile(
        L"shaders/05_lighting/lighting.hlsl", "PSMain", "ps_5_0");
    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, g_pixelShader.GetAddressOf());
    ThrowIfFailed(hr, "创建像素着色器失败");

    // 定义输入布局 — POSITION(float4) + NORMAL(float3) + COLOR(float4)
    // 注意：NORMAL 占 12 字节，之后 COLOR 的偏移为 16 + 12 = 28
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_device->CreateInputLayout(
        layoutDesc, 3,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf());
    ThrowIfFailed(hr, "创建输入布局失败");
}

// ============================================================================
// 创建顶点缓冲区和常量缓冲区
// ============================================================================

static void CreateBuffers()
{
    // ---- 顶点缓冲区 ----
    Vertex* cubeVertices = nullptr;
    UINT vertexCount = 0;
    CreateLitCubeVertices(&cubeVertices, &vertexCount);

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage          = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth      = sizeof(Vertex) * vertexCount;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = cubeVertices;

    HRESULT hr = g_device->CreateBuffer(&vbDesc, &vbData, g_vertexBuffer.GetAddressOf());
    ThrowIfFailed(hr, "创建顶点缓冲区失败");
    delete[] cubeVertices;

    // ---- 常量缓冲区 ----
    // 存放 World 矩阵、WorldViewProj 矩阵、光照方向
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
 * 2. 根据时间计算旋转矩阵和动态光源方向
 * 3. 更新常量缓冲区中的 World、WorldViewProj、LightDir
 * 4. 绘制带光照的立方体
 * 5. 呈现
 */
static void RenderFrame()
{
    if (!g_context || !g_renderTargetView)
        return;

    // 清除渲染目标为深色背景
    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);
    g_context->ClearDepthStencilView(g_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ---- 计算变换矩阵 ----
    static DWORD startTime = GetTickCount();
    float time = (GetTickCount() - startTime) / 1000.0f;

    // 世界矩阵：绕 Y 轴旋转
    XMMATRIX world = XMMatrixRotationY(time * 0.8f);

    // 观察矩阵
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 1.5f, -3.5f, 0.0f),
        XMVectorSet(0.0f, 0.0f,  0.0f,  0.0f),
        XMVectorSet(0.0f, 1.0f,  0.0f,  0.0f)
    );

    // 投影矩阵
    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);

    // 组合 WorldViewProj 矩阵
    XMMATRIX worldViewProj = world * view * proj;

    // ---- 动态光源方向 ----
    // 光源方向随时间缓慢旋转，产生动态光照效果
    float lightAngle = time * 0.5f;
    XMVECTOR lightDirVec = XMVector3Normalize(
        XMVectorSet(sinf(lightAngle), -1.0f, cosf(lightAngle), 0.0f)
    );
    XMFLOAT3 lightDir;
    XMStoreFloat3(&lightDir, lightDirVec);

    // ---- 更新常量缓冲区 ----
    // HLSL 中矩阵默认列主序，而 DirectXMath 也是列主序，
    // 但 HLSL cbuffer 期望行主序读取，因此需要转置
    Constants consts = {};
    consts.World         = XMMatrixTranspose(world);
    consts.WorldViewProj = XMMatrixTranspose(worldViewProj);
    consts.LightDir      = lightDir;
    consts.Padding       = 0.0f;

    g_context->UpdateSubresource(g_constantBuffer.Get(), 0, nullptr, &consts, 0, 0);

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

    // 绘制立方体（36 个顶点 = 12 个三角形）
    g_context->Draw(36, 0);

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
    wc.lpszClassName = L"D3D11LightingClass";

    RegisterClassEx(&wc);

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0,
        L"D3D11LightingClass",
        L"D3D11 光照示例",
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
