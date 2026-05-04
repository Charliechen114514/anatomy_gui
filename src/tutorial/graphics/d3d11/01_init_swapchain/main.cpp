/**
 * 01_init_swapchain - D3D11 初始化与 SwapChain
 *
 * 本程序演示了 Direct3D 11 的最基本初始化流程：
 * 1. D3D11CreateDeviceAndSwapChain   — 创建设备、设备上下文和交换链
 * 2. GetBuffer + CreateRenderTargetView — 从交换链获取后台缓冲并创建渲染目标视图
 * 3. OMSetRenderTargets              — 将渲染目标绑定到输出合并阶段
 * 4. ClearRenderTargetView           — 每帧清除渲染目标为指定颜色
 * 5. SwapChain::Present              — 呈现后台缓冲到屏幕（垂直同步）
 * 6. WM_SIZE: ResizeBuffers          — 窗口大小改变时重新调整交换链缓冲区
 * 7. 设备移除处理 (DXGI_ERROR_DEVICE_REMOVED)
 *
 * 这是 D3D11 的 "Hello World"：将屏幕清除为不断变化的颜色。
 *
 * 参考: tutorial/native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>

// 引入公共 COM 辅助工具（ComPtr<T> 和 ThrowIfFailed）
#include "../common/ComHelper.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D3D11InitClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

// 使用 ComPtr 管理 COM 对象的生命周期，无需手动 Release

static ComPtr<ID3D11Device>           g_device;        // D3D11 设备（用于创建资源）
static ComPtr<ID3D11DeviceContext>    g_context;       // D3D11 设备上下文（用于提交渲染命令）
static ComPtr<IDXGISwapChain>        g_swapChain;     // 交换链（管理前后台缓冲切换）
static ComPtr<ID3D11RenderTargetView> g_renderTarget;  // 渲染目标视图（指向后台缓冲）

// ============================================================================
// D3D11 初始化
// ============================================================================

/**
 * 创建 D3D11 设备、设备上下文和交换链
 *
 * DXGI_SWAP_CHAIN_DESC 描述了交换链的配置：
 * - BufferCount = 1      — 双缓冲（一个前台 + 一个后台）
 * - BufferDesc.Format    — 每像素 32 位，RGBA 各 8 位
 * - SampleDesc           — 多重采样抗锯齿（这里设为 1 表示不使用 MSAA）
 * - Windowed = TRUE      — 窗口模式
 * - SwapEffect           — Discard 表示 Present 后后台缓冲内容不再保留
 */
static void InitD3D11(HWND hwnd)
{
    // 填写交换链描述
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount       = 1;                                      // 双缓冲
    swapChainDesc.BufferDesc.Width   = 0;                                     // 自动匹配窗口大小
    swapChainDesc.BufferDesc.Height  = 0;
    swapChainDesc.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;           // 32 位 RGBA
    swapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;                   // 60 Hz
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.SampleDesc.Count   = 1;                                     // 不使用 MSAA
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.OutputWindow       = hwnd;                                  // 渲染目标窗口
    swapChainDesc.Windowed           = TRUE;                                  // 窗口模式
    swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;             // Present 后丢弃后台缓冲
    swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // 作为渲染目标

    // 驱动类型与特性等级
    // HARDWARE 表示使用 GPU 硬件加速
    // 9_1 是最低特性等级，确保在几乎所有 GPU 上都能运行
    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    // 同时创建设备、设备上下文和交换链
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                // 默认适配器（使用主显卡）
        driverType,             // 硬件驱动
        nullptr,                // 不使用软件光栅化器
        0,                      // 不创建调试设备
        featureLevels,          // 特性等级数组
        _countof(featureLevels),// 特性等级数量
        D3D11_SDK_VERSION,      // SDK 版本
        &swapChainDesc,         // 交换链描述
        &g_swapChain,           // [out] 交换链
        &g_device,              // [out] 设备
        &selectedFeatureLevel,  // [out] 选中的特性等级
        &g_context              // [out] 设备上下文
    );
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain 失败");

    // 从交换链获取后台缓冲（Buffer index = 0），创建渲染目标视图
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "GetBuffer 失败");

    // 创建渲染目标视图（RTV），告诉 GPU 将后台缓冲作为渲染目标
    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "CreateRenderTargetView 失败");

    // 将渲染目标绑定到输出合并（Output Merger）阶段
    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 窗口大小调整
// ============================================================================

/**
 * 窗口大小改变时，需要重新调整交换链缓冲区和渲染目标
 *
 * 步骤：
 * 1. 释放旧的渲染目标视图（否则 ResizeBuffers 会失败）
 * 2. 调用 ResizeBuffers 重新分配后台缓冲
 * 3. 重新获取后台缓冲并创建新的渲染目标视图
 * 4. 重新绑定到输出合并阶段
 */
static void OnResize(UINT width, UINT height)
{
    // 宽度或高度为 0 表示窗口被最小化，跳过处理
    if (width == 0 || height == 0 || !g_swapChain)
        return;

    // 第一步：释放旧的渲染目标视图
    // 必须在 ResizeBuffers 之前释放所有对后台缓冲的引用
    g_renderTarget.Reset();

    // 第二步：重新调整交换链缓冲区大小
    // 参数为 0 表示保持 BufferCount 和 Format 不变
    HRESULT hr = g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    ThrowIfFailed(hr, "ResizeBuffers 失败");

    // 第三步：重新获取后台缓冲并创建渲染目标视图
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "Resize 后 GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "Resize 后 CreateRenderTargetView 失败");

    // 第四步：重新绑定渲染目标
    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 渲染
// ============================================================================

/**
 * 每帧渲染函数
 *
 * 这里只做最简单的操作：将屏幕清除为不断变化的颜色
 * 使用时间的 sin/cos 函数生成平滑的颜色过渡
 */
static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 基于时间计算颜色，实现平滑的颜色循环动画
    static DWORD startTime = GetTickCount();
    float elapsed = (GetTickCount() - startTime) / 1000.0f; // 秒

    // 使用 sin 函数生成 [0, 1] 范围的颜色分量
    float r = 0.5f + 0.5f * sinf(elapsed * 0.7f);          // 红色分量（慢速变化）
    float g = 0.5f + 0.5f * sinf(elapsed * 1.1f + 2.094f); // 绿色分量（相位偏移 120 度）
    float b = 0.5f + 0.5f * sinf(elapsed * 0.9f + 4.189f); // 蓝色分量（相位偏移 240 度）
    float clearColor[4] = { r, g, b, 1.0f };                // RGBA，alpha 恒为 1.0（不透明）

    // 清除渲染目标为指定颜色
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 呈现：将后台缓冲显示到屏幕
    // 参数 1 = SyncInterval = 1 表示开启垂直同步（VSync）
    // 参数 2 = 0 为常规标志
    HRESULT hr = g_swapChain->Present(1, 0);

    // 检查设备是否被移除（例如显卡驱动更新、GPU 崩溃等）
    // DXGI_ERROR_DEVICE_REMOVED 需要重新创建设备
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        // 简单处理：提示用户并退出
        // 在生产环境中，应该释放所有资源并重新初始化
        MessageBox(nullptr,
            L"GPU 设备已被移除或重置。\n程序即将退出。",
            L"设备错误", MB_ICONERROR | MB_OK);
        PostQuitMessage(1);
    }
}

// ============================================================================
// 窗口过程
// ============================================================================

/**
 * 窗口过程 — 处理窗口消息
 *
 * WM_SIZE: 窗口大小改变时调整交换链缓冲区
 * WM_DESTROY: 释放资源并退出消息循环
 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
    {
        // 窗口大小改变时，调整交换链缓冲区
        UINT width  = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        OnResize(width, height);
        return 0;
    }

    case WM_DESTROY:
    {
        // 释放所有 D3D11 资源（ComPtr 会自动 Release）
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
    wc.style         = CS_HREDRAW | CS_VREDRAW;   // 窗口大小改变时重绘
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
 * 流程：
 * 1. CoInitializeEx 初始化 COM 库（D3D11 基于 COM）
 * 2. 注册窗口类并创建窗口
 * 3. 初始化 D3D11（设备、交换链、渲染目标）
 * 4. 进入消息循环（PeekMessage 模式，空闲时持续渲染）
 * 5. 退出时清理 COM
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 初始化 COM 库（D3D11 的所有接口都是 COM 对象）
    // COINIT_MULTITHREADED 表示多线程模式
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
        L"D3D11 初始化示例",        // 窗口标题
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

    // ========================================================================
    // 消息循环（PeekMessage 模式）
    //
    // 与 GetMessage 阻塞模式不同，PeekMessage 是非阻塞的：
    // - 有消息时处理消息
    // - 没有消息时立即返回 FALSE，然后执行渲染
    // 这样可以确保在没有窗口消息时，GPU 也在持续渲染画面
    // ========================================================================
    MSG msg = {};
    bool running = true;

    while (running)
    {
        // 处理所有待处理的窗口消息
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

        // 空闲时持续渲染
        if (running)
        {
            Render();
        }
    }

    // 清理 COM
    CoUninitialize();

    return (int)msg.wParam;
}
