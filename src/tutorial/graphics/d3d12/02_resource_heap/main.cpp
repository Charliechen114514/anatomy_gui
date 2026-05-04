/**
 * 02_resource_heap - D3D12 资源与堆管理示例
 *
 * 本程序演示了 Direct3D12 中的资源（Resource）和堆（Heap）管理：
 * 1. CreateCommittedResource   — 创建资源（上传堆 vs 默认堆）
 * 2. D3D12_RESOURCE_DESC      — 资源描述（缓冲区、纹理等）
 * 3. D3D12_HEAP_PROPERTIES    — 堆属性（上传堆、默认堆、回读堆）
 * 4. ResourceBarrier          — 资源状态转换（PRESENT <-> RENDER_TARGET）
 * 5. 数据从上传堆复制到默认堆   — CPU 到 GPU 的数据传输
 *
 * 本示例使用硬编码顶点着色器（顶点在着色器中生成），渲染一个彩色三角形，
 * 无需顶点缓冲区。这是 D3D12 资源管理的入门示例。
 *
 * 参考: tutorial/native_win32/45_ProgrammingGUI_Graphics_D3D12_ResourceHeap.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cmath>

#include "ComHelper.hpp"
#include "D3D12Compat.hpp"

// ============================================================================
// 链接指令
// ============================================================================

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"D3D12ResourceHeapClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;
static const int      FRAME_COUNT       = 2;

// ============================================================================
// 内嵌着色器 — 使用着色器生成顶点（无需顶点缓冲区）
// ============================================================================

/**
 * 本示例使用一个简单的 HLSL 着色器，直接在顶点着色器中生成三角形顶点。
 * SV_VertexID 提供顶点索引（0, 1, 2），着色器根据索引计算位置和颜色。
 * 这样无需顶点缓冲区即可绘制三角形，适合入门示例。
 */
static const char* g_shaderSource = R"(
    // 顶点着色器 — 使用 SV_VertexID 在着色器中生成三角形顶点
    struct VS_OUTPUT
    {
        float4 pos : SV_POSITION;
        float4 col : COLOR;
    };

    VS_OUTPUT VSMain(uint vertexID : SV_VertexID)
    {
        VS_OUTPUT output;

        // 三角形的三个顶点位置（标准化设备坐标）
        float2 positions[3] =
        {
            float2( 0.0f,  0.5f),   // 顶部
            float2( 0.5f, -0.5f),   // 右下
            float2(-0.5f, -0.5f),   // 左下
        };

        // 三个顶点的颜色
        float3 colors[3] =
        {
            float3(1.0f, 0.0f, 0.0f),   // 红色
            float3(0.0f, 1.0f, 0.0f),   // 绿色
            float3(0.0f, 0.0f, 1.0f),   // 蓝色
        };

        output.pos = float4(positions[vertexID], 0.0f, 1.0f);
        output.col = float4(colors[vertexID], 1.0f);
        return output;
    }

    // 像素着色器 — 直接输出顶点颜色
    float4 PSMain(VS_OUTPUT input) : SV_TARGET
    {
        return input.col;
    }
)";

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    HWND hwnd;

    // D3D12 核心对象
    ComPtr<ID3D12Device>             device;
    ComPtr<ID3D12CommandQueue>       commandQueue;
    ComPtr<ID3D12CommandAllocator>   commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    // 交换链与渲染目标
    ComPtr<IDXGISwapChain3>          swapChain;
    ComPtr<ID3D12Resource>           renderTargets[FRAME_COUNT];
    ComPtr<ID3D12DescriptorHeap>     rtvHeap;
    UINT rtvDescriptorSize;

    // 围栏同步
    ComPtr<ID3D12Fence> fence;
    UINT64              fenceValue;
    HANDLE              fenceEvent;

    // 管线状态 — 包含着色器和渲染状态
    ComPtr<ID3D12RootSignature>      rootSignature;
    ComPtr<ID3D12PipelineState>      pipelineState;

    UINT frameIndex;
    LARGE_INTEGER startTime;
    LARGE_INTEGER frequency;
};

// ============================================================================
// 辅助函数：编译着色器
// ============================================================================

/**
 * 使用 D3DCompile 从源代码编译着色器。
 * 本示例使用内嵌着色器代码，无需外部 .hlsl 文件。
 */
static ComPtr<ID3DBlob> CompileShader(
    const char* source,
    const char* entryPoint,
    const char* target)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source,                        // 着色器源代码
        strlen(source),               // 源代码长度
        nullptr,                       // 调试用的源文件名
        nullptr,                       // 宏定义
        nullptr,                       // Include 接口
        entryPoint,                    // 入口函数名
        target,                        // 着色器目标（vs_5_0, ps_5_0）
        0,                             // 编译标志
        0,                             // 效果编译标志
        shaderBlob.GetAddressOf(),     // 输出的着色器字节码
        errorBlob.GetAddressOf()       // 输出的错误信息
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            char buf[512];
            snprintf(buf, sizeof(buf), "着色器编译失败: %s",
                     (const char*)errorBlob->GetBufferPointer());
            MessageBoxA(nullptr, buf, "Shader Error", MB_ICONERROR);
        }
        ThrowIfFailed(hr, "D3DCompile 失败");
    }

    return shaderBlob;
}

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
    UINT64 fenceVal = ++state->fenceValue;
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), fenceVal);
    ThrowIfFailed(hr, "Signal 失败");
    WaitForFence(state, fenceVal);
}

// ============================================================================
// InitD3D12 — 初始化 Direct3D 12
// ============================================================================

static void InitD3D12(AppState* state)
{
    HRESULT hr;

    // 创建设备
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

    // 创建命令分配器和命令列表
    hr = state->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
         IID_PPV_ARGS(state->commandAllocator.GetAddressOf()));
    ThrowIfFailed(hr, "CreateCommandAllocator 失败");

    hr = state->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
         state->commandAllocator.Get(), nullptr,
         IID_PPV_ARGS(state->commandList.GetAddressOf()));
    ThrowIfFailed(hr, "CreateCommandList 失败");

    // 创建交换链
    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width              = 0;
    swapDesc.Height             = 0;
    swapDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.Stereo             = FALSE;
    swapDesc.SampleDesc.Count   = 1;
    swapDesc.SampleDesc.Quality = 0;
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

    // ----------------------------------------------------------------------
    // 创建 RTV 描述符堆
    // ----------------------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
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

    // ----------------------------------------------------------------------
    // 创建根签名 — 本示例不需要任何参数
    // 根签名定义了着色器可以访问的资源（常量缓冲区、纹理等）。
    // 这里使用"空"根签名，因为我们的着色器不需要外部资源。
    // ----------------------------------------------------------------------
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 0;
    rootSigDesc.pParameters   = nullptr;
    rootSigDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
         signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    ThrowIfFailed(hr, "D3D12SerializeRootSignature 失败");

    hr = state->device->CreateRootSignature(0,
         signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
         IID_PPV_ARGS(state->rootSignature.GetAddressOf()));
    ThrowIfFailed(hr, "CreateRootSignature 失败");

    // ----------------------------------------------------------------------
    // 演示：创建上传堆资源
    // 上传堆（D3D12_HEAP_TYPE_UPLOAD）是 CPU 可写的内存区域，
    // 用于将数据从 CPU 传输到 GPU。
    // 虽然本示例不使用顶点缓冲区，但展示上传堆的创建方式。
    // ----------------------------------------------------------------------
    {
        // 创建一个上传堆缓冲区（用于演示）
        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CreationNodeMask     = 0;
        uploadHeapProps.VisibleNodeMask      = 0;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width              = 256;     // 256 字节缓冲区（示例）
        bufferDesc.Height             = 1;
        bufferDesc.DepthOrArraySize   = 1;
        bufferDesc.MipLevels          = 1;
        bufferDesc.Format             = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count   = 1;
        bufferDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> uploadBuffer;
        hr = state->device->CreateCommittedResource(
            &uploadHeapProps,                         // 堆属性：上传堆
            D3D12_HEAP_FLAG_NONE,                     // 堆标志
            &bufferDesc,                              // 资源描述
            D3D12_RESOURCE_STATE_GENERIC_READ,        // 初始状态：通用读取
            nullptr,                                   // 清除值（缓冲区不需要）
            IID_PPV_ARGS(uploadBuffer.GetAddressOf()) // 输出资源
        );
        ThrowIfFailed(hr, "CreateCommittedResource (上传堆) 失败");
        // uploadBuffer 在此处超出作用域后自动释放
        // 在实际应用中，您会将其保存在 AppState 中并跨帧使用
    }

    // ----------------------------------------------------------------------
    // 编译着色器并创建管线状态对象（PSO）
    // ----------------------------------------------------------------------
    ComPtr<ID3DBlob> vertexShader = CompileShader(g_shaderSource, "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> pixelShader  = CompileShader(g_shaderSource, "PSMain", "ps_5_0");

    // 输入布局 — 本示例为空（顶点在着色器中生成，无需输入）
    D3D12_INPUT_LAYOUT_DESC inputLayout = {};
    inputLayout.NumElements        = 0;
    inputLayout.pInputElementDescs = nullptr;

    // 管线状态描述
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout           = inputLayout;
    psoDesc.pRootSignature        = state->rootSignature.Get();
    psoDesc.VS                    = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };
    psoDesc.PS                    = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };
    psoDesc.RasterizerState       = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState            = CD3D12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState     = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;  // 不使用深度测试
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count      = 1;

    hr = state->device->CreateGraphicsPipelineState(&psoDesc,
         IID_PPV_ARGS(state->pipelineState.GetAddressOf()));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState 失败");

    // 关闭命令列表（初始化期间不需要录制）
    hr = state->commandList->Close();
    ThrowIfFailed(hr, "关闭命令列表失败");

    // 创建围栏
    hr = state->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
         IID_PPV_ARGS(state->fence.GetAddressOf()));
    ThrowIfFailed(hr, "CreateFence 失败");

    state->fenceValue = 0;
    state->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!state->fenceEvent)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent 失败");

    QueryPerformanceFrequency(&state->frequency);
    QueryPerformanceCounter(&state->startTime);
}

// ============================================================================
// PopulateCommandList — 录制渲染命令
// ============================================================================

static void PopulateCommandList(AppState* state)
{
    HRESULT hr;

    // 重置命令分配器和命令列表
    hr = state->commandAllocator->Reset();
    ThrowIfFailed(hr, "CommandAllocator::Reset 失败");

    hr = state->commandList->Reset(state->commandAllocator.Get(), state->pipelineState.Get());
    ThrowIfFailed(hr, "CommandList::Reset 失败");

    // 资源屏障：PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    state->commandList->ResourceBarrier(1, &barrier);

    // 设置渲染目标
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += state->frameIndex * state->rtvDescriptorSize;
    state->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // 清除背景为深灰色
    float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    state->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // ----------------------------------------------------------------------
    // 设置管线状态并绘制三角形
    // 由于顶点在着色器中通过 SV_VertexID 生成，无需设置顶点缓冲区。
    // DrawInstanced(3, 1, 0, 0) = 绘制 3 个顶点，1 个实例
    // ----------------------------------------------------------------------
    state->commandList->SetGraphicsRootSignature(state->rootSignature.Get());
    state->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state->commandList->DrawInstanced(3, 1, 0, 0);

    // 资源屏障：RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER barrierEnd = {};
    barrierEnd.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierEnd.Transition.pResource   = state->renderTargets[state->frameIndex].Get();
    barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierEnd.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    state->commandList->ResourceBarrier(1, &barrierEnd);

    hr = state->commandList->Close();
    ThrowIfFailed(hr, "CommandList::Close 失败");
}

// ============================================================================
// Render — 渲染一帧
// ============================================================================

static void Render(AppState* state)
{
    PopulateCommandList(state);

    // 执行命令列表
    ID3D12CommandList* ppCommandLists[] = { state->commandList.Get() };
    state->commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // 发出围栏信号
    UINT64 currentFence = ++state->fenceValue;
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), currentFence);
    ThrowIfFailed(hr, "Signal 失败");

    // 呈现
    hr = state->swapChain->Present(1, 0);
    ThrowIfFailed(hr, "Present 失败");

    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // 等待帧完成
    WaitForFence(state, currentFence);
}

// ============================================================================
// 清理资源
// ============================================================================

static void OnDestroy(AppState* state)
{
    WaitForGpu(state);

    if (state->fenceEvent)
    {
        CloseHandle(state->fenceEvent);
        state->fenceEvent = nullptr;
    }

    state->pipelineState.Reset();
    state->rootSignature.Reset();
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
        L"D3D12 资源管理示例",
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
    InitD3D12(state);
    InvalidateRect(hwnd, nullptr, FALSE);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
