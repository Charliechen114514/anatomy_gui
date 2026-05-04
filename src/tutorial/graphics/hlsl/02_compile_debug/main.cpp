/**
 * 02_compile_debug - HLSL 编译调试示例
 *
 * 本程序演示了 HLSL 着色器的编译、反射和调试技术：
 * 1. D3DCompileFromFile — 使用 D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION 标志编译
 * 2. D3DReflect         — 着色器反射（ID3D11ShaderReflection），检查着色器元数据
 * 3. 编译错误处理       — 故意编译包含错误的 debug_pixel.hlsl，展示错误信息
 * 4. 着色器反汇编       — 通过 ID3DBlob 获取着色器反汇编代码
 * 5. 正确着色器渲染     — 编译正确的着色器并渲染彩色三角形
 *
 * 参考: tutorial/native_win32/35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cstdio>

// 引入公共 COM 辅助工具
#include "../common/ComHelper.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"HLSLCompileDebugClass";
static const int      WINDOW_WIDTH      = 800;
static const int      WINDOW_HEIGHT     = 600;

// ============================================================================
// 顶点结构体
// ============================================================================

struct Vertex
{
    float x, y, z, w;   // POSITION
    float r, g, b, a;   // COLOR
};

// ============================================================================
// 全局 D3D11 资源
// ============================================================================

static ComPtr<ID3D11Device>            g_device;
static ComPtr<ID3D11DeviceContext>     g_context;
static ComPtr<IDXGISwapChain>         g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_renderTarget;

static ComPtr<ID3D11VertexShader>     g_vertexShader;
static ComPtr<ID3D11PixelShader>      g_pixelShader;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;

// 反射信息字符串（用于显示）
static char  g_reflectionInfo[2048] = {};
static bool  g_hasReflectionInfo    = false;

// ============================================================================
// D3D11 初始化
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

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    ThrowIfFailed(hr, "GetBuffer 失败");

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_renderTarget.GetAddressOf());
    ThrowIfFailed(hr, "CreateRenderTargetView 失败");

    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
}

// ============================================================================
// 着色器编译（带调试标志）
// ============================================================================

/**
 * 编译着色器文件（带调试支持）
 *
 * D3DCOMPILE_DEBUG            — 在编译后的字节码中嵌入调试信息
 * D3DCOMPILE_SKIP_OPTIMIZATION — 禁用优化，便于调试时查看变量值
 *
 * 这些标志在开发阶段非常有用，发布时应移除以获得更好的性能
 */
static ComPtr<ID3DBlob> CompileShader(
    const wchar_t* fileName,
    const char* entryPoint,
    const char* shaderModel,
    bool enableDebug = true)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    // 编译标志
    // D3DCOMPILE_DEBUG: 嵌入调试信息（行号、变量名等）
    // D3DCOMPILE_SKIP_OPTIMIZATION: 跳过优化，保留中间变量
    UINT compileFlags = 0;
    if (enableDebug)
    {
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }

    HRESULT hr = D3DCompileFromFile(
        fileName,
        nullptr,
        nullptr,
        entryPoint,
        shaderModel,
        compileFlags,
        0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            // 构建详细的错误信息字符串
            char errorMsg[1024];
            snprintf(errorMsg, sizeof(errorMsg),
                "着色器编译失败!\n\n文件: %ls\n入口: %s\n目标: %s\n\n错误信息:\n%s",
                fileName, entryPoint, shaderModel,
                (const char*)errorBlob->GetBufferPointer());

            OutputDebugStringA(errorMsg);

            // 对于故意失败的 debug_pixel.hlsl，只记录错误不弹窗
            // （会在 main 中单独处理）
        }
        return nullptr;
    }

    return shaderBlob;
}

// ============================================================================
// 着色器反射 — D3DReflect
// ============================================================================

/**
 * 使用 D3DReflect 进行着色器反射
 *
 * 着色器反射可以查询编译后着色器的元数据：
 * - 常量缓冲区（Constant Buffers）的数量和描述
 * - 绑定资源（纹理、采样器等）
 * - 输入/输出参数
 * - 指令计数等统计信息
 *
 * 这在运行时动态查询着色器接口时非常有用
 */
static void PerformShaderReflection(ID3DBlob* shaderBlob)
{
    if (!shaderBlob) return;

    // 创建着色器反射接口
    ComPtr<ID3D11ShaderReflection> reflector;
    HRESULT hr = D3DReflect(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        IID_ID3D11ShaderReflection,
        (void**)reflector.GetAddressOf()
    );

    if (FAILED(hr))
    {
        snprintf(g_reflectionInfo, sizeof(g_reflectionInfo),
            "D3DReflect 失败: 0x%08X", (unsigned)hr);
        g_hasReflectionInfo = true;
        return;
    }

    // 获取着色器描述
    D3D11_SHADER_DESC shaderDesc;
    hr = reflector->GetDesc(&shaderDesc);
    if (FAILED(hr))
    {
        snprintf(g_reflectionInfo, sizeof(g_reflectionInfo),
            "GetDesc 失败: 0x%08X", (unsigned)hr);
        g_hasReflectionInfo = true;
        return;
    }

    // 构建反射信息字符串
    int offset = 0;

    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "=== 着色器反射信息 ===\n\n");

    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "常量缓冲区数量:      %u\n", shaderDesc.ConstantBuffers);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "绑定资源数量:        %u\n", shaderDesc.BoundResources);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "输入参数数量:        %u\n", shaderDesc.InputParameters);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "输出参数数量:        %u\n", shaderDesc.OutputParameters);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "指令计数:            %u\n", shaderDesc.InstructionCount);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "临时寄存器计数:      %u\n", shaderDesc.TempRegisterCount);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "临时数组计数:        %u\n", shaderDesc.TempArrayCount);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "纹理操作计数:        %u\n", shaderDesc.TextureNormalInstructions);
    offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
        "纹理加载计数:        %u\n", shaderDesc.TextureLoadInstructions);

    // 遍历常量缓冲区
    if (shaderDesc.ConstantBuffers > 0)
    {
        offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
            "\n--- 常量缓冲区详情 ---\n");

        for (UINT i = 0; i < shaderDesc.ConstantBuffers; i++)
        {
            ID3D11ShaderReflectionConstantBuffer* cbuffer = reflector->GetConstantBufferByIndex(i);
            D3D11_SHADER_BUFFER_DESC bufferDesc;
            cbuffer->GetDesc(&bufferDesc);

            offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
                "\n  [%u] 名称: %s\n", i, bufferDesc.Name ? bufferDesc.Name : "(匿名)");
            offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
                "      大小: %u 字节\n", bufferDesc.Size);
            offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
                "      变量数: %u\n", bufferDesc.Variables);

            // 遍历缓冲区中的变量
            for (UINT v = 0; v < bufferDesc.Variables && v < 8; v++)
            {
                ID3D11ShaderReflectionVariable* var = cbuffer->GetVariableByIndex(v);
                D3D11_SHADER_VARIABLE_DESC varDesc;
                var->GetDesc(&varDesc);

                offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
                    "      变量[%u]: %s (偏移:%u, 大小:%u)\n",
                    v, varDesc.Name ? varDesc.Name : "?", varDesc.StartOffset, varDesc.Size);
            }
        }
    }

    // 遍历输入参数
    if (shaderDesc.InputParameters > 0)
    {
        offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
            "\n--- 输入参数 ---\n");

        for (UINT i = 0; i < shaderDesc.InputParameters; i++)
        {
            D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
            reflector->GetInputParameterDesc(i, &paramDesc);

            offset += snprintf(g_reflectionInfo + offset, sizeof(g_reflectionInfo) - offset,
                "  [%u] 语义: %s%d, 寄存器: %u, 掩码: 0x%02X\n",
                i,
                paramDesc.SemanticName ? paramDesc.SemanticName : "?",
                paramDesc.SemanticIndex,
                paramDesc.Register,
                paramDesc.Mask);
        }
    }

    g_hasReflectionInfo = true;
}

// ============================================================================
// 着色器反汇编
// ============================================================================

/**
 * 获取着色器反汇编代码
 *
 * D3DDisassemble 可以将编译后的字节码反汇编为可读的汇编指令
 * 这对调试和理解着色器执行流程非常有帮助
 */
static void ShowDisassembly(ID3DBlob* shaderBlob, const char* shaderName)
{
    if (!shaderBlob) return;

    ComPtr<ID3DBlob> disassembly;
    HRESULT hr = D3DDisassemble(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        D3D_DISASM_ENABLE_COLOR_CODE,    // 启用颜色编码
        nullptr,                          // 无注释
        disassembly.GetAddressOf()
    );

    if (SUCCEEDED(hr) && disassembly)
    {
        // 将反汇编代码的前几行输出到调试窗口
        char title[128];
        snprintf(title, sizeof(title), "=== %s 反汇编（前 512 字节）===\n", shaderName);
        OutputDebugStringA(title);

        const char* asmCode = (const char*)disassembly->GetBufferPointer();
        // 只输出前 512 字节，避免过多输出
        size_t len = strlen(asmCode);
        if (len > 512) len = 512;

        char buf[520];
        memcpy(buf, asmCode, len);
        buf[len] = '\0';
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    }
}

// ============================================================================
// 着色器与管线初始化
// ============================================================================

/**
 * 初始化着色器管线
 *
 * 流程：
 * 1. 编译顶点着色器 → 反射 + 反汇编
 * 2. 尝试编译错误的 debug_pixel.hlsl → 展示错误处理
 * 3. 编译正确的 pixel.hlsl → 反射 + 反汇编
 * 4. 创建着色器对象和输入布局
 */
static bool InitShaders()
{
    // ---- 第一步：编译顶点着色器 ----
    // 使用调试标志编译，便于反射和反汇编
    ComPtr<ID3DBlob> vsBlob = CompileShader(
        L"shaders/02_compile_debug/vertex.hlsl",
        "VSMain",
        "vs_5_0",
        true   // 启用调试标志
    );
    if (!vsBlob)
    {
        MessageBox(nullptr,
            L"无法编译顶点着色器!\n请确认 shaders 目录位于可执行文件同级目录下。",
            L"编译错误", MB_ICONERROR);
        return false;
    }

    // 对顶点着色器进行反射分析
    PerformShaderReflection(vsBlob.Get());

    // 输出顶点着色器反汇编到调试窗口
    ShowDisassembly(vsBlob.Get(), "顶点着色器 (VS)");

    // 创建顶点着色器
    HRESULT hr = g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        g_vertexShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateVertexShader 失败");

    // ---- 第二步：定义输入布局 ----
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_device->CreateInputLayout(
        layoutDesc, _countof(layoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreateInputLayout 失败");

    // ---- 第三步：编译错误的着色器（演示错误处理） ----
    // debug_pixel.hlsl 包含一个未声明的变量，编译必定失败
    OutputDebugStringA("\n=== 尝试编译错误的着色器 ===\n");

    ComPtr<ID3DBlob> errorBlob;
    ComPtr<ID3DBlob> dummyBlob;

    // 直接使用 D3DCompileFromFile 获取错误信息
    HRESULT errorHr = D3DCompileFromFile(
        L"shaders/02_compile_debug/debug_pixel.hlsl",
        nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        dummyBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    // 编译应该失败 — 展示错误信息
    if (FAILED(errorHr) && errorBlob)
    {
        const char* errorMsg = (const char*)errorBlob->GetBufferPointer();

        // 在调试窗口输出错误
        OutputDebugStringA("debug_pixel.hlsl 编译错误（预期行为）:\n");
        OutputDebugStringA(errorMsg);

        // 弹出 MessageBox 展示错误信息
        char msgBoxText[1024];
        snprintf(msgBoxText, sizeof(msgBoxText),
            "这是预期的编译错误演示!\n\n"
            "文件: debug_pixel.hlsl\n"
            "错误:\n%s\n\n"
            "点击确定后，将使用正确的 pixel.hlsl 继续渲染。",
            errorMsg);
        MessageBoxA(nullptr, msgBoxText,
            "HLSL 编译错误演示", MB_ICONWARNING | MB_OK);
    }
    else if (SUCCEEDED(errorHr))
    {
        // 不应该到达这里
        MessageBox(nullptr,
            L"debug_pixel.hlsl 意外编译成功!\n该文件应包含语法错误。",
            L"意外结果", MB_ICONWARNING);
    }

    // ---- 第四步：编译正确的像素着色器 ----
    ComPtr<ID3DBlob> psBlob = CompileShader(
        L"shaders/02_compile_debug/pixel.hlsl",
        "PSMain",
        "ps_5_0",
        true   // 启用调试标志
    );
    if (!psBlob)
    {
        MessageBox(nullptr,
            L"无法编译像素着色器!\n请确认 shaders 目录位于可执行文件同级目录下。",
            L"编译错误", MB_ICONERROR);
        return false;
    }

    // 对像素着色器进行反射分析
    PerformShaderReflection(psBlob.Get());

    // 输出像素着色器反汇编到调试窗口
    ShowDisassembly(psBlob.Get(), "像素着色器 (PS)");

    // 创建像素着色器
    hr = g_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        g_pixelShader.GetAddressOf()
    );
    ThrowIfFailed(hr, "CreatePixelShader 失败");

    // 显示反射信息汇总
    if (g_hasReflectionInfo)
    {
        MessageBoxA(nullptr, g_reflectionInfo,
            "着色器反射信息", MB_ICONINFORMATION | MB_OK);
    }

    return true;
}

// ============================================================================
// 顶点数据初始化
// ============================================================================

static void InitVertexBuffer()
{
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.0f, 1.0f,    1.0f, 0.0f, 0.0f, 1.0f },  // 顶部 — 红色
        {  0.5f, -0.5f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f, 1.0f },  // 右下 — 绿色
        { -0.5f, -0.5f, 0.0f, 1.0f,    0.0f, 0.0f, 1.0f, 1.0f },  // 左下 — 蓝色
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth      = sizeof(vertices);
    bufferDesc.Usage          = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    HRESULT hr = g_device->CreateBuffer(
        &bufferDesc, &initData, g_vertexBuffer.GetAddressOf());
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

static void Render()
{
    if (!g_context || !g_renderTarget)
        return;

    // 清除为深蓝色背景
    float clearColor[4] = { 0.05f, 0.05f, 0.2f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);

    // 设置输入布局
    g_context->IASetInputLayout(g_inputLayout.Get());

    // 绑定顶点缓冲区
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);

    // 设置图元拓扑
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置着色器
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

    // 设置视口
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = (FLOAT)WINDOW_WIDTH;
    viewport.Height   = (FLOAT)WINDOW_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &viewport);

    // 绘制三角形
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
 * 着色器编译调试流程图：
 *
 *   ┌──────────────────┐
 *   │ debug_pixel.hlsl │ ← 故意包含错误的着色器
 *   └────────┬─────────┘
 *            │ D3DCompileFromFile (DEBUG | SKIP_OPTIMIZATION)
 *            ▼
 *   ┌──────────────────┐
 *   │    编译失败 ✓     │ ← 预期行为
 *   │ errorBlob 包含:   │
 *   │ "未声明的标识符"  │
 *   └────────┬─────────┘
 *            │ MessageBox 显示错误
 *            ▼
 *   ┌──────────────────┐
 *   │  pixel.hlsl      │ ← 正确的像素着色器
 *   └────────┬─────────┘
 *            │ D3DCompileFromFile (成功)
 *            ▼
 *   ┌──────────────────┐     ┌──────────────────┐
 *   │   字节码 Blob    │────→│   D3DReflect      │
 *   └────────┬─────────┘     │ 查询: 常量缓冲区  │
 *            │               │ 绑定资源 / 参数   │
 *            │               └──────────────────┘
 *            │               ┌──────────────────┐
 *            ├──────────────→│  D3DDisassemble   │
 *            │               │ 反汇编为汇编代码  │
 *            │               └──────────────────┘
 *            ▼
 *   ┌──────────────────┐
 *   │ CreatePixelShader │
 *   └──────────────────┘
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
        L"HLSL 编译调试示例",
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

    // 初始化着色器（包含编译、反射、反汇编和错误处理演示）
    if (!InitShaders())
    {
        CoUninitialize();
        return 1;
    }

    // 创建顶点缓冲区
    InitVertexBuffer();

    // ========================================================================
    // 消息循环
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
