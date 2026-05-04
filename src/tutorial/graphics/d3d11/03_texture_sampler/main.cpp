/**
 * 03_texture_sampler - D3D11 纹理与采样器
 *
 * 本程序在 02_vertex_input 基础上，演示 D3D11 的纹理和采样器使用：
 * 1. 在内存中生成 64x64 棋盘格纹理数据
 * 2. CreateTexture2D + D3D11_SUBRESOURCE_DATA — 创建纹理并上传数据到 GPU
 * 3. CreateShaderResourceView — 创建着色器资源视图（SRV），让着色器能访问纹理
 * 4. CreateSamplerState — 创建采样器状态（配置纹理过滤和寻址模式）
 * 5. 顶点结构体包含位置 + 纹理坐标 (TEXCOORD)
 * 6. PSSetShaderResources + PSSetSamplers — 将纹理和采样器绑定到像素着色器
 * 7. 渲染一个带纹理的四边形（由两个三角形组成）
 * 8. 纹理坐标范围 0~4，使纹理重复 4x4 次
 *
 * 参考: tutorial/native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md
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

static const wchar_t* WINDOW_CLASS_NAME = L"D3D11TextureSamplerClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;
static const int      TEXTURE_SIZE      = 64;    // 纹理边长（像素）

// ============================================================================
// 顶点结构体定义
// ============================================================================

/**
 * 纹理顶点结构体：位置 + 纹理坐标
 *
 * - x, y, z: 裁剪空间位置
 * - u, v: 纹理坐标（范围通常 0~1，超过 1 时根据寻址模式决定行为）
 *
 * 纹理坐标原点 (0,0) 在纹理左上角，(1,1) 在右下角。
 * 这里使用 0~4 的范围，配合 WRAP 寻址模式，纹理会重复 4 次。
 */
struct TexVertex
{
    float x, y, z;     // 位置 (POSITION 语义)
    float u, v;         // 纹理坐标 (TEXCOORD 语义)
};

// 四边形由两个三角形组成
// 四个顶点，通过 6 个索引（两个三角形）绘制
// 纹理坐标范围 0~4，使 64x64 的棋盘格纹理重复 4x4 = 16 次
static const TexVertex QUAD_VERTICES[] =
{
    { -0.8f,  0.8f, 0.0f,   0.0f, 0.0f },  // 左上角
    {  0.8f,  0.8f, 0.0f,   4.0f, 0.0f },  // 右上角
    {  0.8f, -0.8f, 0.0f,   4.0f, 4.0f },  // 右下角
    { -0.8f, -0.8f, 0.0f,   0.0f, 4.0f },  // 左下角
};

// 索引缓冲：两个三角形组成一个四边形
// 三角形 1: 0-1-2 (左上-右上-右下)
// 三角形 2: 0-2-3 (左上-右下-左下)
static const unsigned short QUAD_INDICES[] =
{
    0, 1, 2,   // 第一个三角形
    0, 2, 3,   // 第二个三角形
};

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static ComPtr<ID3D11Device>            g_device;
static ComPtr<ID3D11DeviceContext>     g_context;
static ComPtr<IDXGISwapChain>         g_swapChain;
static ComPtr<ID3D11RenderTargetView>  g_renderTarget;

static ComPtr<ID3D11VertexShader>     g_vertexShader;
static ComPtr<ID3D11PixelShader>      g_pixelShader;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;
static ComPtr<ID3D11Buffer>           g_indexBuffer;   // 索引缓冲
static ComPtr<ID3D11ShaderResourceView> g_textureSRV;  // 纹理着色器资源视图
static ComPtr<ID3D11SamplerState>      g_samplerState;  // 采样器状态

// ============================================================================
// 生成棋盘格纹理数据
// ============================================================================

/**
 * 在内存中生成一个 64x64 的棋盘格纹理
 *
 * 棋盘格是经典的测试纹理，可以清楚地看到纹理的重复和过滤效果。
 * 每个格子大小为 8x8 像素，整个纹理分为 8x8 = 64 个格子。
 *
 * 颜色方案：
 * - 深色格: 深橙色 (200, 100, 20)
 * - 浅色格: 浅米色 (255, 220, 180)
 *
 * 纹理数据格式: RGBA 各 8 位，每像素 4 字节
 */
static void GenerateCheckerboard(BYTE* pixelData, int width, int height)
{
    const int CHECK_SIZE = 8;  // 每个格子的像素大小

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // 判断当前像素属于哪种颜色的格子
            int checkX = x / CHECK_SIZE;
            int checkY = y / CHECK_SIZE;
            bool isLight = ((checkX + checkY) % 2 == 0);

            // 计算像素在数据缓冲区中的偏移
            // 每像素 4 字节: R, G, B, A（按 DXGI_FORMAT_R8G8B8A8_UNORM 排列）
            int index = (y * width + x) * 4;

            if (isLight)
            {
                pixelData[index + 0] = 255;  // R
                pixelData[index + 1] = 220;  // G
                pixelData[index + 2] = 180;  // B
                pixelData[index + 3] = 255;  // A (不透明)
            }
            else
            {
                pixelData[index + 0] = 200;  // R
                pixelData[index + 1] = 100;  // G
                pixelData[index + 2] =  20;  // B
                pixelData[index + 3] = 255;  // A (不透明)
            }
        }
    }
}

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

    // 设置视口
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
// 创建纹理和采样器
// ============================================================================

/**
 * 创建棋盘格纹理并配置采样器
 *
 * CreateTexture2D:
 * - Width/Height: 纹理尺寸
 * - Format: DXGI_FORMAT_R8G8B8A8_UNORM — 每像素 32 位（RGBA 各 8 位，归一化到 0~1）
 * - MipLevels = 1: 只有一级 mipmap
 * - BindFlags = D3D11_BIND_SHADER_RESOURCE: 允许着色器访问此纹理
 *
 * CreateSamplerState:
 * - Filter = MIN_MAG_MIP_LINEAR: 三线性过滤（在缩放时进行线性插值）
 * - AddressU/V = WRAP: 纹理坐标超出 [0,1] 时进行平铺重复
 */
static void InitTextureAndSampler()
{
    // --- 第一步：在内存中生成棋盘格纹理数据 ---
    const int texSize  = TEXTURE_SIZE;
    const int pixelBytes = 4; // RGBA 各 1 字节
    BYTE* texData = new BYTE[texSize * texSize * pixelBytes];
    GenerateCheckerboard(texData, texSize, texSize);

    // --- 第二步：创建 2D 纹理 ---
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width            = texSize;                         // 纹理宽度
    texDesc.Height           = texSize;                         // 纹理高度
    texDesc.MipLevels        = 1;                               // Mipmap 等级数（1 = 不使用 mipmap）
    texDesc.ArraySize        = 1;                               // 纹理数组大小（1 = 单张纹理）
    texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;     // 像素格式
    texDesc.SampleDesc.Count = 1;                               // 不使用多重采样
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;             // GPU 独占访问
    texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;     // 允许着色器读取
    texDesc.CPUAccessFlags   = 0;
    texDesc.MiscFlags        = 0;

    // 初始化数据：指定纹理内容的内存位置和行间距
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem      = texData;                            // 指向纹理数据的指针
    initData.SysMemPitch  = texSize * pixelBytes;               // 每行的字节数（宽度 * 每像素字节数）
    initData.SysMemSlicePitch = 0;                              // 仅用于 3D 纹理

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = g_device->CreateTexture2D(&texDesc, &initData, &texture);
    ThrowIfFailed(hr, "CreateTexture2D 失败");

    // 纹理数据已上传到 GPU，释放 CPU 端内存
    delete[] texData;

    // --- 第三步：创建着色器资源视图（SRV）---
    // SRV 是着色器访问纹理的接口
    // 没有纹理就不能让像素着色器采样纹理
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = texDesc.Format;          // 格式与纹理一致
    srvDesc.ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D; // 2D 纹理
    srvDesc.Texture2D.MipLevels       = 1;                       // 只有一个 mipmap 等级
    srvDesc.Texture2D.MostDetailedMip = 0;                       // 从最详细的 mipmap 开始

    hr = g_device->CreateShaderResourceView(
        texture.Get(),     // 纹理对象
        &srvDesc,          // SRV 描述
        &g_textureSRV      // [out] 着色器资源视图
    );
    ThrowIfFailed(hr, "CreateShaderResourceView 失败");

    // --- 第四步：创建采样器状态 ---
    // 采样器定义了 GPU 如何从纹理中读取（采样）颜色值
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // 线性过滤
    // 线性过滤：在相邻纹素之间进行线性插值，使纹理缩放时更平滑
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;       // U 方向：平铺重复
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;       // V 方向：平铺重复
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;       // W 方向（仅 3D 纹理）
    samplerDesc.MipLODBias     = 0.0f;                             // Mipmap 偏移
    samplerDesc.MaxAnisotropy  = 1;                                // 最大各向异性（线性过滤时无效）
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;           // 不使用比较
    samplerDesc.BorderColor[0] = 0.0f;                             // 边框颜色（仅 BORDER 模式使用）
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD         = 0.0f;                             // 最小 mipmap 等级
    samplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;                // 最大 mipmap 等级

    hr = g_device->CreateSamplerState(
        &samplerDesc,     // 采样器描述
        &g_samplerState   // [out] 采样器状态
    );
    ThrowIfFailed(hr, "CreateSamplerState 失败");
}

// ============================================================================
// 着色器编译与创建
// ============================================================================

/**
 * 编译带纹理的着色器
 *
 * 着色器中使用：
 * - Texture2D txDiffuse : register(t0)  — 纹理寄存器 t0
 * - SamplerState samLinear : register(s0) — 采样器寄存器 s0
 *
 * 像素着色器中使用 txDiffuse.Sample(samLinear, input.tex) 采样纹理
 */
static void InitShaders()
{
    const wchar_t* shaderPath = L"shaders/03_texture_sampler/textured.hlsl";

    // --- 编译顶点着色器 ---
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        shaderPath, nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &vsBlob, &errorBlob
    );

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

    hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &g_vertexShader
    );
    ThrowIfFailed(hr, "CreateVertexShader 失败");

    // --- 创建输入布局（位置 + 纹理坐标）---
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    // 3 个 float: x, y, z
            0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       // 2 个 float: u, v
            0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0          // 偏移 12 字节（跳过 3 个 float）
        },
    };

    hr = g_device->CreateInputLayout(
        layoutDesc, _countof(layoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &g_inputLayout
    );
    ThrowIfFailed(hr, "CreateInputLayout 失败");

    // --- 编译像素着色器 ---
    ComPtr<ID3DBlob> psBlob;

    hr = D3DCompileFromFile(
        shaderPath, nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &psBlob, &errorBlob
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

    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &g_pixelShader
    );
    ThrowIfFailed(hr, "CreatePixelShader 失败");
}

// ============================================================================
// 创建顶点缓冲和索引缓冲
// ============================================================================

/**
 * 创建四边形的顶点缓冲和索引缓冲
 *
 * 索引缓冲 (Index Buffer) 可以复用顶点数据：
 * - 四边形有 4 个顶点，但由 2 个三角形共 6 个顶点组成
 * - 使用索引缓冲只需存储 4 个顶点 + 6 个索引，而不是 6 个顶点
 * - 索引缓冲还可以帮助 GPU 缓存和重用顶点着色器的计算结果
 */
static void InitBuffers()
{
    // --- 创建顶点缓冲 ---
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth      = sizeof(QUAD_VERTICES);
    vbDesc.Usage          = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = QUAD_VERTICES;

    HRESULT hr = g_device->CreateBuffer(&vbDesc, &vbData, &g_vertexBuffer);
    ThrowIfFailed(hr, "CreateBuffer (顶点缓冲) 失败");

    // --- 创建索引缓冲 ---
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth      = sizeof(QUAD_INDICES);
    ibDesc.Usage          = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags      = D3D11_BIND_INDEX_BUFFER;       // 绑定为索引缓冲
    ibDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = QUAD_INDICES;

    hr = g_device->CreateBuffer(&ibDesc, &ibData, &g_indexBuffer);
    ThrowIfFailed(hr, "CreateBuffer (索引缓冲) 失败");
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
 * 与 02_vertex_input 相比，新增了：
 * - 绑定索引缓冲（IASetIndexBuffer）
 * - 绑定纹理到像素着色器（PSSetShaderResources）
 * - 绑定采样器到像素着色器（PSSetSamplers）
 * - 使用 DrawIndexed 代替 Draw（通过索引绘制）
 */
static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 清除为深色背景
    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 设置输入布局
    g_context->IASetInputLayout(g_inputLayout.Get());

    // 绑定顶点缓冲
    UINT stride = sizeof(TexVertex);
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);

    // 绑定索引缓冲
    // DXGI_FORMAT_R16_UINT 表示每个索引为 16 位无符号整数
    g_context->IASetIndexBuffer(g_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // 设置图元拓扑：三角形列表
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置着色器
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

    // 将纹理绑定到像素着色器的寄存器 t0
    // 参数：起始槽位, 视图数量, 视图数组指针
    g_context->PSSetShaderResources(0, 1, g_textureSRV.GetAddressOf());

    // 将采样器绑定到像素着色器的寄存器 s0
    // 参数：起始槽位, 采样器数量, 采样器数组指针
    g_context->PSSetSamplers(0, 1, g_samplerState.GetAddressOf());

    // 使用索引绘制：6 个索引，从索引 0 开始，绘制到基础顶点 0
    // DrawIndexed 比 Draw 更高效，因为可以复用顶点数据
    g_context->DrawIndexed(6, 0, 0);

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
        // 逆序释放所有资源
        g_samplerState.Reset();
        g_textureSRV.Reset();
        g_indexBuffer.Reset();
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
        L"D3D11 纹理采样器示例",
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

    // 创建纹理和采样器
    InitTextureAndSampler();

    // 编译着色器
    InitShaders();

    // 创建顶点和索引缓冲
    InitBuffers();

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
