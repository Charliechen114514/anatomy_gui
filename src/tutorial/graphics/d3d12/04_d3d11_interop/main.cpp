/**
 * 04_d3d11_interop - D3D12 D3D11 互操作示例
 *
 * 本程序演示了 Direct3D 12 与 Direct3D 11 / Direct2D 的互操作：
 * 1. D3D11On12CreateDevice           — 在 D3D12 设备上创建 D3D11 设备
 * 2. ID3D11On12Device                — D3D11-on-12 互操作接口
 * 3. CreateWrappedResource           — 将 D3D12 资源包装为 D3D11 可用资源
 * 4. AcquireWrappedResources         — 获取包装资源（D3D11 端使用前）
 * 5. ReleaseWrappedResources         — 释放包装资源（D3D12 端使用前）
 * 6. D2D1CreateDevice                — 基于 D3D11 设备创建 Direct2D 设备
 * 7. ID2D1DeviceContext::CreateBitmapFromDxgiSurface — 从 DXGI 表面创建 D2D 位图
 *
 * 渲染流程：
 * - D3D12 清除背景为循环颜色
 * - D2D 通过 D3D11 互操作在背景上绘制半透明文本叠加
 *
 * 参考: tutorial/native_win32/47_ProgrammingGUI_Graphics_D3D12_D3D11Interop.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11on12.h>
#include <d3dcompiler.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <cstdio>
#include <cmath>

#include "ComHelper.hpp"

// ============================================================================
// 链接指令
// ============================================================================

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D3D12InteropClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;
static const int      FRAME_COUNT       = 2;

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    HWND hwnd;

    // D3D12 核心
    ComPtr<ID3D12Device>             device;
    ComPtr<ID3D12CommandQueue>       commandQueue;
    ComPtr<ID3D12CommandAllocator>   commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> commandList;

    // 交换链
    ComPtr<IDXGISwapChain3>          swapChain;
    ComPtr<ID3D12Resource>           renderTargets[FRAME_COUNT];
    ComPtr<ID3D12DescriptorHeap>     rtvHeap;
    UINT rtvDescriptorSize;

    // 围栏
    ComPtr<ID3D12Fence> fence;
    UINT64              fenceValues[FRAME_COUNT];
    HANDLE              fenceEvent;
    UINT                frameIndex;

    // ----------------------------------------------------------------------
    // D3D11-on-12 互操作
    // D3D11On12 允许在 D3D12 设备上运行 D3D11 代码。
    // 这对于使用 D2D（基于 D3D11）在 D3D12 渲染目标上绘制非常有用。
    // ----------------------------------------------------------------------
    ComPtr<ID3D11Device>             d3d11Device;         // D3D11 设备
    ComPtr<ID3D11DeviceContext>      d3d11Context;        // D3D11 设备上下文
    ComPtr<ID3D11On12Device>         d3d11On12Device;     // 互操作接口

    // 包装资源 — 将 D3D12 渲染目标包装为 D3D11 可用的纹理
    ComPtr<ID3D11Resource>           wrappedResources[FRAME_COUNT];

    // D2D — 用于 2D 文本渲染
    ComPtr<ID2D1Factory3>            d2dFactory;
    ComPtr<ID2D1Device2>             d2dDevice;
    ComPtr<ID2D1DeviceContext2>      d2dContext;
    ComPtr<ID2D1Bitmap1>             d2dBitmaps[FRAME_COUNT];  // 每帧一个 D2D 位图
    ComPtr<ID2D1SolidColorBrush>     textBrush;

    // DirectWrite — 文本格式化
    ComPtr<IDWriteFactory>           dwriteFactory;
    ComPtr<IDWriteTextFormat>        textFormat;

    // 计时
    LARGE_INTEGER startTime;
    LARGE_INTEGER frequency;
};

// ============================================================================
// WaitForFence / WaitForGpu — 同步辅助函数
// ============================================================================

static void WaitForFence(AppState* state, UINT64 waitValue)
{
    if (state->fence->GetCompletedValue() >= waitValue)
        return;
    HRESULT hr = state->fence->SetEventOnCompletion(waitValue, state->fenceEvent);
    ThrowIfFailed(hr, "SetEventOnCompletion 失败");
    WaitForSingleObject(state->fenceEvent, INFINITE);
}

static void WaitForGpu(AppState* state)
{
    UINT64 fenceVal = ++state->fenceValues[state->frameIndex];
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), fenceVal);
    ThrowIfFailed(hr, "Signal 失败");
    WaitForFence(state, fenceVal);
}

// ============================================================================
// InitD3D12 — 初始化 D3D12 核心
// ============================================================================

static void InitD3D12(AppState* state)
{
    HRESULT hr;

    // 创建 D3D12 设备
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(state->device.GetAddressOf()));
    ThrowIfFailed(hr, "D3D12CreateDevice 失败");

    // 创建命令队列
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = state->device->CreateCommandQueue(&queueDesc,
         IID_PPV_ARGS(state->commandQueue.GetAddressOf()));
    ThrowIfFailed(hr, "CreateCommandQueue 失败");

    // 每帧一个命令分配器
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        hr = state->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
             IID_PPV_ARGS(state->commandAllocators[i].GetAddressOf()));
        ThrowIfFailed(hr, "CreateCommandAllocator 失败");
        state->fenceValues[i] = 0;
    }

    // 创建命令列表
    hr = state->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
         state->commandAllocators[0].Get(), nullptr,
         IID_PPV_ARGS(state->commandList.GetAddressOf()));
    ThrowIfFailed(hr, "CreateCommandList 失败");

    // 创建交换链
    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width              = 0;
    swapDesc.Height             = 0;
    swapDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count   = 1;
    swapDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount        = FRAME_COUNT;
    swapDesc.Scaling            = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;

    ComPtr<IDXGIFactory4> factory;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()));
    ThrowIfFailed(hr, "CreateDXGIFactory2 失败");

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        state->commandQueue.Get(), state->hwnd,
        &swapDesc, nullptr, nullptr, &swapChain1);
    ThrowIfFailed(hr, "CreateSwapChainForHwnd 失败");

    hr = swapChain1.As(&state->swapChain);
    ThrowIfFailed(hr, "获取 SwapChain3 失败");
    factory->MakeWindowAssociation(state->hwnd, DXGI_MWA_NO_ALT_ENTER);

    // RTV 描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    hr = state->device->CreateDescriptorHeap(&rtvHeapDesc,
         IID_PPV_ARGS(state->rtvHeap.GetAddressOf()));
    ThrowIfFailed(hr, "CreateDescriptorHeap 失败");

    state->rtvDescriptorSize = state->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 获取渲染目标并创建 RTV
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        hr = state->swapChain->GetBuffer(i, IID_PPV_ARGS(state->renderTargets[i].GetAddressOf()));
        ThrowIfFailed(hr, "GetBuffer 失败");
        state->device->CreateRenderTargetView(state->renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += state->rtvDescriptorSize;
    }

    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // 创建围栏
    hr = state->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
         IID_PPV_ARGS(state->fence.GetAddressOf()));
    ThrowIfFailed(hr, "CreateFence 失败");

    state->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!state->fenceEvent)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent 失败");
}

// ============================================================================
// InitD3D11On12 — 初始化 D3D11-on-12 互操作
// ============================================================================

/**
 * D3D11On12CreateDevice 创建一个 D3D11 设备，它包装了 D3D12 设备。
 * 这个 D3D11 设备可以与 D3D12 共享渲染目标等资源。
 * 通过 ID3D11On12Device 接口，可以在 D3D11 和 D3D12 之间转换资源。
 */
static void InitD3D11On12(AppState* state)
{
    HRESULT hr;

    // 创建 D3D11-on-12 设备
    // 这个 D3D11 设备运行在 D3D12 设备之上
    hr = D3D11On12CreateDevice(
        state->device.Get(),                   // 底层 D3D12 设备
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,      // 标志：支持 BGRA（D2D 需要）
        nullptr, 0,                            // 功能级别（nullptr = 自动选择）
        reinterpret_cast<IUnknown**>(state->commandQueue.GetAddressOf()),  // 关联的 D3D12 命令队列
        1,                                     // 命令队列数量
        0,                                     // Node 数量
        state->d3d11Device.GetAddressOf(),     // 输出 D3D11 设备
        state->d3d11Context.GetAddressOf(),    // 输出 D3D11 上下文
        nullptr                                // 返回的功能级别
    );
    ThrowIfFailed(hr, "D3D11On12CreateDevice 失败");

    // 获取 ID3D11On12Device 接口（用于创建包装资源）
    hr = state->d3d11Device.As(&state->d3d11On12Device);
    ThrowIfFailed(hr, "获取 ID3D11On12Device 失败");

    // ----------------------------------------------------------------------
    // 为每个交换链缓冲区创建 D3D11 包装资源
    // CreateWrappedResource 将 D3D12 资源包装为 D3D11 可用的纹理。
    // 这样 D3D11/D2D 就可以在 D3D12 的渲染目标上绘制。
    // ----------------------------------------------------------------------
    D3D11_RESOURCE_FLAGS resourceFlags = {};
    resourceFlags.BindFlags = D3D11_BIND_RENDER_TARGET;

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        hr = state->d3d11On12Device->CreateWrappedResource(
            state->renderTargets[i].Get(),     // D3D12 资源
            &resourceFlags,                    // D3D11 资源标志
            D3D12_RESOURCE_STATE_RENDER_TARGET, // 输入状态（D3D11 使用时）
            D3D12_RESOURCE_STATE_PRESENT,      // 输出状态（D3D11 使用后）
            IID_PPV_ARGS(state->wrappedResources[i].GetAddressOf())
        );
        ThrowIfFailed(hr, "CreateWrappedResource 失败");
    }
}

// ============================================================================
// InitD2D — 初始化 Direct2D 和 DirectWrite
// ============================================================================

static void InitD2D(AppState* state)
{
    HRESULT hr;

    // 创建 D2D 工厂
    D2D1_FACTORY_OPTIONS options = {};
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
         state->d2dFactory.GetAddressOf());
    ThrowIfFailed(hr, "D2D1CreateFactory 失败");

    // 从 D3D11 设备创建 D2D 设备
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = state->d3d11Device.As(&dxgiDevice);
    ThrowIfFailed(hr, "获取 IDXGIDevice 失败");

    hr = state->d2dFactory->CreateDevice(dxgiDevice.Get(),
         state->d2dDevice.GetAddressOf());
    ThrowIfFailed(hr, "CreateD2DDevice 失败");

    // 创建 D2D 设备上下文
    hr = state->d2dDevice->CreateDeviceContext(
         D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
         state->d2dContext.GetAddressOf());
    ThrowIfFailed(hr, "CreateDeviceContext 失败");

    // 为每个包装资源创建 D2D 位图
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        // 从包装资源获取 DXGI 表面
        ComPtr<IDXGISurface> dxgiSurface;
        hr = state->wrappedResources[i]->QueryInterface(
             IID_PPV_ARGS(dxgiSurface.GetAddressOf()));
        ThrowIfFailed(hr, "获取 IDXGISurface 失败");

        // 从 DXGI 表面创建 D2D 位图
        D2D1_BITMAP_PROPERTIES1 bitmapProps = {};
        bitmapProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET
                                  | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        bitmapProps.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bitmapProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

        hr = state->d2dContext->CreateBitmapFromDxgiSurface(
             dxgiSurface.Get(), &bitmapProps,
             state->d2dBitmaps[i].GetAddressOf());
        ThrowIfFailed(hr, "CreateBitmapFromDxgiSurface 失败");
    }

    // 创建文本画刷（白色半透明）
    hr = state->d2dContext->CreateSolidColorBrush(
         D2D1::ColorF(D2D1::ColorF::White, 0.9f),
         state->textBrush.GetAddressOf());
    ThrowIfFailed(hr, "CreateSolidColorBrush 失败");

    // 创建 DirectWrite 工厂
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
         __uuidof(IDWriteFactory),
         reinterpret_cast<IUnknown**>(state->dwriteFactory.GetAddressOf()));
    ThrowIfFailed(hr, "DWriteCreateFactory 失败");

    // 创建文本格式
    hr = state->dwriteFactory->CreateTextFormat(
         L"Microsoft YaHei",                       // 字体名称
         nullptr,                                   // 字体集合
         DWRITE_FONT_WEIGHT_NORMAL,                 // 字重
         DWRITE_FONT_STYLE_NORMAL,                  // 字体样式
         DWRITE_FONT_STRETCH_NORMAL,                // 字体拉伸
         28.0f,                                     // 字号
         L"zh-CN",                                  // 区域设置
         state->textFormat.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateTextFormat 失败");

    hr = state->textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    ThrowIfFailed(hr, "SetTextAlignment 失败");

    hr = state->textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    ThrowIfFailed(hr, "SetParagraphAlignment 失败");
}

// ============================================================================
// PopulateD3D12Commands — 录制 D3D12 清除命令
// ============================================================================

static void PopulateD3D12Commands(AppState* state, const float clearColor[4])
{
    HRESULT hr;

    hr = state->commandAllocators[state->frameIndex]->Reset();
    ThrowIfFailed(hr, "CommandAllocator::Reset 失败");

    hr = state->commandList->Reset(
        state->commandAllocators[state->frameIndex].Get(), nullptr);
    ThrowIfFailed(hr, "CommandList::Reset 失败");

    // 资源屏障：PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    state->commandList->ResourceBarrier(1, &barrier);

    // 清除背景
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += state->frameIndex * state->rtvDescriptorSize;
    state->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    state->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 注意：不在这里转换回 PRESENT 状态
    // D2D 绘制完成后才会转换回 PRESENT

    hr = state->commandList->Close();
    ThrowIfFailed(hr, "CommandList::Close 失败");
}

// ============================================================================
// RenderD2DOverlay — 使用 D2D 绘制文本叠加
// ============================================================================

/**
 * D3D11-on-12 互操作的渲染流程：
 * 1. D3D12 执行命令（清除背景）-> ExecuteCommandLists
 * 2. 获取包装资源（AcquireWrappedResources）— D3D11 接管
 * 3. D2D 绘制文本叠加
 * 4. 释放包装资源（ReleaseWrappedResources）— D3D12 接管
 * 5. D3D12 转换回 PRESENT 状态 -> Present
 */
static void RenderD2DOverlay(AppState* state, const wchar_t* text)
{
    HRESULT hr;

    // ----------------------------------------------------------------------
    // 步骤 1: 获取包装资源 — 将 D3D12 资源"借给" D3D11 使用
    // AcquireWrappedResources 会将资源状态从输出状态转换为输入状态
    // ----------------------------------------------------------------------
    ID3D11Resource* resources[1] = { state->wrappedResources[state->frameIndex].Get() };
    state->d3d11On12Device->AcquireWrappedResources(resources, 1);

    // ----------------------------------------------------------------------
    // 步骤 2: D2D 绘制
    // 设置 D2D 目标位图并绘制文本
    // ----------------------------------------------------------------------
    state->d2dContext->SetTarget(state->d2dBitmaps[state->frameIndex].Get());

    state->d2dContext->BeginDraw();

    // 绘制半透明背景面板
    D2D1_SIZE_F size = state->d2dBitmaps[state->frameIndex]->GetSize();
    float panelHeight = 80.0f;
    float panelY = (size.height - panelHeight) / 2.0f;

    // 创建半透明黑色背景画刷
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    hr = state->d2dContext->CreateSolidColorBrush(
         D2D1::ColorF(D2D1::ColorF::Black, 0.5f), bgBrush.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        D2D1_RECT_F bgRect = D2D1::RectF(0, panelY, size.width, panelY + panelHeight);
        state->d2dContext->FillRectangle(bgRect, bgBrush.Get());
    }

    // 绘制居中文字
    D2D1_RECT_F textRect = D2D1::RectF(0, panelY, size.width, panelY + panelHeight);
    state->d2dContext->DrawText(
        text, (UINT32)wcslen(text),
        state->textFormat.Get(),
        textRect,
        state->textBrush.Get()
    );

    hr = state->d2dContext->EndDraw();
    ThrowIfFailed(hr, "D2D EndDraw 失败");

    // ----------------------------------------------------------------------
    // 步骤 3: 释放包装资源 — 将资源"还给" D3D12
    // ReleaseWrappedResources 会将资源状态从输入状态转换为输出状态
    // ----------------------------------------------------------------------
    state->d3d11On12Device->ReleaseWrappedResources(resources, 1);

    // 刷新 D3D11 上下文（确保所有 D3D11 命令已完成）
    state->d3d11Context->Flush();
}

// ============================================================================
// Render — 渲染一帧
// ============================================================================

static void Render(AppState* state)
{
    // 计算循环背景颜色
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float elapsed = (float)(now.QuadPart - state->startTime.QuadPart)
                  / (float)state->frequency.QuadPart;

    float t = elapsed * 0.3f;
    float r = 0.1f + 0.1f * (float)sin(t);
    float g = 0.15f + 0.1f * (float)sin(t + 2.094f);
    float b = 0.25f + 0.15f * (float)sin(t + 4.189f);

    float clearColor[4] = { r, g, b, 1.0f };

    // ----------------------------------------------------------------------
    // 阶段 1: D3D12 渲染（清除背景）
    // ----------------------------------------------------------------------
    PopulateD3D12Commands(state, clearColor);

    ID3D12CommandList* ppCommandLists[] = { state->commandList.Get() };
    state->commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // 等待 D3D12 命令完成
    UINT64 d3d12Fence = ++state->fenceValues[state->frameIndex];
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), d3d12Fence);
    ThrowIfFailed(hr, "Signal 失败");
    WaitForFence(state, d3d12Fence);

    // ----------------------------------------------------------------------
    // 阶段 2: D2D 文本叠加（通过 D3D11 互操作）
    // ----------------------------------------------------------------------
    // 构建要显示的文本
    wchar_t overlayText[256];
    swprintf(overlayText, 256,
        L"D3D12 + D2D 互操作\n"
        L"D3D12 渲染背景 | D2D 渲染文字叠加");
    RenderD2DOverlay(state, overlayText);

    // ----------------------------------------------------------------------
    // 阶段 3: 转换资源状态并呈现
    // 重新打开命令列表来转换资源状态
    // ----------------------------------------------------------------------
    hr = state->commandAllocators[state->frameIndex]->Reset();
    ThrowIfFailed(hr, "CommandAllocator::Reset 失败");

    hr = state->commandList->Reset(
        state->commandAllocators[state->frameIndex].Get(), nullptr);
    ThrowIfFailed(hr, "CommandList::Reset 失败");

    // 资源屏障：RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    state->commandList->ResourceBarrier(1, &barrier);

    hr = state->commandList->Close();
    ThrowIfFailed(hr, "CommandList::Close 失败");

    state->commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // 呈现
    hr = state->swapChain->Present(1, 0);
    ThrowIfFailed(hr, "Present 失败");

    // 切换到下一帧
    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // 等待新帧可用
    if (state->fenceValues[state->frameIndex] > 0)
        WaitForFence(state, state->fenceValues[state->frameIndex]);
}

// ============================================================================
// OnDestroy — 清理所有资源
// ============================================================================

static void OnDestroy(AppState* state)
{
    WaitForGpu(state);

    if (state->fenceEvent)
    {
        CloseHandle(state->fenceEvent);
        state->fenceEvent = nullptr;
    }

    // D2D / DWrite 资源
    state->textBrush.Reset();
    state->textFormat.Reset();
    state->dwriteFactory.Reset();
    for (int i = 0; i < FRAME_COUNT; ++i)
        state->d2dBitmaps[i].Reset();
    state->d2dContext.Reset();
    state->d2dDevice.Reset();
    state->d2dFactory.Reset();

    // D3D11 资源
    for (int i = 0; i < FRAME_COUNT; ++i)
        state->wrappedResources[i].Reset();
    state->d3d11On12Device.Reset();
    state->d3d11Context.Reset();
    state->d3d11Device.Reset();

    // D3D12 资源
    state->commandList.Reset();
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        state->commandAllocators[i].Reset();
        state->renderTargets[i].Reset();
    }
    state->rtvHeap.Reset();
    state->fence.Reset();
    state->swapChain.Reset();
    state->commandQueue.Reset();
    state->device.Reset();
}

// ============================================================================
// InitAll — 完整初始化流程
// ============================================================================

static void InitAll(AppState* state)
{
    InitD3D12(state);     // 步骤 1: 初始化 D3D12 核心
    InitD3D11On12(state); // 步骤 2: 初始化 D3D11-on-12 互操作
    InitD2D(state);       // 步骤 3: 初始化 D2D 和 DirectWrite

    QueryPerformanceFrequency(&state->frequency);
    QueryPerformanceCounter(&state->startTime);
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

    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"D3D12 D3D11 互操作示例",
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

    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    InitAll(state);
    InvalidateRect(hwnd, nullptr, FALSE);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
