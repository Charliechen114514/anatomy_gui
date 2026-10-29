# 通用GUI编程技术——图形渲染实战（四十六）——描述符堆与根签名：D3D12资源绑定模型

> 在上一篇文章中，我们聊了D3D12的资源堆管理——如何创建Committed Resource、如何管理上传堆和默认堆、以及资源屏障的使用。但光把资源放进GPU可见的内存还不够，Shader需要知道去哪里找这些资源。D3D12不像D3D11那样帮你自动绑定资源到Shader寄存器，它把这个责任完全交给了开发者。这一篇我们就来拆解D3D12的资源绑定模型：描述符堆（Descriptor Heap）和根签名（Root Signature）。

## 环境说明

在开始之前，先说明一下我们写这篇文章时的开发和测试环境：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.9+)
- **Windows SDK**: 10.0.22621.0 或更高
- **图形API**: Direct3D 12
- **调试层**: 启用D3D12 Debug Layer（开发阶段强烈建议开启）
- **测试GPU**: 支持Feature Level 12_0的独立显卡（NVIDIA/AMD均可）

⚠️ 注意，D3D12的调试层在开发阶段非常关键。如果你还没在代码里启用它，建议加上：

```cpp
// 启用D3D12调试层（仅Debug模式）
#if defined(_DEBUG)
{
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
}
#endif
```

调试层会在控制台输出大量的有用信息，包括描述符绑定错误、根签名不匹配等问题。没有这个工具，排查D3D12的绑定问题简直是在黑暗中摸索。

## 从D3D11到D3D12：资源绑定的范式转变

如果你之前有D3D11的开发经验，那应该对这套流程很熟悉：创建一个Shader Resource View（SRV），然后调用`ID3D11DeviceContext::PSSetShaderResources`把它绑定到像素着色器的某个slot上。一切都很简单，驱动会帮你处理所有底层细节。

D3D12把这个模型彻底颠覆了。在D3D12中，你需要手动管理两样东西：描述符堆（Descriptor Heap）是一块GPU可见的内存区域，里面存放着描述符数组；根签名（Root Signature）则定义了Shader如何访问这些描述符。你可以把描述符堆理解为一个巨大的"名片盒"，每个描述符是一张"名片"，上面写着资源的具体信息（格式、维度、内存位置等）。而根签名则是一张"查找表"，告诉GPU去哪个名片盒的哪个位置找哪张名片。

为什么微软要搞这么复杂？因为性能。D3D11的绑定模型虽然简单，但驱动在幕后做了大量的状态跟踪和验证工作，这些开销在高频率的绘制调用中会变得非常明显。D3D12把控制权交给开发者之后，驱动就不再需要做那些隐式工作了，代价就是我们需要自己理解和维护这套绑定机制。

说实话，我们第一次接触这套体系的时候血压确实有点拉满——概念多、结构体嵌套深、错误信息有时候不太直观。但一旦你理解了核心逻辑，就会发现它其实是非常有条理的。我们一步步来。

## 描述符堆：GPU可见的资源描述数组

描述符（Descriptor）是D3D12中对资源视图的统称。在D3D11中你用的`ID3D11ShaderResourceView`、`ID3D11UnorderedAccessView`、`ID3D11RenderTargetView`等，在D3D12中统一变成了描述符。每个描述符是一个固定大小的数据块，记录了资源的类型、格式、内存位置等元信息。描述符的具体大小取决于GPU硬件，所以我们不能直接操作描述符的内存内容，只能通过API函数来创建和操作它们。

描述符堆就是一块连续的内存区域，用来存放一批描述符。根据[Microsoft Learn的官方文档](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)，D3D12定义了几种不同类型的描述符堆，每种堆只能存放对应类型的描述符。

### 描述符堆的类型

D3D12的描述符堆类型由`D3D12_DESCRIPTOR_HEAP_TYPE`枚举定义：

```cpp
typedef enum D3D12_DESCRIPTOR_HEAP_TYPE {
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV = 0,  // 常量缓冲区视图、着色器资源视图、无序访问视图
    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,            // 采样器
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,                // 渲染目标视图
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,                // 深度模板视图
    D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES
} D3D12_DESCRIPTOR_HEAP_TYPE;
```

这里最常用的就是`D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV`，因为大部分Shader绑定的资源（纹理、常量缓冲区、UAV）都归到这一类。采样器有自己独立的堆类型，这是因为某些GPU硬件上采样器的数量和布局与普通资源描述符不同。RTV和DSV通常不需要Shader直接访问，它们有各自的堆类型。

### Shader可见与非Shader可见

描述符堆有一个关键标志位`D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`。只有设置了这个标志的堆才能被Shader直接访问。RTV和DSV堆不需要这个标志，因为它们只在管线绑定时使用，不直接暴露给Shader。

这个区别很重要。Shader可见的描述符堆会占用GPU的一块专用地址空间，而且某些硬件上数量是有限的。所以不要无脑创建大量的Shader可见堆。通常一个CBV/SRV/UAV堆和一个Sampler堆就够用了，我们在堆中按偏移分配不同的区域给不同的绘制调用。

### 创建描述符堆

我们来看一下如何创建一个CBV/SRV/UAV类型的Shader可见描述符堆。根据[Microsoft Learn的文档](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)，`CreateDescriptorHeap`函数接受一个`D3D12_DESCRIPTOR_HEAP_DESC`结构体：

```cpp
// 描述符堆描述信息
D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.NumDescriptors = 128;  // 预分配128个描述符槽位
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
heapDesc.NodeMask = 0;  // 单GPU设置为0

ComPtr<ID3D12DescriptorHeap> pHeap;
HRESULT hr = pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHeap));
if (FAILED(hr))
{
    // 处理创建失败
    return false;
}
```

这里有几个要点值得展开说说。`NumDescriptors`是预分配的描述符数量，这个数量决定了堆的大小。你不需要精确计算到每个描述符，但要确保不会用超了——用超了的行为是未定义的，轻则画面错误，重则驱动崩溃。`NodeMask`在多GPU（LDA节点）场景下使用，单GPU设置为0即可。

### 描述符句柄与偏移

描述符堆创建好之后，我们需要通过句柄（Handle）来访问其中的描述符。D3D12定义了两种句柄类型：`D3D12_CPU_DESCRIPTOR_HANDLE`用于CPU端操作（比如创建描述符），`D3D12_GPU_DESCRIPTOR_HANDLE`用于GPU端操作（比如在命令列表中绑定描述符表）。

获取堆的起始句柄：

```cpp
D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pHeap->GetCPUDescriptorHandleForHeapStart();
D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pHeap->GetGPUDescriptorHandleForHeapStart();
```

这里有一个非常容易踩的坑：描述符的大小不是固定的，它取决于GPU硬件。所以你不能简单地把句柄的指针偏移`sizeof(某个结构体)`来访问下一个描述符。正确的做法是使用`GetDescriptorHandleIncrementSize`获取增量：

```cpp
UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

然后通过一个辅助函数来计算第N个描述符的句柄：

```cpp
// 内联辅助函数：获取堆中第index个描述符的CPU句柄
inline D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(
    ID3D12DescriptorHeap* pHeap, UINT index, UINT descriptorSize)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = pHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * descriptorSize;
    return handle;
}
```

⚠️ 注意，这个`descriptorSize`一定要在创建描述符时使用正确的堆类型来查询。CBV_SRV_UAV堆的增量大小和Sampler堆的增量大小可能不同。如果混用了，你会得到错误的偏移，GPU读到的就是垃圾数据。

### 在堆中创建描述符

有了句柄之后，我们就可以在堆的特定位置创建描述符了。以创建一个CBV（常量缓冲区视图）为例：

```cpp
// 假设我们有一个常量缓冲区资源 pConstantBuffer
D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
cbvDesc.BufferLocation = pConstantBuffer->GetGPUVirtualAddress();
cbvDesc.SizeInBytes = (constantBufferSize + 255) & ~255;  // 必须对齐到256字节

// 在堆的第0个位置创建CBV
D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = GetDescriptorHandle(pHeap.Get(), 0, descriptorSize);
pDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);
```

常量缓冲区的大小必须对齐到256字节这个限制是D3D12硬性规定的，和D3D11不同。如果你的缓冲区实际数据只有64字节，你也得分配256字节。这一点在[Microsoft Learn的CBV文档](https://learn.microsoft.com/en-us/windows/win32/direct3d12/constant-buffer-view)中有明确说明。

SRV的创建类似，但描述信息不同：

```cpp
// 假设我们有一个2D纹理资源 pTexture
D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Texture2D.MipLevels = 1;
srvDesc.Texture2D.MostDetailedMip = 0;
srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

// 在堆的第1个位置创建SRV
D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = GetDescriptorHandle(pHeap.Get(), 1, descriptorSize);
pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, srvHandle);
```

## 根签名：定义Shader资源布局

现在描述符堆里有了数据，但GPU还不知道怎么去找这些描述符。这就是根签名要做的事情。根签名定义了Shader可以访问哪些资源以及如何访问它们。根据[Microsoft Learn的根签名概述](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)，你可以把它类比为D3D11中的输入布局加上资源绑定slot的总和——不过功能更强大也更复杂。

### 根签名的三种参数类型

根签名中可以包含三种类型的参数，每种参数有不同的性能特征：

**Root Constants（根常量）**是最直接的。它直接在根签名中嵌入32位常量值，不需要任何描述符。根签名的空间有限（64个DWORD），但根常量访问速度最快，因为它们直接存储在根签名中，Shader可以像访问寄存器一样立即访问它们。适合传递一些小型参数，比如MVP矩阵的索引、材质ID等。

**Root Descriptor（根描述符）**直接在根签名中写入一个资源的GPU虚拟地址。访问速度也很快（只消耗2个DWORD），但只能用于CBV、SRV和UAV（不支持采样器），并且只支持Buffer类型的资源（不支持Texture）。这意味着你不能用根描述符来直接绑定纹理。

**Descriptor Table（描述符表）**指向描述符堆中的一个连续范围。它只消耗1个DWORD的根签名空间，但它的间接寻址意味着多一次内存读取。好处是它可以一次绑定一批描述符，而且支持所有类型的资源（包括纹理和采样器）。

### 根签名的空间限制

根签名有一个硬性限制：总共不超过64个DWORD的空间。每种参数类型占用的空间如下：

- Root Constants：每个32位常量占1个DWORD
- Root Descriptor：每个占2个DWORD（存储64位GPU虚拟地址）
- Descriptor Table：每个占1个DWORD（存储到描述符堆的偏移）

这意味着如果你全部用Root Constants，最多可以放64个32位常量（相当于16个float4或者4个4x4矩阵）。如果全部用Descriptor Table，最多可以定义64个表。实际使用中我们会混合使用这三种类型，根据具体的性能需求来选择。

### 构建根签名描述结构

我们现在来构建一个实际可用的根签名，它包含一个CBV、一个SRV和一个静态采样器。这个根签名足够用来渲染一个带纹理的矩形——一个典型的2D GUI场景。

首先定义Descriptor Table的范围：

```cpp
// 定义描述符范围：1个CBV
D3D12_DESCRIPTOR_RANGE cbvRange = {};
cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
cbvRange.NumDescriptors = 1;
cbvRange.BaseShaderRegister = 0;        // 对应HLSL中的 register(b0)
cbvRange.RegisterSpace = 0;
cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
```

`D3D12_DESCRIPTOR_RANGE`结构体定义了描述符表中的一段连续范围。`RangeType`指定了这段范围的类型（CBV、SRV、UAV或Sampler），`NumDescriptors`是这个范围中的描述符数量，`BaseShaderRegister`是这段范围在HLSL中对应的起始寄存器编号。`OffsetInDescriptorsFromTableStart`设为`D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND`表示紧跟上一个范围排列，这样我们不需要手动计算偏移。

接下来定义SRV的范围：

```cpp
// 定义描述符范围：1个SRV
D3D12_DESCRIPTOR_RANGE srvRange = {};
srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
srvRange.NumDescriptors = 1;
srvRange.BaseShaderRegister = 0;        // 对应HLSL中的 register(t0)
srvRange.RegisterSpace = 0;
srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
```

然后我们把这两个范围组合到根参数中：

```cpp
// 根参数[0]：描述符表，包含CBV + SRV
D3D12_ROOT_PARAMETER rootParameters[2] = {};

// 第一个根参数是一个描述符表，包含上面定义的两个范围
D3D12_DESCRIPTOR_RANGE ranges[2] = { cbvRange, srvRange };
rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootParameters[0].DescriptorTable.NumDescriptorRanges = 2;
rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;
rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
```

这里`ShaderVisibility`设为`D3D12_SHADER_VISIBILITY_ALL`表示所有Shader阶段都可以访问这个参数。如果你的CBV只在顶点着色器使用，可以设为`D3D12_SHADER_VISIBILITY_VERTEX`来优化。不过对于GUI渲染这种简单场景，ALL就够了。

接下来我们再加一个采样器参数。采样器也可以放在描述符表中，但更方便的做法是使用静态采样器。静态采样器直接嵌入根签名中，不需要在描述符堆中分配空间，也不需要在运行时创建。对于不频繁修改的采样器状态（比如最常用的线性采样），静态采样器是最简洁的选择：

```cpp
// 静态采样器
D3D12_STATIC_SAMPLER_DESC staticSampler = {};
staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
staticSampler.MipLODBias = 0.0f;
staticSampler.MaxAnisotropy = 1;
staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
staticSampler.MinLOD = 0.0f;
staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
staticSampler.ShaderRegister = 0;       // 对应HLSL中的 register(s0)
staticSampler.RegisterSpace = 0;
staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
```

最后，组装根签名描述：

```cpp
// 根签名描述
D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 1;                       // 1个根参数（描述符表）
rootSigDesc.pParameters = rootParameters;
rootSigDesc.NumStaticSamplers = 1;                    // 1个静态采样器
rootSigDesc.pStaticSamplers = &staticSampler;
rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
```

`Flags`中的`ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`是必须的，如果你使用了输入装配器（IA）来提供顶点数据的话。对于GUI渲染，我们通常需要这个标志。

### 序列化与创建根签名

根签名描述结构定义好之后，我们需要先把它序列化成二进制格式，然后才能创建`ID3D12RootSignature`对象。根据[Microsoft Learn的文档](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12serializerootsignature)，序列化使用`D3D12SerializeRootSignature`函数：

```cpp
// 序列化根签名
ComPtr<ID3DBlob> pSignatureBlob;
ComPtr<ID3DBlob> pErrorBlob;
HRESULT hr = D3D12SerializeRootSignature(
    &rootSigDesc,
    D3D_ROOT_SIGNATURE_VERSION_1,
    &pSignatureBlob,
    &pErrorBlob
);
if (FAILED(hr))
{
    if (pErrorBlob)
    {
        OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
    }
    return false;
}

// 创建根签名对象
ComPtr<ID3D12RootSignature> pRootSignature;
hr = pDevice->CreateRootSignature(
    0,  // nodeMask
    pSignatureBlob->GetBufferPointer(),
    pSignatureBlob->GetBufferSize(),
    IID_PPV_ARGS(&pRootSignature)
);
if (FAILED(hr))
{
    return false;
}
```

⚠️ 注意，如果根签名的描述结构有任何问题（比如范围重叠、寄存器冲突等），`D3D12SerializeRootSignature`会失败，并且错误信息会通过`pErrorBlob`返回。调试时一定要打印这个错误信息，它会告诉你具体哪里配置不对。很多人（包括我们）在这里栽过跟头，明明一个寄存器编号写错了，结果花了好几个小时排查。

### HLSL中嵌入根签名

除了在C++中创建根签名，你还可以直接在HLSL Shader代码中通过`[RootSignature(...)]`属性来定义。这种方式的好处是根签名和Shader代码放在一起，不容易出错。但缺点是你需要维护两个版本的根签名定义（HLSL和C++）保持一致，实际项目中通常会选择其中一种方式统一使用。

HLSL嵌入方式如下：

```hlsl
#define MyRootSignature \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "DescriptorTable(CBV(b0), SRV(t0)), " \
    "StaticSampler(s0)"

cbuffer Constants : register(b0)
{
    float4x4 g_mvp;
    float4 g_color;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

[RootSignature(MyRootSignature)]
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.uv) * g_color;
}
```

这种字符串格式的根签名定义非常紧凑，可读性也不错。但要注意，如果你在C++端也创建了根签名并绑定到命令列表，两者必须兼容——也就是说HLSL中定义的根签名布局必须能覆盖C++端绑定的所有资源。

## 在命令列表中绑定资源

根签名和描述符堆都创建好之后，我们还需要在渲染时将它们绑定到命令列表上。这个过程分三步：设置根签名、设置描述符堆、设置根描述符表。

### 设置根签名

在记录命令之前，首先要告诉命令列表使用哪个根签名：

```cpp
pCommandList->SetGraphicsRootSignature(pRootSignature.Get());
```

这个调用必须在设置描述符表之前完成。如果你在设置根签名之前就调用了`SetGraphicsRootDescriptorTable`，调试层会报错。

### 设置描述符堆

接下来告诉命令列表当前活跃的描述符堆是哪些：

```cpp
ID3D12DescriptorHeap* ppHeaps[] = { pHeap.Get() };
pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
```

这里有个细节：`SetDescriptorHeaps`会替换之前设置的所有堆，而不是追加。所以如果你同时需要CBV_SRV_UAV堆和Sampler堆，需要一次性把它们都传进去：

```cpp
ID3D12DescriptorHeap* ppHeaps[] = {
    pCbvSrvUavHeap.Get(),
    pSamplerHeap.Get()   // 如果你使用动态采样器堆而不是静态采样器
};
pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
```

### 设置根描述符表

最后，将描述符堆中的具体位置绑定到根签名的对应参数槽：

```cpp
// 根参数[0]是描述符表，绑定到堆的起始位置
D3D12_GPU_DESCRIPTOR_HANDLE tableHandle =
    pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
pCommandList->SetGraphicsRootDescriptorTable(0, tableHandle);
```

`SetGraphicsRootDescriptorTable`的第一个参数是根参数的索引（0对应我们定义的第一个根参数），第二个参数是GPU描述符句柄，指向堆中描述符表的起始位置。GPU会根据根签名中定义的范围，从这个起始位置开始读取对应数量的描述符。

在我们的例子中，根参数[0]的描述符表包含了两个范围（1个CBV + 1个SRV），所以GPU会从`tableHandle`开始连续读取2个描述符。第一个是CBV（对应`register(b0)`），第二个是SRV（对应`register(t0)`）。

### 完整的渲染命令序列

把上面所有步骤串起来，一个典型的渲染流程的命令记录如下：

```cpp
// 重置命令列表
pCommandList->Reset(pCommandAllocator.Get(), pPipelineState.Get());

// 设置根签名（必须在设置描述符表之前）
pCommandList->SetGraphicsRootSignature(pRootSignature.Get());

// 设置描述符堆
ID3D12DescriptorHeap* ppHeaps[] = { pCbvSrvUavHeap.Get() };
pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

// 绑定描述符表到根参数[0]
CD3DX12_GPU_DESCRIPTOR_HANDLE tableHandle(
    pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
pCommandList->SetGraphicsRootDescriptorTable(0, tableHandle);

// 设置视口、裁剪矩形
pCommandList->RSSetViewports(1, &viewport);
pCommandList->RSSetScissorRects(1, &scissorRect);

// 设置渲染目标
pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

// 资源屏障：从Present切换到RenderTarget
CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    pRenderTarget.Get(),
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_RENDER_TARGET);
pCommandList->ResourceBarrier(1, &barrier);

// 清除渲染目标
pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
pCommandList->ClearDepthStencilView(dsvHandle,
    D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

// 设置顶点缓冲区、图元拓扑
pCommandList->IASetVertexBuffers(0, 1, &vbv);
pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

// 绘制
pCommandList->DrawInstanced(4, 1, 0, 0);  // 4个顶点的矩形

// 资源屏障：从RenderTarget切换到Present
barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    pRenderTarget.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PRESENT);
pCommandList->ResourceBarrier(1, &barrier);

// 关闭命令列表
pCommandList->Close();
```

这段代码展示了一个完整的D3D12渲染命令序列。你可以看到，资源绑定的三步（设置根签名、设置描述符堆、设置根描述符表）都集中在命令列表的前半部分。一旦这些状态设置好了，后续的Draw调用就会使用这些绑定的资源。

## 根签名版本1.0 vs 1.1

在D3D12的开发过程中，微软对根签名进行了扩展。根签名1.1在1.0的基础上引入了描述符和数据的静态性标志（Static Sampler之外的改进），允许开发者向驱动提供更多关于描述符使用方式的提示信息，从而让驱动做更激进的优化。

根据[Microsoft Learn的文档](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1)，1.1版本的核心变化在`D3D12_ROOT_DESCRIPTOR_TABLE`和`D3D12_ROOT_DESCRIPTOR`中新增了`D3D12_ROOT_DESCRIPTOR_FLAGS`字段：

```cpp
// 根签名1.1的描述符范围
typedef struct D3D12_DESCRIPTOR_RANGE1 {
    D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
    UINT NumDescriptors;
    UINT BaseShaderRegister;
    UINT RegisterSpace;
    UINT OffsetInDescriptorsFromTableStart;
    D3D12_DESCRIPTOR_RANGE_FLAGS Flags;  // 新增的标志
    UINT TableDescriptorCount;           // 对于非均匀索引的表大小
} D3D12_DESCRIPTOR_RANGE1;
```

`Flags`字段可以设置为以下值的组合：

- `D3D12_DESCRIPTOR_RANGE_FLAG_NONE`：无特殊标志
- `D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE`：描述符可能会在命令列表执行期间被修改（兼容1.0行为）
- `D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE`：描述符指向的数据可能会变化
- `D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE`：数据在执行时保持静态
- `D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC`：数据完全静态

⚠️ 注意版本兼容性。如果你使用`D3D_ROOT_SIGNATURE_VERSION_1_1`来序列化根签名，你需要确保目标系统支持1.1版本。Windows 10 Creators Update（15063）及以后的版本支持根签名1.1。如果你需要兼容更旧的系统，就使用1.0版本。在实际项目中，因为Win10 15063发布于2017年，现在基本不用担心兼容性问题了。但如果你使用了`D3D12SerializeVersionedRootSignature`而不是`D3D12SerializeRootSignature`，它会自动处理版本协商。

对于1.1版本的根签名，序列化时使用的是`D3D12SerializeVersionedRootSignature`而不是`D3D12SerializeRootSignature`。前者可以处理任意版本的根签名，后者只能处理1.0版本。

## 多帧渲染的描述符管理

在实际应用中，我们通常会有多帧同时在GPU中执行（通常2到3帧）。这意味着我们需要为每一帧维护独立的描述符区域，避免一帧还在使用描述符时另一帧覆盖了它。

一个常见的策略是把描述符堆分成多个区域，每帧使用其中一个。假设我们有3帧在飞行（in-flight），堆大小为128个描述符：

```cpp
const UINT FrameCount = 3;
const UINT DescriptorsPerFrame = 40;  // 每帧40个描述符

// 第frameIndex帧的描述符起始位置
UINT frameOffset = frameIndex * DescriptorsPerFrame;
D3D12_CPU_DESCRIPTOR_HANDLE frameCpuHandle =
    pHeap->GetCPUDescriptorHandleForHeapStart();
frameCpuHandle.ptr += frameOffset * descriptorSize;

D3D12_GPU_DESCRIPTOR_HANDLE frameGpuHandle =
    pHeap->GetGPUDescriptorHandleForHeapStart();
frameGpuHandle.ptr += frameOffset * descriptorSize;
```

这种按帧分区的方式简单有效。每帧在自己的区域内创建描述符，绑定时使用对应的GPU句柄。只要帧数不超过预分配的区域大小，就不会出现冲突。

另一个更灵活的方案是使用线性分配器（Linear Allocator），每帧维护一个当前偏移指针，每分配一个描述符就递增指针。帧开始时重置指针。这种方式不需要预先固定每帧的描述符数量，更加灵活。

## ⚠️ 踩坑预警

这一节集中说一下在描述符堆和根签名使用过程中最常见的几个坑。

### 坑1：描述符表范围越界

这是最危险的坑。如果你在根签名中定义了一个描述符表包含2个描述符，但在描述符堆中只在对应位置创建了1个描述符，GPU读取第二个描述符时就会读到未初始化的内存。这种行为是未定义的——可能什么都不显示，可能显示随机图案，也可能在某些硬件上直接导致设备丢失（Device Removed）。

防范方法很简单：确保堆中每个需要的位置都有有效的描述符。我建议在创建堆之后，先用一个循环把所有描述符位置都初始化为一个"空"描述符（比如一个1x1的黑色纹理的SRV），然后再用实际资源覆盖需要的位置。这样即使有遗漏，也不会导致GPU读到完全随机的数据。

### 坑2：忘记SetDescriptorHeaps

`SetDescriptorHeaps`这个调用很容易被遗忘。如果你绑定了描述符表但没有先设置描述符堆，调试层会报一个错误。但如果你禁用了调试层（比如在Release构建中），这个错误是静默的——画面直接就是错误的，你可能要花很长时间才能定位到原因。

我的建议是，把根签名设置、描述符堆设置和描述符表绑定封装成一个辅助函数，每次渲染前统一调用：

```cpp
void BindRootSignatureAndResources(
    ID3D12GraphicsCommandList* pCmdList,
    ID3D12RootSignature* pRootSig,
    ID3D12DescriptorHeap* pHeap,
    D3D12_GPU_DESCRIPTOR_HANDLE tableHandle)
{
    pCmdList->SetGraphicsRootSignature(pRootSig);
    ID3D12DescriptorHeap* ppHeaps[] = { pHeap };
    pCmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    pCmdList->SetGraphicsRootDescriptorTable(0, tableHandle);
}
```

### 坑3：根签名和Pipeline State不匹配

Pipeline State Object（PSO）在创建时需要指定根签名（或者PSO使用的根签名布局必须与命令列表上绑定的根签名兼容）。如果你创建PSO时用了根签名A，但在渲染时绑定了根签名B，GPU的行为是未定义的。

确保PSO创建时指定的根签名和实际绑定的根签名一致。最安全的做法是在PSO的描述结构中显式指定根签名：

```cpp
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = pRootSignature.Get();
// ... 其他字段
```

### 坑4：CBV大小对齐

前面提到了CBV的大小必须对齐到256字节。但容易忽略的是，`D3D12_CONSTANT_BUFFER_VIEW_DESC`的`SizeInBytes`字段也必须是256的倍数。如果你的常量缓冲区结构体大小不是256的倍数，需要向上取整：

```cpp
// 正确的CBV大小计算
constexpr UINT cbvSize = (sizeof(MyConstants) + 255) & ~255;
```

如果你的结构体恰好是256字节，那没问题。但如果只有100字节，你实际上需要分配256字节。这不是浪费，而是硬件的要求。

## 常见问题

**Q: 一个描述符堆能同时绑定CBV_SRV_UAV和Sampler吗？**

不能。每种堆类型只能存放对应类型的描述符。如果你需要同时使用CBV/SRV和Sampler，你需要创建两个独立的堆（一个`CBV_SRV_UAV`类型，一个`SAMPLER`类型），然后通过`SetDescriptorHeaps`同时绑定这两个堆。当然，如果你使用静态采样器，就不需要创建Sampler堆了。

**Q: 根签名的参数索引和HLSL中的register编号是什么关系？**

根参数的索引（传给`SetGraphicsRootDescriptorTable`、`SetGraphicsRoot32BitConstants`等的第一个参数）是你在`D3D12_ROOT_PARAMETER`数组中的顺序。HLSL中的`register(b0)`、`register(t0)`、`register(s0)`则由`BaseShaderRegister`字段决定。两者的编号是独立的——根参数索引0不一定对应register 0。这取决于你如何排列根参数数组。

**Q: 我应该用Root Constants还是Descriptor Table来传常量缓冲区？**

对于小型常量（比如几个float），用Root Constants更高效，因为它避免了额外的内存间接寻址。但对于大块数据（比如MVP矩阵），Descriptor Table更合适，因为根签名空间有限（64个DWORD）。一般经验是：如果数据不超过16个DWORD（64字节），考虑Root Constants；否则用Descriptor Table引用CBV。

**Q: 描述符堆满了怎么办？**

你需要创建一个新的堆。但要注意，每个命令列表在任一时刻只能绑定一个每种类型的Shader可见堆。所以更好的做法是在项目初期就规划好堆的大小，预留足够的空间。DirectXTK12库提供了一个`DescriptorHeap`辅助类来管理描述符的动态分配和回收，值得参考。

## 总结

到这里，我们已经完整地走过了D3D12资源绑定模型的核心部分。从描述符堆的概念和创建，到根签名的三种参数类型，再到具体的绑定流程和多帧管理，我们覆盖了D3D12资源绑定中最关键的知识点。

回头看这个体系，你会发现D3D12的设计哲学非常一致：把控制权交给开发者，让你精确地控制GPU的每一个行为。描述符堆让你自己管理资源描述的GPU内存布局，根签名让你精确地定义Shader如何访问这些资源。虽然学习曲线陡峭，但一旦掌握，你就拥有了最大程度的性能优化空间。

在下一篇文章中，我们要聊一个更实际的话题——D3D12与D3D11的互操作。在很多实际项目中，你可能需要在D3D12渲染管线中混合使用D2D来绘制UI，或者逐步把一个D3D11项目迁移到D3D12。微软提供了D3D11On12互操作层来支持这种场景，我们下一篇就来拆解它。

## 练习

1. **基础：创建描述符堆并绑定CBV**。创建一个D3D12项目，初始化设备和命令队列，创建一个包含256个描述符的CBV_SRV_UAV堆，并在其中创建一个常量缓冲区视图。验证描述符句柄的偏移是否正确。

2. **进阶：构建完整的根签名**。定义一个根签名，包含一个Descriptor Table（1个CBV + 1个SRV）和一个静态采样器。用`D3D12SerializeRootSignature`序列化后创建根签名对象。写一个简单的像素着色器来验证资源绑定是否正确——比如用常量缓冲区中的颜色值乘以纹理采样值。

3. **挑战：多帧描述符管理**。实现一个3帧并行的描述符管理方案。为每帧在描述符堆中划分独立的区域，确保帧之间的描述符互不干扰。运行你的程序，观察多帧同时执行时资源绑定是否稳定。

4. **调研：根签名1.1的优化效果**。阅读[Microsoft Learn上关于根签名版本1.1的文档](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1)，了解`DESCRIPTORS_VOLATILE`和`DATA_STATIC`标志的含义。思考：在什么场景下，从1.0迁移到1.1能带来可测量的性能提升？

---

**参考资料**:
- [Descriptor Heaps - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
- [Creating Descriptor Heaps - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptor-heaps)
- [ID3D12Device::CreateDescriptorHeap - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
- [ID3D12DescriptorHeap - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12descriptorheap)
- [Root Signatures Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)
- [Creating a Root Signature - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-root-signature)
- [D3D12SerializeRootSignature - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12serializerootsignature)
- [Root Signature Version 1.1 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1)
- [D3D12_ROOT_SIGNATURE_DESC - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_signature_desc)
- [D3D12_ROOT_PARAMETER - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_parameter)
- [Constant Buffer View - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/constant-buffer-view)
