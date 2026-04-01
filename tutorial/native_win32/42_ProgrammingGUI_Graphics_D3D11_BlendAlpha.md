# 通用GUI编程技术——图形渲染实战（四十二）——混合与透明渲染：从Alpha混合到粒子系统

> 在上一篇文章中，我们用 Phong 光照模型让立方体有了真实的明暗感——环境光打底、漫反射计算亮度、镜面高光带来那一道闪亮。但到目前为止，我们画的所有东西都是不透明的：后画的覆盖先画的，深度缓冲帮我们解决遮挡问题，世界就这么简单。
>
> 现实世界可没这么简单。窗户玻璃是半透明的，烟雾和火焰有透明的部分，粒子特效的每一颗粒子边缘都要和背景平滑融合。今天我们要解决的就是 Alpha 混合——如何让 GPU 把"新颜色"和"旧颜色"按比例混合在一起，以及透明渲染带来了哪些不透明的渲染永远不会遇到的坑。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11（链接 `d3d11.lib`、`d3dcompiler.lib`）
- **前置知识**: 文章 41（光照模型基础），文章 40（深度缓冲与 3D 变换）

## 理解 Alpha 混合的数学本质

在开始写代码之前，我们必须先搞清楚 Alpha 混合到底在数学上做了什么。D3D11 的混合阶段（Output Merger）是渲染管线的最后一个阶段——像素着色器输出一个颜色，混合阶段把这个颜色和渲染目标上已有的颜色进行组合。组合的公式是：

```
finalColor = srcColor * srcBlendFactor + dstColor * dstBlendFactor
```

其中 `srcColor` 是像素着色器输出的颜色（源颜色），`dstColor` 是渲染目标上当前的颜色（目标颜色），`srcBlendFactor` 和 `dstBlendFactor` 是混合因子，它们决定了源和目标各自的权重。

最经典的 Alpha 混合配置是让源颜色乘以源 Alpha，目标颜色乘以 1 减去源 Alpha：

```
finalColor = srcColor * srcAlpha + dstColor * (1 - srcAlpha)
```

这就是所谓的"标准透明混合"。当 `srcAlpha = 1.0` 时，最终颜色就是源颜色（完全不透明）；当 `srcAlpha = 0.0` 时，最终颜色就是目标颜色（完全透明）；当 `srcAlpha = 0.5` 时，源和目标各占一半。

D3D11 还允许对 Alpha 通道单独配置混合公式，但在绝大多数场景下，Alpha 通道的混合和颜色通道保持一致就够了。需要注意的是，D3D11 用 `D3D11_BLEND_DESC` 结构体来描述混合状态，它可以为每个渲染目标独立配置混合参数——如果你有多个渲染目标（MRT），灵活性是有的，但在我们的场景中只用一个渲染目标。

## 第一步——创建透明混合状态

在 D3D11 中，混合行为由 `ID3D11BlendState` 对象控制。默认情况下，D3D11 使用的是不透明的混合模式——也就是源颜色直接覆盖目标颜色。要让透明混合生效，我们需要创建一个自定义的混合状态并绑定到管线。

创建混合状态的代码如下：

```cpp
D3D11_BLEND_DESC blendDesc = {};
blendDesc.RenderTarget[0].BlendEnable = TRUE;

// 源颜色 = 源颜色 * 源Alpha
blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
// 目标颜色 = 目标颜色 * (1 - 源Alpha)
blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
// 颜色混合操作：加法
blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

// Alpha通道的混合：同样使用标准公式
blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

ComPtr<ID3D11BlendState> pBlendState;
HRESULT hr = g_pDevice->CreateBlendState(&blendDesc, &pBlendState);
if (FAILED(hr))
{
    // 创建失败
    return false;
}
```

我们来逐字段解读一下。`BlendEnable = TRUE` 打开混合，这是最基本的开关。`SrcBlend = D3D11_BLEND_SRC_ALPHA` 表示源颜色乘以源 Alpha 值，`DestBlend = D3D11_BLEND_INV_SRC_ALPHA` 表示目标颜色乘以 `1 - srcAlpha`，`BlendOp = D3D11_BLEND_OP_ADD` 表示两者相加。这三个字段组合起来就实现了前面提到的标准透明混合公式。

Alpha 通道的混合参数略有不同——`SrcBlendAlpha = D3D11_BLEND_ONE` 意味着源 Alpha 直接保留（不乘以任何系数），目标 Alpha 乘以 `1 - srcAlpha`。这样做的原因是 Alpha 通道代表的是"透明度"，我们希望新的 Alpha 值反映的是混合后的实际透明度。

创建好混合状态后，在渲染时绑定它：

```cpp
// 绑定混合状态（最后两个参数是混合因子和采样掩码，通常用默认值）
float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
g_pContext->OMSetBlendState(pBlendState.Get(), blendFactor, 0xFFFFFFFF);
```

`OMSetBlendState` 是 Output Merger 阶段设置混合状态的函数。`blendFactor` 用于某些特殊的混合模式（比如 `D3D11_BLEND_BLEND_FACTOR`），在标准透明混合中用不到，传全零就行。最后一个参数是采样掩码，用于多重采样（MSAA）场景，全 1 表示所有采样点都参与。

当你需要恢复到不透明渲染时，只需传 `nullptr` 作为混合状态：

```cpp
g_pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
```

根据 [Microsoft Learn - ID3D11DeviceContext::OMSetBlendState](https://learn.microsoft.com/en-us/windows/win32/api/d3d10/nf-d3d10-id3d10devicecontext-omsetblendstate) 的文档，传 `nullptr` 会恢复到默认混合状态——也就是不透明的直接覆盖模式。

## 第二步——理解透明物体的渲染顺序问题

混合状态准备好了，但如果直接拿来渲染，你很可能会遇到一个非常诡异的现象：从某些角度看，透明物体遮挡了后面的不透明物体；换一个角度，该被透明物体遮挡的东西反而跑到了前面。

这个问题的根源在于渲染顺序。在不透明渲染中，深度缓冲完美地解决了遮挡问题——不管你按什么顺序画，近的总是覆盖远的。但 Alpha 混合不一样——它依赖目标颜色（渲染目标上已有的颜色）来计算最终结果。如果你先画了前面的透明物体，渲染目标上还没有后面的不透明物体的颜色，混合的结果就是透明物体和清屏颜色（通常是黑色或深蓝色）的混合，而不是和背景物体的混合。

所以透明渲染有一个铁律：**先画所有不透明物体，然后从远到近（Back-to-Front）画所有透明物体**。

具体来说，正确的渲染流程是这样的：

1. 开启深度测试和深度写入，正常渲染所有不透明物体。
2. 对所有透明物体按距离摄像机的远近排序，远的排前面。
3. 开启混合，**关闭深度写入**（但保持深度测试开启），按排序后的顺序逐个渲染透明物体。

为什么要关闭深度写入？因为如果透明物体写入了深度值，那么排在它后面的透明物体（虽然也应该是半透明的）在深度测试时会被判定为"被遮挡"而丢弃，结果就是你看不到后面的透明物体了。关闭深度写入确保了所有透明物体都能参与混合，但保持深度测试开启确保了透明物体仍然会被不透明物体正确遮挡。

在代码中，这需要为透明物体创建一个单独的深度模板状态：

```cpp
// 透明物体使用的深度状态：测试开启，写入关闭
D3D11_DEPTH_STENCIL_DESC transparentDepthDesc = {};
transparentDepthDesc.DepthEnable = TRUE;
transparentDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // 禁止深度写入
transparentDepthDesc.DepthFunc = D3D11_COMPARISON_LESS;

ComPtr<ID3D11DepthStencilState> pTransparentDepthState;
g_pDevice->CreateDepthStencilState(&transparentDepthDesc, &pTransparentDepthState);
```

渲染时切换深度状态：

```cpp
// 先画不透明物体（默认深度状态）
RenderOpaqueObjects();

// 切换到透明深度状态
g_pContext->OMSetDepthStencilState(pTransparentDepthState.Get(), 1);

// 开启混合
g_pContext->OMSetBlendState(pBlendState.Get(), blendFactor, 0xFFFFFFFF);

// 画透明物体（已排序）
RenderTransparentObjects();

// 恢复默认
g_pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
g_pContext->OMSetDepthStencilState(nullptr, 1);
```

## 预乘 Alpha vs 直通 Alpha

在讨论 Alpha 混合时，有一个容易忽略但影响重大的概念：预乘 Alpha（Premultiplied Alpha）和直通 Alpha（Straight Alpha）。

直通 Alpha 是我们最直觉的理解方式——颜色是 RGB，透明度是独立的 Alpha 通道。一个半透明的红色像素可能是 `(1.0, 0.0, 0.0, 0.5)`，意思是"纯红色，50% 透明"。

预乘 Alpha 则要求 RGB 值预先乘以 Alpha 值。同一个半透明红色像素在预乘 Alpha 下应该是 `(0.5, 0.0, 0.0, 0.5)`。预乘 Alpha 的混合公式变成了：

```
finalColor = srcColor + dstColor * (1 - srcAlpha)
```

注意源颜色不再乘以 `srcAlpha` 了——因为它已经被预先乘过了。

预乘 Alpha 有几个重要的好处。首先，它避免了"半透明边缘"的问题。当纹理被采样并缩放时，边缘像素的颜色是相邻像素的插值。如果使用直通 Alpha，一个完全不透明红色像素 `(1.0, 0.0, 0.0, 1.0)` 和一个完全透明像素 `(0.0, 0.0, 0.0, 0.0)` 的中间插值是 `(0.5, 0.0, 0.0, 0.5)`——颜色和 Alpha 都是 0.5，混合时颜色会乘以 0.5 变成 `(0.25, 0.0, 0.0)`，边缘看起来会比预期暗。而预乘 Alpha 的插值结果是 `(0.5, 0.0, 0.0, 0.5)`，混合时直接加到目标上，边缘亮度正确。

其次，预乘 Alpha 允许"加法混合"和"透明混合"用同一套混合参数，不需要在运行时切换混合状态。这在粒子系统中特别有用——火焰粒子的中心可以是完全不透明但边缘逐渐透明，发光粒子的 RGB 值甚至可以超过 Alpha 值，产生"叠加发光"效果。

如果要用预乘 Alpha，混合状态需要调整为：

```cpp
blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;           // 源已经预乘，不再乘Alpha
blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
```

Shader 中也需要在输出前预乘：

```hlsl
float4 PS_Main(PSInput input) : SV_TARGET
{
    float4 color = /* 采样纹理或计算颜色 */;
    color.rgb *= color.a;  // 预乘Alpha
    return color;
}
```

对于这个系列来说，我们先用标准的直通 Alpha 配置（`SrcBlend = SRC_ALPHA`），因为它的直觉更清晰。但当你将来做粒子系统或 2D 精灵渲染时，强烈建议切换到预乘 Alpha。

## 第三步——实现粒子系统

有了混合状态和渲染顺序的知识，我们现在来实现一个最简单的粒子系统——100 个半透明方块，每个有随机位置、随机速度和随机透明度，在 3D 空间中飘动。

### 定义粒子结构体

```cpp
struct Particle
{
    XMFLOAT3 position;     // 世界空间位置
    XMFLOAT3 velocity;     // 运动速度
    XMFLOAT4 color;        // RGBA颜色
    float    life;         // 剩余生命值（秒）
    float    maxLife;      // 初始生命值
};

const int PARTICLE_COUNT = 100;
Particle particles[PARTICLE_COUNT];
```

### 粒子初始化

```cpp
void InitParticles()
{
    std::srand(42);  // 固定种子，方便复现

    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        particles[i].position = XMFLOAT3(
            (std::rand() % 200 - 100) / 20.0f,   // [-5, 5]
            (std::rand() % 200 - 100) / 20.0f,   // [-5, 5]
            (std::rand() % 200 - 100) / 20.0f);  // [-5, 5]

        particles[i].velocity = XMFLOAT3(
            (std::rand() % 100 - 50) / 100.0f,
            (std::rand() % 100 - 50) / 100.0f,
            (std::rand() % 100 - 50) / 100.0f);

        // 随机颜色，带透明度
        float r = (std::rand() % 256) / 255.0f;
        float g = (std::rand() % 256) / 255.0f;
        float b = (std::rand() % 256) / 255.0f;
        float a = 0.3f + (std::rand() % 70) / 100.0f;  // [0.3, 1.0]
        particles[i].color = XMFLOAT4(r, g, b, a);

        particles[i].life = 3.0f + (std::rand() % 40) / 10.0f;  // [3, 7] 秒
        particles[i].maxLife = particles[i].life;
    }
}
```

### 粒子更新

每帧更新粒子位置和生命值，生命值耗尽后重生：

```cpp
void UpdateParticles(float dt)
{
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        // 更新位置
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].position.z += particles[i].velocity.z * dt;

        // 减少生命值
        particles[i].life -= dt;

        // 生命结束时重生
        if (particles[i].life <= 0.0f)
        {
            particles[i].position = XMFLOAT3(
                (std::rand() % 200 - 100) / 20.0f,
                (std::rand() % 200 - 100) / 20.0f,
                (std::rand() % 200 - 100) / 20.0f);

            particles[i].velocity = XMFLOAT3(
                (std::rand() % 100 - 50) / 100.0f,
                (std::rand() % 100 - 50) / 100.0f,
                (std::rand() % 100 - 50) / 100.0f);

            float a = 0.3f + (std::rand() % 70) / 100.0f;
            particles[i].color.w = a;

            particles[i].life = 3.0f + (std::rand() % 40) / 10.0f;
            particles[i].maxLife = particles[i].life;
        }

        // 根据生命值调整透明度（淡出效果）
        float lifeRatio = particles[i].life / particles[i].maxLife;
        particles[i].color.w *= lifeRatio;
    }
}
```

### 按距离排序

透明渲染的关键步骤——将粒子按距离摄像机的远近从远到近排序：

```cpp
void SortParticlesBackToFront(const XMFLOAT3& cameraPos)
{
    std::sort(particles, particles + PARTICLE_COUNT,
        [&cameraPos](const Particle& a, const Particle& b)
        {
            XMVECTOR camVec = XMLoadFloat3(&cameraPos);

            XMVECTOR vecA = XMLoadFloat3(&a.position);
            float distA = XMVectorGetX(XMVector3LengthSq(vecA - camVec));

            XMVECTOR vecB = XMLoadFloat3(&b.position);
            float distB = XMVectorGetX(XMVector3LengthSq(vecB - camVec));

            return distA > distB;  // 远的排在前面
        });
}
```

这里用距离的平方（`XMVector3LengthSq`）而不是距离本身来比较，避免开方运算。因为我们只需要比较远近，不需要精确距离，距离平方的比较结果和距离完全一致。

### 渲染粒子

每个粒子用一个小的 billboard 四边形来渲染。为了让粒子始终面向摄像机，我们需要在 C++ 端根据摄像机方向计算粒子的四个顶点位置：

```cpp
void RenderParticles()
{
    XMFLOAT3 cameraPos = XMFLOAT3(0, 2, -5);  // 与光照文章一致的摄像机位置
    SortParticlesBackToFront(cameraPos);

    // 切换到透明深度状态
    g_pContext->OMSetDepthStencilState(g_pTransparentDepthState.Get(), 1);

    // 开启混合
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pContext->OMSetBlendState(g_pBlendState.Get(), blendFactor, 0xFFFFFFFF);

    // 为每个粒子生成billboard四边形并渲染
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        // 更新CBuffer中的世界矩阵和颜色
        XMMATRIX world = XMMatrixTranslationFromVector(
            XMLoadFloat3(&particles[i].position));
        // 简化处理：粒子不旋转，只是平移一个小方块
        world *= XMMatrixScaling(0.2f, 0.2f, 0.2f);

        PerFrameCB cb = {};
        XMStoreFloat4x4(&cb.worldViewProj,
            XMMatrixTranspose(world * g_viewProj));
        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));

        // 粒子颜色直接作为材质颜色
        cb.particleColor = particles[i].color;

        D3D11_MAPPED_SUBRESOURCE ms;
        g_pContext->Map(g_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, &cb, sizeof(PerFrameCB));
        g_pContext->Unmap(g_pCB, 0);

        // 绘制粒子四边形
        g_pContext->Draw(4, 0);  // 4个顶点
    }

    // 恢复默认状态
    g_pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    g_pContext->OMSetDepthStencilState(nullptr, 1);
}
```

⚠️ 注意这里有一个性能隐患——100 个粒子意味着 100 次 `Draw` 调用和 100 次 CBuffer 更新。在实际项目中，你会用实例化（Instancing）或者动态顶点缓冲来把所有粒子合并到一次 Draw Call 中。但对于学习目的，逐粒子绘制更直观，也更容易理解每一步在做什么。

## 第四步——粒子 Shader

粒子使用的 Shader 和之前的光照 Shader 有所不同——粒子不需要光照计算，只需要输出带 Alpha 的颜色：

```hlsl
cbuffer PerFrameCB : register(b0)
{
    matrix g_WorldViewProj;
    matrix g_World;
    float4 g_ParticleColor;  // 粒子颜色（含Alpha）
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput VS_Main(float3 pos : POSITION)
{
    PSInput output;
    output.position = mul(float4(pos, 1.0f), g_WorldViewProj);
    output.color = g_ParticleColor;
    return output;
}

float4 PS_Main(PSInput input) : SV_TARGET
{
    // 直接输出颜色，Alpha通道由混合阶段处理
    return input.color;
}
```

Shader 非常简单——顶点着色器只做 MVP 变换，像素着色器直接输出颜色。Alpha 混合的"混合"操作不在 Shader 中发生，而是在管线的 Output Merger 阶段由硬件自动执行。这也是为什么混合不需要在 Shader 中写任何特殊代码的原因——你只需要确保输出的颜色包含正确的 Alpha 值，混合状态会完成剩下的工作。

## 完整的渲染循环

把所有部分组合起来，完整的渲染循环如下：

```cpp
void RenderFrame(float dt)
{
    // 更新粒子
    UpdateParticles(dt);

    // 清屏
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    g_pContext->ClearRenderTargetView(g_pRTV, clearColor);
    g_pContext->ClearDepthStencilView(g_pDSV,
        D3D11_CLEAR_DEPTH, 1.0f, 0);

    // ========== 第1步：渲染不透明物体 ==========
    g_pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    g_pContext->OMSetDepthStencilState(nullptr, 1);
    RenderOpaqueObjects();  // 渲染立方体等不透明物体

    // ========== 第2步：渲染透明粒子 ==========
    RenderParticles();

    // Present
    g_pSwapChain->Present(1, 0);
}
```

渲染循环中最重要的结构就是"先不透明，后透明"的两阶段划分。不管你的场景多复杂，这个顺序都是必须的——所有不透明物体必须在透明物体之前渲染完毕。

## ⚠️ 踩坑预警

透明渲染有几个经典的坑，每一个都能让你调试半天。

**坑点一：忘记关闭透明物体的深度写入**

这是最常见也最让人困惑的问题。如果你渲染透明物体时仍然开启了深度写入，那么第一个被渲染的透明粒子会写入深度值，后续所有在这个粒子后面的透明粒子在深度测试时都会失败，结果就是你只能看到最前面的一层粒子——不管它有多透明。这在调试时看起来就像是"粒子消失了"，让人怀疑是不是混合状态没生效，但实际上是深度写入在捣鬼。

**坑点二：渲染 UI 叠加层时忘记关闭深度测试**

当你在 3D 场景上叠加一个 2D 的 UI 层（比如准星、血条、文字）时，如果 UI 的绘制仍然开启了深度测试，UI 可能会被场景中的 3D 物体遮挡。解决方案是在绘制 UI 之前关闭深度测试（`DepthEnable = FALSE`），或者将 UI 的 Z 坐标设为最近的值。

**坑点三：排序错误导致视觉伪影**

Back-to-Front 排序在粒子数量大时（几千个粒子）会带来明显的 CPU 开销。如果你偷懒不排序，粒子之间的混合顺序就是不确定的——从不同角度看效果不一样。一种常见的优化是使用加法混合（`SrcBlend = ONE, DestBlend = ONE`），加法混合不依赖顺序，不需要排序，但它只适合"发光"类型的粒子（火焰、能量效果），不适合需要"半透明遮挡"的场景。

**坑点四：混合状态未恢复导致后续渲染异常**

如果你在渲染透明物体后忘了恢复混合状态，后续的不透明物体会继续使用透明混合——结果是所有东西都变透明了。养成习惯：每次 `OMSetBlendState` 修改混合状态后，在渲染结束后恢复默认。

## 常见问题与调试

### 混合后颜色变暗

检查你的混合参数配置。如果你用了预乘 Alpha 的混合参数（`SrcBlend = ONE`）但 Shader 输出的颜色没有预乘 Alpha，结果就是颜色比预期亮。反过来，如果你用了直通 Alpha 的混合参数（`SrcBlend = SRC_ALPHA`）但 Shader 已经预乘了 Alpha，颜色就会比预期暗。保持混合参数和 Shader 逻辑的一致性。

### 粒子边缘有锯齿

如果你的粒子使用点精灵（Point Sprite）或者简单的方形，边缘会有明显的锯齿。解决方法是在 Shader 中做圆形裁剪：根据片元到中心的距离来计算 Alpha，越靠近边缘 Alpha 越小，实现平滑过渡。

### 大量粒子时排序开销过高

对于上千个粒子，CPU 端的排序可能成为瓶颈。可以考虑以下优化方案：使用 GPU 排序（Compute Shader 中的 Bitonic Sort 或 Radix Sort）、使用与顺序无关的透明度算法（OIT，如 Depth Peeling 或 Weighted Blended OIT）、或者用加法混合完全避免排序。

## 总结

这一篇我们拆解了 D3D11 的 Alpha 混合机制。

从数学上看，混合就是 `src * srcAlpha + dst * (1 - srcAlpha)` 这个简单的线性插值公式。但要让它在实际渲染中正确工作，需要配合正确的渲染顺序（先不透明后透明、透明物体从远到近）和正确的深度状态（透明物体关闭深度写入）。我们创建了一个标准的 Alpha 混合状态（`D3D11_BLEND_DESC`），实现了一个 100 个粒子的简单粒子系统，并且在实践中遇到了透明渲染最典型的几个坑。

我们还讨论了预乘 Alpha 和直通 Alpha 的区别——预乘 Alpha 在边缘插值质量和混合状态灵活性上都有优势，是实际项目中的推荐做法。

到这里，我们的 D3D11 之旅已经覆盖了渲染管线的核心部分——初始化、顶点输入、纹理采样、深度缓冲、光照模型和混合透明。下一篇开始，我们要跨入一个全新的领域：Direct3D 12。这不是一次简单的版本升级，而是一场设计哲学的根本性变革。

---

## 练习

1. 在光照模型的立方体场景中添加 100 个半透明粒子，粒子从立方体周围随机位置飘出，带透明度，按距离排序渲染。观察粒子和不透明立方体之间的遮挡关系是否正确。

2. 修改粒子的混合模式为加法混合（`SrcBlend = D3D11_BLEND_ONE, DestBlend = D3D11_BLEND_ONE`），对比加法混合和标准 Alpha 混合的视觉效果差异。思考为什么加法混合不需要排序。

3. 在 Shader 中为粒子实现圆形裁剪：将粒子从方形变成圆形，边缘平滑过渡（根据片元到中心的距离计算 Alpha）。提示：使用 `discard` 或者将 Alpha 乘以距离衰减因子。

4. 尝试使用预乘 Alpha：修改混合参数为 `SrcBlend = ONE`，同时在 Shader 中预乘颜色（`color.rgb *= color.a`），对比标准 Alpha 混合和预乘 Alpha 在粒子边缘的表现差异。

---

**参考资料**:
- [D3D11_BLEND_DESC structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_blend_desc)
- [ID3D11DeviceContext::OMSetBlendState - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d10/nf-d3d10-id3d10devicecontext-omsetblendstate)
- [ID3D11BlendState interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nn-d3d11-id3d11blendstate)
- [D3D11_BLEND enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_blend)
- [Premultiplied Alpha - Shawn Hargreaves' blog](https://shawnhargreaves.com/blogpremultiplied-alpha.html)
- [Output Merger Stage - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage)
- [Transparency Sorting - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage)
