---
title: GPU 管线：从顶点到像素的各阶段
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# GPU 管线：从顶点到像素的各阶段

按[篇目索引](index.md)的登记，本篇开的是 Direct3D 段的头一篇。往前数的话，文本段的[《整形：连字、kerning、BiDi》](20_shaping.md)、[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)与[《测量、命中测试与光标》](22_text_metrics.md)，都已跟您见过面。

主线咱们一句话就能摆开：内存里躺着一个三角形的三个顶点，本篇咱们陪它走完全程，看它在 GPU 的流水线上一站一站变成屏幕上的像素。本线的开篇[《从状态到像素》](01_status_to_pixels.md)陪重绘的消息走完了 CPU 侧的全程，等咱们把画面交到 GPU 手里，剩下的像素怎么造，就是本篇要补的那一半。走到出口的时候，前面几段里那些只闻其名的说法应该都有了着落：《特效：Effect 图》里咱们听见过一句话，每个 Effect 节点说的就是一段像素着色器。Skia 也有自己的 GPU 后端。这两样的根都扎在本篇讲的流水线上。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够，D3D11 的调用咱们从零讲起。事实的底本主要取自书架（也就是 `tutorial/native_win32/` 目录里那套带完整示例的老教程）的 D3D11 系列：顶点输入的底本在书架的[顶点缓冲与输入布局章](../native_win32/38_ProgrammingGUI_Graphics_D3D11_VertexInput.md)，深度与混合的底本在[深度缓冲章](../native_win32/40_ProgrammingGUI_Graphics_D3D11_Depth3D.md)与[混合透明章](../native_win32/42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md)，纹理采样的底本在[纹理采样章](../native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md)。下文为了省字，咱们把这几章简称作顶点输入篇、纹理篇、深度篇、混合篇。咱们哪一段拿不准，就回对应的章节核对原文。

## 五个方框的全程

咱们把全程画成一张图，五个方框就是本篇的全部角色：

```text
顶点缓冲 ──→ IA 输入装配 ──→ VS 顶点着色器 ──→ RS 光栅化 ──→ PS 像素着色器 ──→ OM 输出合并 ──→ 后台缓冲区
```

五个缩写是 Direct3D 给各段起的正式名字，咱们挨个认脸：

- IA（Input Assembler，输入装配）：从咱们准备好的缓冲区里取字节，按声明的格式拼成顶点，再按图元拓扑连成三角形，原料的装卸全归它。
- VS（Vertex Shader，顶点着色器）：一段咱们自己写的小程序，每个顶点各跑一次，最常见的活是把模型坐标搬进屏幕坐标，矩阵乘法就住在这里。
- RS（Rasterizer Stage，光栅化）：固定功能的硬件段，没有咱们的代码，它把三角形内部铺成像素格点，把顶点带的颜色与纹理坐标插值到每个像素上也是它的活。
- PS（Pixel Shader，像素着色器）：又是一段咱们写的程序，每个像素各跑一次，它决定这个像素最终的颜色。
- OM（Output Merger，输出合并）：深度测试、模板测试与混合都住在这里，PS 交出来的颜色能不能写进渲染目标、怎么跟旧颜色掺，由它说了算。

能塞代码的方框有两个，VS、PS 两段的程序全由咱们写。IA、OM 靠咱们给的配置组装，RS 是硬件自带的固定段。本篇的活就是把五个方框都过一遍手，素材就用那个最简的三角形。

## 输入装配：把原料摆上流水线

咱们在 CPU 这边看到的三个顶点，其实就是一排 float：每个顶点占了七个数，坐标占了三个，颜色占了四个，它们整块躺在一段连续的内存里。GPU 拿到的是一大块字节，它其实不知道哪儿是坐标、哪儿是颜色，所以输入装配这一段要咱们喂两样东西，装原料的顶点缓冲，加一份说明字节怎么切的输入布局。咱们来看原料：

```cpp
struct Vertex
{
    XMFLOAT3 position;  // 3D position (x, y, z)
    XMFLOAT4 color;     // RGBA color
};

Vertex triangleVertices[] =
{
    { XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // top, red
    { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },  // bottom right, green
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },  // bottom left, blue
};
```

咱们记一笔坐标的含义：x 与 y 的取值范围都从 -1 到 1，左下角处 x 与 y 都取了 -1，右上角处取的都是 1，窗口的正中就是原点，这套区间叫标准化设备坐标（NDC 是 Normalized Device Coordinates 的缩写，咱们往后都叫它 NDC）。书架特意强调过它的 y 轴朝上，跟 GDI、Direct2D 那边 y 朝下的习惯正好相反，从 2D 世界搬代码过来的您要在这里掉个头。

咱们把这块内存送上 GPU，走的是 `CreateBuffer`：

```cpp
D3D11_BUFFER_DESC bd = {};
bd.ByteWidth = sizeof(triangleVertices);   // total bytes to upload
bd.Usage     = D3D11_USAGE_IMMUTABLE;      // never modified after creation
bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;   // how the pipeline may use it

D3D11_SUBRESOURCE_DATA initData = {};
initData.pSysMem = triangleVertices;       // CPU-side bytes to copy

ID3D11Buffer* pVertexBuffer = nullptr;
pDevice->CreateBuffer(&bd, &initData, &pVertexBuffer);
```

desc 里的三个字段咱们各念一句。`ByteWidth` 是上传的总字节数。`BindFlags` 写的 `D3D11_BIND_VERTEX_BUFFER`，声明的是顶点缓冲的用途。`Usage` 给的 `D3D11_USAGE_IMMUTABLE`，表示的是内容创建之后不再改。

IMMUTABLE 是书架眼里性能最好的档，因为 GPU 知道数据是不会变的，所以可以做更激进的优化。顶点的内容要是每帧都得改（比如粒子系统），咱们就换成 `D3D11_USAGE_DYNAMIC`，搭配的 `D3D11_CPU_ACCESS_WRITE` 也要一起开。

`initData.pSysMem` 指着咱们在 CPU 端备好的数组，`CreateBuffer` 在创建的同时就把字节搬了上去。

### 输入布局是字节的说明书

咱们再认说明书，它的本体也是一个数组，一行对顶点里的一个字段：

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,
      D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
      D3D11_APPEND_ALIGNED_ELEMENT,
      D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

pDevice->CreateInputLayout(layout, _countof(layout),
                           pVSBlob->GetBufferPointer(),  // VS bytecode, required
                           pVSBlob->GetBufferSize(), &pInputLayout);
```

咱们把它当说明书读：语义名写的是 `POSITION`，对应 HLSL 那边冒号后面的标签，驱动拿这个名字把缓冲区的字节映射到着色器的输入变量上。格式 `DXGI_FORMAT_R32G32B32_FLOAT` 说的是三个 float，颜色的格式 `DXGI_FORMAT_R32G32B32A32_FLOAT` 装的是四个。第五个参数按理该写的是字节偏移，咱们填的 `D3D11_APPEND_ALIGNED_ELEMENT` 让 D3D 自己算，`COLOR` 会紧跟在 `POSITION` 的后面，咱们就不必手写 12 这个数，结构体改了也不用惦记着改偏移。

最要紧的一条在 `CreateInputLayout` 的后两个参数：它必须拿到 VS 的编译结果（也就是 `pVSBlob` 里装着的字节码），D3D11 拿布局对着 VS 的输入签名校验，咱们换了着色器却忘了换布局，返回的就是 `E_INVALIDARG`。书架给的排查也朴素：语义名的拼写对不对，`float3` 的格式对没对上 `DXGI_FORMAT_R32G32B32_FLOAT`，传的字节码是不是 VS 的而不是 PS 的。

### 连线的约定与索引缓冲

咱们还得告诉 IA 怎么连线，连线的方式归 `IASetPrimitiveTopology` 声明。`D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST`，是最常用的档，每 3 个顶点拼成一个独立的三角形。`TRIANGLESTRIP` 是共享边的三角带，画连续的曲面时省顶点。书架还列了 `LINELIST`、`POINTLIST` 一类，画线画点各有各的档。绘制之前咱们把三个绑定调用一起落下去：

```cpp
UINT stride = sizeof(Vertex);  // bytes per vertex
UINT offset = 0;
context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
context->IASetInputLayout(pInputLayout);
context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
```

`stride` 这个参数书架单独警告过：它告诉 GPU 的是每隔多少字节取一个顶点，咱们要是手滑把它写成 `sizeof(XMFLOAT3)`，GPU 就按错误的间隔读数据，画面上出来的是一堆乱七八糟的三角形，编译的阶段不报错，运行的时候也不报错，症状全落在咱们的眼睛里。

顶点数据还有一桩省内存的余地。一个矩形是由两个三角形拼成的，对角线上的顶点两边都要用，照顶点缓冲的存法它得存两份，索引缓冲（index buffer）解决的就是这个浪费：顶点的存储照旧只有一份，另开的缓冲里存的是编号，绘制的时候 IA 会按编号回取顶点。配套的调用是 `IASetIndexBuffer` 与 `DrawIndexed`，索引的格式常用 `DXGI_FORMAT_R16_UINT`，一个编号占的是两个字节。书架的 D3D11 系列没拿索引缓冲当主角，咱们按 SDK 的机制说到这儿，本篇的示例也不启用它，您知道它归 IA 管就够了。

## 顶点着色器：每个顶点跑一次

着色器（shader）这个词咱们在[《特效：Effect 图》](11_d2d_effects.md)就见过，它就是一段跑在 GPU 上的小程序。VS 管的是顶点这一头，它的粒度是每个顶点各跑一次，三个顶点的三角形就让 GPU 并行跑三份。咱们看书架配套的最简版：

```hlsl
struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;   // system value: clip-space position
    float4 color    : COLOR;
};

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);  // pad to float4, w = 1
    output.color = input.color;
    return output;
}
```

这一版 VS 干的只是搬运的活：把 float3 补成了 float4，末位补的 1 是齐次坐标的 w。冒号后面的 `SV_POSITION` 是系统值语义，它告诉 GPU 的这一项输出是裁剪空间的位置，而后面的透视除法与视口映射，硬件接着就替咱们做了，咱们不写一行代码。书架深度篇的原话咱们原样记着：GPU 会自动做透视除法（把齐次坐标的 xyz 除以 w）和视口映射，把它转换成屏幕上的像素坐标和深度值。

那矩阵乘法在哪儿？咱们看更常见的一份 VS：

```hlsl
cbuffer Constants : register(b0) {
    float4x4 mvpMatrix;
};

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), mvpMatrix);
    output.color = input.color;
    return output;
}
```

`mul` 这一行就是[《坐标变换》](09_d2d_transforms.md)那篇的 GPU 版对照。D2D 那边咱们调 `SetTransform`，矩阵乘法藏在 Direct2D 内部替咱们做了，咱们看不见也改不着。到了 D3D11 这边，乘法本身就在咱们自己写的 VS 里，矩阵的挑选与连乘的次数，全由咱们自己排。三个矩阵分管的是三段路，书架的白话咱们借来念一遍：`Model` 把物体从它自己的模型空间放进世界，`View` 把整个世界搬到摄像机的眼前，`Projection` 把视图空间压成了平面，近大远小就是在这一压里产生的。

常量缓冲（cbuffer 是 constant buffer 的缩写）是 CPU 往 GPU 递参数的通道，`register(b0)` 是它住的槽位。矩阵这类每帧都变的东西，咱们不让它走顶点数据，走的就是常量缓冲。

书架专门提醒过一处咱们容易漏的点：DirectXMath 在 CPU 侧是行主序，HLSL 默认的读法是列主序，同一块内存两边读出来的行列是颠倒的，所以传给着色器之前要用 `XMMatrixTranspose` 转置一次。漏了转置不报编译错，运行的时候也不报错，画出来的画面就是不对，物体的消失、变形、飞出屏幕都可能源自它。

每帧更新的写法，书架用的是 `Map`，配的档是 `D3D11_MAP_WRITE_DISCARD`，意思是咱们不要旧内容了，驱动便可以另给一块新的内存，所以 CPU 与 GPU 就不必互相等，这套细节咱们留到[《着色器与常量缓冲》](26_d3d11_shaders.md)再接。

## 光栅化：把三角形铺成像素

VS 交出来的还是三个顶点，可屏幕上的三角形是一整片像素，从三个点到一整片的转换就是光栅化（rasterization）的活。这一段是固定功能的硬件段，咱们塞不进代码，能配的是视口与背面剔除两处。

视口（viewport）回答的是 NDC 落到窗口哪一块的问题，咱们看它怎么配：

```cpp
D3D11_VIEWPORT viewport{};
viewport.TopLeftX = 0.0f;
viewport.TopLeftY = 0.0f;
viewport.Width    = static_cast<float>(width);
viewport.Height   = static_cast<float>(height);
viewport.MinDepth = 0.0f;
viewport.MaxDepth = 1.0f;
context->RSSetViewports(1, &viewport);
```

咱们配的宽高参与换算，NDC 的两个角点就落到了像素坐标上，`MinDepth` 与 `MaxDepth` 框住深度值的去处。书架的深度篇在窗口尺寸变化的处理流程里，重建的末尾都要重设一次视口，咱们把这件事记成：视口描述的是当帧的屏幕，所以窗口的尺寸一变它就得跟着换。

背面剔除（backface culling）管的是背对着咱们的三角形，它的开关咱们接着配：

```cpp
D3D11_RASTERIZER_DESC rsDesc = {};
rsDesc.FillMode = D3D11_FILL_SOLID;
rsDesc.CullMode = D3D11_CULL_BACK;
// FrontCounterClockwise stays FALSE: clockwise winding counts as front.

ID3D11RasterizerState* pRasterState = nullptr;
pDevice->CreateRasterizerState(&rsDesc, &pRasterState);
context->RSSetState(pRasterState);
```

这副骨架咱们按 SDK 的字段写，笔者没有编译验证过它。绕序（winding order）是三角形三个顶点的连接方向，D3D11 的默认约定是顺时针为正面，书架的叮嘱是从物体外部看过去顶点要按顺时针排。默认状态下背面剔除其实是关着的，绕序排错了也不出事。咱们一旦像上面这样开了 `D3D11_CULL_BACK`，绕序反了的三角形就会被整块丢弃。画 3D 封闭物体时剔除替咱们省掉一半的像素活，画 2D 界面时咱们多半宁可关掉它，因为界面三角形的朝向不一定守约定，整块的三角形丢了才是大事。

光栅化还有一件顺手干的活，动静很小却是后面两段要用的输入，它的名字叫插值。三个顶点各带了一份颜色，三角形内部的像素拿到的却是三者的加权平均，书架顶点输入篇的渐变三角形就是这么来的：顶部红、右边绿、左边蓝的排布，中间一片柔和的过渡，咱们没写一行插值的代码。纹理坐标（UV）走的是同一条路，三个顶点的 UV 被插到每个像素上，像素着色器拿到的就是每个像素自己的一份。

## 像素着色器：每个像素跑一次

咱们写的程序到了 PS 这一段又回来了。PS 也是着色器的一种，只是粒度换了，每个被光栅化出来的像素各跑一次，它算出一个颜色交了出去。最简的版本书架只写了一行：

```hlsl
float4 PS_Main(VSOutput input) : SV_TARGET
{
    return input.color;  // hand the interpolated color to the merger
}
```

`SV_TARGET` 说的是输出要写进渲染目标，插值来的颜色原样交货，屏幕上的渐变咱们就见到了。真正让 PS 威力大起来的是采样与算式两样活。

采样的活咱们看书架纹理篇的版本。带纹理的顶点多带一份 UV，VSOutput 也把 color 换成了 `texCoord`（输入布局那边同样要添对应的一行，这里按纹理篇的完整版写）：

```hlsl
Texture2D    g_Texture : register(t0);   // texture slot t0
SamplerState g_Sampler : register(s0);   // sampler slot s0

struct VSOutputTex {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 PS_Main(VSOutputTex input) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, input.texCoord);
}
```

咱们看 `Sample` 这一行，它收的是采样器与 UV，交回该位置的插值颜色。采样器（sampler）是独立的状态对象，过滤模式与寻址模式都归它管：`POINT` 档取的是最近的纹素，放大的时候一格一格像马赛克，`LINEAR` 档则对邻近的纹素加权平均，放大就平滑了。UV 超出 0 到 1 的部分怎么处理也是它说了算，`WRAP` 走的是重复，`CLAMP` 钳的是边缘。

CPU 侧要落的绑定有两个，一个管纹理的是 `PSSetShaderResources`，一个管采样器的是 `PSSetSamplers`。书架的排查写得很干脆：纹理画出来是黑的，多半是咱们漏了绑定两句中的一句。

算式说的是咱们自己的数学。颜色渐变的公式、光照模型、粒子按生命值淡出，全都写在 PS 里了，每个像素各算各的。《特效：Effect 图》里咱们留过一句话，每一个 Effect 节点就是一段跑在 GPU 上的像素着色器，串三层的 Effect 图就是三次 pass 的开销（pass 是图形学的行话，指一遍完整的绘制走完整条流水线）。现在咱们能把它放回流水线上了：D2D 的 Effect、Skia 的 SkSL 着色器（[《SkSL：运行时着色器》](17_sksl.md)里咱们写过它），编译到最后落到 GPU 上的都是 PS 这边的活。

## 输出合并：深度、模板与混合

PS 交出来的颜色还不能直接进后台缓冲区，咱们还得过门口的最后一段。输出合并的手里有三样家伙：深度测试、模板测试与混合。前两样干的活是挡像素，第三样管的是掺颜色。

咱们来认深度测试（depth test），它解决的是遮挡的问题，靠的是一张深度缓冲（depth buffer），书架的讲法很直白：它给每个像素存下的只有一个深度值，就是离摄像机的远近。绑定的位置在 `OMSetRenderTargets` 的第三个参数（最后一个）：

```cpp
context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
```

深度测试默认的比较函数是 `D3D11_COMPARISON_LESS`，深度写入默认也是开着的，合起来的意思就是：新像素离咱们更近才让它写进来，远的那个就被丢弃了。

每帧的开头，咱们要拿 `ClearDepthStencilView` 把深度清成 1.0，因为在 D3D11 的约定里深度 0 是最近、1 是最远，全清成的是最远，头一批像素才进得了门。书架专门列过一条提醒：清成 0.0f 的话，任何像素的深度都不会比 0 更小，一帧就什么都画不出来了。

常用的格式咱们挑的是 `DXGI_FORMAT_D24_UNORM_S8_UINT`，24 位的深度配上 8 位的模板，它的尺寸必须跟渲染目标一致，窗口的尺寸一变两边就得一起重建。

模板测试（stencil test）用的是同一张纹理上的 8 位模板值。它给每个像素都配了一个整数，绘制前咱们拿一个参考值跟它比：参考值跟模板值各按 `StencilReadMask` 掩码罩过，再按小于、等于一类的比较函数比出结果，比出来的结果决定像素留不留，圈范围、做描边一类的活靠它。本篇的示例不启用它，咱们知道它跟深度测试并排站在门口就好。

混合（blending）管的是新颜色与旧颜色怎么掺。公式咱们摆出来：

```text
finalColor = srcColor * srcBlendFactor + dstColor * dstBlendFactor
```

`srcColor` 是 PS 交出来的颜色，`dstColor` 是渲染目标上已有的颜色，两个系数由咱们配。标准透明混合的配置是源乘自己的 alpha，目标乘的是 1 减 alpha：

```cpp
D3D11_BLEND_DESC blendDesc = {};
blendDesc.RenderTarget[0].BlendEnable = TRUE;
blendDesc.RenderTarget[0].SrcBlend    = D3D11_BLEND_SRC_ALPHA;      // src * a
blendDesc.RenderTarget[0].DestBlend   = D3D11_BLEND_INV_SRC_ALPHA;  // dst * (1 - a)
blendDesc.RenderTarget[0].BlendOp     = D3D11_BLEND_OP_ADD;
blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;            // src alpha kept as-is
blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;  // dst alpha * (1 - a)
blendDesc.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;

ID3D11BlendState* pBlendState = nullptr;
pDevice->CreateBlendState(&blendDesc, &pBlendState);

float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
context->OMSetBlendState(pBlendState, blendFactor, 0xFFFFFFFF);
```

咱们拿三个档看一下：alpha 给 1.0 的时候新颜色全票通过，给 0.0 的时候旧颜色原样留下，给 0.5 的时候就是一半对一半。

[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)里咱们数过 opacity 的乘法链，颜色的 alpha 算一层、画刷的 opacity 算一层、图层的又算一层，乘出来的才是屏幕上的透明度。当时咱们看不见运算的现场，现在现场就摆在咱们眼前了：D2D 那边的透明合成，落到硬件层走的正是上面的公式。

咱们恢复不透明的直写模式也只要一句 `OMSetBlendState` 传 `nullptr`，按官方文档的说法，默认的行为是新颜色直接盖掉旧颜色。

书架混合篇还讲了预乘 alpha 的版本：咱们把 `SrcBlend` 改成 `D3D11_BLEND_ONE`，源颜色就不再乘自己的 alpha 了。[《特效：Effect 图》](11_d2d_effects.md)里图片解码后统一转成的 PBGRA 格式，说的正是预乘格式，咱们到那边回看就好。

透明还有一条书架反复强调的纪律：不透明的物体排在前面画，透明的物体跟在后面、按从远到近的顺序画，画透明的物体时，咱们还要把深度写入关掉，用的开关是 `D3D11_DEPTH_WRITE_MASK_ZERO`。道理就藏在混合的公式里：透明的像素要跟身后的旧颜色掺，要是透明的那批画在了前面，身后不透明的还没写进来，掺的就是清屏色。等深度写入关掉了，后到的透明像素不会被门外的深度测试错杀，从远到近的顺序则保证掺出来的层次跟真实的前后一致。

## 示例：一个三角形走完全程

零件都凑在咱们桌上了，咱们把它们按绘制的顺序拼起来。下面的代码是从书架各章的成稿里抽出来、按本篇的顺序重拼的骨架，笔者没有在本仓库里编译过它。书架原文自带完整的初始化与错误处理，您要跑整份程序，入口就在书架的[初始化与交换链章](../native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md)。

```cpp
void Render()
{
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    context->ClearRenderTargetView(renderTargetView, clearColor);

    // IA: raw material on the belt
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    context->IASetInputLayout(pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // our two programs, each on its post
    context->VSSetShader(pVertexShader, nullptr, 0);
    context->PSSetShader(pPixelShader, nullptr, 0);

    context->Draw(3, 0);

    swapChain->Present(1, 0);
}
```

咱们把这串调用按流水线读一遍。头一行的调用是 `ClearRenderTargetView`，拿的是背景色，把后台缓冲区整个铺满了。接下来三行 `IASet` 是输入装配那段的行头，两行 `SetShader` 把咱们写的两段程序挂上各自的岗位。`Draw(3, 0)` 的两个参数，给的是顶点数 3 与起点 0，这一声就是本帧的发令枪。末行的 `Present` 把画好的后台缓冲区换上屏幕，`Present` 的参数 1 表示的是等垂直同步，它的来路咱们在[《屏幕、帧与垂直同步》](02_screen_frame_vsync.md)讲过。从 Draw 到 Present 之间的那五段，咱们看不见，它们会顺着配好的行头自动走完，GPU 转完的时候，一个红绿蓝渐变的三角形就落到了窗口上。

## 圆角、阴影与模糊的归宿

五个方框咱们都铺完了，回头看 GUI 手里那几样讲究的效果，它们的活最后落在哪几段？圆角矩形的圆角，几何一侧咱们拿到的是一段段小三角形，曲线就是这样用直边逼近出来的（把曲线细化成小段的事，咱们在[《几何与路径》](08_d2d_geometry.md)见过 Direct2D 的版本，那边的 `Simplify` 把曲线拍平成直线段）。圆角边缘那一圈平滑的抗锯齿过渡，机制上靠的是像素着色器输出带 alpha 的颜色，把覆盖率折算成了透明度，混合的那段再把它跟背景掺起来。

柔和阴影是一张模糊过的影子图垫在底下，模糊本身是对邻域像素的加权采样，干的还是 PS 采样与算式的活，《特效》篇里的 GaussianBlur 与 Shadow 两个节点，咱们当时就说过它们是着色器。

窗口合成的最外一圈还有 DWM，它把各家窗口的画面也按同一类的混合公式拼到一起。本线的第七段还排着[《透明、阴影、模糊》](33_transparency_effects.md)与[《DWM：重定向表面与窗口合成》](30_dwm_composition.md)，咱们到那两篇里接着讲。

## 您可以动手的三处

示例是拼装的骨架，下面三处观察咱们改的都是一行、编的都是一遍：

1. 咱们把 `IASetVertexBuffers` 传的 stride 从 `sizeof(Vertex)` 改成 `sizeof(XMFLOAT3)`，GPU 按错误的间隔取顶点，画面上的颜色就乱了套，书架列的正是这个症状。
2. 咱们把三角形后两个顶点的顺序对调，默认状态下它照样在，因为 D3D11 默认不剔背面。咱们再按上文的光栅化状态开 `D3D11_CULL_BACK`，三角形就整个不见了，绕序与剔除的配合您就亲眼看到了。
3. 咱们把视口的 Width 减半，三角形跟着缩进窗口的左半边，NDC 到像素的换算归视口管的这件事，眼睛一遍就明白。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 输入装配的三个 `IASet` 调用各绑什么？stride 设错了症状是什么？
2. `CreateInputLayout` 为什么必须传 VS 的字节码？换了着色器忘了换布局，返回的是什么码？
3. NDC 的范围与 y 轴方向是什么？跟 GDI 的坐标系差在哪儿？
4. 透视除法与视口映射发生在 VS 之后的哪里？它们归咱们写吗？
5. 背面剔除默认开不开？D3D11 的默认约定里顺时针绕序算正面还是背面？
6. 深度测试默认的比较函数是什么？清深度缓冲为什么清 1.0 而不是 0.0？
7. 标准 alpha 混合的公式是什么？`SrcBlend` 与 `DestBlend` 各设成什么？
8. Effect 节点与流水线的哪一段对应？串三层的 Effect 图对应几次 pass？

## 下一步

本篇咱们把 Draw 当成了发令枪，枪响了之后流水线就自己转了起来，可设备的来历、立即上下文的管辖、交换链的配置，咱们全借了书架的现成代码。下一篇[《D3D11 设备与立即上下文》](24_d3d11_device.md)就把它的底细补上。想动手的您也可以拿着书架的初始化章把本篇的骨架跑起来。
