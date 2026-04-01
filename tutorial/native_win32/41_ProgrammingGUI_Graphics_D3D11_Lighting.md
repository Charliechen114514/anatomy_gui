# 通用GUI编程技术——图形渲染实战（四十一）——光照模型基础：Phong光照与法线变换

> 在上一篇文章中，我们渲染了一个旋转的 3D 立方体——每面不同颜色，鼠标可以拖拽旋转。但这看起来还是太"假"了，因为每个面的颜色是纯平的，没有明暗变化。现实世界中，一个物体之所以看起来有立体感，是因为光从不同角度照射，产生了亮面、暗面和阴影。
>
> 今天我们要实现的就是最经典的光照模型——Phong 光照。它由三个分量组成：环境光（Ambient，无处不在的基础亮度）、漫反射（Diffuse，根据光线和表面法线的夹角计算）和镜面高光（Specular，那道闪亮的高光）。掌握了 Phong 光照，你的 3D 场景就会从"彩色方块"升级为"有明暗的立体对象"。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11（链接 `d3d11.lib`、`d3dcompiler.lib`）
- **前置知识**: 文章 40（深度缓冲与 3D 变换）

## Phong 光照模型的三个分量

Phong 光照模型将光照强度分解为三部分：

环境光分量是最基础的光照。现实世界中光线会在墙壁、地面、空气中的微粒上反复反射，最终到达你看到的所有表面。环境光用一个常量来近似这种"无处不在的散射光"：`ambient = k_a * lightColor`，其中 `k_a` 是材质的环境光反射系数（0 到 1），`lightColor` 是环境光颜色。环境光让物体即使在"背光面"也不会完全漆黑。

漫反射分量的核心是 Lambert 余弦定律——表面接收到的光照强度和光线方向与表面法线之间夹角的余弦成正比。当光线垂直照射表面（角度为 0）时最亮，当光线与表面平行（角度为 90 度）时为 0。公式是 `diffuse = k_d * max(dot(N, L), 0) * lightColor`，其中 N 是表面法线（单位向量），L 是从表面指向光源的方向（单位向量），k_d 是材质的漫反射系数。

镜面高光分量模拟的是光滑表面的"高光点"。当观察方向接近光线的反射方向时，你会看到一个亮点。公式是 `specular = k_s * pow(max(dot(R, V), 0), shininess) * lightColor`，其中 R 是光线在法线方向的反射向量，V 是从表面指向摄像机的方向，shininess 控制高光的锐利程度（值越大，高光越小越亮）。

## 修改顶点结构：添加法线

要让 GPU 计算光照，每个顶点需要知道面的朝向——这就是法线（Normal）。法线是一个垂直于表面的单位向量，指向表面"外面"的方向。

```cpp
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};

// 立方体的 24 个顶点（每面 4 个，每面法线不同）
Vertex cubeVertices[] =
{
    // 前面 (z = +0.5f)，法线指向 +Z
    { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0, 0, 1), XMFLOAT4(1,0,0,1) },
    { XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT3(0, 0, 1), XMFLOAT4(1,0,0,1) },
    { XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT3(0, 0, 1), XMFLOAT4(1,0,0,1) },
    { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0, 0, 1), XMFLOAT4(1,0,0,1) },

    // 后面 (z = -0.5f)，法线指向 -Z
    { XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT3(0, 0,-1), XMFLOAT4(0,1,0,1) },
    { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0, 0,-1), XMFLOAT4(0,1,0,1) },
    { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0, 0,-1), XMFLOAT4(0,1,0,1) },
    { XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT3(0, 0,-1), XMFLOAT4(0,1,0,1) },

    // ... 其余 4 面，每面法线指向各自方向
};
```

⚠️ 注意一个关键点：立方体有 8 个几何顶点，但我们需要 24 个顶点（每面 4 个）。因为同一个几何顶点在不同面上法线方向不同——比如位于 (0.5, 0.5, 0.5) 的角点，在前面法线是 (0,0,1)，在右面法线是 (1,0,0)，在上面法线是 (0,1,0)。如果你只提供 8 个顶点共用一条法线，光照结果会完全错误——GPU 会对法线做插值，角落处的法线变成模糊的中间值，每个面看起来都是弧形的。

## 法线变换：逆转置矩阵

这是一个让很多初学者困惑的概念，但理解它至关重要。

当你对物体施加 Model 变换（平移、旋转、缩放）时，顶点位置用 `MVP` 矩阵变换。你可能会想当然地认为法线也用同一个矩阵变换就行了。但在非均匀缩放的情况下，直接用 Model 矩阵变换法线会导致法线方向变形——不再垂直于表面。

假设你把物体沿 X 轴拉伸 2 倍（Model 矩阵的对角线是 (2,1,1)），一个原来 45 度倾斜的表面在变换后变成了更平坦的形状，但如果你用同一个矩阵变换法线，法线会被拉向 X 方向，不再垂直于变换后的表面。

正确的做法是用 Model 矩阵的**逆转置矩阵**来变换法线：`normal_world = mul((float3x3)transpose(inverse(M)), normal_local)`。

在 C++ 端，你需要在 CBuffer 中传入法线矩阵：

```cpp
XMMATRIX M = XMMatrixRotationY(angle);  // Model 矩阵
XMMATRIX N = XMMatrixTranspose(XMMatrixInverse(nullptr, M));  // 法线矩阵

cb.normalMatrix = N;  // 传入 CBuffer
```

如果 Model 矩阵只包含旋转和均匀缩放（没有非均匀缩放），法线矩阵和 Model 矩阵的效果是一样的——你可以直接用 Model 矩阵变换法线。但养成用逆转置矩阵的习惯更安全，因为将来你加了非均匀缩放后不会忘记修改。

## HLSL 光照 Shader

完整的 Phong 光照 Pixel Shader：

```hlsl
cbuffer PerFrameCB : register(b0)
{
    matrix g_WorldViewProj;
    matrix g_World;           // World 矩阵（用于变换法线）
    matrix g_NormalMatrix;    // 法线矩阵（World 的逆转置）
    float3 g_LightPos;        // 光源位置（世界空间）
    float  g_Padding1;
    float3 g_CameraPos;       // 摄像机位置（世界空间）
    float  g_Padding2;
    float3 g_LightColor;      // 光源颜色
    float  g_Padding3;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION1;    // 世界空间位置
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

float4 PS_Main(PSInput input) : SV_TARGET
{
    // 确保法线是单位向量（插值后可能变短）
    float3 N = normalize(input.normal);

    // 光源方向：从表面指向光源
    float3 L = normalize(g_LightPos - input.worldPos);

    // 视线方向：从表面指向摄像机
    float3 V = normalize(g_CameraPos - input.worldPos);

    // 反射方向：光线在法线方向的反射
    float3 R = reflect(-L, N);

    // === 环境光 ===
    float3 ambient = 0.15f * g_LightColor;

    // === 漫反射（Lambert） ===
    float diff = max(dot(N, L), 0.0f);
    float3 diffuse = diff * g_LightColor * input.color.rgb;

    // === 镜面高光（Phong） ===
    float spec = pow(max(dot(R, V), 0.0f), 32.0f);  // shininess = 32
    float3 specular = spec * g_LightColor * 0.5f;    // 高光强度系数 0.5

    // 合成最终颜色
    float3 finalColor = ambient + diffuse + specular;
    return float4(finalColor, 1.0f);
}
```

对应修改 Vertex Shader，将法线从模型空间变换到世界空间：

```hlsl
PSInput VS_Main(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), g_WorldViewProj);
    output.worldPos = mul(float4(input.position, 1.0f), g_World).xyz;
    output.normal   = mul(input.normal, (float3x3)g_NormalMatrix);
    output.color    = input.color;
    return output;
}
```

注意法线变换用的是 `(float3x3)g_NormalMatrix`，不是 `g_WorldViewProj`。法线是方向向量（不需要平移），所以截取为 3x3 矩阵。

## 半程向量优化：Blinn-Phong

原始的 Phong 模型有一个计算上的优化空间：`reflect` 函数和 `pow(dot(R, V))` 在某些角度下计算开销较大。1977 年 Jim Blinn 提出了一个简化：用"半程向量"（Half Vector）替代反射向量。

半程向量是光线方向和视线方向的平均：`H = normalize(L + V)`。然后用 `dot(N, H)` 替代 `dot(R, V)`：

```hlsl
float3 H = normalize(L + V);          // 半程向量
float spec = pow(max(dot(N, H), 0.0f), 32.0f);  // Blinn-Phong
```

Blinn-Phong 的视觉效果和 Phong 略有不同——高光的形状略有变化，但在大多数场景下几乎看不出区别。性能上 Blinn-Phong 更优（少一次 `reflect` 计算），而且在大角度下高光衰减更自然。OpenGL 固定管线的光照模型用的就是 Blinn-Phong。

## CBuffer 数据更新

在渲染循环中，每帧更新 CBuffer 传入最新的矩阵和光源位置：

```cpp
void UpdateConstantBuffer(float angle, float time)
{
    XMMATRIX world = XMMatrixRotationY(angle);
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0, 2, -5, 1),      // 摄像机位置
        XMVectorSet(0, 0, 0, 1),       // 看向原点
        XMVectorSet(0, 1, 0, 1));      // 上方向
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, 800.0f / 600.0f, 0.1f, 100.0f);
    XMMATRIX wvp = world * view * proj;
    XMMATRIX normalMat = XMMatrixTranspose(XMMatrixInverse(nullptr, world));

    PerFrameCB cb = {};
    XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(wvp));
    XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
    XMStoreFloat4x4(&cb.normalMatrix, XMMatrixTranspose(normalMat));

    // 光源绕 Y 轴旋转
    cb.lightPos = XMFLOAT3(
        3.0f * sinf(time),
        2.0f,
        3.0f * cosf(time));
    cb.cameraPos = XMFLOAT3(0, 2, -5);
    cb.lightColor = XMFLOAT3(1, 1, 1);  // 白光

    // Map/Unmap 更新
    D3D11_MAPPED_SUBRESOURCE ms;
    g_pContext->Map(g_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, &cb, sizeof(PerFrameCB));
    g_pContext->Unmap(g_pCB, 0);
}
```

注意光源位置是随时间旋转的（`sinf(time)` 和 `cosf(time)`），这样你能看到光照在立方体表面移动的效果，非常直观。

## 常见问题与调试

### 问题1：光照颜色太暗或太亮

Phong 光照的最终颜色是三个分量之和，如果都乘了 `lightColor` 和 `materialColor`，可能会超过 1.0。D3D11 默认会将超过 1.0 的值钳制到 1.0（饱和处理），不会报错但画面会"过曝"——大面积纯白色。调整各分量的系数使最终值保持在合理范围。

### 问题2：法线看起来不对（背面被照亮）

检查你的法线方向是否指向表面外侧。如果法线指向了表面内侧，`dot(N, L)` 的结果在背面时反而为正，背光面被照亮。确保法线和三角形顶点的绕序一致（逆时针为正面）。

### 问题3：高光出现在错误的位置

最常见的原因是摄像机位置没有正确传入 CBuffer。镜面高光依赖视线方向 V，如果你忘了更新 `g_CameraPos`，V 就是从表面指向错误位置的方向，高光出现在莫名其妙的地方。确保 CBuffer 中的 `cameraPos` 和 `XMMatrixLookAtLH` 的第一个参数一致。

## 总结

Phong 光照模型是实时图形学的入门基础，它用三个简单分量——环境光、漫反射、镜面高光——就能让一个平面着色的立方体变得有立体感。关键的技术要点是法线变换必须用逆转置矩阵（不能直接用 Model 矩阵），以及 Shader 中的法线必须在插值后重新归一化（`normalize`）。

下一步，我们要解决渲染透明物体的问题。目前我们画的所有东西都是不透明的——后画的覆盖先画的。但现实世界有玻璃、烟雾、粒子效果……这些半透明物体怎么渲染？这就涉及到 Alpha 混合和渲染顺序的问题，比光照模型又要复杂一个层次。

---

## 练习

1. 为旋转立方体添加 Phong 光照，实现一个绕立方体旋转的点光源，观察光照在六面上移动的效果。
2. 修改 shininess 值（从 2 到 128），对比高光从小而亮到大而模糊的变化。
3. 实现多光源：添加第二个点光源（不同颜色），在 Shader 中累加两个光源的贡献。
4. 用 Blinn-Phong 替换 Phong 高光计算，对比两者在高角度下的视觉差异。

---

**参考资料**:
- [Phong Reflection Model - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-per-component-math)
- [XMMatrixInverse - DirectXMath - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixinverse)
- [HLSL Intrinsic Functions - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions)
- [Lighting (Direct3D 9) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d9/lighting-mathematics)
