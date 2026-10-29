# 通用GUI编程技术——图形渲染实战（四十）——深度缓冲与3D变换：从平面到立体

> 上一篇我们搞定了纹理采样器，把一张2D图片贴到了矩形上，GPU的渲染管线已经能完整跑通了：顶点缓冲送进去、常量缓冲传参数、像素着色器采样纹理输出颜色。但到目前为止，我们渲染的所有东西都是"平的"——一张矩形，一个三角形，哪怕贴上了精美的纹理，本质上还是2D的剪纸片。真正的3D世界需要解决两个核心问题：物体在空间中的位置变换，以及前后遮挡关系。今天我们就要把这两件事一起拿下。

说实话，第一次搞3D变换的时候我被矩阵折腾得不轻。DirectXMath那一套行向量约定、列主序存储、传给Shader之前还要转置——光是搞清楚这些约定就花了我一个下午。但一旦理解了这套体系，后面所有3D渲染的基础就都打通了。

## 环境说明

在开始之前，先确认一下我们的工作环境：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, /std:c++17)
- **图形API**: Direct3D 11 (Feature Level 11_0)
- **数学库**: DirectXMath (directxmath.h)
- **前提条件**: 已完成前39篇，理解D3D11渲染管线基础、纹理采样器的使用

这一篇我们会从头搭建深度缓冲，实现完整的MVP矩阵变换，并封装一个Arcball摄像机，最终渲染出一个每面颜色不同的旋转立方体。

## 第一个问题：为什么需要深度缓冲

如果你之前尝试过在3D空间里画多个物体，你可能会遇到一个非常诡异的现象：后面画的物体总是覆盖前面画的物体，不管它在空间中实际上是在前面还是后面。这是因为默认情况下，GPU只关心像素的绘制顺序——后绘制的覆盖先绘制的，就像在纸上画画一样，后涂的颜料盖住先涂的。

但对于3D场景来说，这种"画家算法"完全行不通。想象一个立方体：你从某个角度看过去，有些面在前面，有些面在后面，而且随着视角变化前后关系还会改变。你不可能在每帧都手动排序所有三角形的绘制顺序——那代价太大了，而且对于交叉重叠的三角形，排序根本无法给出正确结果。

深度缓冲（也叫Z-Buffer）就是为了解决这个问题而诞生的。它的原理非常直觉：对于屏幕上的每一个像素，除了存储颜色值之外，再额外存储一个深度值。这个深度值表示该像素在3D空间中离摄像机的距离。当GPU要绘制一个新的片段时，它会把这个片段的深度值和深度缓冲中已有的深度值进行比较——如果新片段更远，就丢弃它；如果新片段更近，就更新颜色缓冲和深度缓冲。

这样，不管你以什么顺序绘制物体，最终结果都是正确的：离摄像机近的物体总是遮挡远的物体。这个方法简单粗暴，但对绝大多数场景都有效。

## 创建深度缓冲纹理与DSV

理解了原理之后，我们来看看怎么在D3D11里创建和使用深度缓冲。这个过程和创建渲染目标纹理非常类似——本质上深度缓冲就是一张特殊的纹理，只不过它存的不是颜色，而是深度信息。

### 创建深度缓冲纹理

首先，我们需要创建一个2D纹理作为深度缓冲。这里有一个非常关键的点：深度缓冲的尺寸必须和渲染目标（后缓冲）完全一致，否则在绑定时会报错。

```cpp
// 创建深度缓冲纹理
Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;

D3D11_TEXTURE2D_DESC depthDesc = {};
depthDesc.Width = windowWidth;    // 必须和后缓冲一致
depthDesc.Height = windowHeight;  // 必须和后缓冲一致
depthDesc.MipLevels = 1;
depthDesc.ArraySize = 1;
depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // 24位深度 + 8位模板
depthDesc.SampleDesc.Count = 1;
depthDesc.SampleDesc.Quality = 0;
depthDesc.Usage = D3D11_USAGE_DEFAULT;
depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;  // 绑定为深度模板

HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);
if (FAILED(hr)) {
    // 创建失败处理
}
```

我们来看一下这里的关键参数。`DXGI_FORMAT_D24_UNORM_S8_UINT` 是最常用的深度缓冲格式，它把24位分配给深度值（范围0.0到1.0），8位分配给模板值（整数0-255）。如果你不需要模板测试，也可以用 `DXGI_FORMAT_D32_FLOAT`，但 `D24_UNORM_S8_UINT` 在大多数硬件上性能更好，兼容性也更好。`BindFlags` 设为 `D3D11_BIND_DEPTH_STENCIL` 表示这个纹理将用作深度模板缓冲——如果你还想在着色器里读取深度值（比如做后期处理），需要加上 `D3D11_BIND_SHADER_RESOURCE`，但这需要用 `TYPELESS` 格式创建纹理，我们后面再展开。

关于这个格式名称，`D24` 表示24位深度，`UNORM` 表示无符号归一化整数（映射到0.0~1.0的浮点数），`S8` 表示8位模板，`UINT` 表示模板值为无符号整数。这个名字虽然看起来像天书，但理解了每个部分的含义就很好记了。根据 [Microsoft Learn 的 DXGI_FORMAT 文档](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)，这些格式标识符遵循统一的命名规则。

### 创建深度模板视图

纹理创建好了，但GPU不能直接使用纹理对象——我们需要创建一个视图（View）来告诉GPU如何访问这个纹理。对于深度缓冲，我们创建的是深度模板视图（Depth Stencil View，DSV）：

```cpp
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
dsvDesc.Texture2D.MipSlice = 0;

hr = device->CreateDepthStencilView(
    depthStencilBuffer.Get(), &dsvDesc, &depthStencilView);
if (FAILED(hr)) {
    // 创建失败处理
}
```

`ViewDimension` 设为 `D3D11_DSV_DIMENSION_TEXTURE2D` 表示我们使用的是一个普通2D纹理。如果你用了多重采样（MSAA），这里要改成 `D3D11_DSV_DIMENSION_TEXTURE2DMS`。`MipSlice = 0` 表示使用第0级mipmap——深度缓冲通常只有一级mipmap，所以这里永远是0。

### 绑定到输出合并阶段

视图创建好之后，我们需要在渲染时把它绑定到管线的输出合并（Output Merger）阶段。这和绑定渲染目标视图是同一组API：

```cpp
// 同时绑定渲染目标视图和深度模板视图
context->OMSetRenderTargets(
    1,                          // 渲染目标数量
    renderTargetView.GetAddressOf(),  // 渲染目标视图数组
    depthStencilView.Get()      // 深度模板视图
);
```

之前我们调用这个函数时，最后一个参数传的是 `nullptr`，表示不使用深度缓冲。现在把它换成我们刚创建的DSV，深度测试就自动启用了。D3D11默认的深度测试配置是：深度函数为 `D3D11_COMPARISON_LESS`，深度写入开启。这意味着只有深度值比已有值更小的片段才会通过测试并写入缓冲——正好符合我们"近处遮挡远处"的需求。

### 每帧清空深度缓冲

和渲染目标一样，深度缓冲在每帧开始时也需要清空。不清空的话，上一帧的深度值会残留，导致新帧的物体被错误遮挡。清空深度缓冲使用的函数是 `ClearDepthStencilView`：

```cpp
// 清空渲染目标（用背景色填充）
const float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
context->ClearRenderTargetView(renderTargetView.Get(), clearColor);

// 清空深度缓冲（设为1.0，即最远）
// 清空模板缓冲（设为0）
context->ClearDepthStencilView(
    depthStencilView.Get(),
    D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
    1.0f,  // 深度值清为1.0（最远）
    0      // 模板值清为0
);
```

清空深度值为1.0是因为在D3D11的左手坐标系中，投影后的深度值范围是0.0（近裁剪面）到1.0（远裁剪面）。初始时我们把所有像素的深度设为最远（1.0），这样任何物体的深度值都会比它小，就能通过测试被绘制上去。

⚠️ 这里有一个非常常见的坑：深度缓冲的尺寸必须和渲染目标完全一致。如果窗口大小变了（用户拖动窗口边框、全屏切换等），你调用了 `SwapChain->ResizeBuffers()` 重建了后缓冲，但忘了同步重建深度缓冲，那绑定时就会失败，画面直接黑屏。所以在窗口大小变化的处理中，必须同时重建深度缓冲。

```cpp
// 窗口大小变化时的正确处理流程
void OnResize(UINT newWidth, UINT newHeight) {
    // 1. 释放旧的渲染目标和深度缓冲视图
    context->OMSetRenderTargets(0, nullptr, nullptr);
    renderTargetView.Reset();
    depthStencilView.Reset();
    depthStencilBuffer.Reset();

    // 2. 重建后缓冲
    swapChain->ResizeBuffers(1, newWidth, newHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);

    // 3. 重新创建渲染目标视图
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);

    // 4. 用新的尺寸重新创建深度缓冲（和创建代码一样）
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = newWidth;
    depthDesc.Height = newHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    device->CreateDepthStencilView(
        depthStencilBuffer.Get(), &dsvDesc, &depthStencilView);

    // 5. 重新绑定
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(),
        depthStencilView.Get());

    // 6. 更新视口
    viewport.Width = static_cast<float>(newWidth);
    viewport.Height = static_cast<float>(newHeight);
    context->RSSetViewports(1, &viewport);
}
```

很好，深度缓冲的基础设施搭好了。但光有深度缓冲还不够——我们现在的顶点全都是在2D屏幕空间里定义的，没有"前后"的概念。要让深度缓冲真正发挥作用，我们需要把顶点从3D模型空间变换到2D屏幕空间，这就是MVP矩阵要做的事情。

## MVP变换矩阵：把3D搬到屏幕上

MVP矩阵是3D图形编程中最核心的概念之一，它由三个矩阵相乘得到：Model（模型矩阵）、View（视图矩阵）、Projection（投影矩阵）。这三个矩阵分别完成三次坐标变换，把一个3D空间中的点最终映射到屏幕上的2D位置。

### 三次变换的含义

你可以这样理解这三次变换的角色：Model矩阵负责把物体从它自己的"模型空间"放到世界空间中的正确位置——包括平移、旋转、缩放。View矩阵负责把整个世界从世界空间变换到以摄像机为原点的"视图空间"——相当于摄像机不动，把整个世界反向移动。Projection矩阵负责把3D的视图空间压缩到2D的投影空间——近大远小的透视效果就是在这个阶段产生的。

最终，一个顶点从模型空间到裁剪空间的变换就是：`outputPos = vertexPos * M * V * P`。注意这里我写的是行向量左乘矩阵的形式，这是DirectXMath的约定，后面我们会详细讨论这个约定的含义。

### DirectXMath中的矩阵创建

DirectXMath库提供了一组非常方便的函数来创建这些矩阵。我们来逐个看看。

首先是View矩阵。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixlookatlh)，`XMMatrixLookAtLH` 的签名是：

```cpp
XMMATRIX XMMatrixLookAtLH(
    FXMVECTOR EyePosition,   // 摄像机位置
    FXMVECTOR FocusPosition, // 注视目标点
    FXMVECTOR UpDirection    // 上方向向量
);
```

函数名中的 `LH` 表示左手坐标系（Left-Handed），这是Direct3D的传统坐标系约定。你需要提供三个参数：摄像机在世界空间中的位置、摄像机看向的目标点、以及世界的"上"方向（通常是 (0,1,0)）。

然后是Projection矩阵。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixperspectivefovlh)，`XMMatrixPerspectiveFovLH` 的签名是：

```cpp
XMMATRIX XMMatrixPerspectiveFovLH(
    float FovAngleY,      // 垂直视场角（弧度）
    float AspectRatio,    // 宽高比
    float NearZ,          // 近裁剪面距离
    float FarZ            // 远裁剪面距离
);
```

视场角（FOV）决定了摄像机的可视范围，通常用 `XM_PIDIV4`（即45度）作为一个自然的起始值。宽高比就是窗口宽度除以高度。近远裁剪面定义了可见的深度范围——比近平面更近或比远平面更远的物体都会被裁掉。

Model矩阵则更自由，你可以用 `XMMatrixRotationY`、`XMMatrixTranslation`、`XMMatrixScaling` 等函数组合出任意变换。对于我们的旋转立方体，一个绕Y轴持续旋转的Model矩阵就够了：

```cpp
float angle = static_cast<float>(timer.GetTotalSeconds());
XMMATRIX model = XMMatrixRotationY(angle);
```

### 行向量约定与转置

现在到了最容易踩坑的地方。根据 [Microsoft Learn 的 XMMATRIX 文档](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/ns-directxmath-xmmatrix)，DirectXMath使用**行主序**（row-major）存储矩阵，数学上也采用**行向量**约定——也就是说向量写在左边，矩阵写在右边，`v' = v * M`。

但HLSL着色器默认使用的是**列主序**（column-major）存储。这意味着同样一段内存数据，CPU和GPU会以不同的方式解释它——如果你直接把DirectXMath的矩阵数据拷贝到常量缓冲中传给Shader，矩阵的行列会被颠倒，变换结果就完全错了。

解决方法是传给Shader之前做一次转置。根据 [Microsoft Learn 文档](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixtranspose)，`XMMatrixTranspose` 的作用就是求矩阵的转置：

```cpp
// 构建MVP矩阵
XMMATRIX view = XMMatrixLookAtLH(
    XMVectorSet(0.0f, 2.0f, -5.0f, 1.0f),  // 摄像机位置
    XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),    // 看向原点
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)     // 上方向
);

XMMATRIX proj = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,                              // 45度FOV
    static_cast<float>(windowWidth) / windowHeight,  // 宽高比
    0.1f,    // 近裁剪面
    100.0f   // 远裁剪面
);

XMMATRIX mvp = model * view * proj;

// 转置后传给Shader
XMMATRIX mvpT = XMMatrixTranspose(mvp);
```

⚠️ 这个转置步骤千万别漏！漏了转置不会报编译错误，不会报运行时错误，画面就是不对——物体可能消失、变形、飞到屏幕外。而且这种错误很难定位，因为所有API调用都返回S_OK。我们在这里被坑过好几次，每次都是盯着代码看了半天才想起来忘了转置。

当然，还有另一种做法：在HLSL中声明矩阵时加上 `row_major` 修饰符，或者在着色器文件开头加上 `#pragma pack_matrix(row_major)`，这样HLSL就会以行主序解释矩阵数据，你就不需要转置了。但微软的官方示例和大多数教程都采用转置的方式，所以我们也沿用这个惯例，保持和主流代码的一致性。

## 定义立方体顶点数据

矩阵变换搞定了，现在我们需要一个3D物体来应用这些变换。最经典的3D测试物体就是立方体——它有8个顶点、6个面、12个三角形，足够验证深度缓冲的效果，又不会太复杂。

### 立方体的顶点定义

一个立方体有6个面，每个面是一个正方形（2个三角形），所以一共需要12个三角形，36个顶点（如果用三角形列表）。虽然立方体只有8个几何顶点，但因为每个顶点在不同面上需要不同的颜色（或者法线），所以我们不能用索引缓冲来共享顶点——至少在本篇的颜色演示中是这样。

```cpp
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

// 立方体的36个顶点（每面6个顶点，2个三角形）
Vertex cubeVertices[] = {
    // 前面 (z = +1) — 红色
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

    // 后面 (z = -1) — 绿色
    { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

    // 上面 (y = +1) — 蓝色
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

    // 下面 (y = -1) — 黄色
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

    // 右面 (x = +1) — 青色
    { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

    // 左面 (x = -1) — 品红色
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
};
```

这个立方体以原点为中心，边长为2（从-1到1），每个面使用不同的颜色。注意三角形的绕序（winding order）：在D3D11默认的左手坐标系中，从外部看过去，三角形的三个顶点应该是顺时针排列的。这个绕序决定了面的朝向——后续如果开启背面剔除（Backface Culling），绕序反了的面会被丢弃。

### 创建顶点缓冲

顶点数据准备好了，接下来创建顶点缓冲：

```cpp
D3D11_BUFFER_DESC vbDesc = {};
vbDesc.ByteWidth = sizeof(cubeVertices);
vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
vbDesc.CPUAccessFlags = 0;

D3D11_SUBRESOURCE_DATA vbData = {};
vbData.pSysMem = cubeVertices;

Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer);
```

这里用 `IMMUTABLE` 因为立方体的顶点数据不会改变。如果你想让立方体在CPU端动态更新顶点位置（比如做变形动画），需要用 `D3D11_USAGE_DYNAMIC` 并设置 `CPUAccessFlags = D3D11_CPU_ACCESS_WRITE`。

## 常量缓冲与HLSL着色器

现在我们需要一个常量缓冲来传递MVP矩阵，以及对应的顶点着色器和像素着色器。

### 常量缓冲结构定义

C++端的常量缓冲结构需要和HLSL中的 `cbuffer` 严格对齐。这里我们传一个MVP矩阵就够了：

```cpp
// 必须和HLSL中的cbuffer布局一致
struct CBConstants {
    XMFLOAT4X4 mvpMatrix;
};
```

创建和更新常量缓冲：

```cpp
D3D11_BUFFER_DESC cbDesc = {};
cbDesc.ByteWidth = sizeof(CBConstants);
cbDesc.Usage = D3D11_USAGE_DYNAMIC;
cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);

// 每帧更新MVP矩阵
void UpdateConstantBuffer(float angle) {
    XMMATRIX model = XMMatrixRotationY(angle);
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 2.0f, -5.0f, 1.0f),
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        static_cast<float>(windowWidth) / windowHeight,
        0.1f, 100.0f
    );

    XMMATRIX mvp = model * view * proj;
    mvp = XMMatrixTranspose(mvp);  // 别忘了转置！

    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &mvp, sizeof(XMMATRIX));
    context->Unmap(constantBuffer.Get(), 0);
}
```

这里使用 `Map/Unmap` 而不是 `UpdateSubresource` 来更新动态缓冲，因为前者在高频更新场景下性能更好——`MAP_WRITE_DISCARD` 告诉驱动我们不需要保留旧数据，它可以直接给我们一块新的内存来写入，避免了GPU和CPU之间的同步等待。

### 顶点着色器

着色器代码比较简洁——顶点着色器只做一件事：用MVP矩阵变换顶点位置。

```hlsl
cbuffer Constants : register(b0) {
    float4x4 mvpMatrix;
};

struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;
    // 用MVP矩阵把模型空间位置变换到裁剪空间
    output.position = mul(float4(input.position, 1.0f), mvpMatrix);
    output.color = input.color;
    return output;
}
```

注意 `SV_POSITION` 这个语义——它是系统值（System Value）语义，告诉GPU这个输出是变换后的裁剪空间位置。GPU会自动做透视除法（把齐次坐标的xyz除以w）和视口映射，把它转换成屏幕上的像素坐标和深度值。这个深度值就是深度缓冲用来做比较的那个值。

`mul(float4(input.position, 1.0f), mvpMatrix)` 这个调用中，我们把位置向量扩展成齐次坐标（w=1表示这是一个点而不是方向向量），然后和矩阵相乘。因为HLSL的 `mul` 函数在向量在左边时执行的是行向量乘法 `v * M`，这正好和DirectXMath的约定一致。

### 像素着色器

像素着色器就更简单了，直接输出插值后的颜色：

```hlsl
struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return input.color;
}
```

光栅化阶段会对顶点着色器输出的颜色做三角形内的线性插值，所以像素着色器收到的 `color` 是经过插值的平滑颜色。对于我们的纯色面片来说，同一个面的三个顶点颜色相同，插值结果还是同一个颜色，不会有视觉差异。

## 封装Arcball摄像机

固定视角的摄像机用来看立方体终究差点意思——你没法从各个角度观察深度缓冲的效果。我们来封装一个Arcball摄像机，让用户可以通过鼠标拖拽来旋转视角。

Arcball摄像机的概念很简单：摄像机始终注视着一个目标点，但它围绕这个目标点在一个球面上运动。用户拖拽鼠标时改变球面上的经纬度，从而改变观察角度。这种交互方式在很多3D查看器、模型编辑器中都非常常见。

```cpp
#include <directxmath.h>
using namespace DirectX;

class ArcballCamera {
public:
    ArcballCamera() :
        m_theta(0.0f),        // 方位角（水平旋转）
        m_phi(XM_PIDIV4),     // 仰角（垂直旋转），初始45度
        m_radius(5.0f),       // 距离目标的距离
        m_target(0, 0, 0),    // 注视目标：原点
        m_fov(XM_PIDIV4),     // 视场角：45度
        m_aspect(1.0f),       // 宽高比
        m_nearZ(0.1f),        // 近裁剪面
        m_farZ(100.0f)        // 远裁剪面
    {}

    // 鼠标拖拽旋转
    void OnMouseMove(float dx, float dy) {
        m_theta += dx * 0.005f;     // 水平旋转
        m_phi -= dy * 0.005f;       // 垂直旋转（注意方向反转）

        // 限制仰角范围，避免万向锁
        m_phi = XMMax(0.1f, XMMin(XM_PI - 0.1f, m_phi));
    }

    // 鼠标滚轮缩放
    void OnMouseWheel(float delta) {
        m_radius -= delta * 0.005f;
        m_radius = XMMax(1.0f, XMMin(50.0f, m_radius));
    }

    // 更新宽高比（窗口大小变化时调用）
    void SetAspect(float aspect) { m_aspect = aspect; }

    // 获取摄像机世界空间位置
    XMVECTOR GetPosition() const {
        float x = m_radius * sinf(m_phi) * cosf(m_theta);
        float y = m_radius * cosf(m_phi);
        float z = m_radius * sinf(m_phi) * sinf(m_theta);
        return XMVectorSet(x, y, z, 1.0f);
    }

    // 获取View矩阵
    XMMATRIX GetViewMatrix() const {
        return XMMatrixLookAtLH(
            GetPosition(),
            XMLoadFloat3(&m_target),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        );
    }

    // 获取Projection矩阵
    XMMATRIX GetProjMatrix() const {
        return XMMatrixPerspectiveFovLH(
            m_fov, m_aspect, m_nearZ, m_farZ);
    }

private:
    float m_theta;    // 方位角
    float m_phi;      // 仰角
    float m_radius;   // 观察距离
    XMFLOAT3 m_target; // 注视目标
    float m_fov;      // 视场角
    float m_aspect;   // 宽高比
    float m_nearZ;    // 近裁剪面
    float m_farZ;     // 远裁剪面
};
```

这里用球坐标（theta, phi, radius）来描述摄像机在球面上的位置。`theta` 是水平旋转角（绕Y轴），`phi` 是和Y轴正方向的夹角。仰角被限制在0.1到PI-0.1之间，是为了避免万向锁问题——当phi恰好为0或PI时，摄像机正好在Y轴上，此时theta的变化无法产生水平旋转效果。

在消息处理中，我们把鼠标事件转发给摄像机：

```cpp
ArcballCamera camera;

// 在WM_MOUSEMOVE中
case WM_MOUSEMOVE:
    if (wParam & MK_LBUTTON) {  // 左键按下时拖拽
        float dx = (float)(short)LOWORD(lParam) - lastMouseX;
        float dy = (float)(short)HIWORD(lParam) - lastMouseY;
        camera.OnMouseMove(dx, dy);
        lastMouseX = (float)(short)LOWORD(lParam);
        lastMouseY = (float)(short)HIWORD(lParam);
    }
    return 0;

case WM_MOUSEWHEEL:
    camera.OnMouseWheel((float)(short)HIWORD(wParam));
    return 0;
```

## 渲染循环：把所有东西串起来

好了，所有的零件都准备好了——深度缓冲、MVP矩阵、立方体顶点、着色器、Arcball摄像机。现在把它们全部串到渲染循环里：

```cpp
void Render() {
    // 清空
    const float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
    context->ClearDepthStencilView(depthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 更新MVP矩阵
    float angle = static_cast<float>(timer.GetTotalSeconds());
    XMMATRIX model = XMMatrixRotationY(angle);
    XMMATRIX view = camera.GetViewMatrix();
    XMMATRIX proj = camera.GetProjMatrix();
    XMMATRIX mvp = XMMatrixTranspose(model * view * proj);

    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &mvp, sizeof(XMMATRIX));
    context->Unmap(constantBuffer.Get(), 0);

    // 设置管线状态
    context->IASetInputLayout(inputLayout.Get());
    context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    context->PSSetShader(pixelShader.Get(), nullptr, 0);

    // 绑定渲染目标+深度缓冲
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(),
        depthStencilView.Get());

    // 绘制
    context->Draw(36, 0);

    // 呈现
    swapChain->Present(1, 0);
}
```

到这里编译运行一下，你应该能看到一个旋转的彩色立方体，而且前面的面正确地遮挡了后面的面。拖拽鼠标可以旋转视角，滚轮可以缩放——现在你可以从各个角度欣赏深度缓冲的魔力了。

你会发现，当你把视角转到某些角度时，比如从正上方或正下方看过去，深度缓冲让上面的面正确地遮挡了下面的面。如果你临时去掉深度缓冲（把 `OMSetRenderTargets` 的最后一个参数设为 `nullptr`），你会看到后面的面"穿透"了前面的面，画面一团糟——这就直观地说明了深度缓冲的必要性。

## ⚠️ 踩坑预警

这一篇涉及的内容比较多，踩坑点也不少。我把最容易遇到的问题整理一下。

第一个坑就是前面反复强调的矩阵转置问题。DirectXMath是行主序，HLSL默认是列主序，忘了转置矩阵会导致变换结果完全错误。症状通常是物体消失或者变形，但不会产生任何错误码。如果你编译运行后发现画面是黑的或者物体在奇怪的位置，第一件事就是检查有没有忘记 `XMMatrixTranspose`。

第二个坑是深度缓冲的尺寸同步。在窗口大小变化时（包括全屏切换），必须在 `ResizeBuffers` 之后同步重建深度缓冲，否则 `OMSetRenderTargets` 会静默失败。注意我说的是"静默失败"——这个函数不会返回错误码，它就是不绑定，你的画面就直接变黑。

第三个坑是清空深度缓冲的值。清空深度缓冲时要用1.0f，不是0.0f。因为D3D11的深度测试默认是 `D3D11_COMPARISON_LESS`（小于才通过），初始值应该是最大值（最远），这样所有物体都能通过测试。如果你错误地清成了0.0f，那任何物体的深度值都不会比0更小，结果就是什么都画不出来。

第四个坑是三角形的绕序。在D3D11的默认状态下，背面剔除是关闭的，所以绕序反了也不会有问题。但如果你后续手动开启了背面剔除（通过光栅化器状态的 `CullMode`），绕序反了的面会被丢弃。对于我们的立方体来说，从外部观察时三角形应该是顺时针绕序（左手坐标系约定）。

还有一个关于投影矩阵的参数陷阱：`NearZ` 不能设为0。`XMMatrixPerspectiveFovLH` 内部会做除以NearZ的操作，如果NearZ为0会导致除零错误，矩阵中出现NaN/Infinity，最终画面全部消失。一般取0.01到1.0之间比较合适。同理，FarZ不能等于NearZ。

## 常见问题

**Q: 为什么我的立方体看起来像一个扁平的矩形？**

这通常是宽高比传错了。检查 `XMMatrixPerspectiveFovLH` 的第二个参数 `AspectRatio` 是否正确——它应该是后缓冲宽度除以高度。如果窗口是800x600，宽高比就是1.333。如果你传了1.0，那在一个宽屏窗口上物体就会看起来被水平拉伸。

**Q: 旋转到某些角度时立方体消失了怎么办？**

检查摄像机位置的Y坐标是否恰好等于目标点的Y坐标，并且UpDirection是(0,1,0)。当摄像机正好在目标点正上方或正下方时，View矩阵的构建会因为Up方向和视线方向平行而退化（叉积为零向量）。Arcball摄像机中我们限制了phi的范围就是为了避免这种情况。

**Q: 为什么我看不到立方体的某些面？**

可能是因为开启了背面剔除。检查你的光栅化器状态描述中的 `CullMode`。如果设为 `D3D11_CULL_BACK`，那绕序为逆时针的三角形会被丢弃。确保你的三角形绕序正确（从外部看顺时针），或者临时设为 `D3D11_CULL_NONE` 来排查。

**Q: 使用Map/Unmap更新常量缓冲时，为什么要用D3D11_MAP_WRITE_DISCARD？**

`D3D11_MAP_WRITE_DISCARD` 告诉GPU驱动"我不需要之前的缓冲内容，给我一块新内存"。驱动会在后台管理一个环形缓冲区，每次Map返回一块GPU不再使用的新内存，避免了CPU和GPU争用同一块内存导致的同步等待。如果你用 `D3D11_MAP_WRITE`，CPU可能需要等GPU读完旧数据才能写入新数据，这在每帧都更新常量缓冲的场景下会造成明显的性能下降。

## 总结

这篇我们完成了从2D到3D的关键跨越。深度缓冲解决了3D渲染中最基本的遮挡问题——没有它，3D世界就是一团乱麻。MVP变换矩阵把物体从模型空间一步步变换到屏幕空间——Model放物体、View定视角、Projection做透视，三者缺一不可。Arcball摄像机给了我们一个简单但实用的交互方式来从各个角度观察3D场景。

当然，我们的立方体现在看起来还是有点"平"——没有光影效果，每个面都是纯色，就像用彩纸糊出来的盒子。这是因为我们还没有加入光照计算。光照能让物体产生明暗变化和立体感，是让3D场景"活"起来的关键。下一篇我们就要实现Phong光照模型，让这个立方体看起来像一个真正的3D物体。

---

## 练习

1. **渲染多个立方体**：修改常量缓冲，除了MVP矩阵外再传一个单独的Model矩阵。在场景中渲染3个立方体，分别位于不同的位置，使用不同的旋转速度。（提示：每个立方体需要单独更新常量缓冲并调用Draw）

2. **添加键盘控制**：除了鼠标拖拽旋转视角外，添加WASD键控制摄像机在场景中平移，空格键上升，Shift键下降。这需要把Arcball摄像机扩展为一个自由飞行摄像机（Free-fly Camera）。

3. **渲染旋转正二十面体**：用20个等边三角形拼成一个正二十面体的顶点数据，用和立方体相同的MVP变换流程渲染它，每面颜色不同。观察深度缓冲在更复杂的几何体上的表现。

4. **实验深度测试函数**：创建一个自定义的 `ID3D11DepthStencilState`，把深度比较函数从 `D3D11_COMPARISON_LESS` 改为 `D3D11_COMPARISON_GREATER`（或者 `D3D11_COMPARISON_ALWAYS`），观察渲染结果的变化。思考一下为什么结果会是这样。

---

**参考资料**:
- [D3D11_DEPTH_STENCIL_VIEW_DESC - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_depth_stencil_view_desc)
- [XMMatrixLookAtLH - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixlookatlh)
- [XMMatrixPerspectiveFovLH - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixperspectivefovlh)
- [XMMatrixTranspose - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixtranspose)
- [XMMATRIX structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/ns-directxmath-xmmatrix)
- [DXGI_FORMAT enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
- [DirectX Factor: Vertex Shaders and Transforms - Microsoft Learn](https://learn.microsoft.com/en-us/archive/msdn-magazine/2014/september/directx-factor-vertex-shaders-and-transforms)
