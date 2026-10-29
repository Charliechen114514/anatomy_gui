---
title: 通用GUI编程技术——图形渲染实战（六十一）——2D精灵渲染器：从Sprite Batching到DirectWrite纹理化
---

# 通用GUI编程技术——图形渲染实战（六十一）——2D精灵渲染器：从Sprite Batching到DirectWrite纹理化

> 经过整整六个章节的 D3D11 基础学习——从初始化 Swap Chain 到顶点输入，从纹理采样到深度缓冲，从光照模型到 Alpha 混合——我们现在终于到了一个综合运用所学知识的阶段项目。今天要实现的 2D 精灵渲染器（Sprite Renderer）把前面所有的知识点串联在一起：正交投影、动态顶点缓冲、纹理采样、Alpha 混合，还要引入 D2D/D3D11 互操作和 DirectWrite 文字渲染。
>
> 你可能会问：为什么要做 2D 精灵渲染？答案很简单——2D 精灵无处不在。游戏 UI 的每一个按钮、粒子特效的每一颗粒子、Tile-based 地图的每一个瓦片，本质上都是精灵。而精灵渲染的核心挑战不是"画一个带纹理的矩形"——那太简单了——而是如何高效地同时渲染数百甚至数千个精灵。这就是 Sprite Batching（精灵批处理）要做的事。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022) 或 MinGW-w64
- **图形库**: Direct3D 11 + Direct2D 1.1 + DirectWrite + WIC（链接 `d3d11.lib`、`d2d1.lib`、`dwrite.lib`、`d3dcompiler.lib`、`windowscodecs.lib`）
- **前置知识**: 文章 37-42（D3D11 初始化、顶点输入、纹理采样、深度缓冲、光照、Alpha 混合）

## 第一步——理解 Sprite Batching 的意义

在开始写代码之前，我们必须搞清楚为什么需要 Sprite Batching。

### Draw Call 是性能杀手

GPU 渲染管线的启动是有开销的。每次你调用 `Draw()` 或 `DrawIndexed()`，CPU 都要向 GPU 提交一个"绘制命令"——设置顶点缓冲、设置着色器、设置纹理、设置混合状态……这些操作涉及 CPU-GPU 之间的同步。这个同步的开销，我们称之为 Draw Call 开销。

一个 Draw Call 的 CPU 端开销大约在几微秒到几十微秒之间。如果你每帧画 500 个精灵，每个精灵一个 Draw Call，CPU 端光提交命令就要花掉几毫秒。再加上游戏逻辑、物理模拟、AI 计算等等，你的帧时间预算（16.67ms @ 60FPS）很快就捉襟见肘了。

### 核心思路：按纹理分组，一次 Draw Call 画完

Sprite Batching 的核心思想很简单：**把使用同一个纹理、同一个混合状态的所有精灵合并到同一个顶点缓冲里，用一次 Draw Call 全部画出来**。

具体做法分三步：

1. **收集阶段**：`SpriteBatch::Draw()` 不立即渲染，而是把精灵的参数（位置、大小、UV、颜色、纹理指针）存入一个待渲染列表。
2. **排序分组阶段**：`SpriteBatch::End()` 被调用时，按纹理 ID 对所有精灵排序，把使用同一纹理的精灵排在一起。
3. **提交阶段**：对每个纹理组，把所有精灵的顶点数据写入动态顶点缓冲，绑定纹理，执行一次 `Draw()`。

排序和分组的好处是，GPU 不需要在不同的纹理之间来回切换。纹理切换（`PSSetShaderResources`）虽然比 Draw Call 便宜，但在大量精灵时仍然是可感知的开销。

```
朴素方案：
  精灵1 -> SetTexture(tex1) -> Draw()
  精灵2 -> SetTexture(tex2) -> Draw()
  精灵3 -> SetTexture(tex1) -> Draw()   // 又切回 tex1！
  精灵4 -> SetTexture(tex1) -> Draw()
  ... 500 个精灵 = 500 次 Draw Call

批处理方案：
  精灵排序分组 -> [tex1: 精灵1,3,4,...] [tex2: 精灵2,...]
  SetTexture(tex1) -> Draw(所有 tex1 的精灵)   // 1 次 Draw Call
  SetTexture(tex2) -> Draw(所有 tex2 的精灵)   // 1 次 Draw Call
  ... 2-3 次 Draw Call 搞定！
```

这就是 Sprite Batching 带来的数量级性能提升。

## 第二步——正交投影：2D 渲染的数学基础

在之前的 3D 渲染中，我们使用透视投影（Perspective Projection），近大远小。但 2D 精灵渲染不需要近大远小——屏幕上每个像素对应的世界空间大小应该是完全一致的。这就需要正交投影（Orthographic Projection）。

### 为什么不用透视投影

透视投影会把远处的物体缩小、近处的物体放大。对于一个 2D 游戏来说，所有的精灵都在同一个平面上（Z = 0），如果用透视投影，精灵的视口位置会受到 Z 值的影响——这完全不是我们想要的效果。正交投影则不同：它不做任何透视缩放，只是把世界空间的一个矩形区域"平铺"到屏幕上。

### 屏幕坐标系的约定

在 Windows 的屏幕坐标系中，(0, 0) 在左上角，X 向右增大，Y 向下增大。D3D11 的 `XMMatrixOrthographicOffCenterLH` 函数允许我们指定投影的范围，通过把 top 设为窗口高度、bottom 设为 0，我们就能让投影结果和屏幕坐标系一致：

```cpp
// 创建正交投影矩阵（屏幕坐标系：左上角原点）
XMMATRIX projMatrix = XMMatrixOrthographicOffCenterLH(
    0.0f,           // left:   屏幕左边缘 = 0
    (float)width,   // right:  屏幕右边缘 = 窗口宽度
    (float)height,  // top:    屏幕上边缘 = 窗口高度（注意不是0！）
    0.0f,           // bottom: 屏幕下边缘 = 0
    0.0f,           // nearZ:  近裁剪面
    1.0f            // farZ:   远裁剪面
);
```

这里有个容易搞混的地方：`XMMatrixOrthographicOffCenterLH` 的第三个参数 `top` 和第四个参数 `bottom`。在数学上，投影矩阵的 Y 轴是向上的——bottom 在下方，top 在上方。但我们想要的屏幕坐标是 Y 向下的。所以把 `top` 设为 `height`、`bottom` 设为 `0`，实际上就是在翻转 Y 轴：屏幕坐标 Y=0（顶部）映射到投影空间 Y=height（top），屏幕坐标 Y=height（底部）映射到投影空间 Y=0（bottom）。最终效果就是：屏幕左上角 (0, 0) 对应 NDC 的 (-1, 1)，右下角 (width, height) 对应 NDC 的 (1, -1)。

我们的 MVP 矩阵只需要这个投影矩阵乘上一个单位世界矩阵和视图矩阵（也可以是单位矩阵，因为 2D 不需要相机）：

```cpp
XMMATRIX wvp = XMMatrixIdentity() * projMatrix;  // 2D 不需要 view 矩阵
wvp = XMMatrixTranspose(wvp);  // HLSL 默认列主序，C++ 行主序，需要转置
```

## 第三步——动态顶点缓冲与 SpriteVertex

理解了投影之后，我们需要解决一个关键问题：精灵的顶点数据每帧都在变化（位置、旋转、缩放），怎么高效地把数据传给 GPU？

### 为什么用动态顶点缓冲

静态顶点缓冲（`D3D11_USAGE_DEFAULT`）在创建后就不能被 CPU 修改了。如果每帧要更新顶点数据，你有两个选择：

1. **创建新的顶点缓冲**：每帧 `CreateBuffer` 一次。这是最差的做法——GPU 资源的创建和销毁非常昂贵。
2. **使用动态顶点缓冲**：创建一次，每帧用 `Map/Unmap` 更新内容。

动态顶点缓冲的创建方式：

```cpp
D3D11_BUFFER_DESC vbDesc = {};
vbDesc.ByteWidth = MAX_SPRITES * 4 * sizeof(SpriteVertex);  // 每精灵4顶点
vbDesc.Usage = D3D11_USAGE_DYNAMIC;          // 动态缓冲
vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // CPU 可写
vbDesc.MiscFlags = 0;

ComPtr<ID3D11Buffer> pDynamicVB;
pDevice->CreateBuffer(&vbDesc, nullptr, &pDynamicVB);
```

### Map/Discard 模式

每帧更新动态缓冲时，使用 `D3D11_MAP_WRITE_DISCARD` 标志。这个标志的含义是：**丢弃缓冲的旧内容，返回一块新的内存给 CPU 写入**。GPU 仍在使用旧缓冲渲染上一帧的数据，不会因为 CPU 的写入而阻塞。

```cpp
D3D11_MAPPED_SUBRESOURCE mapped;
HRESULT hr = pContext->Map(
    pDynamicVB.Get(),
    0,                          // 子资源索引（顶点缓冲只有0）
    D3D11_MAP_WRITE_DISCARD,    // 丢弃旧数据
    0,                          // 无标志
    &mapped                     // 输出映射后的内存指针
);
if (SUCCEEDED(hr))
{
    // 把所有精灵的顶点数据写入 mapped.pData
    memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(SpriteVertex));

    // 写完后必须 Unmap！
    pContext->Unmap(pDynamicVB.Get(), 0);
}
```

### SpriteVertex 布局

每个精灵由 4 个顶点组成一个四边形，每个顶点的数据是：

```cpp
struct SpriteVertex
{
    XMFLOAT2 position;   // 屏幕空间位置 (x, y)        - 8 字节
    XMFLOAT2 texcoord;   // 纹理坐标 (u, v)            - 8 字节
    XMFLOAT4 color;      // 顶点颜色 RGBA (r, g, b, a) - 16 字节
};                       // 总计: 32 字节/顶点
```

32 字节是一个精心设计的大小——它是 16 字节的整数倍，不会导致 GPU 内存对齐问题。对应的 Input Layout：

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
```

## 第四步——精灵图集与 UV 子区域

### 为什么用图集

如果每个精灵使用一个单独的纹理文件，那么 100 种精灵就需要 100 次纹理切换——Sprite Batching 的优势荡然无存。精灵图集（Sprite Atlas）把多个小精灵打包到一张大纹理上，这样所有使用同一图集的精灵只需要绑定一次纹理。

```
精灵图集（4x4 网格示例）:
+------+------+------+------+
| (0,0)| (1,0)| (2,0)| (3,0)|
| 红色 | 绿色 | 蓝色 | 黄色 |
+------+------+------+------+
| (0,1)| (1,1)| (2,1)| (3,1)|
| 紫色 | 青色 | 橙色 | 粉色 |
+------+------+------+------+
| ...  | ...  | ...  | ...  |
+------+------+------+------+
```

### UV 坐标的计算

假设图集是一个 `cols x rows` 的网格，每个子精灵的 UV 范围可以通过索引计算：

```cpp
// 计算子精灵在图集中的 UV 范围
void CalcSpriteUV(int index, int cols, int rows,
                  float& u0, float& v0, float& u1, float& v1)
{
    int col = index % cols;
    int row = index / cols;
    float cellW = 1.0f / cols;
    float cellH = 1.0f / rows;
    u0 = col * cellW;
    v0 = row * cellH;
    u1 = u0 + cellW;
    v1 = v0 + cellH;
}
```

`SpriteBatch::Draw` 接口接受 UV 参数，允许调用者指定要渲染图集中的哪个子区域：

```cpp
struct SpriteDrawParams
{
    float x, y;              // 精灵中心位置（屏幕坐标）
    float width, height;     // 精灵大小
    float rotation = 0.0f;   // 旋转角度（弧度）
    float originX = 0.5f;    // 旋转中心 X（0~1，0.5 = 中心）
    float originY = 0.5f;    // 旋转中心 Y
    float u0 = 0.0f, v0 = 0.0f;  // UV 左上角
    float u1 = 1.0f, v1 = 1.0f;  // UV 右下角
    XMFLOAT4 color = {1,1,1,1};   // 顶点颜色（白色=不调色）
    ID3D11ShaderResourceView* pTexture = nullptr;  // 纹理 SRV
};
```

## 第五步——DirectWrite 离屏渲染到纹理

这一步是这个项目中最精彩的互操作部分。我们的目标是用 DirectWrite 渲染高质量文字，然后把渲染结果作为精灵纹理使用——这样 FPS 计数器和文字 UI 就能和精灵一起参与批处理。

### D2D/D3D11 互操作路径

D2D 和 D3D11 都基于 DXGI，它们可以通过 DXGI Surface 共享纹理。具体的互操作步骤是：

1. 创建一个 D3D11 纹理，同时绑定 `RENDER_TARGET` 和 `SHADER_RESOURCE`。
2. 从这个纹理获取 DXGI Surface。
3. 用这个 Surface 创建 D2D 渲染目标（`CreateDxgiSurfaceRenderTarget`）。
4. 用 D2D/DirectWrite 在这个渲染目标上画文字。
5. 把 D3D11 纹理作为 Shader Resource View 绑定到 D3D11 管线。

### 创建可共享的 D3D11 纹理

```cpp
D3D11_TEXTURE2D_DESC texDesc = {};
texDesc.Width = 512;
texDesc.Height = 64;
texDesc.MipLevels = 1;
texDesc.ArraySize = 1;
texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // D2D 兼容的格式
texDesc.SampleDesc.Count = 1;
texDesc.BindFlags = D3D11_BIND_RENDER_TARGET    // D2D 要往上面画
                  | D3D11_BIND_SHADER_RESOURCE;  // D3D11 要采样它
texDesc.Usage = D3D11_USAGE_DEFAULT;

ComPtr<ID3D11Texture2D> pTextTexture;
pDevice->CreateTexture2D(&texDesc, nullptr, &pTextTexture);

// 创建 SRV 供 D3D11 采样
ComPtr<ID3D11ShaderResourceView> pTextSRV;
pDevice->CreateShaderResourceView(pTextTexture.Get(), nullptr, &pTextSRV);
```

### 从 D3D11 纹理创建 D2D 渲染目标

```cpp
// 获取 DXGI Surface
ComPtr<IDXGISurface> pSurface;
pTextTexture->QueryInterface(__uuidof(IDXGISurface), &pSurface);

// 创建 D2D 渲染目标
D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,
                       D2D1_ALPHA_MODE_PREMULTIPLIED)  // 关键！
);

ComPtr<ID2D1RenderTarget> pD2DRenderTarget;
pD2D1Factory->CreateDxgiSurfaceRenderTarget(
    pSurface.Get(), &rtProps, &pD2DRenderTarget);
```

### 渲染文字到纹理

```cpp
void TextRenderer::DrawText(const std::wstring& text,
                            float x, float y,
                            const D2D1_COLOR_F& color)
{
    pD2DRenderTarget->BeginDraw();

    // 清除为透明（否则上一帧的文字会残留）
    pD2DRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    // 创建文字画刷
    ComPtr<ID2D1SolidColorBrush> pBrush;
    pD2DRenderTarget->CreateSolidColorBrush(color, &pBrush);

    // 使用 DirectWrite 绘制
    pD2DRenderTarget->DrawText(
        text.c_str(),
        (UINT32)text.length(),
        pTextFormat.Get(),     // IDWriteTextFormat
        D2D1::RectF(x, y, 512, 64),
        pBrush.Get()
    );

    pD2DRenderTarget->EndDraw();
}
```

### 为什么用 D2D1_ALPHA_MODE_PREMULTIPLIED

这里有一个关键的细节：D2D 渲染目标使用的是 `D2D1_ALPHA_MODE_PREMULTIPLIED`（预乘 Alpha）。

D2D 内部渲染时，文字抗锯齿边缘的像素颜色是预乘过的。例如一个白色文字的半透明边缘像素，在预乘 Alpha 下是 `(0.5, 0.5, 0.5, 0.5)` 而不是 `(1.0, 1.0, 1.0, 0.5)`。如果我们用 `D2D1_ALPHA_MODE_STRAIGHT`，D2D 仍然会输出预乘格式，但混合阶段假设的是直通格式，结果就是文字边缘变暗、出现黑色光晕。

所以在 D3D11 端渲染文字精灵时，混合状态要使用预乘 Alpha 的配置：

```cpp
blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;            // 源已预乘
blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
```

## 第六步——完整代码走读

### SpriteBatch 类设计

SpriteBatch 采用经典的 `Begin/Draw/End` 三段式接口，这也是 XNA SpriteBatch 和几乎所有 2D 渲染引擎的标准设计：

```cpp
class SpriteBatch
{
public:
    void Init(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
              int screenWidth, int screenHeight);
    void Begin();   // 开始收集精灵
    void Draw(const SpriteDrawParams& params);  // 提交精灵参数
    void End();     // 排序、分组、渲染

private:
    void FlushBatch(ID3D11ShaderResourceView* pTexture,
                    const std::vector<SpriteVertex>& vertices);
    void EmitQuad(const SpriteDrawParams& params,
                  std::vector<SpriteVertex>& outVertices);

    ComPtr<ID3D11Buffer>          m_pDynamicVB;
    ComPtr<ID3D11VertexShader>    m_pVS;
    ComPtr<ID3D11PixelShader>     m_pPS;
    ComPtr<ID3D11InputLayout>     m_pInputLayout;
    ComPtr<ID3D11BlendState>      m_pBlendState;
    ComPtr<ID3D11SamplerState>    m_pSampler;
    XMMATRIX                      m_projMatrix;

    std::vector<SpriteDrawParams> m_pendingSprites;
    bool m_inBatch = false;
};
```

### EmitQuad：从参数生成四个顶点

`EmitQuad` 是 SpriteBatch 的核心算法。它根据精灵的位置、大小、旋转角度和 UV 参数，计算出四个顶点的屏幕坐标和纹理坐标：

```cpp
void SpriteBatch::EmitQuad(const SpriteDrawParams& p,
                            std::vector<SpriteVertex>& out)
{
    // 半尺寸
    float hw = p.width * 0.5f;
    float hh = p.height * 0.5f;

    // 旋转中心偏移
    float ox = p.originX * p.width;
    float oy = p.originY * p.height;

    // 四个角的局部坐标（相对于旋转中心）
    XMFLOAT2 corners[4] = {
        { -ox,        -oy },        // 左上
        { -ox + p.width, -oy },     // 右上
        { -ox,        -oy + p.height }, // 左下
        { -ox + p.width, -oy + p.height } // 右下
    };

    // UV 坐标
    XMFLOAT2 uvs[4] = {
        { p.u0, p.v0 },   // 左上
        { p.u1, p.v0 },   // 右上
        { p.u0, p.v1 },   // 左下
        { p.u1, p.v1 }    // 右下
    };

    // 旋转 + 平移
    float cosR = cosf(p.rotation);
    float sinR = sinf(p.rotation);

    for (int i = 0; i < 4; i++)
    {
        float lx = corners[i].x;
        float ly = corners[i].y;

        // 2D 旋转
        float rx = lx * cosR - ly * sinR;
        float ry = lx * sinR + ly * cosR;

        SpriteVertex v;
        v.position = { p.x + rx, p.y + ry };
        v.texcoord = uvs[i];
        v.color = p.color;
        out.push_back(v);
    }
}
```

旋转的数学很简单：标准的 2D 旋转矩阵。注意 `origin` 的处理——旋转中心可以不是精灵的中心。当 `originX = 0.5, originY = 0.5` 时，旋转中心在精灵正中央；当 `originX = 0, originY = 0` 时，旋转中心在左上角。

### TextureLoader：程序化生成精灵图集

这个项目不使用外部图片文件，而是用 WIC（Windows Imaging Component）在内存中程序化生成一个精灵图集纹理。每个子精灵是一个带渐变色的彩色方块：

```cpp
// TextureLoader::CreateSpriteAtlas 的核心思路
ComPtr<ID3D11ShaderResourceView> TextureLoader::CreateSpriteAtlas(
    ID3D11Device* pDevice, int cellSize, int cols, int rows)
{
    int texWidth = cellSize * cols;
    int texHeight = cellSize * rows;
    std::vector<uint8_t> pixels(texWidth * texHeight * 4);  // BGRA

    // 为每个单元格生成不同的颜色和图案
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            // 根据索引生成不同的颜色
            int idx = row * cols + col;
            // ... 填充像素数据（渐变、圆形、方形等图案）
        }
    }

    // 创建 D3D11 纹理
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texWidth;
    desc.Height = texHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = texWidth * 4;

    ComPtr<ID3D11Texture2D> pTexture;
    pDevice->CreateTexture2D(&desc, &initData, &pTexture);

    ComPtr<ID3D11ShaderResourceView> pSRV;
    pDevice->CreateShaderResourceView(pTexture.Get(), nullptr, &pSRV);
    return pSRV;
}
```

程序化生成的优势是不需要管理外部资源文件，适合教学演示。

### TextRenderer 初始化

TextRenderer 封装了 D2D/DWrite 的初始化和文字渲染逻辑：

```cpp
class TextRenderer
{
public:
    void Init(ID3D11Device* pDevice);
    void UpdateText(const std::wstring& text, float fontSize = 24.0f);
    ID3D11ShaderResourceView* GetTextureSRV() { return m_pTextSRV.Get(); }
    float GetTextWidth() { return m_textWidth; }

private:
    ComPtr<ID2D1Factory>           m_pD2DFactory;
    ComPtr<IDWriteFactory>         m_pDWriteFactory;
    ComPtr<ID2D1RenderTarget>      m_pD2DRenderTarget;
    ComPtr<IDWriteTextFormat>      m_pTextFormat;
    ComPtr<ID3D11Texture2D>        m_pTextTexture;
    ComPtr<ID3D11ShaderResourceView> m_pTextSRV;
    float m_textWidth = 0;
};
```

初始化时创建 D2D Factory、DWrite Factory、可共享的 D3D11 纹理和 D2D 渲染目标。每次调用 `UpdateText` 时，在 D2D 渲染目标上重新绘制文字，D3D11 端的纹理内容自动更新（因为它们共享同一个 DXGI Surface）。

### Shader

精灵渲染的 Shader 非常简单——顶点着色器只做正交投影变换，像素着色器采样纹理并乘以顶点颜色：

```hlsl
cbuffer SpriteCB : register(b0)
{
    matrix g_WorldViewProj;
};

struct VSInput
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD;
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color    : COLOR;
};

PSInput VS_Main(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 0.0f, 1.0f),
                          g_WorldViewProj);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}

float4 PS_Main(PSInput input) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.texcoord);
    return texColor * input.color;
}
```

注意顶点着色器中 `float4(input.position, 0.0f, 1.0f)`——Z 分量设为 0，因为我们做的是 2D 渲染，所有精灵都在 Z=0 平面上。W 分量设为 1.0 是齐次坐标的标准做法。

### 演示场景

主程序渲染 500 个精灵、一个背景精灵和 FPS 文字，完整地展示了 Sprite Batching 的效果：

```cpp
void RenderFrame()
{
    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    pContext->ClearRenderTargetView(pRTV, clearColor);

    // 更新 FPS 文字
    wchar_t fpsText[64];
    swprintf_s(fpsText, L"FPS: %.0f", currentFPS);
    textRenderer.UpdateText(fpsText);

    // 开始 Sprite Batch
    spriteBatch.Begin();

    // 绘制背景精灵
    SpriteDrawParams bg = {};
    bg.x = screenWidth / 2.0f;
    bg.y = screenHeight / 2.0f;
    bg.width = (float)screenWidth;
    bg.height = (float)screenHeight;
    bg.pTexture = pBackgroundSRV.Get();
    spriteBatch.Draw(bg);

    // 绘制 500 个动态精灵
    for (int i = 0; i < 500; i++)
    {
        SpriteDrawParams sprite = {};
        sprite.x = sprites[i].x;
        sprite.y = sprites[i].y;
        sprite.width = sprites[i].size;
        sprite.height = sprites[i].size;
        sprite.rotation = sprites[i].angle;
        // 从图集中选择子精灵
        CalcSpriteUV(sprites[i].atlasIndex, ATLAS_COLS, ATLAS_ROWS,
                     sprite.u0, sprite.v0, sprite.u1, sprite.v1);
        sprite.color = sprites[i].color;
        sprite.pTexture = pAtlasSRV.Get();  // 都用同一个图集
        spriteBatch.Draw(sprite);
    }

    // 绘制 FPS 文字精灵
    SpriteDrawParams fpsSprite = {};
    fpsSprite.x = 10.0f;
    fpsSprite.y = 10.0f;
    fpsSprite.width = textRenderer.GetTextWidth();
    fpsSprite.height = 64.0f;
    fpsSprite.pTexture = textRenderer.GetTextureSRV();
    fpsSprite.originX = 0.0f;
    fpsSprite.originY = 0.0f;
    spriteBatch.Draw(fpsSprite);

    // 结束批处理——排序分组 + 渲染
    spriteBatch.End();

    pSwapChain->Present(1, 0);
}
```

精灵的动画更新在每帧的 `Update()` 中完成：位置移动、旋转递增、边界反弹等。

## 第七步——性能分析

让我们来量化 Sprite Batching 带来的性能提升。

### 朴素方案 vs 批处理方案

| 指标 | 朴素方案（逐精灵绘制） | Sprite Batching |
|------|----------------------|-----------------|
| Draw Calls / 帧 | 502（500 精灵 + 背景 + FPS） | ~3（背景 + 图集精灵 + 文字） |
| 纹理切换 / 帧 | 502 | 3 |
| CPU 端开销 | ~5ms（Draw Call 提交） | ~0.5ms（排序 + 顶点生成） |
| 帧率（参考值） | 30-40 FPS | 200+ FPS |

为什么是约 3 次 Draw Call 而不是 1 次？因为我们有三组使用不同纹理的精灵：

1. **背景精灵**：使用 `pBackgroundSRV` 纹理，1 次 Draw Call
2. **500 个图集精灵**：都使用 `pAtlasSRV` 纹理，1 次 Draw Call
3. **FPS 文字精灵**：使用 TextRenderer 的纹理，1 次 Draw Call

如果所有精灵都用同一个图集纹理，那就能合并成 2 次 Draw Call 甚至 1 次。这正是图集的威力——纹理越少，Draw Call 越少。

### 排序的代价

对 500 个精灵按纹理 ID 排序的时间可以忽略不计（`std::sort` 在 500 个元素上大约几微秒）。但如果你有上万个小精灵，可以考虑使用桶排序（Bucket Sort）——因为纹理数量通常很少（几个到十几个），直接按纹理 ID 分桶比通用排序快得多。

## ⚠️ 踩坑预警

精灵渲染有几个经典的坑，每一个都可能让你调试半天。

**坑点一：忘记 Unmap 动态缓冲**

如果你在 `Map` 之后忘记调用 `Unmap`，GPU 将永远无法读取这个缓冲——因为 `Map` 把缓冲的访问权交给了 CPU，`Unmap` 才会把访问权交回给 GPU。结果是你的精灵全部消失，但 `Map` 和 `Draw` 都不会报错。养成习惯：`Map` 和 `Unmap` 必须成对出现，就像 `malloc/free` 一样。

**坑点二：D2D Surface 使用了错误的 Alpha 模式**

如果你把 `D2D1_ALPHA_MODE_PREMULTIPLIED` 改成 `D2D1_ALPHA_MODE_STRAIGHT`，文字周围会出现一圈黑色光晕。这是因为 D2D 内部使用预乘 Alpha 渲染抗锯齿边缘，但 D3D11 的混合阶段按直通 Alpha 来混合。预乘 Alpha 的颜色值比直通 Alpha 更大（已乘过 Alpha），直通混合会再乘一次 Alpha，导致边缘变暗。**解决方案**：D2D 渲染目标始终使用 `PREMULTIPLIED`，D3D11 端使用 `SrcBlend = ONE` 的混合状态。

**坑点三：动态缓冲容量溢出**

如果你在创建动态缓冲时设置了 `MAX_SPRITES = 1000`，但某一帧实际提交了 1500 个精灵，顶点数据会写越界——轻则画面错乱，重则访问违规崩溃。解决方案是在 `FlushBatch` 时检查当前批次的顶点数是否超过缓冲容量：

```cpp
if (batchVertices.size() > MAX_VERTICES)
{
    // 重新创建更大的缓冲，或者分批渲染
    RecreateBuffer(batchVertices.size());
}
```

更好的做法是在 `Draw` 中就检查是否即将溢出，如果快满了就先 Flush 当前批次再继续收集。

**坑点四：D2D 渲染目标忘记清除为透明**

`TextRenderer::UpdateText` 中，如果不在 `BeginDraw` 之后先 `Clear` 为透明（`D2D1::ColorF(0,0,0,0)`），上一帧的文字会留在纹理上，新文字叠加在旧文字上方，几帧之后纹理上就变成一团模糊的色块。每帧渲染文字之前，必须先清空纹理。

## 总结

这一篇我们完成了一个完整的 2D 精灵渲染器，它串联了之前 D3D11 章节的所有知识点：

- **Sprite Batching** 通过排序分组把 500 个精灵从 500 次 Draw Call 降到约 3 次，这是 2D 渲染性能优化的基石。
- **正交投影** 用 `XMMatrixOrthographicOffCenterLH(0, width, height, 0, 0, 1)` 建立了与屏幕坐标一致的投影矩阵。
- **动态顶点缓冲** 使用 `D3D11_USAGE_DYNAMIC` + `Map/Discard` 模式高效更新每帧变化的顶点数据。
- **精灵图集** 把多个子精灵打包到一张纹理中，配合 UV 子区域参数实现一次纹理绑定渲染多种精灵。
- **D2D/D3D11 互操作** 通过 `CreateDxgiSurfaceRenderTarget` 在 D3D11 纹理上使用 DirectWrite 渲染文字，实现了高质量文字精灵。

这个项目是一个典型的"阶段总结"——它不是教你一个全新的概念，而是把零散的知识点组装成一个可运行、有实用价值的系统。在真实的游戏引擎和 UI 框架中，精灵渲染器的核心逻辑和这里几乎一模一样，只是规模更大、优化更极致。

---

## 练习

1. **精灵动画支持**：为 `SpriteDrawParams` 添加一个 `animationSpeed` 参数，在主循环中根据时间自动切换图集中的子精灵索引，实现帧动画效果（例如一个角色有 8 帧行走动画，每 100ms 切换一帧）。

2. **按 Y 坐标排序**：在 `SpriteBatch::End()` 中，添加一种排序模式——按精灵的 Y 坐标从小到大排序（Y 越大越后画）。这种排序在等距视角（Isometric）游戏中非常重要，因为它实现了简单的深度遮挡：Y 坐标越大的精灵距离摄像机越近，应该画在越上面。观察排序前后精灵之间的遮挡关系变化。

3. **PNG 文件加载**：使用 WIC（`IWICImagingFactory` -> `CreateDecoderFromFilename` -> `CreateFormatConverter`）加载一个 PNG 文件到内存，创建 D3D11 纹理和 SRV，然后用 `SpriteBatch::Draw` 渲染。提示：参考 TextureLoader 的纹理创建代码，但用 WIC 解码代替程序化生成像素数据。

4. **粒子发射器**：基于 SpriteBatch 实现一个简单的粒子发射器。每帧从发射点生成若干粒子，每个粒子有位置、速度、生命值、大小和透明度。粒子随时间淡出（`color.w` 减小），生命结束后回收。目标：同时渲染 1000+ 粒子仍然保持 60 FPS，验证批处理的性能优势。

---

**参考资料**:
- [XMMatrixOrthographicOffCenterLH - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixorthographicoffcenterlh)
- [D3D11_MAPPED_SUBRESOURCE - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_mapped_subresource)
- [ID2D1RenderTarget::CreateDxgiSurfaceRenderTarget - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createdxgisurfacerendertarget)
- [D2D1_ALPHA_MODE enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dcommon/ne-dcommon-d2d1_alpha_mode)
- [DirectWrite - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directwrite/direct-write-portal)
- [Windows Imaging Component (WIC) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-lh)
- [D3D11_USAGE enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_usage)
- [Sprite Batching in XNA - Shawn Hargreaves' blog](https://shawnhargreaves.com/blog/spritebatch-batching.html)
