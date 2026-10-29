# 通用GUI编程技术——图形渲染实战（三十八）——顶点缓冲与输入布局：GPU的第一个三角形

> 在上一篇文章中，我们搭建了 D3D11 的完整初始化框架——设备、上下文、交换链，以及渲染循环的清屏+呈现。但说实话，只会清屏的程序没什么意思，一片纯色能看什么呢？今天我们要迈出真正的第一步：让 GPU 画一个三角形。
>
> 要让 GPU 画三角形，我们需要解决两个问题。第一，三角形的数据（三个顶点的位置和颜色）从哪里来？答案是通过顶点缓冲（Vertex Buffer）把数据从 CPU 内存搬到 GPU 内存。第二，GPU 怎么知道这堆字节数据里哪些是坐标、哪些是颜色？答案是通过输入布局（Input Layout）告诉 GPU 数据的结构。搞懂这两个概念，你就掌握了 D3D11 几何体渲染的核心机制。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11（链接 `d3d11.lib`、`d3dcompiler.lib`）
- **前置知识**: 文章 37（D3D11 初始化与 SwapChain）

## 顶点数据结构

在 D3D11 中，"顶点"不再只是空间坐标，它可以包含任何你想传递给 Shader 的数据——位置、颜色、纹理坐标、法线、切线……完全由你定义。我们先从最简单的"位置+颜色"开始：

```cpp
struct Vertex
{
    XMFLOAT3 position;  // 3D 位置 (x, y, z)
    XMFLOAT4 color;     // RGBA 颜色
};
```

然后定义一个三角形的三个顶点：

```cpp
Vertex triangleVertices[] =
{
    { XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // 顶部 - 红色
    { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },  // 右下 - 绿色
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },  // 左下 - 蓝色
};
```

坐标范围 -1 到 1 是 D3D11 的标准化设备坐标（NDC，Normalized Device Coordinates）。(-1,-1) 是窗口左下角，(1,1) 是窗口右上角，(0,0) 是窗口中心。注意 Y 轴向上，这和 GDI/GDI+ 的坐标系（Y 向下）完全相反。

## 创建顶点缓冲（ID3D11Buffer）

有了数据，下一步是创建 GPU 缓冲区并把数据上传上去：

```cpp
ID3D11Buffer* g_pVertexBuffer = NULL;

void CreateVertexBuffer(ID3D11Device* pDevice)
{
    Vertex vertices[] = {
        { XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(1,0,0,1) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0,1,0,1) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0,0,1,1) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);                          // 总字节数
    bd.Usage = D3D11_USAGE_IMMUTABLE;                         // 创建后不可修改
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;                  // 绑定为顶点缓冲
    bd.CPUAccessFlags = 0;                                    // CPU 不访问
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;                              // 指向 CPU 端数据

    HRESULT hr = pDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
    if (FAILED(hr)) {
        // 创建失败处理
    }
}
```

`D3D11_USAGE_IMMUTABLE` 表示这个缓冲区的内容在创建之后永远不会改变。这是性能最好的选项——GPU 知道数据不会变，可以做更激进的优化。如果你的顶点数据需要动态更新（比如粒子系统），应该用 `D3D11_USAGE_DYNAMIC` 配合 `D3D11_CPU_ACCESS_WRITE`。

`D3D11_SUBRESOURCE_DATA` 是初始化数据结构，`pSysMem` 指向你要上传的 CPU 内存。如果你传入 `NULL`（不提供初始数据），缓冲区会被创建但内容未定义，你需要后续通过 `UpdateSubresource` 或 `Map/Unmap` 填充数据。

## 输入布局（ID2D1InputLayout）

GPU 拿到一大块字节数据后，需要知道怎么解析它。输入布局就是"数据说明书"——告诉 GPU 缓冲区中每个字段叫什么名字、是什么类型、在哪个偏移位置。

```cpp
ID3D11InputLayout* g_pInputLayout = NULL;

void CreateInputLayout(ID3D11Device* pDevice, ID3DBlob* pVSBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        {
            "POSITION",                              // 语义名称（对应 HLSL 中的 : POSITION）
            0,                                        // 语义索引
            DXGI_FORMAT_R32G32B32_FLOAT,              // 数据格式（3 个 float）
            0,                                        // 输入槽（如果用多个 VBuffer）
            0,                                        // 字节偏移（位置在结构体开头）
            D3D11_INPUT_PER_VERTEX_DATA,              // 输入分类
            0                                         // 实例数据步长（非实例化时为 0）
        },
        {
            "COLOR",                                  // 语义名称（对应 HLSL 中的 : COLOR）
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,           // 4 个 float
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,             // 自动计算偏移（跟在 POSITION 后面）
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
    };

    UINT numElements = _countof(layout);

    pDevice->CreateInputLayout(
        layout,
        numElements,
        pVSBlob->GetBufferPointer(),   // Vertex Shader 字节码（必须！）
        pVSBlob->GetBufferSize(),
        &g_pInputLayout
    );
}
```

输入布局有几个关键参数需要理解。

**语义名称（Semantic Name）**：就是 HLSL 中结构体成员后面跟的那个冒号标签。比如 HLSL 中写 `float3 pos : POSITION`，对应的语义名称就是 `"POSITION"`。这个名字是 GPU 驱动将缓冲区数据映射到 Shader 输入变量的桥梁。

**`D3D11_APPEND_ALIGNED_ELEMENT`**：这个常量告诉 D3D11 自动计算字段偏移。第一个字段（POSITION）偏移为 0，第二个字段（COLOR）紧跟其后。如果你手动计算，COLOR 的偏移应该是 `sizeof(XMFLOAT3)` = 12 字节。但用 `APPEND_ALIGNED_ELEMENT` 更安全，不会因为结构体定义变化而忘记更新偏移值。

⚠️ 注意最关键的一点：**输入布局必须和 Vertex Shader 的字节码匹配**。`CreateInputLayout` 的第三和第四个参数需要你传入 VS 的编译结果。D3D11 会根据 VS 的输入签名验证你的布局描述是否兼容。如果你换了 Shader 但忘了更新输入布局，`CreateInputLayout` 会返回错误。

## Shader 代码

配套的 Vertex Shader 和 Pixel Shader：

```hlsl
// Vertex Shader
struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;   // 系统语义：裁剪空间位置
    float4 color    : COLOR;
};

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);  // 补齐为 float4
    output.color = input.color;
    return output;
}
```

```hlsl
// Pixel Shader
struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 PS_Main(PSInput input) : SV_TARGET
{
    return input.color;  // 直接输出顶点颜色
}
```

VS 把 3D 位置补齐为齐次坐标（w=1），PS 直接输出颜色。GPU 会自动在三个顶点之间做颜色插值——顶部顶点是红色，左下是蓝色，三角形中间的像素颜色是三者的加权平均，产生平滑的渐变效果。这就是光栅化（Rasterization）阶段自动完成的工作。

## 绘制：Draw Call

一切准备就绪，绘制只需要几行代码：

```cpp
void Render()
{
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    g_pContext->ClearRenderTargetView(g_pRTV, clearColor);

    // 设置顶点缓冲
    UINT stride = sizeof(Vertex);  // 每个顶点的字节大小
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(
        0,                      // 输入槽索引
        1,                      // 缓冲区数量
        &g_pVertexBuffer,       // 缓冲区指针
        &stride,                // 步幅
        &offset                 // 起始偏移
    );

    // 设置输入布局
    g_pContext->IASetInputLayout(g_pInputLayout);

    // 设置图元拓扑
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置 Shader
    g_pContext->VSSetShader(g_pVertexShader, NULL, 0);
    g_pContext->PSSetShader(g_pPixelShader, NULL, 0);

    // 发出绘制命令！
    g_pContext->Draw(
        3,   // 顶点数量
        0    // 起始顶点索引
    );

    g_pSwapChain->Present(1, 0);
}
```

`IASetVertexBuffers` 中的 `stride` 参数非常关键——它告诉 GPU 每隔多少字节取一个顶点数据。如果你设错了 stride（比如设成了 `sizeof(XMFLOAT3)` 而不是 `sizeof(Vertex)`），GPU 会以错误的间隔读取数据，画面会变成一堆乱七八糟的三角形。

### 图元拓扑类型

`IASetPrimitiveTopology` 指定 GPU 如何将顶点组合成图元。常用类型包括：

`D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST` 是最常用的——每 3 个顶点组成一个独立三角形。`TRIANGLESTRIP` 则是共享边的连续三角形带，适合绘制连续曲面。`LINESTRIP` 和 `LINELIST` 用于线段绘制。`POINTLIST` 用于粒子渲染。`1-4` 的控制点补丁类型用于曲面细分（Tessellation）。

## 常见问题与调试

### 问题1：画面是空白的，没有三角形

依次检查：输入布局是否正确绑定？顶点缓冲的 stride 是否匹配？Shader 是否编译成功并绑定？如果你使用了深度缓冲但忘记清除，三角形可能被深度测试拒绝。添加 `g_pContext->ClearDepthStencilView(g_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0)` 到清屏代码中。

### 问题2：三角形颜色不对或闪烁

stride 参数设错是最常见的原因。确保 `stride = sizeof(Vertex)` 而不是 `sizeof(XMFLOAT3)` 或其他值。另外，如果你的输入布局描述和 Vertex 结构体的内存布局不一致，GPU 会以错误的偏移读取颜色数据。

### 问题3：CreateInputLayout 返回 E_INVALIDARG

输入布局和 Vertex Shader 字节码不匹配。检查语义名称是否拼写正确（HLSL 区分大小写吗？实际上不区分，但保持一致是好习惯），检查数据格式是否匹配（HLSL 的 `float3` 对应 `DXGI_FORMAT_R32G32B32_FLOAT`），确保你传入的是正确的 VS 字节码（不是 PS 的）。

## 总结

顶点缓冲和输入布局是 D3D11 几何体渲染的基础。顶点缓冲是数据的容器，输入布局是数据的说明书，两者配合才能让 GPU 正确理解并渲染你的几何数据。整个绘制流程可以概括为：准备数据 → 创建 VBuffer → 定义 Layout → 编译 Shader → 绑定一切 → Draw。

下一步，我们要给三角形贴上纹理。现在的三角形只有顶点颜色插值，看起来还是相当简陋的。纹理映射（Texture Mapping）可以让三角形显示任意的图片内容——照片、UI 元素、字体渲染结果……这是从"几何体渲染"迈向"真实感图形"的关键一步。

---

## 练习

1. 修改顶点数据，绘制两个不同颜色的三角形组成一个矩形。
2. 添加第四个顶点，用 `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP` 绘制一个正方形（注意顶点顺序）。
3. 实现顶点颜色随时间变化的动画：在 CBuffer 中传入时间变量，在 VS 中根据时间偏移颜色。
4. 创建两个独立的顶点缓冲和输入布局，在同一个渲染循环中分别绘制不同形状的三角形。

---

**参考资料**:
- [ID3D11Device::CreateBuffer - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createbuffer)
- [ID3D11Device::CreateInputLayout - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createinputlayout)
- [D3D11_INPUT_ELEMENT_DESC structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_input_element_desc)
- [ID3D11DeviceContext::Draw - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-draw)
- [D3D11_PRIMITIVE_TOPOLOGY enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d11_primitive_topology)
