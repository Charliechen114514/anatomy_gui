---
title: 着色器与常量缓冲
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 着色器与常量缓冲

按[篇目索引](index.md)的登记，本篇在 Direct3D 这一批文章里排的是第四位。排在它前面的三位，[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)、[《D3D11 设备与立即上下文》](24_d3d11_device.md)与[《资源与视图》](25_d3d11_resources.md)，下文咱们分别简称它们为管线篇、设备篇、资源篇。再往后的下一篇是[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)，下文咱们简称它为交换链篇。

本篇能直接衔接的就是前面三篇。管线篇把 IA、VS、RS、PS、OM 五站的分工摆开过，设备篇介绍了设备与立即上下文，一位管的是造东西，另一位管的是发命令，资源篇垫了缓冲与视图这对概念，b、t 槽位和 Map 更新的路数它也替本篇预铺过。更早的旧经验也能用上：[《SkSL：运行时着色器》](17_sksl.md)里咱们认过着色器的概念，跑在绘制最里层、对每个像素各算一遍的小程序。D3D11 把这桩活分给了两段小程序，本篇的主线就是把咱们自己的这两段装进 GPU：用 HLSL 写出来、编成字节码、配上输入布局与常量缓冲，装配的路一直铺到 Draw 调用为止。读者设定照旧，您会 C++ 和 Win32 的基础用法就够。设备怎么建、缓冲家族里还有哪几位成员，分别归设备篇和资源篇的正事，咱们本篇只在用到的地方带一句。

书架上还有一批现成的参考书：[HLSL 语言基础](../native_win32/34_ProgrammingGUI_Graphics_HLSL_Basics.md)、[编译与调试](../native_win32/35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md)、[CBuffer 专题](../native_win32/36_ProgrammingGUI_Graphics_HLSL_CBuffer.md)与[顶点输入专题](../native_win32/38_ProgrammingGUI_Graphics_D3D11_VertexInput.md)四篇都写好了，本篇的事实性论断以它们和官方文档为据。另外说明一句：本篇的代码都是讲解用的骨架，笔者没有把它们放进工程编译验证过，您真要上手跑的时候，以手上的 SDK，配上 [Microsoft 的 Direct3D 官方文档](https://learn.microsoft.com/en-us/windows/win32/direct3d11)，当作最后的依据。

## 两段小程序的最小样子

D3D11 的管线里，咱们能插手的两段住在两头。咱们来说顶点着色器（Vertex Shader，后文咱们简称 VS）每个顶点跑一趟，管的是把顶点从模型坐标搬进裁剪坐标。排在另一头的像素着色器（Pixel Shader，后文咱们简称 PS）每个像素跑一趟，管的是算出这一像素该是什么颜色。中间把三角形铺成像素的光栅化，归固定功能的硬件管，咱们本篇不写它。

咱们把最小的一对摆出来：

```hlsl
struct VSInput {
    float3 position : POSITION;   // semantic: what this field carries
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;  // system value: clip-space position
    float4 color    : COLOR;
};

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.color    = input.color;
    return output;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    return input.color;
}
```

咱们逐行来认。冒号后面的名字叫语义（semantic），拿白话讲就是贴在字段上的标签，写明这个字段装的是什么、该跟谁配对。`POSITION` 和 `COLOR` 走的是普通语义，名字是咱们自己定的，配对的事交给后面的输入布局。`SV_` 打头的是系统值语义，`SV_POSITION` 是 VS 输出里强制的一项，装的必须是裁剪空间的坐标。`SV_TARGET` 则是 PS 的返回值去处，意思是算出来的颜色写进渲染目标的当前像素。

咱们拿 `float3` 补上一个 `1.0f`，拼成了 `float4`，这一步咱们得看一眼：矩阵乘法要四个分量，而位置向量天然是三个，补上的第四个叫齐次坐标的 w 分量，平移变换就靠它才进得了乘法。

有一个差别您拿着 SkSL 的印象过来要留神：VS 的输出到了 PS 那头，值已经不一样了。光栅化铺像素的时候，顶点之间的属性按位置加权摊开，PS 收到的 `COLOR` 是插值之后的数。三角形的三个顶点各是红、绿、蓝的话，屏幕上铺开的就是一片渐变，中间每个像素的颜色都是三份加权的结果。所以同一份 `VSOutput` 结构，VS 那头是顶点的原值，PS 这头是摊开之后的值，这个流水线的细节归[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)讲，咱们知道这个效果就行。

## cbuffer 与 b、t、s 三个前缀

光有顶点带来的数还不够，咱们还缺一条道：每帧都在变的时间、每个物体各不相同的世界矩阵，咱们得从 C++ 那边把数递进公式。干这桩事的声明叫 cbuffer，中文的名字就是常量缓冲，它的样子长这样：

```hlsl
cbuffer PerObject : register(b0)
{
    float4x4 gWorld;
    float4   gColor;
};
```

`register(b0)` 点的是寄存器槽位，咱们可以把它想成 GPU 侧一排编了号的插座，C++ 那边往几号插座里塞了东西，而 HLSL 这边就从几号插座读。前缀分了三种，各装各的，咱们摆成一张表：

| 前缀 | 装的东西 | C++ 侧交货的对象 |
|:---:|:---|:---|
| b | 常量缓冲 | ID3D11Buffer，经 VSSetConstantBuffers 绑上 |
| t | 纹理等只读资源 | ID3D11ShaderResourceView |
| s | 采样器 | ID3D11SamplerState |

还有个 u 前缀是给无序访问视图用的，而计算着色器和像素着色器都能用它读写，本篇咱们用不到，您知道有这么一号就行。表里 t 和 s 两位的完整讲究，在书架的[纹理与采样器](../native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md)一篇里，咱们不铺开。

有了 cbuffer，咱们回头给 VS 加上世界矩阵，要改的只有一行：

```hlsl
VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), gWorld);
    output.color    = input.color;
    return output;
}
```

`mul` 是 HLSL 内置的矩阵乘法函数，向量放在左边走的就是行向量乘法。寄存器的编号是咱们自己定的，但是两边必须对上：HLSL 里写 `register(b0)`，C++ 那边绑定的时候就得塞 0 号槽。编号对不上的话，咱们不开调试层就没有报错，GPU 照着错位的槽位读数据，画面默默地不对，所以这类哑巴错最磨人。调试层是建设备时可选带的一道运行时检查，开着的话这类错会在输出窗口里报出来，咱们到输入布局一节还会劝您常开它。还有一点咱们要留意，VS 和 PS 各有各自的一套槽位，同一个常量缓冲两边都要读的话，咱们就得两边各绑一遍。

## 从 .hlsl 到字节码

咱们写好的 .hlsl 是给人读的源码，GPU 只吃编好的字节码，而中间得过一道编译。运行时干这件事的路子靠 `D3DCompileFromFile`：

```cpp
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

ComPtr<ID3DBlob> vsBlob;
ComPtr<ID3DBlob> errorBlob;

UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

HRESULT hr = D3DCompileFromFile(L"basic.vs.hlsl", nullptr, nullptr,
                                "VS_Main", "vs_5_0", flags, 0,
                                &vsBlob, &errorBlob);
if (FAILED(hr)) {
    if (errorBlob) {
        // the blob holds the compiler's message, with line numbers
        OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
    }
    return false;
}
```

两个最要紧的参数是入口名和目标。咱们的一个 .hlsl 文件里可以住好几个着色器函数，入口名点的是这一趟编哪个。`vs_5_0` 和 `ps_5_0` 指的是编译目标，对应的是 Shader Model 5.0，D3D11 的标配档位。`ID3DBlob` 拿白话讲就是一块带长度的字节袋子，编出来的字节码装一个，编译器的报错文本装另一个，而报错里带着行号，咱们拿着行号回源文件改就是了。

调试期的两个标志是一对：`D3DCOMPILE_DEBUG` 往字节码里嵌调试信息，而 `D3DCOMPILE_SKIP_OPTIMIZATION` 让指令别被优化得跟源码对不上号，咱们在 GPU 调试器里断点才停得明白。书架的[编译与调试](../native_win32/35_ProgrammingGUI_Graphics_HLSL_CompileDebug.md)把整套标志和调试工具讲得全，您往深处走的时候移步那边。

编译的另一条路是离线编。构建的时候用 SDK 自带的 fxc.exe 把 .hlsl 编成 .cso 字节码文件，程序跑起来的时候直接读文件，一步就到位了。两条路咱们怎么取舍？开发期图的是改完就能跑，运行时编译给的就是这个：不用给构建系统加工序，不过每次启动都得多编一遍，发布的时候还得带上 D3DCompiler_47.dll。而离线路子启动快、发布干净，但是改一行 HLSL 就得重新走一遍构建。开发期用前者、发布换后者是常见的搭配，两边产出的字节码格式一样，所以两边切换不吃力。

字节码到了手，咱们就交给设备造成着色器对象：

```cpp
ComPtr<ID3D11VertexShader> vertexShader;
device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                           nullptr, &vertexShader);

ComPtr<ID3D11PixelShader> pixelShader;
device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                          nullptr, &pixelShader);
```

两个接口不同的只是名字，套路是一样的：吃字节码，交回一个能绑到管线上的对象。您留意一件事，VS 的 blob 造完着色器别急着放，下一节咱们造输入布局还要用它一次。

## 输入布局：把 C++ 的顶点对到语义上

咱们在 C++ 那边定义的顶点结构，到了 GPU 眼里只是一串字节，哪个字节装的是位置、哪个字节装的是颜色，咱们得附上一份说明书，它的名字就叫输入布局：

```cpp
struct Vertex {
    XMFLOAT3 position;   // 12 bytes
    XMFLOAT4 color;      // 16 bytes
};

D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
      D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
      D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

ComPtr<ID3D11InputLayout> inputLayout;
device->CreateInputLayout(layout, ARRAYSIZE(layout),
                          vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                          &inputLayout);
```

数组里每一项描述一个字段：语义名对上 HLSL 里的冒号标签，格式写明了是三个 float 还是四个，槽号和偏移说的是数据躺在哪，`D3D11_APPEND_ALIGNED_ELEMENT` 则是让 D3D 自己算当前字段的偏移，省得咱们手数。

要紧的一处在 `CreateInputLayout` 的后两个参数：它要吃 VS 的字节码。原因在于说明书必须跟 VS 的输入签名核对，咱们声明了 `POSITION`，VS 的输入里也得真有这个语义，字段的类型还得兼容，核对不过的话 `CreateInputLayout` 当场返回 E_INVALIDARG。这在告诉咱们：布局和着色器对不上的错误，在创建的时候就拦下了，也就轮不到绘制了。反过来的失误更隐蔽：布局造好了，绘制前忘了调 `IASetInputLayout`，这时候 `Draw` 会出什么事，笔者不咬死。调试层开着的话（建设备时带上 `D3D11_CREATE_DEVICE_DEBUG` 标志），您会在输出窗口里看到提示。没开的话，可能一路沉默到画面不对的那一步。稳妥的办法就是调试层常开，哑巴错少了大半。

## 常量缓冲：对齐、排布与更新

cbuffer 在 C++ 那边的实体还是一个 `ID3D11Buffer`，只是咱们创建它的时候，参数有它自己的讲究：

```cpp
struct PerObject {
    XMMATRIX world;    // 64 bytes: four 16-byte rows
    XMFLOAT4 color;    // 16 bytes
};
static_assert(sizeof(PerObject) % 16 == 0, "cbuffer size must be 16-byte aligned");

D3D11_BUFFER_DESC desc{};
desc.ByteWidth      = sizeof(PerObject);
desc.Usage          = D3D11_USAGE_DYNAMIC;
desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

ComPtr<ID3D11Buffer> constantBuffer;
device->CreateBuffer(&desc, nullptr, &constantBuffer);
```

咱们排布 cbuffer 时，头一条讲究的就是 16 字节。常量缓冲按 16 字节一行地排布，向量是不许跨行的，整个缓冲的字节数也得是 16 的倍数，`ByteWidth` 不合要求的话 `CreateBuffer` 报 E_INVALIDARG。排布上最容易错位的是 `float3`。而 HLSL 这边，`float3` 后面紧跟的一个 `float` 会补进同一行剩下的 4 字节，两个 `float3` 却各自占了一行。C++ 那边就不一样了，结构体里的第二个 `XMFLOAT3` 会从偏移 12 接着排。两边的排布就岔开了，读出来的数全串了位。省心的办法是咱们装参数一律用 `float4`，等于白送了 4 字节，从此就不用对偏移表了。

咱们上传矩阵时另有一道排布的岔。HLSL 默认按列主序读缓冲里的矩阵，DirectXMath 的 `XMMATRIX` 却按行主序存，直接把数据 memcpy 过去的话，矩阵等于被转了一圈，画面上的表现通常是整片几何拧着变形。修法的路有两条：上传前拿 `XMMatrixTranspose` 转置一道，或者在 HLSL 的声明上写 `row_major` 让它按行主序读。咱们两条里挑一条走就够，两头都转等于转了回去。

每帧要变的数据，咱们更新走 Map：

```cpp
PerObject data{};
data.world = XMMatrixTranspose(worldMatrix);
data.color = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f);

D3D11_MAPPED_SUBRESOURCE mapped{};
context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
memcpy(mapped.pData, &data, sizeof(data));
context->Unmap(constantBuffer.Get(), 0);
```

`D3D11_MAP_WRITE_DISCARD` 值得咱们单独讲一讲。GPU 读上一帧的数据可能还没读完，咱们直接改旧内存的话，驱动能做的只是让 CPU 等着 GPU 让路，帧率就卡在了这里。DISCARD 的意思是旧内存不动，换一块新的给咱们写，CPU 和 GPU 谁也不用等谁了。有的朋友图省事每帧销毁重建整个缓冲，功能上倒是也能跑，但是创建销毁是白折腾驱动，老的缓冲可能还攥在 GPU 手里，路数上就落了下乘。另一条更新路走的是 `UpdateSubresource`，内部要多走一道中转的拷贝，几秒才更一次的数据用它合适，每帧都更的话那道拷贝就白交了。书架的[CBuffer 专题](../native_win32/36_ProgrammingGUI_Graphics_HLSL_CBuffer.md)把 Map 报错和排布错位的现场都摆出来了，您对照着看更扎实。

## 装配与绘制

前面几节的产出，到了绘制这一步要按次序挂到立即上下文上。立即上下文就是咱们往 GPU 发绘制命令的那个接口对象，它的正事归设备篇。下面的代码里还有两位不是本篇造的：`vertexBuffer` 是顶点缓冲，怎么创建归[资源篇](25_d3d11_resources.md)的正事，咱们此处直接拿来用。`renderTargetView` 是渲染目标的视图，决定的是像素画到哪块纹理上，它的来历也归资源篇的视图一节，它包着的那块后台缓冲则要等交换链篇。D3D11 的上下文是一台状态机，咱们设过的布局、绑过的缓冲全都留着，直到咱们下一次改它，所以装配就是把每一格状态填对，然后咱们喊一声开工：

```cpp
// 1) input assembly: where vertices come from, how to read them
UINT stride = sizeof(Vertex);
UINT offset = 0;
context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
context->IASetInputLayout(inputLayout.Get());
context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

// 2) shaders and their parameters
context->VSSetShader(vertexShader.Get(), nullptr, 0);
context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());  // slot 0 = register(b0)
context->PSSetShader(pixelShader.Get(), nullptr, 0);

// 3) where the pixels land, then draw
context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
context->Draw(3, 0);
```

咱们从头往下过。`IASetVertexBuffers` 的 stride 是每个顶点的字节数，offset 是从缓冲的第几个字节起读，多个顶点缓冲的话各占一个槽。`IASetPrimitiveTopology` 告诉硬件的是：三个顶点算一个三角形。着色器与常量缓冲的绑定各归各的入口，注释里标了槽号与 `register(b0)` 的对应。最后 `Draw(3, 0)` 的意思是拿三个顶点、从 0 号顶点起，GPU 就顺着管线跑起来了。

常量缓冲的更新时机插在装配之前：Map 写好数据之后绑槽位，然后咱们再 Draw。绘制完一帧之后把结果送上窗口的那一步是 Present，属于交换链的正事，也就是下一篇[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)要讲的，您到那边读。

## 下一步

本篇的骨架您最好亲手敲一遍：建设备的代码从书架的[D3D11 初始化一篇](../native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md)取，配上本篇的 VS/PS 对、输入布局和一个会动的世界矩阵，咱们开着调试层跑。这套骨架笔者没有编译验证过，跑不通的地方多半在链接库（d3d11.lib 和 d3dcompiler.lib 都要）和 HLSL 文件的路径上，报错文本都会递到您手里。等三角形转起来了，本篇的事就到头了。它画的东西怎么上窗、怎么不掉帧，咱们留给交换链那篇接着讲。
