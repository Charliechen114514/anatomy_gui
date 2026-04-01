# 通用GUI编程技术——图形渲染实战（三十九）——纹理与采样器：从WIC加载到GPU渲染

> 上一篇文章中，我们用顶点缓冲和输入布局画出了 GPU 的第一个三角形——三个顶点，三种颜色，GPU 自动插值出漂亮的渐变效果。但顶点颜色终究是"程序员画笔"级别的着色方式，想做照片级的真实感渲染，你需要的是纹理映射（Texture Mapping）。
>
> 纹理本质上就是一张图片——或者更准确地说，是一块可以被 GPU 采样的二维数据数组。GPU 通过纹理坐标（UV 坐标，范围 0 到 1）从纹理中取出对应位置的颜色值，然后涂到三角形的每个像素上。今天我们要讲的是：怎么创建纹理、怎么加载图片到纹理、怎么配置采样器，以及一个经常被忽略的 Mipmap 问题。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct3D 11 + WIC（链接 `d3d11.lib`、`d3dcompiler.lib`、`windowscodecs.lib`、`ole32.lib`）
- **前置知识**: 文章 37-38（D3D11 初始化 + 顶点缓冲）

## 修改顶点结构体：添加纹理坐标

要把纹理贴到三角形上，顶点数据需要增加 UV 坐标：

```cpp
struct Vertex
{
    XMFLOAT3 position;  // 位置
    XMFLOAT2 texCoord;  // 纹理坐标 (u, v)，范围 [0, 1]
};

// 全屏四边形的四个顶点（用 TriangleStrip）
Vertex quadVertices[] =
{
    { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },  // 左上
    { XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },  // 右上
    { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },  // 左下
    { XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },  // 右下
};
```

UV 坐标 (0,0) 对应纹理的左上角，(1,1) 对应右下角。GPU 在光栅化时会自动对 UV 坐标做插值——三角形中间的像素拿到的 UV 值是三个顶点 UV 的加权平均，然后用这个 UV 值去纹理中采样，就得到了该像素的颜色。

输入布局需要相应更新：

```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
      D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
      D3D11_APPEND_ALIGNED_ELEMENT,
      D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
```

注意语义名称从 `"COLOR"` 改成了 `"TEXCOORD"`，数据格式从 `R32G32B32A32_FLOAT` 改成了 `R32G32_FLOAT`（两个 float 分量）。

## 用 WIC 加载图片到 D3D11 纹理

D3D11 没有内置的图片加载函数，但 Windows 提供了 WIC（Windows Imaging Component）——一套强大的图像编解码框架。下面是一个完整的"从文件加载 PNG/JPEG 并创建 D3D11 纹理 + SRV"的工具函数：

```cpp
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

HRESULT LoadTextureFromFile(ID3D11Device* pDevice, const WCHAR* filePath,
    ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppSRV)
{
    // 初始化 WIC 工厂
    IWICImagingFactory* pFactory = NULL;
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory));

    // 创建解码器
    IWICBitmapDecoder* pDecoder = NULL;
    HRESULT hr = pFactory->CreateDecoderFromFilename(filePath, NULL,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) { pFactory->Release(); return hr; }

    // 获取第一帧
    IWICBitmapFrameDecode* pFrame = NULL;
    pDecoder->GetFrame(0, &pFrame);

    // 获取图片尺寸
    UINT width, height;
    pFrame->GetSize(&width, &height);

    // 转换像素格式为 32bpp BGRA（D3D11 最常用的格式）
    IWICFormatConverter* pConverter = NULL;
    pFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(pFrame,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, NULL, 0.0f,
        WICBitmapPaletteTypeCustom);

    // 计算图像数据大小并分配内存
    UINT rowPitch = width * 4;          // 每行字节数（4 字节/像素）
    UINT imageSize = rowPitch * height;
    BYTE* pPixels = new BYTE[imageSize];

    // 拷贝像素数据
    pConverter->CopyPixels(NULL, rowPitch, imageSize, pPixels);

    // 创建 D3D11 纹理
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;                 // 暂时不用 Mipmap
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;    // GPU 只读
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pPixels;
    initData.SysMemPitch = rowPitch;

    hr = pDevice->CreateTexture2D(&texDesc, &initData, ppTexture);

    // 创建着色器资源视图（SRV）
    if (SUCCEEDED(hr)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        pDevice->CreateShaderResourceView(*ppTexture, &srvDesc, ppSRV);
    }

    // 清理
    delete[] pPixels;
    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();

    return hr;
}
```

这个函数的流程是：用 WIC 打开图片文件 → 解码为 32 位 BGRA 像素 → 拷贝到内存数组 → 用这个数组作为初始数据创建 D3D11 纹理 → 创建 SRV。

⚠️ 注意像素格式的对应关系。WIC 用 `GUID_WICPixelFormat32bppBGRA`，D3D11 用 `DXGI_FORMAT_B8G8R8A8_UNORM`——它们表达的是同一种像素布局（Blue-Green-Red-Alpha 各 8 位），只是命名约定不同。如果你从 JPEG 加载，WIC 解码出的原始格式可能是 24bpp RGB，这时 `FormatConverter` 会帮你转换为 32bpp BGRA。但如果你跳过了格式转换步骤直接用原始数据，D3D11 纹理的 `Format` 就要对应修改，否则画面颜色会错乱。

## 采样器状态（Sampler State）

GPU 从纹理中取颜色值的过程叫"采样"。采样器控制采样行为的方方面面：当 UV 超出 0-1 范围时怎么办？纹理被缩小时怎么过滤？被放大时怎么过滤？

```cpp
ID3D11SamplerState* g_pSampler = NULL;

void CreateSampler(ID3D11Device* pDevice)
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // 三线性过滤
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;      // U 方向：重复
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;      // V 方向：重复
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    pDevice->CreateSamplerState(&sampDesc, &g_pSampler);
}
```

### 过滤模式

`Filter` 字段控制 GPU 如何处理"一个像素对应纹理中多个纹素"或"多个像素对应一个纹素"的情况：

`D3D11_FILTER_MIN_MAG_MIP_POINT` 是最近点采样——直接取最近的纹素，速度快但画面会有马赛克感。`D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT` 是双线性过滤——对周围的 4 个纹素做加权平均，放大时效果明显改善。`D3D11_FILTER_MIN_MAG_MIP_LINEAR` 是三线性过滤——在双线性基础上对两个 Mipmap 层级再做插值，缩小时效果最佳。`D3D11_FILTER_ANISOTROPIC` 是各向异性过滤——对倾斜表面（如地面）的纹理质量最好，但性能开销也最大。

### 寻址模式

`AddressU/V/W` 控制 UV 超出 [0,1] 范围时的行为：

`D3D11_TEXTURE_ADDRESS_WRAP` 是平铺重复——UV 值 1.5 等价于 0.5，适合地面砖块这类需要无缝重复的纹理。`D3D11_TEXTURE_ADDRESS_CLAMP` 是钳制到边缘——UV 超出 1 就用 1 的值，超出 0 就用 0 的值，适合单张不需要重复的图片。`D3D11_TEXTURE_ADDRESS_MIRROR` 是镜像重复——奇数倍区间做镜像翻转，接缝处更自然。`D3D11_TEXTURE_ADDRESS_BORDER` 是使用边框色——超出范围返回你指定的颜色。

## Mipmap：不可忽视的细节

Mipmap 是纹理的"金字塔"——原始纹理是第 0 级，每升一级分辨率减半。GPU 在渲染远处的纹理时会自动选择更小的 Mipmap 级别，避免"摩尔纹"（Moiré Pattern）伪影。

⚠️ 如果你创建纹理时 `MipLevels = 1`（只有一个层级），在纹理缩小到很小时会出现闪烁和锯齿状的纹理噪声。解决办法是用 `GenerateMips` 自动生成完整的 Mipmap 链：

```cpp
// 创建支持自动生成 Mipmap 的纹理
texDesc.MipLevels = 0;    // 0 = 自动计算完整 Mipmap 链
texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

// 创建纹理和 SRV 后...
g_pContext->GenerateMips(g_pTextureSRV);  // 自动生成所有 Mipmap 级别
```

`GenerateMips` 要求 SRV 是从一个能生成 Mipmap 的纹理创建的，而且纹理的 `BindFlags` 必须包含 `D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET`。

## Shader 代码

配套的纹理采样 Shader 非常简洁：

```hlsl
Texture2D    g_Texture  : register(t0);   // 纹理寄存器 t0
SamplerState g_Sampler  : register(s0);   // 采样器寄存器 s0

struct VSInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput VS_Main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, input.texCoord);
}
```

`Texture2D::Sample` 是 HLSL 中最常用的纹理采样方法——它接受一个采样器状态和一个 UV 坐标，返回该位置的插值颜色。GPU 硬件会根据采样器的过滤模式自动选择合适的 Mipmap 级别和过滤方式。

## 常见问题与调试

### 问题1：纹理缩小出现摩尔纹

你忘记生成 Mipmap 了。`MipLevels = 1` 的纹理在缩小时只有最近点采样可用，会产生闪烁的噪声伪影。添加 `GenerateMips` 调用或者离线预生成 Mipmap。

### 问题2：颜色偏蓝或偏红

WIC 解码出的像素格式和 D3D11 纹理格式不匹配。JPEG 解码后可能是 RGB 而非 BGRA，如果你不经过 `FormatConverter` 转换就直接创建 D3D11 纹理，红蓝通道会互换。始终用 `GUID_WICPixelFormat32bppBGRA` + `DXGI_FORMAT_B8G8R8A8_UNORM` 的组合。

### 问题3：纹理完全不显示（黑色）

检查你是否绑定了 SRV 和采样器：`PSSetShaderResources(0, 1, &g_pTextureSRV)` 和 `PSSetSamplers(0, 1, &g_pSampler)`。这两个调用缺一不可。

## 总结

纹理映射是从"几何体渲染"迈向"真实感图形"的关键一步。核心流程是：加载图片 → 创建 D3D11 纹理 → 创建 SRV → 创建采样器 → 在 PS 中采样。WIC 负责解码图片，D3D11 负责管理 GPU 端的纹理数据，采样器控制采样行为，Shader 负责把纹理颜色映射到像素上。

下一步，我们要进入真正的 3D 世界了。目前为止我们画的都在二维平面上（z=0），但 D3D11 是一个 3D 图形 API。下一篇文章会讲解深度缓冲和 MVP 变换矩阵——前者让 GPU 知道哪个三角形在前面哪个在后面，后者让你的 3D 模型正确地投影到 2D 屏幕上。

---

## 练习

1. 渲染一个带纹理的正方形，按键盘 1/2/3 切换 Point/Linear/Anisotropic 过滤模式，对比放大后的画质差异。
2. 将 `AddressU/V` 设为 `MIRROR`，用一张小纹理铺满整个屏幕，观察镜像平铺效果。
3. 修改 UV 坐标为超过 1.0 的值（如 3.0），配合 `WRAP` 和 `CLAMP` 模式分别测试，对比效果。
4. 故意不生成 Mipmap（`MipLevels = 1`），将纹理缩小到很小，观察摩尔纹现象，然后添加 `GenerateMips` 修复。

---

**参考资料**:
- [ID3D11Device::CreateTexture2D - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d)
- [D3D11_SAMPLER_DESC structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_sampler_desc)
- [WIC Imaging Component - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-windows-imaging-codec)
- [ID3D11DeviceContext::GenerateMips - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-generatemips)
- [D3D11_FILTER enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_filter)
