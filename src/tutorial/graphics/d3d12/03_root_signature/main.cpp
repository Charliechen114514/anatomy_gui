/**
 * 03_root_signature - D3D12 根签名与描述符示例
 *
 * 本程序演示了 Direct3D 12 中根签名和描述符的使用：
 * 1. CreateRootSignature            — 创建根签名（1 个常量缓冲区参数，寄存器 b0）
 * 2. CreateDescriptorHeap (CBV)     — 创建 CBV_SRV_UAV 描述符堆
 * 3. CreateCommittedResource        — 创建常量缓冲区资源（上传堆）
 * 4. CreateConstantBufferView       — 创建常量缓冲区视图（CBV）
 * 5. CreateCommittedResource (VB)   — 创建顶点缓冲区资源（上传堆）
 * 6. D3D12_VERTEX_BUFFER_VIEW      — 顶点缓冲区视图
 * 7. IASetVertexBuffers             — 输入装配器设置顶点缓冲区
 * 8. 输入布局 (D3D12_INPUT_ELEMENT_DESC) — 顶点数据格式描述
 *
 * 本示例渲染一个旋转的彩色三角形，通过常量缓冲区传递变换矩阵。
 * 使用外部着色器文件: shaders/root_sig.hlsl
 *
 * 参考: tutorial/native_win32/46_ProgrammingGUI_Graphics_D3D12_DescRootSig.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>

#include "ComHelper.hpp"
#include "D3D12Compat.hpp"

// 使用 DirectXMath 命名空间中的向量/矩阵类型
using namespace DirectX;

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

static const wchar_t* WINDOW_CLASS_NAME = L"D3D12RootSigClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;
static const int      FRAME_COUNT       = 2;

// 传递给着色器的常量缓冲区结构体（必须与 HLSL 中的 cbuffer 匹配）
// 注意：HLSL 中矩阵默认是列主序，DirectXMath 使用行主序，
// 所以需要在设置常量缓冲区时进行转置。
struct SceneConstants
{
    XMFLOAT4X4 worldViewProj;   // 世界-视图-投影矩阵
};

// 顶点结构体 — 位置 + 颜色
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    HWND hwnd;

    // D3D12 核心对象
    ComPtr<ID3D12Device>             device;
    ComPtr<ID3D12CommandQueue>       commandQueue;
    ComPtr<ID3D12CommandAllocator>   commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> commandList;

    // 交换链与渲染目标
    ComPtr<IDXGISwapChain3>          swapChain;
    ComPtr<ID3D12Resource>           renderTargets[FRAME_COUNT];
    ComPtr<ID3D12DescriptorHeap>     rtvHeap;
    UINT rtvDescriptorSize;

    // 围栏同步
    ComPtr<ID3D12Fence> fence;
    UINT64              fenceValues[FRAME_COUNT];
    HANDLE              fenceEvent;
    UINT                frameIndex;

    // 根签名与管线状态
    ComPtr<ID3D12RootSignature>      rootSignature;
    ComPtr<ID3D12PipelineState>      pipelineState;

    // ----------------------------------------------------------------------
    // 描述符堆 — 用于常量缓冲区视图（CBV）
    // D3D12 中，着色器资源通过描述符堆来访问。
    // CBV_SRV_UAV 类型堆可以存放常量缓冲区、着色器资源视图和无序访问视图。
    // ----------------------------------------------------------------------
    ComPtr<ID3D12DescriptorHeap>     cbvHeap;
    UINT cbvDescriptorSize;

    // 常量缓冲区资源（每帧一个，避免 GPU/CPU 竞争）
    ComPtr<ID3D12Resource>           constantBuffers[FRAME_COUNT];
    UINT8*                           cbvDataBegin[FRAME_COUNT];  // 映射后的 CPU 端指针

    // 顶点缓冲区
    ComPtr<ID3D12Resource>           vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW        vertexBufferView;

    // 动画状态
    LARGE_INTEGER startTime;
    LARGE_INTEGER frequency;
    float         aspectRatio;
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
// InitD3D12 — 初始化
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

    // 每帧一个命令分配器
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        hr = state->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
             IID_PPV_ARGS(state->commandAllocators[i].GetAddressOf()));
        ThrowIfFailed(hr, "CreateCommandAllocator 失败");
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
    ThrowIfFailed(hr, "CreateDescriptorHeap (RTV) 失败");

    state->rtvDescriptorSize = state->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 创建渲染目标视图
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
    // 创建根签名 — 1 个常量缓冲区参数
    //
    // 根签名定义了着色器可以访问的资源绑定"布局"。
    // 这里定义了一个根参数：一个 32-bit 常量的描述符表，
    // 指向 CBV (Constant Buffer View) 描述符，绑定到寄存器 b0。
    // 这对应着色器中的 "cbuffer Constants : register(b0)"。
    // ----------------------------------------------------------------------
    D3D12_DESCRIPTOR_RANGE1 ranges[1] = {};
    ranges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors                    = 1;
    ranges[0].BaseShaderRegister                = 0;     // 寄存器 b0
    ranges[0].RegisterSpace                     = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[0].Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

    D3D12_ROOT_PARAMETER1 rootParameters[1] = {};
    rootParameters[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges   = ranges;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1 = {};
    rootSigDesc1.NumParameters = 1;
    rootSigDesc1.pParameters   = rootParameters;
    rootSigDesc1.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDesc.Desc_1_1 = rootSigDesc1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeVersionedRootSignature(&versionedDesc,
         signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    ThrowIfFailed(hr, "D3D12SerializeVersionedRootSignature 失败");

    hr = state->device->CreateRootSignature(0,
         signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
         IID_PPV_ARGS(state->rootSignature.GetAddressOf()));
    ThrowIfFailed(hr, "CreateRootSignature 失败");

    // ----------------------------------------------------------------------
    // 创建 CBV_SRV_UAV 描述符堆
    // 用于存放常量缓冲区视图（CBV）描述符
    // 着色器描述符堆（SHADER_VISIBLE）可以被 GPU 着色器访问
    // ----------------------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.NumDescriptors = FRAME_COUNT;  // 每帧一个 CBV
    cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = state->device->CreateDescriptorHeap(&cbvHeapDesc,
         IID_PPV_ARGS(state->cbvHeap.GetAddressOf()));
    ThrowIfFailed(hr, "CreateDescriptorHeap (CBV) 失败");

    state->cbvDescriptorSize = state->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ----------------------------------------------------------------------
    // 创建常量缓冲区资源并映射（每帧一个）
    // 使用上传堆（UPLOAD heap）创建，因为 CPU 需要每帧更新数据。
    // Map() 获取 CPU 端指针，之后可以直接写入数据。
    // ----------------------------------------------------------------------
    UINT cbSize = sizeof(SceneConstants);
    // CB 大小必须是 256 字节对齐（D3D12 要求）
    cbSize = (cbSize + 255) & ~255;

    D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle =
        state->cbvHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        // 创建上传堆缓冲区
        D3D12_HEAP_PROPERTIES uploadProps = {};
        uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC cbDesc = {};
        cbDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        cbDesc.Width            = cbSize;
        cbDesc.Height           = 1;
        cbDesc.DepthOrArraySize = 1;
        cbDesc.MipLevels        = 1;
        cbDesc.Format           = DXGI_FORMAT_UNKNOWN;
        cbDesc.SampleDesc.Count = 1;
        cbDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = state->device->CreateCommittedResource(
            &uploadProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(state->constantBuffers[i].GetAddressOf()));
        ThrowIfFailed(hr, "CreateCommittedResource (常量缓冲区) 失败");

        // 映射常量缓冲区 — 获取 CPU 端写入指针
        // Map 的范围可以设为空（0, nullptr）表示映射整个资源
        hr = state->constantBuffers[i]->Map(
            0, nullptr,
            reinterpret_cast<void**>(&state->cbvDataBegin[i]));
        ThrowIfFailed(hr, "Map 常量缓冲区失败");

        // 创建常量缓冲区视图（CBV）
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = state->constantBuffers[i]->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes    = cbSize;

        state->device->CreateConstantBufferView(&cbvDesc, cbvHandle);
        cbvHandle.ptr += state->cbvDescriptorSize;
    }

    // ----------------------------------------------------------------------
    // 创建顶点缓冲区（上传堆）
    // 定义三角形的三个顶点：位置 + 颜色
    // ----------------------------------------------------------------------
    Vertex triangleVertices[3] = {
        { XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 顶部 — 红色
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 右下 — 绿色
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 左下 — 蓝色
    };

    const UINT vertexBufferSize = sizeof(triangleVertices);

    D3D12_HEAP_PROPERTIES uploadProps = {};
    uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC vbDesc = {};
    vbDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width            = vertexBufferSize;
    vbDesc.Height           = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels        = 1;
    vbDesc.Format           = DXGI_FORMAT_UNKNOWN;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = state->device->CreateCommittedResource(
        &uploadProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(state->vertexBuffer.GetAddressOf()));
    ThrowIfFailed(hr, "CreateCommittedResource (顶点缓冲区) 失败");

    // 复制顶点数据到上传堆缓冲区
    UINT8* pData = nullptr;
    hr = state->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    ThrowIfFailed(hr, "Map 顶点缓冲区失败");
    memcpy(pData, triangleVertices, vertexBufferSize);
    state->vertexBuffer->Unmap(0, nullptr);

    // 创建顶点缓冲区视图
    state->vertexBufferView.BufferLocation = state->vertexBuffer->GetGPUVirtualAddress();
    state->vertexBufferView.StrideInBytes  = sizeof(Vertex);
    state->vertexBufferView.SizeInBytes    = vertexBufferSize;

    // ----------------------------------------------------------------------
    // 加载着色器并创建管线状态
    // ----------------------------------------------------------------------
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    // 从文件加载编译好的着色器（需要在运行时编译 .hlsl 文件）
    // 这里使用 D3DCompileFromFile 从外部文件编译
    hr = D3DCompileFromFile(
        L"shaders/03_root_signature/root_sig.hlsl",
        nullptr, nullptr,
        "VSMain", "vs_5_0",
        0, 0,
        vertexShader.GetAddressOf(),
        nullptr);
    if (FAILED(hr))
    {
        // 如果文件加载失败，尝试内嵌着色器作为后备
        const char* fallbackShader = R"(
            cbuffer Constants : register(b0) { matrix WorldViewProj; };
            struct VS_INPUT { float4 pos : POSITION; float4 col : COLOR; };
            struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = mul(WorldViewProj, input.pos);
                output.col = input.col;
                return output;
            }
            float4 PSMain(PS_INPUT input) : SV_TARGET { return input.col; }
        )";
        hr = D3DCompile(fallbackShader, strlen(fallbackShader),
             nullptr, nullptr, nullptr,
             "VSMain", "vs_5_0", 0, 0,
             vertexShader.GetAddressOf(), nullptr);
        ThrowIfFailed(hr, "编译顶点着色器失败");

        hr = D3DCompile(fallbackShader, strlen(fallbackShader),
             nullptr, nullptr, nullptr,
             "PSMain", "ps_5_0", 0, 0,
             pixelShader.GetAddressOf(), nullptr);
        ThrowIfFailed(hr, "编译像素着色器失败");
    }
    else
    {
        hr = D3DCompileFromFile(
            L"shaders/03_root_signature/root_sig.hlsl",
            nullptr, nullptr,
            "PSMain", "ps_5_0",
            0, 0,
            pixelShader.GetAddressOf(),
            nullptr);
        ThrowIfFailed(hr, "编译像素着色器失败");
    }

    // 输入布局 — 描述顶点数据格式
    // 必须与 Vertex 结构体和 HLSL 中的 VS_INPUT 匹配
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        // 语义名称     语义索引  格式                  输入槽  字节偏移  输入槽类
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // 创建管线状态对象（PSO）
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout           = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature        = state->rootSignature.Get();
    psoDesc.VS                    = { vertexShader->GetBufferPointer(),
                                      vertexShader->GetBufferSize() };
    psoDesc.PS                    = { pixelShader->GetBufferPointer(),
                                      pixelShader->GetBufferSize() };
    psoDesc.RasterizerState       = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState            = CD3D12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState     = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count      = 1;

    hr = state->device->CreateGraphicsPipelineState(&psoDesc,
         IID_PPV_ARGS(state->pipelineState.GetAddressOf()));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState 失败");

    // 关闭命令列表
    hr = state->commandList->Close();
    ThrowIfFailed(hr, "关闭命令列表失败");

    // 创建围栏
    hr = state->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
         IID_PPV_ARGS(state->fence.GetAddressOf()));
    ThrowIfFailed(hr, "CreateFence 失败");

    for (int i = 0; i < FRAME_COUNT; ++i)
        state->fenceValues[i] = 0;

    state->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!state->fenceEvent)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent 失败");

    // 初始化计时器
    QueryPerformanceFrequency(&state->frequency);
    QueryPerformanceCounter(&state->startTime);
    state->aspectRatio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
}

// ============================================================================
// PopulateCommandList — 录制渲染命令
// ============================================================================

static void PopulateCommandList(AppState* state)
{
    HRESULT hr;

    // 重置当前帧的命令分配器
    hr = state->commandAllocators[state->frameIndex]->Reset();
    ThrowIfFailed(hr, "CommandAllocator::Reset 失败");

    hr = state->commandList->Reset(
        state->commandAllocators[state->frameIndex].Get(),
        state->pipelineState.Get());
    ThrowIfFailed(hr, "CommandList::Reset 失败");

    // 设置根签名
    state->commandList->SetGraphicsRootSignature(state->rootSignature.Get());

    // ----------------------------------------------------------------------
    // 设置描述符堆 — 告诉 GPU 从哪个堆读取描述符
    // IASetPrimitiveTopology 设置图元拓扑为三角形列表
    // ----------------------------------------------------------------------
    ID3D12DescriptorHeap* ppHeaps[] = { state->cbvHeap.Get() };
    state->commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // 设置根描述符表 — 将 CBV 描述符表绑定到根参数索引 0
    // 使用 GPU 描述符句柄（GPU 端地址）
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        state->cbvHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += state->frameIndex * state->cbvDescriptorSize;

    state->commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

    // 设置视口和裁剪矩形
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = (float)WINDOW_WIDTH;
    viewport.Height   = (float)WINDOW_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left   = 0;
    scissorRect.top    = 0;
    scissorRect.right  = WINDOW_WIDTH;
    scissorRect.bottom = WINDOW_HEIGHT;

    state->commandList->RSSetViewports(1, &viewport);
    state->commandList->RSSetScissorRects(1, &scissorRect);

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

    // 清除背景
    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    state->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // ----------------------------------------------------------------------
    // 更新常量缓冲区 — 计算旋转矩阵
    // ----------------------------------------------------------------------
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float elapsed = (float)(now.QuadPart - state->startTime.QuadPart)
                  / (float)state->frequency.QuadPart;

    // 旋转角度随时间变化
    float angle = elapsed * 1.0f;  // 弧度/秒

    // 构建世界-视图-投影矩阵
    XMMATRIX world  = XMMatrixRotationZ(angle);
    // 考虑宽高比，使三角形不变形
    XMMATRIX proj   = XMMatrixOrthographicLH(2.0f * state->aspectRatio, 2.0f, 0.1f, 10.0f);
    XMMATRIX wvp    = world * proj;

    // 将矩阵数据写入常量缓冲区
    // DirectXMath 矩阵需要转置（HLSL 默认列主序）
    SceneConstants constants;
    XMStoreFloat4x4(&constants.worldViewProj, XMMatrixTranspose(wvp));
    memcpy(state->cbvDataBegin[state->frameIndex], &constants, sizeof(constants));

    // ----------------------------------------------------------------------
    // 设置顶点缓冲区并绘制
    // IASetVertexBuffers — 将顶点缓冲区绑定到输入装配器
    // DrawInstanced(3, 1, 0, 0) — 绘制 3 个顶点，1 个实例
    // ----------------------------------------------------------------------
    state->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state->commandList->IASetVertexBuffers(0, 1, &state->vertexBufferView);
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

    ID3D12CommandList* ppCommandLists[] = { state->commandList.Get() };
    state->commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // 发出围栏信号
    UINT64 currentFence = ++state->fenceValues[state->frameIndex];
    HRESULT hr = state->commandQueue->Signal(state->fence.Get(), currentFence);
    ThrowIfFailed(hr, "Signal 失败");

    hr = state->swapChain->Present(1, 0);
    ThrowIfFailed(hr, "Present 失败");

    // 切换到下一帧
    UINT prevFrame = state->frameIndex;
    state->frameIndex = state->swapChain->GetCurrentBackBufferIndex();

    // 等待新帧的命令分配器可用（确保上一轮该帧的 GPU 工作已完成）
    if (state->fenceValues[state->frameIndex] > 0)
        WaitForFence(state, state->fenceValues[state->frameIndex]);
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

    // 解除常量缓冲区映射
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (state->constantBuffers[i])
            state->constantBuffers[i]->Unmap(0, nullptr);
    }

    state->vertexBuffer.Reset();
    state->pipelineState.Reset();
    state->rootSignature.Reset();
    state->cbvHeap.Reset();
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        state->constantBuffers[i].Reset();
        state->commandAllocators[i].Reset();
        state->renderTargets[i].Reset();
    }
    state->commandList.Reset();
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

    case WM_SIZE:
    {
        if (state)
        {
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0)
                state->aspectRatio = (float)width / (float)height;
        }
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
        L"D3D12 根签名示例",
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
