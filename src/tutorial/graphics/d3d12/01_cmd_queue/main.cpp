/**
 * 01_cmd_queue - D3D12 命令队列与围栏示例
 *
 * 本程序演示了 Direct3D 12 中最基础的概念：
 * 1. D3D12CreateDevice             — 创建 D3D12 设备
 * 2. CreateCommandQueue            — 创建命令队列（GPU 端执行引擎）
 * 3. CreateCommandAllocator        — 创建命令分配器（管理命令内存）
 * 4. CreateCommandList             — 创建命令列表（记录 GPU 命令）
 * 5. CreateFence + Event           — CPU/GPU 同步原语
 * 6. ExecuteCommandLists + Signal  — 提交命令并发出围栏信号
 * 7. RTV 描述符堆 + 交换链渲染目标  — 基本的显示输出
 *
 * 本示例是"最简单的 D3D12 程序"：仅清除屏幕为循环渐变色。
 *
 * 参考: tutorial/native_win32/44_ProgrammingGUI_Graphics_D3D12_CmdQueue.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdio>
#include <cmath>

#include "ComHelper.hpp"

// ============================================================================
// 链接指令 — 告诉链接器需要哪些系统库
// ============================================================================

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D3D12CmdQueueClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;
static const int      FRAME_COUNT       = 2;   // 双缓冲帧数

// ============================================================================
// 应用程序状态 — 包含所有 D3D12 资源
// ============================================================================

struct AppState
{
    // 窗口句柄
    HWND hwnd;

    // D3D12 核心对象
    ComPtr<ID3D12Device>        device;         // D3D12 设备
    ComPtr<ID3D12CommandQueue>  commandQueue;   // 命令队列
    ComPtr<ID3D12CommandAllocator> commandAllocator; // 命令分配器
    ComPtr<ID3D12GraphicsCommandList> commandList;   // 命令列表

    // 交换链
    ComPtr<IDXGISwapChain3>     swapChain;
    ComPtr<ID3D12Resource>      renderTargets[FRAME_COUNT]; // 渲染目标缓冲区

    // RTV 描述符堆 — 用于存储渲染目标视图
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT rtvDescriptorSize;                     // 每个描述符的大小

    // 围栏 — 用于 CPU/GPU 同步
    ComPtr<ID3D12Fence> fence;
    UINT64              fenceValue;     // 当前围栏值
    HANDLE              fenceEvent;     // 围栏事件对象

    // 帧索引
    UINT frameIndex;

    // 动画计时
    LARGE_INTEGER startTime;
    LARGE_INTEGER frequency;
};

// ============================================================================
// WaitForFence — 等待围栏达到指定值（CPU/GPU 同步的核心函数）
// ============================================================================

/**
 * 等待 GPU 将围栏信号发到指定值。
 *
 * 原理：
 * 1. 检查当前围栏的 CompletedValue 是否 >= waitValue
 * 2. 如果已经达到，说明 GPU 已完成，直接返回
 * 3. 如果未达到，设置围栏事件（SetEventOnCompletion），然后等待事件触发
 */
static void WaitForFence(AppState* state, UINT64 waitValue)
{
    // 如果 GPU 已经完成，直接返回
    if (state->fence->GetCompletedValue() >= waitValue)
        return;

    // 设置当围栏达到 waitValue 时触发事件
    HRESULT hr = state->fence->SetEventOnCompletion(waitValue, state->fenceEvent);
    ThrowIfFailed(hr, "SetEventOnCompletion 失败");

    // 无限等待事件触发（CPU 阻塞直到 GPU 信号到达）
    WaitForSingleObject(state->fenceEvent, INFINITE);
}

// ============================================================================
// WaitForGpu — 等待 GPU 完成所有已提交的工作
// ============================================================================

static void WaitForGpu(AppState* state)
{
    // 向命令队列发送一个围栏信号
    // Signal 会将围栏值设为 fenceValue，当 GPU 执行到这里时生效
    UINT64 fenceVal = ++state->fenceValue;
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), fenceVal);
    ThrowIfFailed(hr, "CommandQueue::Signal 失败");

    // 在 CPU 端等待该围栏值
    WaitForFence(state, fenceVal);
}

// ============================================================================
// InitD3D12 — 初始化 Direct3D 12
// ============================================================================

static void InitD3D12(AppState* state)
{
    HRESULT hr;

    // ----------------------------------------------------------------------
    // 步骤 1: 创建 D3D12 设备
    // D3D12CreateDevice 是整个 D3D12 的入口点。
    // 我们请求功能级别 11_0（Direct3D 11.0 级别的硬件功能）。
    // ----------------------------------------------------------------------
    hr = D3D12CreateDevice(
        nullptr,                        // 默认适配器
        D3D_FEATURE_LEVEL_11_0,         // 最低功能级别
        IID_PPV_ARGS(state->device.GetAddressOf())
    );
    ThrowIfFailed(hr, "D3D12CreateDevice 失败 — 请确保显卡支持 D3D12");

    // ----------------------------------------------------------------------
    // 步骤 2: 创建命令队列
    // D3D12_COMMAND_LIST_TYPE_DIRECT 表示直接命令队列，
    // 可以执行所有 GPU 命令（绘制、计算、复制等）。
    // 这是 D3D12 与 D3D11 的重要区别：D3D12 需要显式管理命令队列。
    // ----------------------------------------------------------------------
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = state->device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(state->commandQueue.GetAddressOf())
    );
    ThrowIfFailed(hr, "CreateCommandQueue 失败");

    // ----------------------------------------------------------------------
    // 步骤 3: 创建命令分配器
    // 命令分配器管理命令列表的底层内存。
    // 每帧结束后需要 Reset 以回收内存。
    // ----------------------------------------------------------------------
    hr = state->device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(state->commandAllocator.GetAddressOf())
    );
    ThrowIfFailed(hr, "CreateCommandAllocator 失败");

    // ----------------------------------------------------------------------
    // 步骤 4: 创建命令列表
    // 命令列表是录制 GPU 命令的接口。
    // 初始状态为"打开"（open），可以立即录制命令。
    // ----------------------------------------------------------------------
    hr = state->device->CreateCommandList(
        0,                                          // 节点掩码（单 GPU 为 0）
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        state->commandAllocator.Get(),              // 关联的命令分配器
        nullptr,                                    // 初始管线状态（无）
        IID_PPV_ARGS(state->commandList.GetAddressOf())
    );
    ThrowIfFailed(hr, "CreateCommandList 失败");

    // 关闭命令列表（初始化阶段不录制命令）
    hr = state->commandList->Close();
    ThrowIfFailed(hr, "关闭命令列表失败");

    // ----------------------------------------------------------------------
    // 步骤 5: 创建交换链
    // 使用 DXGI 创建交换链，与 D3D11 类似但需要指定命令队列。
    // ----------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width              = 0;                      // 自动匹配窗口
    swapChainDesc.Height             = 0;
    swapChainDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM; // 32 位 RGBA
    swapChainDesc.Stereo             = FALSE;
    swapChainDesc.SampleDesc.Count   = 1;                      // 不使用多重采样
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount        = FRAME_COUNT;            // 双缓冲
    swapChainDesc.Scaling            = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Flip 模式
    swapChainDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;

    // 获取 DXGI 工厂（用于创建交换链）
    ComPtr<IDXGIFactory4> factory;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()));
    ThrowIfFailed(hr, "CreateDXGIFactory2 失败");

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        state->commandQueue.Get(),       // D3D12 需要传入命令队列！
        state->hwnd,
        &swapChainDesc,
        nullptr, nullptr,
        &swapChain1
    );
    ThrowIfFailed(hr, "CreateSwapChainForHwnd 失败");

    // 获取 SwapChain3 接口（支持 GetCurrentBackBufferIndex）
    hr = swapChain1.As(&state->swapChain);
    ThrowIfFailed(hr, "获取 IDXGISwapChain3 失败");

    // 禁止 Alt+Enter 全屏切换（示例程序不需要）
    factory->MakeWindowAssociation(state->hwnd, DXGI_MWA_NO_ALT_ENTER);

    // ----------------------------------------------------------------------
    // 步骤 6: 创建 RTV 描述符堆
    // RTV (Render Target View) 描述符堆存储渲染目标视图。
    // 每个交换链缓冲区需要一个 RTV。
    // ----------------------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = state->device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(state->rtvHeap.GetAddressOf())
    );
    ThrowIfFailed(hr, "CreateDescriptorHeap (RTV) 失败");

    // 获取描述符大小（不同 GPU 上描述符大小可能不同）
    state->rtvDescriptorSize = state->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ----------------------------------------------------------------------
    // 步骤 7: 获取渲染目标并创建 RTV
    // 从交换链获取每个缓冲区的资源，然后为每个创建渲染目标视图。
    // ----------------------------------------------------------------------
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state->rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        hr = state->swapChain->GetBuffer(i, IID_PPV_ARGS(state->renderTargets[i].GetAddressOf()));
        ThrowIfFailed(hr, "GetBuffer 失败");

        // 为交换链缓冲区创建渲染目标视图
        state->device->CreateRenderTargetView(
            state->renderTargets[i].Get(),   // 目标资源
            nullptr,                          // 描述（nullptr = 使用资源默认格式）
            rtvHandle                         // 描述符句柄
        );

        // 移动到下一个描述符位置
        rtvHandle.ptr += state->rtvDescriptorSize;
    }

    // 获取当前后缓冲区索引
    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // ----------------------------------------------------------------------
    // 步骤 8: 创建围栏
    // 围栏是 D3D12 的核心同步机制，用于协调 CPU 和 GPU。
    // ----------------------------------------------------------------------
    hr = state->device->CreateFence(
        0,                                  // 初始值
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(state->fence.GetAddressOf())
    );
    ThrowIfFailed(hr, "CreateFence 失败");

    state->fenceValue = 0;

    // 创建事件对象（用于等待围栏通知）
    state->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!state->fenceEvent)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent 失败");

    // 初始化计时器
    QueryPerformanceFrequency(&state->frequency);
    QueryPerformanceCounter(&state->startTime);
}

// ============================================================================
// OnDestroy — 清理所有 D3D12 资源
// ============================================================================

static void OnDestroy(AppState* state)
{
    // 确保所有 GPU 命令已完成后再释放资源
    WaitForGpu(state);

    // 关闭事件句柄
    if (state->fenceEvent)
    {
        CloseHandle(state->fenceEvent);
        state->fenceEvent = nullptr;
    }

    // ComPtr 会自动释放 COM 对象，但需要按正确顺序：
    // 命令列表 -> 命令分配器 -> 渲染目标 -> 描述符堆 -> 围栏 -> 命令队列 -> 交换链 -> 设备
    // ComPtr 析构顺序与声明顺序相反，所以这里显式 Reset
    state->commandList.Reset();
    state->commandAllocator.Reset();
    for (int i = 0; i < FRAME_COUNT; ++i)
        state->renderTargets[i].Reset();
    state->rtvHeap.Reset();
    state->fence.Reset();
    state->swapChain.Reset();
    state->commandQueue.Reset();
    state->device.Reset();
}

// ============================================================================
// PopulateCommandList — 录制一帧的渲染命令
// ============================================================================

/**
 * 录制命令列表：清除渲染目标为指定颜色。
 *
 * D3D12 中命令列表的工作流程：
 * 1. Reset 命令分配器和命令列表（准备录制）
 * 2. 设置资源屏障（ResourceBarrier）— 转换资源状态
 * 3. 录制渲染命令（ClearRenderTargetView 等）
 * 4. 再次设置资源屏障（转换回呈现状态）
 * 5. 关闭命令列表（完成录制）
 */
static void PopulateCommandList(AppState* state, const float clearColor[4])
{
    HRESULT hr;

    // 重置命令分配器（回收上一帧的命令内存）
    hr = state->commandAllocator->Reset();
    ThrowIfFailed(hr, "CommandAllocator::Reset 失败");

    // 重置命令列表（关联新的命令分配器，开始录制新命令）
    hr = state->commandList->Reset(state->commandAllocator.Get(), nullptr);
    ThrowIfFailed(hr, "CommandList::Reset 失败");

    // ------------------------------------------------------------------
    // 资源屏障：将渲染目标从"呈现"(PRESENT) 转换为"渲染目标"(RTV) 状态
    // D3D12 要求显式管理资源状态转换！
    // 这是 D3D12 与 D3D11 最重要的区别之一。
    // ------------------------------------------------------------------
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    state->commandList->ResourceBarrier(1, &barrier);

    // ------------------------------------------------------------------
    // 设置渲染目标并清除
    // ------------------------------------------------------------------
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += state->frameIndex * state->rtvDescriptorSize;

    // 设置渲染目标
    state->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // 清除渲染目标为指定颜色
    state->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // ------------------------------------------------------------------
    // 资源屏障：将渲染目标从"渲染目标"(RTV) 转换回"呈现"(PRESENT) 状态
    // ------------------------------------------------------------------
    D3D12_RESOURCE_BARRIER barrierEnd = {};
    barrierEnd.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierEnd.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierEnd.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;

    state->commandList->ResourceBarrier(1, &barrierEnd);

    // 关闭命令列表（完成录制）
    hr = state->commandList->Close();
    ThrowIfFailed(hr, "CommandList::Close 失败");
}

// ============================================================================
// Render — 渲染一帧
// ============================================================================

static void Render(AppState* state)
{
    // 计算循环渐变颜色
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - state->startTime.QuadPart)
                   / (double)state->frequency.QuadPart;

    float t = (float)elapsed * 0.5f; // 慢速循环

    // 使用正弦函数生成平滑的 RGB 渐变
    float r = 0.2f + 0.2f * (float)sin(t);
    float g = 0.2f + 0.2f * (float)sin(t + 2.094f);  // 偏移 2*PI/3
    float b = 0.2f + 0.2f * (float)sin(t + 4.189f);  // 偏移 4*PI/3

    float clearColor[4] = { r, g, b, 1.0f };

    // 录制命令（清除屏幕）
    PopulateCommandList(state, clearColor);

    // 执行命令列表
    ID3D12CommandList* ppCommandLists[] = { state->commandList.Get() };
    state->commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // 发出围栏信号 — 当 GPU 执行到这里时将围栏值设为 fenceValue+1
    UINT64 currentFence = ++state->fenceValue;
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), currentFence);
    ThrowIfFailed(hr, "CommandQueue::Signal 失败");

    // 呈现交换链（将渲染结果显示到屏幕）
    hr = state->swapChain->Present(1, 0); // 1 = 开启 VSync
    ThrowIfFailed(hr, "SwapChain::Present 失败");

    // 更新帧索引
    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // 等待上一帧的 GPU 工作完成（使用围栏同步）
    // 这确保命令分配器可以被安全重置
    WaitForFence(state, currentFence);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        state = new AppState();
        state->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;
    }

    case WM_DESTROY:
    {
        if (state)
        {
            OnDestroy(state);
            delete state;
        }
        PostQuitMessage(0);
        return 0;
    }

    case WM_PAINT:
    {
        if (state && state->device)
            Render(state);

        // 不调用 BeginPaint/EndPaint，因为我们用 D3D12 渲染
        // 但需要验证区域以避免重复 WM_PAINT 消息
        ValidateRect(hwnd, nullptr);
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
    wc.lpfnWndProc   = WindowProc;
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

    // 注册窗口类
    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"D3D12 命令队列示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 初始化 D3D12
    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    InitD3D12(state);
    InvalidateRect(hwnd, nullptr, FALSE);

    // 主消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
