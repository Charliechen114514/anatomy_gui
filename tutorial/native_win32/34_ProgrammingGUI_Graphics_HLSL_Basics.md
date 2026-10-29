# 通用GUI编程技术——图形渲染实战（三十四）——HLSL语言基础：GPU编程的第一步

> 上一篇我们搞定了 Direct2D 和 GDI 的互操作，DCRenderTarget 和 GdiInteropRenderTarget 两条路都走通了，渐进迁移的方案也验证过了。在那篇文章的结尾，我提到了 Direct2D 的自定义效果——它允许你写自己的 GPU 着色器。那句话其实是一个伏笔，因为要写自定义效果，你就绕不开 HLSL。不光是 D2D 自定义效果，如果你将来想用 Direct3D 做更底层的图形渲染，HLSL 更是必须掌握的语言。所以从这篇开始，我们要正式踏入 GPU 编程的世界，先从 HLSL 的语言基础学起。

## 环境说明

本篇开发环境如下：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **Shader 编译器**: fxc.exe（Windows SDK 自带），D3DCompiler_47.dll（运行时编译）
- **目标平台**: Direct3D 11，Shader Model 5.0
- **关键头文件**: d3d11.h, d3dcompiler.h
- **关键库**: d3d11.lib, d3dcompiler.lib

本篇是 HLSL 语言基础，所以侧重点是语法和概念，不需要搭建完整的 D3D11 渲染管线。我们会写独立的 Shader 代码并通过 fxc.exe 验证语法正确性。完整的 D3D11 项目搭建会在后续文章中展开。

## 为什么要学 HLSL

在回答"为什么"之前，先简单说明一下 HLSL 是什么。HLSL 全称 High Level Shader Language，是微软为 Direct3D 设计的着色器语言。你写的 HLSL 代码会被编译成 GPU 能够执行的字节码，然后在 GPU 上运行。

你可能会有疑问：我们之前用 Direct2D 画圆、画矩形、画渐变，从来不需要写什么 Shader，那为什么现在要学这个？

答案取决于你想做什么级别的图形编程。如果你只用到 Direct2D 内置的图元和效果，确实不需要直接碰 Shader。但如果你想实现一些内置效果覆盖不了的效果——比如自定义的图像处理算法（边缘检测、色调映射、风格化后处理）、需要精确控制的像素级效果、或者想直接使用 Direct3D 进行底层渲染——那就必须自己写 Shader。HLSL 就是你在 GPU 上编程的语言，就像 C++ 是你在 CPU 上编程的语言一样。

另一个角度：即使你暂时不打算写自定义 Shader，理解 HLSL 的基本概念也会帮助你理解 Direct3D 的渲染管线、理解那些内置效果背后的工作原理，以及理解 GPU 编程的思维方式——数据并行、SIMD 架构、吞吐量优先。这些概念是通用的，即使你以后转向 Vulkan/Metal/WebGPU，Shader 编程的核心思路是一致的。

## HLSL 数据类型体系

HLSL 的数据类型体系设计得非常贴合 GPU 的硬件特性。GPU 天生擅长处理向量运算（同时处理多个分量）和矩阵运算，所以 HLSL 把这些类型做成了内置的一等公民，而不是像 C++ 那样需要借助第三方库。

### 标量类型

最基础的类型是标量。HLSL 支持 `bool`、`int`、`uint`、`float`、`half`（16位浮点，在某些 GPU 上有硬件支持）、`double`。其中最常用的是 `float`——GPU 的 ALU 原生就是 32 位浮点运算，所以 `float` 是最高效的选择。`double` 虽然也支持，但不是所有 GPU 都有完整的双精度浮点硬件，在某些消费级显卡上性能可能是 `float` 的 1/4 甚至更慢，而且某些 Shader Model 版本对 `double` 的支持有限制。

```hlsl
float opacity = 0.75;       // 最常用的类型
int vertexCount = 1024;     // 整数，常用于循环计数
bool isVisible = true;      // 布尔
uint flags = 0xFF00FF00;    // 无符号整数，常用于位操作
```

⚠️ 注意，HLSL 中的 `float` 默认就是 32 位 IEEE 754 浮点数，不像某些语言里有 `float`（32位）和 `double`（64位）的精度选择困扰。在 HLSL 中如果你需要双精度，必须显式声明为 `double`。不过日常 GPU 编程中 99% 的场景用 `float` 就够了，毕竟图形渲染不需要那么高的精度，GPU 的并行吞吐量才是关键。

### 向量类型

向量是 HLSL 中最核心的数据类型之一。GPU 硬件原生支持 SIMD（Single Instruction Multiple Data）操作，一条指令可以同时处理向量的所有分量。这就是为什么颜色（RGBA 四个通道）和位置（XYZW 四个坐标）在 HLSL 中天然就是向量。

向量类型的声明方式是 `typeN`，其中 `type` 是标量类型，`N` 是分量数量（1 到 4）：

```hlsl
float2 uv = float2(0.5, 0.5);           // 2D 纹理坐标
float3 normal = float3(0.0, 1.0, 0.0);  // 法线向量（指向上方）
float4 color = float4(1.0, 0.0, 0.0, 1.0);  // 红色，不透明
int4 indices = int4(0, 1, 2, 3);        // 四个索引
```

向量的分量可以通过多种方式访问，这是 HLSL 一个非常方便的特性：

```hlsl
float4 v = float4(1.0, 2.0, 3.0, 4.0);

// 使用 xyzw（位置语义）
float x = v.x;          // 1.0
float2 xy = v.xy;       // float2(1.0, 2.0)
float3 xyz = v.xyz;     // float3(1.0, 2.0, 3.0)

// 使用 rgba（颜色语义）
float r = v.r;          // 1.0
float3 rgb = v.rgb;     // float3(1.0, 2.0, 3.0)

// Swizzle（重排）——非常强大
float3 yzx = v.yzx;    // float3(2.0, 3.0, 1.0)
float4 xxzz = v.xxzz;   // float4(1.0, 1.0, 3.0, 3.0)
```

你可以把 Swizzle 理解为一种自由的分量重组操作——你可以任意选取和重排向量的分量，甚至可以重复。GPU 硬件原生支持这些操作，不会有任何性能开销。

⚠️ 注意一个很多从 GLSL 转过来的开发者会踩的坑：GLSL 中向量分量用 `xyzw` 表示位置、用 `rgba` 表示颜色、用 `stpq` 表示纹理坐标，而且这些访问器是互斥的——你不能在同一个向量上混用 `x` 和 `r`。但 HLSL 没有这个限制，`xyzw` 和 `rgba` 完全等价，可以自由混用。不过为了代码可读性，建议在表示位置时用 `xyzw`，表示颜色时用 `rgba`，保持语义一致性。

### 矩阵类型

矩阵在图形编程中无处不在——每个 3D 变换（平移、旋转、缩放、投影）都是矩阵运算。HLSL 用 `typeNxM` 表示 N 行 M 列的矩阵：

```hlsl
float4x4 worldMatrix;      // 世界变换矩阵（4x4）
float4x4 viewMatrix;       // 视图变换矩阵
float4x4 projectionMatrix;  // 投影矩阵
float3x3 normalMatrix;     // 法线变换矩阵（3x3）
```

矩阵的访问方式比较灵活，可以按行访问，也可以按元素访问：

```hlsl
float4x4 m = float4x4(
    1, 0, 0, 0,   // 第 0 行
    0, 1, 0, 0,   // 第 1 行
    0, 0, 1, 0,   // 第 2 行
    0, 0, 0, 1    // 第 3 行 —— 单位矩阵
);

float4 row0 = m[0];       // 访问第 0 行，得到 float4(1, 0, 0, 0)
float element = m[1][2];  // 访问第 1 行第 2 列，得到 0
float2 row01_col2 = m._12_22;  // 使用特殊语法访问 (0,1) 和 (1,1)
```

关于矩阵存储顺序（行主序 vs 列主序）是一个经典的困惑点。HLSL 默认使用列主序（column-major）存储矩阵，也就是说矩阵在内存中是按列排列的。但是 C++ 这边（比如 DirectXMath 库的 XMMATRIX）默认是行主序存储。这个不匹配会导致变换结果完全错误。解决方法有两种：要么在 HLSL 中声明矩阵时加 `row_major` 修饰符，要么在 C++ 端把矩阵转置后再传入。我们会在后面的 Constant Buffer 那篇详细讨论这个问题。

### 纹理和采样器类型

纹理（Texture）是 GPU 上存储图像数据的资源，采样器（Sampler）定义了从纹理中读取数据的方式——如何过滤、如何处理超出范围的坐标。

```hlsl
// 纹理类型
Texture2D diffuseMap;           // 2D 纹理，最常用
Texture2DArray shadowMapArray;  // 2D 纹理数组（阴影映射常用）
TextureCube envMap;             // 立方体纹理（环境映射）
Texture3D volumeTexture;        // 3D 体积纹理

// 采样器类型
SamplerState pointSampler;      // 标准采样器
SamplerComparisonState shadowSampler;  // 比较采样器（阴影映射专用）
```

采样器定义了采样行为——使用什么过滤模式（点采样/线性采样/各向异性采样）、使用什么寻址模式（Wrap/Clamp/Mirror/Border）。在 HLSL 中声明 `SamplerState` 后，实际的采样参数由 C++ 端在创建采样器对象时指定。

从纹理中采样数据使用 `Sample` 方法：

```hlsl
Texture2D myTexture : register(t0);   // 绑定到寄存器 t0
SamplerState mySampler : register(s0); // 绑定到寄存器 s0

// 在 Pixel Shader 中采样
float4 color = myTexture.Sample(mySampler, float2(0.5, 0.5));
```

这里出现了 `register(t0)` 和 `register(s0)` 这种语法——它们是 HLSL 的寄存器绑定声明，告诉 GPU 这个纹理/采样器对应哪个寄存器槽位。寄存器空间是 GPU 和 CPU 之间传递资源的"通道编号"。纹理使用 `t` 寄存器，采样器使用 `s` 寄存器，Constant Buffer 使用 `b` 寄存器，无序访问视图使用 `u` 寄存器。这些槽位的编号需要和 C++ 端的绑定代码一一对应。

## HLSL 内置函数

HLSL 提供了大量内置函数，覆盖了图形编程中常见的数学运算。这些函数在 GPU 上有硬件级别的优化，执行效率极高。我们按功能分类来介绍最常用的几个。

### 数学运算函数

```hlsl
// 向量运算
float3 a = float3(1, 2, 3);
float3 b = float3(4, 5, 6);

float d = dot(a, b);         // 点积：1*4 + 2*5 + 3*6 = 32
float3 c = cross(a, b);      // 叉积：(-3, 6, -3)
float3 n = normalize(a);     // 归一化：a / length(a)
float len = length(a);       // 向量长度：sqrt(1+4+9) = sqrt(14)

// 插值
float result = lerp(0.0, 1.0, 0.5);  // 线性插值，结果 0.5
float4 blended = lerp(colorA, colorB, t);  // 颜色混合

// 标量函数
float clamped = clamp(x, 0.0, 1.0);  // 限制在 [0, 1]
float saturated = saturate(x);         // 等同于 clamp(x, 0, 1)，但更快
float abs_val = abs(-3.14);           // 绝对值
float p = pow(2.0, 10.0);            // 幂运算：1024
float sq = sqrt(4.0);                // 平方根：2.0
float mx = max(a, b);                // 最大值
float mn = min(a, b);                // 最小值
```

`dot`（点积）和 `cross`（叉积）是 3D 图形编程中用得最多的两个向量运算。点积常用于计算光照（两个方向的余弦值）和投影，叉积常用于计算法线（两个向量所在平面的垂直方向）。`normalize` 把向量缩放为单位长度，这在光照计算中是必须的。`lerp` 是线性插值，在动画、混合、过渡效果中无处不在。`saturate` 是 `clamp(x, 0, 1)` 的高效版本，很多 GPU 上是单条指令。

### 纹理采样函数

```hlsl
Texture2D tex : register(t0);
SamplerState samp : register(s0);

// 标准采样 —— 使用采样器状态，在 Pixel Shader 中使用
float4 color = tex.Sample(samp, uv);

// 带偏移的采样 —— 在 uv 基础上偏移整数个纹素
float4 color2 = tex.Sample(samp, uv, int2(1, 0));

// 指定 LOD 级别的采样 —— 在非 Pixel Shader 中也能使用
float4 color3 = tex.SampleLevel(samp, uv, 0.0);

// 计算纹理梯度（用于自定义 LOD 计算）
float2 ddx_uv = ddx(uv);    // uv 在屏幕 X 方向的变化率
float2 ddy_uv = ddy(uv);    // uv 在屏幕 Y 方向的变化率
```

这里有几个重要的区别需要理解。`Sample` 方法只能在 Pixel Shader 中使用，因为它依赖 GPU 光栅化阶段计算的导数信息来确定 mip 级别。如果你需要在 Vertex Shader 或 Compute Shader 中采样纹理，必须使用 `SampleLevel` 并手动指定 LOD 级别。`ddx` 和 `ddy` 是偏导数函数，它们在屏幕空间中计算某个变量在 X/Y 方向上的变化率——这对于各种屏幕空间效果（如法线贴图计算、抗锯齿等）非常重要。

### 类型转换函数

```hlsl
float4 f = float4(1.5, 2.7, -1.2, 3.9);
int4 i = (int4)f;           // 截断转换：(1, 2, -1, 3)

// round、floor、ceil、trunc
float a = round(1.5);       // 2.0（四舍五入）
float b = floor(1.5);       // 1.0（向下取整）
float c = ceil(1.5);        // 2.0（向上取整）
float d = trunc(1.5);       // 1.0（截断小数部分）
```

这些函数都可以直接作用于向量，一次处理多个分量——这也是 GPU 编程思维的一个体现：操作天然就是并行的。

## 语义（Semantics）：连接管线阶段的桥梁

语义是 HLSL 中一个独特而关键的概念。在 C++ 中，函数参数通过名字来对应，但 GPU 管线的各个阶段之间传递数据不是通过名字，而是通过语义。你可以把语义理解为"数据的标签"——它告诉 GPU 这个变量是什么类型的数据，应该如何处理。

### 常用语义一览

根据 [Semantics - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics) 的文档，HLSL 语义分为两类：通用语义（如 POSITION、TEXCOORD、COLOR、NORMAL）和系统值语义（以 `SV_` 前缀开头，如 SV_Position、SV_Target、SV_VertexID）。

```hlsl
// 通用语义 —— 用于自定义的数据传递
POSITION    // 顶点位置
NORMAL      // 法线
TEXCOORD    // 纹理坐标（可以传递任意数据，不限于UV）
COLOR       // 顶点颜色
TANGENT     // 切线
BINORMAL    // 副法线

// 系统值语义（System Value）—— 有特殊含义，GPU 会特殊处理
SV_Position     // 经过变换后的齐次裁剪空间位置（VS 输出 / PS 输入）
SV_Target       // 像素着色器输出的颜色值（写入渲染目标）
SV_VertexID     // 顶点的索引（自动生成的序号）
SV_InstanceID   // 实例的索引（实例化绘制时使用）
SV_Depth        // 像素的深度值（可修改深度缓冲）
SV_ClipDistance  // 裁剪距离（用于自定义裁剪平面）
```

关键区别在于：通用语义纯粹是"标签"，GPU 不会对它们做任何特殊处理，只是把数据从前一阶段原样传递到后一阶段（会做插值）。系统值语义则会被 GPU 特殊处理——比如 `SV_Position` 从 Vertex Shader 输出后，GPU 会自动进行透视除法和视口变换，然后才传递给 Pixel Shader。`SV_Target` 则决定了输出颜色写入哪个渲染目标。

### 语义的匹配规则

Vertex Shader 的输入语义需要和 C++ 端的输入布局（Input Layout）匹配。Pixel Shader 的输入语义需要和 Vertex Shader 的输出语义匹配。名字不需要相同，但语义必须对应。

```hlsl
// C++ 端的输入布局
D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

// HLSL 端的 Vertex Shader 输入 —— 通过语义 "POSITION" 和 "TEXCOORD" 匹配
struct VSInput {
    float3 position : POSITION;    // 匹配 C++ 端的 "POSITION" 槽位
    float2 texcoord : TEXCOORD;    // 匹配 C++ 端的 "TEXCOORD" 槽位
};
```

注意这里的匹配逻辑：C++ 端的 `D3D11_INPUT_ELEMENT_DESC` 的第一个参数 `"POSITION"` 是语义名称，第二个参数 `0` 是语义索引。HLSL 端的 `POSITION` 和 `TEXCOORD` 就是和这些名称对应的。变量名 `position` 还是 `pos` 还是 `inPos` 都无所谓，GPU 只看语义。

TEXCOORD 是一个特别灵活的语义——虽然名字叫"纹理坐标"，但实际上它可以传递任何数据。很多开发者用 TEXCOORD0、TEXCOORD1、TEXCOORD2...来传递各种自定义数据（切线、世界坐标、自定义参数等）。这是因为 GPU 不会对 TEXCOORD 做特殊处理，它只是一个"自由数据通道"。

## 第一个完整 Shader：从 Vertex Shader 到 Pixel Shader

理论铺垫够了，现在我们来写第一个完整的 Shader。这个 Shader 实现最简单的功能：接收顶点位置和纹理坐标，输出顶点变换后的位置和颜色。

### Shader 代码结构

一个完整的 Shader 程序通常包含两个部分：Vertex Shader（顶点着色器）和 Pixel Shader（像素着色器，在某些文档中也叫 Fragment Shader）。Vertex Shader 处理每个顶点，输出变换后的位置和其他数据。Pixel Shader 处理每个像素（严格说是每个片段），输出最终的颜色。

```hlsl
// ============================================
// 文件：SimpleShader.hlsl
// 最简 Vertex Shader + Pixel Shader 示例
// ============================================

// ----- Vertex Shader -----

// 输入结构：从 C++ 端接收的数据
struct VSInput {
    float3 position : POSITION;     // 顶点位置（模型空间）
    float2 texcoord : TEXCOORD0;    // 纹理坐标
};

// 输出结构：Vertex Shader 输出，传递给 Pixel Shader
struct VSOutput {
    float4 position : SV_Position;  // 裁剪空间位置（必须有）
    float2 texcoord : TEXCOORD0;    // 传递纹理坐标
};

// 全局变量（从 C++ 端通过 Constant Buffer 传入）
cbuffer TransformBuffer : register(b0) {
    float4x4 worldViewProj;  // 世界-视图-投影组合矩阵
};

// Vertex Shader 入口函数
VSOutput VS_Main(VSInput input) {
    VSOutput output;

    // 将 3D 位置扩展为齐次坐标（w = 1.0），然后做矩阵变换
    output.position = mul(worldViewProj, float4(input.position, 1.0));

    // 直接传递纹理坐标（不需要变换）
    output.texcoord = input.texcoord;

    return output;
}
```

我们来逐行理解这段代码。`VSInput` 结构体定义了 Vertex Shader 的输入，它通过语义 `POSITION` 和 `TEXCOORD0` 从 C++ 端的顶点缓冲区接收数据。`VSOutput` 结构体定义了 Vertex Shader 的输出，`SV_Position` 是必须的——GPU 需要这个值来进行光栅化（决定哪些像素需要被绘制）。`TEXCOORD0` 是我们自定义传递的纹理坐标。

`cbuffer TransformBuffer : register(b0)` 声明了一个 Constant Buffer，绑定到寄存器 `b0`。`worldViewProj` 是一个世界-视图-投影组合矩阵，从 C++ 端传入。`mul(worldViewProj, float4(input.position, 1.0))` 这行做了两件事：先把 `float3` 的位置补上 `w=1.0` 变成齐次坐标（`float4(x, y, z, 1.0)`），然后用矩阵进行变换。`mul` 是 HLSL 的矩阵乘法函数，当矩阵在左边、向量在右边时是标准的列向量乘法。

接下来是 Pixel Shader：

```hlsl
// ----- Pixel Shader -----

// 输入结构：和 Vertex Shader 的输出结构对应
struct PSInput {
    float4 position : SV_Position;  // GPU 自动插值后的屏幕空间位置
    float2 texcoord : TEXCOORD0;    // GPU 自动插值后的纹理坐标
};

// 纹理和采样器
Texture2D diffuseTexture : register(t0);
SamplerState linearSampler : register(s0);

// Pixel Shader 入口函数
float4 PS_Main(PSInput input) : SV_Target {
    // 从纹理采样颜色
    float4 texColor = diffuseTexture.Sample(linearSampler, input.texcoord);

    // 直接输出纹理颜色
    return texColor;
}
```

Pixel Shader 的输入 `PSInput` 必须和 Vertex Shader 的输出 `VSOutput` 语义一一对应。注意 GPU 会自动对 Vertex Shader 的输出进行插值——比如一个三角形的三个顶点分别有纹理坐标 (0,0)、(1,0)、(0,1)，那么三角形内部某个像素的纹理坐标就是三个顶点纹理坐标的加权平均。这个插值是硬件自动完成的，你不需要做任何事情。

`diffuseTexture.Sample(linearSampler, input.texcoord)` 从纹理中采样颜色。`SV_Target` 语义表示这个函数的返回值写入渲染目标（通常是屏幕上的一个像素）。

⚠️ 这里有一个常见的困惑点：VSOutput 和 PSInput 的结构可以不同名（一个叫 VSOutput，一个叫 PSInput），但里面的语义必须对应。变量名也可以不同（`VSOutput` 里叫 `texcoord`，`PSInput` 里叫 `uv` 完全没问题），因为匹配靠的是语义标签，不是变量名。

### 用 fxc.exe 验证语法

我们不需要搭建完整的 D3D11 项目就能验证 Shader 语法。Windows SDK 自带的 fxc.exe 可以离线编译 HLSL 文件，检查语法错误。

把上面的代码保存为 `SimpleShader.hlsl`，然后在 Windows SDK 的命令行环境中执行：

```bash
# 编译 Vertex Shader
fxc.exe /T vs_5_0 /E VS_Main SimpleShader.hlsl /Fo SimpleVS.cso

# 编译 Pixel Shader
fxc.exe /T ps_5_0 /E PS_Main SimpleShader.hlsl /Fo SimplePS.cso
```

参数解释：`/T vs_5_0` 指定编译目标为 Vertex Shader Model 5.0，`/E VS_Main` 指定入口函数名为 `VS_Main`，`/Fo` 指定输出文件。如果编译成功，你会得到 `.cso`（Compiled Shader Object）文件——这就是 GPU 可以直接执行的字节码。如果有语法错误，fxc 会给出详细的错误信息和行号。

如果 fxc 不在你的 PATH 中，它通常位于 `C:\Program Files (x86)\Windows Kits\10\bin\<版本>\x64\` 目录下。你也可以在 Visual Studio 的 Developer Command Prompt 中直接使用。

## cbuffer：从 CPU 传递数据到 GPU

上面的 Shader 中出现了 `cbuffer`，但没有详细解释。现在我们来看看它到底是什么。

cbuffer（Constant Buffer）是 HLSL 中从 CPU 向 GPU 传递"不变数据"的机制。"不变"是相对于每次绘制调用而言的——在一次 Draw Call 中，cbuffer 里的值对所有顶点和像素都是相同的（Constant），不像顶点数据那样每个顶点都不同。

```hlsl
cbuffer PerFrameBuffer : register(b0) {
    float4x4 worldMatrix;      // 64 字节
    float4x4 viewMatrix;       // 64 字节
    float4x4 projectionMatrix; // 64 字节
    float totalTime;           // 4 字节
    float3 padding;            // 12 字节（对齐填充）
};

cbuffer PerObjectBuffer : register(b1) {
    float4x4 objectWorldMatrix; // 每个物体不同的变换
    float4 objectColor;         // 物体基础颜色
};
```

你可以把 cbuffer 理解为一块固定大小的内存区域，C++ 端负责往里面写数据，HLSL 端负责读数据。`register(b0)` 和 `register(b1)` 是寄存器绑定——C++ 端在设置 Constant Buffer 时需要指定对应的槽位号。

cbuffer 的大小有上限——在 Direct3D 11 中，单个 cbuffer 最大 4096 个 16 字节向量，也就是 64KB。不过实际上，每个 cbuffer 通常不会放那么多数据，而是按更新频率分组——每帧更新一次的放一个 cbuffer，每个物体更新一次的放另一个 cbuffer，从不更新的（材质参数）再放一个。这样可以减少不必要的数据传输，提高性能。我们会在第 36 篇专门深入讨论 cbuffer 的对齐规则和更新策略。

## HLSL vs GLSL：通道混淆踩坑

如果你之前接触过 OpenGL/WebGL，那你可能写过 GLSL。虽然 HLSL 和 GLSL 在概念上高度相似，但在一些细节上有差异，容易踩坑。

最大的一个坑是颜色通道的默认分量命名。在 GLSL 中，`vec4` 的分量是 `xyzw`，而颜色通常用 `rgba` 访问——但 `rgba` 和 `xyzw` 在同一个变量上是互斥的。在 HLSL 中，`float4` 的 `xyzw` 和 `rgba` 完全等价，可以在同一个变量上混用。

另一个常见混淆是矩阵乘法的参数顺序。GLSL 中矩阵乘法用 `*` 运算符，`M * v` 表示矩阵左乘向量。HLSL 中使用 `mul(M, v)` 函数，参数顺序的含义取决于矩阵的存储顺序（行主序/列主序）——这容易出错。

```hlsl
// HLSL
float4 result = mul(matrix, vector);  // 标准写法

// GLSL 等价写法
// vec4 result = matrix * vector;
```

还有一个是纹理采样函数的差异。GLSL 用 `texture(sampler, uv)`，HLSL 用 `texture.Sample(sampler, uv)`——方法名和调用方式都不同。如果你从 GLSL 迁移 Shader 代码，这些细节都需要注意。

总结一下主要的差异对照：

```hlsl
// ===== 类型名称 =====
// GLSL: vec2, vec3, vec4, mat4
// HLSL: float2, float3, float4, float4x4

// ===== 纹理采样 =====
// GLSL: vec4 color = texture(mySampler, uv);
// HLSL: float4 color = myTexture.Sample(mySampler, uv);

// ===== 矩阵乘法 =====
// GLSL: vec4 result = M * v;
// HLSL: float4 result = mul(M, v);

// ===== 修饰符 =====
// GLSL: in, out, uniform
// HLSL: 语义系统（POSITION, TEXCOORD, SV_Target 等）

// ===== 精度 =====
// GLSL: lowp, mediump, highp（移动端）
// HLSL: 无显式精度限定符，默认 float 为 32 位
```

了解这些差异后，将来如果需要在不同 API 之间移植 Shader，就不会那么痛苦了。

## 实战练习：手写灰度化 Pixel Shader

理论讲完了，我们来动手写一个真正能用的 Shader——灰度化效果。这个 Shader 接收一张纹理图片，把每个像素从彩色转换为灰度。

灰度化的核心算法很简单：对每个像素的 RGB 值按照人眼对不同颜色的敏感度加权平均。人眼对绿色最敏感，红色次之，蓝色最不敏感，所以经典的权重是 R=0.299、G=0.587、B=0.114（这是 ITU-R BT.601 标准，也是 JPEG 使用的标准）。

```hlsl
// ============================================
// 文件：GrayscaleShader.hlsl
// 灰度化 Pixel Shader
// ============================================

// Vertex Shader —— 全屏四边形（Full Screen Quad）
struct VSInput {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VSOutput VS_Main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

// Pixel Shader —— 灰度化
Texture2D sourceTexture : register(t0);
SamplerState pointSampler : register(s0);

// BT.601 灰度转换权重
static const float3 LUMINANCE_WEIGHTS = float3(0.299, 0.587, 0.114);

float4 PS_Grayscale(VSOutput input) : SV_Target {
    // 从源纹理采样
    float4 color = sourceTexture.Sample(pointSampler, input.texcoord);

    // 计算亮度值
    float luminance = dot(color.rgb, LUMINANCE_WEIGHTS);

    // 输出灰度颜色（保留原始 alpha）
    return float4(luminance, luminance, luminance, color.a);
}
```

我们来逐行理解 Pixel Shader 部分。`dot(color.rgb, LUMINANCE_WEIGHTS)` 做了一次点积运算——等价于 `color.r * 0.299 + color.g * 0.587 + color.b * 0.114`，但用 `dot` 写更简洁且 GPU 执行更高效。得到灰度值后，我们构建一个新的 `float4`，RGB 三个分量都是同一个灰度值，Alpha 通道保留原始值。

用 fxc 编译验证：

```bash
fxc.exe /T vs_5_0 /E VS_Main GrayscaleShader.hlsl /Fo GrayscaleVS.cso
fxc.exe /T ps_5_0 /E PS_Grayscale GrayscaleShader.hlsl /Fo GrayscalePS.cso
```

如果编译通过没有错误，说明语法是正确的。当然，要看到实际效果还需要搭建 D3D11 渲染管线来加载和运行这些 Shader——这是后面几篇要做的事情。

### 灰度化的扩展变体

理解了基本原理后，你可以尝试不同的灰度化方法。比如：

```hlsl
// 方法 1：平均值灰度（简单但效果一般）
float luminance = (color.r + color.g + color.b) / 3.0;

// 方法 2：只取绿色通道（最简单，某些场景够用）
float luminance = color.g;

// 方法 3：可调强度的灰度——通过 uniform 变量控制混合比例
float grayscaleAmount = 0.5; // 0.0 = 全彩色，1.0 = 全灰度
float luminance = dot(color.rgb, LUMINANCE_WEIGHTS);
float3 grayColor = float3(luminance, luminance, luminance);
float3 finalColor = lerp(color.rgb, grayColor, grayscaleAmount);
```

第三种方法用了 `lerp` 在彩色和灰度之间做插值——这是图像处理中一个非常通用的模式：计算效果值，然后用 `lerp` 控制效果强度。你几乎可以用这个模式实现任何可调强度的图像效果。

## 常见问题

### 问题1：HLSL 中 float 精度到底够不够

很多初学者会担心 32 位浮点精度不够用。实际上对于图形渲染来说，float 精度绝大多数情况下是足够的——颜色值在 [0,1] 范围内，法线向量是单位向量，纹理坐标在 [0,1] 范围内。float 在这些范围内的精度远超人眼分辨能力。真正需要 double 精度的场景主要是某些科学计算和数值模拟，一般的图形渲染几乎用不到。

### 问题2：Shader 代码中可以有分支和循环吗

可以，但要谨慎。HLSL 支持 `if/else` 分支和 `for/while` 循环，但 GPU 的执行模型和 CPU 不同——GPU 以线程束（Warp/NVIDIA 或 Wavefront/AMD）为单位执行，一个线程束内的所有线程执行相同的指令。如果出现分支导致线程束内的线程走了不同的路径（称为分支发散，Branch Divergence），那么两条路径都会执行，不满足条件的线程会屏蔽结果，性能就会下降。循环也是同样的道理——如果不同线程的循环次数不同，会按最大的次数执行。

不过也不要过度避免分支——现代 GPU 的分支预测和执行效率已经很好了，对于简单的、大多数线程走同一路径的分支，性能开销很小。关键是避免在紧密循环中有大量发散的分支。

### 问题3：为什么 Vertex Shader 的输入不能直接用 float4

你可以用 float4，也可以用 float3。这取决于你的顶点数据格式。如果顶点缓冲区中每个位置是 3 个 float（x, y, z），那就用 `float3`，在 Vertex Shader 中再补上 `w=1.0` 变成齐次坐标。如果是 4 个 float（x, y, z, w），就用 `float4`。这个需要和 C++ 端的 `D3D11_INPUT_ELEMENT_DESC` 中声明的格式（`DXGI_FORMAT_R32G32B32_FLOAT` vs `DXGI_FORMAT_R32G32B32A32_FLOAT`）保持一致。

## 总结

这一篇我们完成了 HLSL 语言的入门。从数据类型（标量、向量、矩阵、纹理、采样器）到内置函数（dot、cross、normalize、lerp、Sample），从语义系统（POSITION、TEXCOORD、SV_Position、SV_Target）到第一个完整的 Vertex Shader + Pixel Shader，再到灰度化 Shader 的实战练习，这些内容构成了 GPU 编程的语法基础。

HLSL 本身的语法并不复杂——它比 C++ 简单得多，没有指针、没有动态内存分配、没有面向对象。真正需要花时间理解的是 GPU 的执行模型和管线架构——数据是怎么流动的、每个阶段做什么、数据怎么在阶段之间传递。语义系统和 cbuffer 就是这些架构概念的语言层面体现。

下一篇我们将进入 HLSL 的编译与调试——如何把 .hlsl 文件变成 GPU 可执行的字节码、如何处理编译错误、以及如何使用 PIX 和 RenderDoc 这些 GPU 调试工具来排查 Shader 问题。编译和调试是写 Shader 过程中最耗时的部分，掌握这些工具能让你的开发效率提升几个数量级。

---

## 练习

1. **手写纯色 Pixel Shader**：写一个 Pixel Shader，输出一个固定颜色 `float4(0.2, 0.6, 0.9, 1.0)`。用 fxc.exe 编译通过，确保你的开发环境能正常编译 HLSL 文件。

2. **灰度化变体——Sepia 效果**：在灰度化的基础上实现怀旧色调（Sepia）效果。先转灰度，然后用一个色调映射矩阵给灰度值加上暖色调：`float3 sepia = float3(gray * 1.2, gray * 1.0, gray * 0.8)`，最后用 `saturate` 确保值不超过 [0, 1]。

3. **理解 Swizzle 操作**：写一个测试 Shader（不需要实际运行），使用 Swizzle 操作完成以下任务——给定 `float4 color = float4(0.1, 0.3, 0.5, 0.7)`，分别提取 RGB 三个分量（不含 Alpha）、交换 R 和 B 分量、将所有分量设为 Alpha 的值。

4. **cbuffer 定义练习**：定义一个 `cbuffer LightBuffer`（绑定到寄存器 b2），包含以下数据：一个平行光的方向（float3）、一个平行光的颜色（float3）、一个环境光颜色（float3）、一个高光强度（float）。注意考虑 16 字节对齐规则，合理添加 padding。

---

**参考资料**:
- [Data Types (HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-data-types)
- [Semantics - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)
- [Texture Object (HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-type)
- [Sample (DirectX HLSL Texture Object) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-sample)
- [Packing Rules for Constant Variables - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules)
- [Effect-Compiler Tool (fxc.exe) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/fxc)
- [Intrinsic Functions (HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions)
- [Work with shaders and shader resources - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dgetstarted/work-with-shaders-and-shader-resources)
