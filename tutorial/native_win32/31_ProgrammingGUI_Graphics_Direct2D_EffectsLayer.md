# 通用GUI编程技术——图形渲染实战（三十一）——Direct2D效果与图层：高斯模糊到毛玻璃

> 上一篇我们花了很大篇幅搞定了几何体的绘制与变换，从基本形状到 Path Geometry 到 Transform Matrix，Direct2D 的矢量绘图能力已经足够扎实了。但如果你仔细想想，到目前为止我们绘制出来的所有东西看起来都还是"扁平"的——没有模糊、没有阴影、没有半透明叠加。现代 UI 的视觉效果可不会止步于此，像毛玻璃（Frosted Glass）、投影阴影、色彩矩阵变换这些效果几乎无处不在。今天我们要来啃的就是 Direct2D 的效果系统（Effects）和图层系统（Layers），它们是让你从"能画"迈向"画得好看"的关键一跳。

## 环境说明

在开始之前，先说明一下本篇的开发环境：

- **操作系统**: Windows 11 Pro 10.0.26200（Windows 8 及以上即可使用 Effects API）
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **目标平台**: Win32 API 原生开发
- **图形库**: Direct2D 1.1（d2d1_1.h），Direct2D Effects 从 D2D 1.1 开始提供
- **关键头文件**: d2d1.h, d2d1_1.h, d2d1effects.h, d2d1helpers.h

⚠️ 这一篇有一个非常重要的前置条件：ID2D1Effect 是 Direct2D 1.1 的一部分，这意味着你必须使用 ID2D1DeviceContext 而不是我们之前一直在用的 ID2D1HwndRenderTarget。这不仅仅是接口版本的区别，背后牵扯到整个设备（Device）和设备上下文（DeviceContext）的创建方式，后面我们会详细展开。

## 效果系统全景：从内置效果到 Effect Graph

Direct2D 的效果系统是在 Windows 8 引入的，属于 D2D 1.1 的核心功能。它的设计思路很有意思——把每一个图像处理操作抽象成一个"效果节点"（Effect Node），然后通过连接这些节点来构建一个有向无环图（DAG），这就是所谓的 Effect Graph。

### 内置效果一览

微软在 Direct2D 里内置了一大堆开箱即用的效果，覆盖了绝大多数常见的图像处理需求。你可以在 [Built-in Effects - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/built-in-effects) 找到完整列表，这里我们先挑几个最常用的来熟悉一下：

高斯模糊（Gaussian Blur）的 CLSID 是 `CLSID_D2D1GaussianBlur`，这是我们今天的主力效果，用来做毛玻璃和柔焦。阴影（Shadow）的 CLSID 是 `CLSID_D2D1Shadow`，用来给图像添加投影。色彩矩阵（Color Matrix）的 CLSID 是 `CLSID_D2D1ColorMatrix`，可以做颜色空间变换、灰度化、色调调整等。形态学（Morphology）的 CLSID 是 `CLSID_D2D1Morphology`，支持膨胀和腐蚀操作，在图像处理领域应用广泛。除了这些，还有亮度/对比度、色相旋转、饱和度调整、混合模式（Blend）、复合（Composite）等几十种效果。

每个效果本质上就是一个图像处理的"黑盒"——你给它一个输入图像，它按照自己的参数进行处理，然后输出处理后的图像。而 Effect Graph 的强大之处在于，你可以把一个效果的输出连接到另一个效果的输入，形成链式处理。

### 为什么需要 DeviceContext

这里要先解决一个可能困扰你的问题：为什么用 ID2D1Effect 必须要 ID2D1DeviceContext？

原因在于 ID2D1Effect 底层依赖 Direct3D 设备来执行 GPU 加速的图像处理。ID2D1HwndRenderTarget 是 D2D 1.0 时代的接口，它内部虽然也绑定了 Direct3D 设备，但没有暴露 ID2D1Effect 所需的创建和管理接口。ID2D1DeviceContext 是 D2D 1.1 引入的新渲染目标接口，它直接关联一个 ID2D1Device，而这个 Device 封装了底层的 DXGI 设备和 Direct3D 设备。

所以我们的第一步，就是学会怎么创建 ID2D1DeviceContext。

## 第一步——创建 ID2D1DeviceContext

创建 ID2D1DeviceContext 的流程比创建 ID2D1HwndRenderTarget 要多几步，但并不复杂。整个链条是这样的：先创建 D3D 设备，然后通过 D3D 设备获取 DXGI 设备，再用 DXGI 设备创建 D2D 设备，最后从 D2D 设备创建 DeviceContext。

听起来有点绕？你可以把它理解为一条"创建依赖链"——D2D 需要 D3D 的 GPU 能力来加速渲染，而 DXGI 是 D3D 和显卡驱动之间的桥梁。

```cpp
#include <d3d11.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dxgi1_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")

// 全局变量
ID3D11Device*           g_pD3DDevice = nullptr;
ID3D11DeviceContext*    g_pD3DContext = nullptr;
IDXGIDevice*            g_pDXGIDevice = nullptr;
ID2D1Factory*           g_pD2DFactory = nullptr;
ID2D1Device*            g_pD2DDevice = nullptr;
ID2D1DeviceContext*     g_pDC = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
ID2D1Bitmap1*           g_pTargetBitmap = nullptr;

HRESULT CreateDeviceResources(HWND hwnd)
{
    HRESULT hr = S_OK;

    // 1. 创建 D3D 设备和设备上下文
    // 我们用 D3D_DRIVER_TYPE_HARDWARE 让系统自动选择硬件驱动
    // BgraSupport 必须为 TRUE，因为 D2D 需要 BGRA 格式的后台缓冲区
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        nullptr,                    // 默认适配器
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,  // D2D 必须的标志
        nullptr, 0,
        D3D11_SDK_VERSION,
        &g_pD3DDevice,
        &featureLevel,
        &g_pD3DContext
    );
    if (FAILED(hr)) return hr;

    // 2. 从 D3D 设备获取 DXGI 设备
    hr = g_pD3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&g_pDXGIDevice);
    if (FAILED(hr)) return hr;

    // 3. 创建 D2D Factory（注意这里用 D2D1_FACTORY_TYPE_SINGLE_THREADED 就行）
    if (g_pD2DFactory == nullptr) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
        if (FAILED(hr)) return hr;
    }

    // 4. 用 DXGI 设备创建 D2D 设备
    hr = g_pD2DFactory->CreateDevice(g_pDXGIDevice, &g_pD2DDevice);
    if (FAILED(hr)) return hr;

    // 5. 从 D2D 设备创建 DeviceContext
    hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &g_pDC
    );
    if (FAILED(hr)) return hr;

    // 6. 创建 SwapChain 用于窗口呈现
    RECT rc;
    GetClientRect(hwnd, &rc);
    DXGI_SWAP_CHAIN_DESC1 scDesc = { 0 };
    scDesc.Width = rc.right - rc.left;
    scDesc.Height = rc.bottom - rc.top;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // D2D 要求的格式
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGIFactory2* pDXGIFactory = nullptr;
    CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pDXGIFactory);

    hr = pDXGIFactory->CreateSwapChainForHwnd(
        g_pD3DDevice, hwnd, &scDesc, nullptr, nullptr, &g_pSwapChain
    );
    pDXGIFactory->Release();

    if (FAILED(hr)) return hr;

    // 7. 从 SwapChain 的后台缓冲区创建 D2D Bitmap 并设为渲染目标
    return CreateTargetBitmap(hwnd);
}
```

这段代码信息量不小。我们来梳理一下关键点：`D3D11CreateDevice` 的第四个参数 `D3D11_CREATE_DEVICE_BGRA_SUPPORT` 是 Direct2D 能正常工作的前提条件，没有这个标志，后续创建 D2D 设备会失败。SwapChain 的格式必须是 `DXGI_FORMAT_B8G8R8A8_UNORM`，这也是 D2D 所要求的。

接下来是 `CreateTargetBitmap` 函数，这个函数需要在窗口大小变化时也调用：

```cpp
HRESULT CreateTargetBitmap(HWND hwnd)
{
    // 释放旧的 bitmap
    if (g_pTargetBitmap) {
        g_pTargetBitmap->Release();
        g_pTargetBitmap = nullptr;
    }

    // 从 SwapChain 获取后台缓冲区
    IDXGISurface* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&pBackBuffer);
    if (FAILED(hr)) return hr;

    // 从 DXGI Surface 创建 D2D Bitmap
    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    hr = g_pDC->CreateBitmapFromDxgiSurface(pBackBuffer, bmpProps, &g_pTargetBitmap);
    pBackBuffer->Release();

    if (FAILED(hr)) return hr;

    // 设置为渲染目标
    g_pDC->SetTarget(g_pTargetBitmap);
    return S_OK;
}
```

很好，到这里我们已经有了 ID2D1DeviceContext 作为渲染目标，接下来就可以真正上手效果系统了。

## 第二步——高斯模糊效果实战

高斯模糊是视觉效果中最基础也最常用的效果之一。根据 [Gaussian blur effect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/gaussian-blur) 的文档，这个效果只有一个核心属性：`StandardDeviation`（标准差），它控制模糊的强度。

### 创建并应用高斯模糊

我们来看一个完整的例子——把一张位图进行高斯模糊处理：

```cpp
// 假设我们已经有一张 ID2D1Bitmap* g_pSourceBitmap
void DrawWithGaussianBlur(ID2D1DeviceContext* pDC, ID2D1Bitmap* pSourceBitmap)
{
    pDC->BeginDraw();

    // 清除背景
    pDC->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 1. 创建高斯模糊效果
    ID2D1Effect* pBlurEffect = nullptr;
    HRESULT hr = pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pBlurEffect);
    if (FAILED(hr)) return;

    // 2. 设置输入图像
    pBlurEffect->SetInput(0, pSourceBitmap);

    // 3. 设置模糊强度（标准差，值越大越模糊）
    pBlurEffect->SetValue(
        D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,
        5.0f   // 模糊半径
    );

    // 4. 获取效果输出的位置和大小
    D2D1_SIZE_F inputSize = pSourceBitmap->GetSize();
    D2D1_POINT_2F outputPoint = D2D1::Point2F(0.0f, 0.0f);

    // 5. 绘制效果的输出
    pDC->DrawImage(pBlurEffect, outputPoint);

    pBlurEffect->Release();

    hr = pDC->EndDraw();
}
```

注意这里的核心函数调用是 [ID2D1DeviceContext::CreateEffect](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/nf-d2d1_1-id2d1devicecontext-createeffect)。它接受一个 CLSID 作为参数，返回一个 ID2D1Effect 指针。创建之后，你通过 `SetInput` 设置输入图像，通过 `SetValue` 设置效果的属性，最后通过 `DrawImage` 把效果输出绘制到渲染目标上。

`DrawImage` 是 ID2D1DeviceContext 新增的方法，专门用于绘制效果输出。它和 ID2D1HwndRenderTarget 上的 `DrawBitmap` 不同——`DrawImage` 接受的是 ID2D1Image（ID2D1Effect 的输出就是 ID2D1Image），而 `DrawBitmap` 只接受 ID2D1Bitmap。

### 从文件加载位图

上面我们假设已经有一张位图了，但实际开发中你需要从文件加载。在 D2D 1.1 中，有一个非常方便的 WIC（Windows Imaging Component）集成方式：

```cpp
HRESULT LoadBitmapFromFile(ID2D1DeviceContext* pDC, IWICImagingFactory* pWICFactory,
                           PCWSTR uri, ID2D1Bitmap1** ppBitmap)
{
    IWICBitmapDecoder* pDecoder = nullptr;
    HRESULT hr = pWICFactory->CreateDecoderFromFilename(uri, nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) return hr;

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { pDecoder->Release(); return hr; }

    IWICFormatConverter* pConverter = nullptr;
    hr = pWICFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); return hr; }

    hr = pConverter->Initialize(pFrame,
        GUID_WICPixelFormat32bppPBGRA,  // D2D 最喜欢的格式
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);

    if (SUCCEEDED(hr)) {
        D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        hr = pDC->CreateBitmapFromWicBitmap(pConverter, props, ppBitmap);
    }

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    return hr;
}
```

这段代码通过 WIC 从文件解码图像，转换为 D2D 兼容的像素格式，然后创建位图。格式用 `GUID_WICPixelFormat32bppPBGRA`，对应 D2D 的 `DXGI_FORMAT_B8G8R8A8_UNORM` 加上预乘 Alpha。

## 第三步——阴影效果与 Effect Graph

单个效果已经很有用了，但效果系统真正的威力在于 Effect Graph——你可以把多个效果串联起来，形成一个处理管线。

### 投影阴影效果

我们来看一个非常实用的场景：给一张图片添加投影阴影。根据 [Shadow effect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/drop-shadow) 的文档，阴影效果的 CLSID 是 `CLSID_D2D1Shadow`。

```cpp
void DrawWithShadow(ID2D1DeviceContext* pDC, ID2D1Bitmap* pSourceBitmap,
                    float offsetX, float offsetY)
{
    pDC->BeginDraw();
    pDC->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 1. 创建阴影效果
    ID2D1Effect* pShadowEffect = nullptr;
    pDC->CreateEffect(CLSID_D2D1Shadow, &pShadowEffect);
    pShadowEffect->SetInput(0, pSourceBitmap);

    // 设置阴影颜色和模糊程度
    pShadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.5f));
    pShadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, 8.0f);

    // 2. 创建仿射变换效果，将阴影偏移
    ID2D1Effect* pOffsetEffect = nullptr;
    pDC->CreateEffect(CLSID_D2D12DAffineTransform, &pOffsetEffect);
    pOffsetEffect->SetInputEffect(0, pShadowEffect);  // 注意这里用的是 SetInputEffect

    D2D1_MATRIX_3X2_F matTrans = D2D1::Matrix3x2F::Translation(offsetX, offsetY);
    pOffsetEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM, matTrans);

    // 3. 创建复合效果，将阴影和原图合成
    ID2D1Effect* pCompositeEffect = nullptr;
    pDC->CreateEffect(CLSID_D2D1Composite, &pCompositeEffect);
    pCompositeEffect->SetInputEffect(0, pOffsetEffect);    // 阴影在下层
    pCompositeEffect->SetInput(1, pSourceBitmap);           // 原图在上层

    // 4. 绘制最终合成结果
    pDC->DrawImage(pCompositeEffect);

    pCompositeEffect->Release();
    pOffsetEffect->Release();
    pShadowEffect->Release();

    pDC->EndDraw();
}
```

这里我们需要特别关注 `SetInputEffect` 和 `SetInput` 的区别。`SetInput` 接受的是 ID2D1Bitmap（或者更准确地说是 ID2D1Image），而 `SetInputEffect` 接受的是 ID2D1Effect，它内部会自动获取该效果的输出图像。这个区别很微妙但非常关键——当你需要把一个效果的输出作为另一个效果的输入时，用 `SetInputEffect` 更简洁。

另外注意复合效果（Composite Effect）的输入顺序：索引 0 是底层（阴影），索引 1 是上层（原图）。这和 Photoshop 里的图层概念是一致的。

### Effect Graph 的结构

上面这个例子其实已经构建了一个简单的 Effect Graph：

```
SourceBitmap ──→ ShadowEffect ──→ AffineTransformEffect ──→ CompositeEffect ──→ 输出
    │                                                           ↑
    └───────────────────────────────────────────────────────────┘
```

你可以把它理解为一条流水线：原图先经过阴影效果生成阴影图像，然后阴影经过平移变换偏移到指定位置，最后和原图复合在一起。整个处理过程都在 GPU 上执行，效率非常高。

## 第四步——图层系统：透明度与几何裁切

效果系统处理的是图像处理问题，而图层系统解决的是另一个问题：如何控制绘制内容的可见范围和透明度。

根据 [Layers Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-layers-overview) 的说明，图层的工作方式是：你先 Push 一个图层到渲染目标上，之后所有绘制操作都会被这个图层所约束（比如裁切到某个几何形状内，或者应用某个透明度），最后 Pop 这个图层，约束结束。

### PushLayer/PopLayer 的基本用法

```cpp
void DrawWithOpacityLayer(ID2D1DeviceContext* pDC, ID2D1Bitmap* pBitmap,
                          float opacity)
{
    pDC->BeginDraw();
    pDC->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 创建图层参数——仅设置透明度
    D2D1_LAYER_PARAMETERS1 layerParams = D2D1::LayerParameters1();
    // 不设置 geometricMask，整个区域都有效
    // 设置透明度
    layerParams.opacity = opacity;
    layerParams.opacityBrush = nullptr;  // 用固定透明度而不是画刷

    // 在 D2D 1.1 中，不需要预先创建 ID2D1Layer 对象
    // PushLayer 会自动管理
    pDC->PushLayer(layerParams, nullptr);

    // 在图层内绘制内容，所有内容都会被 opacity 影响
    pDC->DrawBitmap(pBitmap,
        D2D1::RectF(50.0f, 50.0f, 400.0f, 300.0f));

    pDC->PopLayer();

    pDC->EndDraw();
}
```

这里有一个比较容易忽略的细节：在 D2D 1.1（ID2D1DeviceContext）中，`PushLayer` 的第二个参数可以传 `nullptr`。系统会自动管理图层的内存分配，不需要你手动创建 ID2D1Layer 对象。这是对 D2D 1.0 的一个改进——在旧版本中，你必须先调用 `CreateLayer` 创建一个图层对象，然后传给 `PushLayer`。

⚠️ 这里必须强调一个铁律：`PushLayer` 和 `PopLayer` 必须成对出现。如果你 Push 了但忘记 Pop，或者多 Pop 了一次，运行时不会给你一个友好的错误提示——它会直接导致渲染结果异常甚至程序崩溃。建议你在写代码的时候就养成良好的配对习惯，可以在 Pop 之后立即加个注释标记。

### 几何裁切图层

图层更强大的用途是几何裁切——你可以在一个不规则形状的区域内绘制内容。这在实现圆形头像、卡片圆角、复杂蒙版等效果时非常有用：

```cpp
void DrawWithGeometricClip(ID2D1DeviceContext* pDC,
                           ID2D1Bitmap* pBitmap,
                           D2D1_ELLIPSE clipEllipse)
{
    pDC->BeginDraw();
    pDC->Clear(D2D1::ColorF(D2D1::ColorF::LightGray));

    // 创建裁切用的椭圆几何体
    ID2D1EllipseGeometry* pClipGeometry = nullptr;
    g_pD2DFactory->CreateEllipseGeometry(clipEllipse, &pClipGeometry);

    // 设置图层参数，使用几何裁切
    D2D1_LAYER_PARAMETERS1 layerParams = { 0 };
    layerParams.contentBounds = D2D1::RectF(
        clipEllipse.point.x - clipEllipse.radiusX - 10,
        clipEllipse.point.y - clipEllipse.radiusY - 10,
        clipEllipse.point.x + clipEllipse.radiusX + 10,
        clipEllipse.point.y + clipEllipse.radiusY + 10
    );
    layerParams.geometricMask = pClipGeometry;
    layerParams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    layerParams.opacity = 1.0f;
    layerParams.opacityBrush = nullptr;

    pDC->PushLayer(layerParams, nullptr);

    // 在裁切区域内绘制位图——椭圆外部的内容会被裁掉
    pDC->DrawBitmap(pBitmap,
        D2D1::RectF(
            clipEllipse.point.x - clipEllipse.radiusX,
            clipEllipse.point.y - clipEllipse.radiusY,
            clipEllipse.point.x + clipEllipse.radiusX,
            clipEllipse.point.y + clipEllipse.radiusY
        ));

    pDC->PopLayer();

    pClipGeometry->Release();
    pDC->EndDraw();
}
```

`geometricMask` 是图层参数中实现裁切的关键字段。它接受一个 ID2D1Geometry 指针，可以是任何几何形状——椭圆、矩形、圆角矩形、Path Geometry 等。所有在图层内的绘制操作都会被裁切到这个几何体内部。`maskAntialiasMode` 控制裁切边缘的抗锯齿质量。

## 第五步——毛玻璃卡片效果实战

现在我们把效果系统和图层系统结合起来，实现一个现代 UI 中非常常见的毛玻璃卡片效果。这个效果的核心思路是：先绘制背景图像，然后在卡片区域对背景进行高斯模糊，再叠加一个半透明的蒙版，最后在卡片上绘制文字或其他内容。

```cpp
// 毛玻璃效果的完整实现
void DrawFrostedGlassCard(ID2D1DeviceContext* pDC,
                          ID2D1Bitmap* pBackgroundBitmap,
                          D2D1_RECT_F cardRect,
                          ID2D1SolidColorBrush* pTextBrush,
                          const wchar_t* text)
{
    pDC->BeginDraw();

    // --- 第一层：绘制原始背景 ---
    D2D1_SIZE_F targetSize = pDC->GetSize();
    pDC->DrawBitmap(pBackgroundBitmap,
        D2D1::RectF(0, 0, targetSize.width, targetSize.height));

    // --- 第二层：在卡片区域绘制高斯模糊的背景 ---
    // 创建高斯模糊效果
    ID2D1Effect* pBlurEffect = nullptr;
    pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pBlurEffect);
    pBlurEffect->SetInput(0, pBackgroundBitmap);
    pBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 12.0f);
    pBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE,
                          D2D1_BORDER_MODE_SOFT);

    // 创建裁切几何体（圆角矩形）
    ID2D1RoundedRectangleGeometry* pCardClip = nullptr;
    D2D1_ROUNDED_RECT roundedRect = {
        cardRect,
        12.0f, 12.0f   // 圆角半径
    };
    g_pD2DFactory->CreateRoundedRectangleGeometry(roundedRect, &pCardClip);

    // 使用图层裁切，只在卡片区域内绘制模糊效果
    D2D1_LAYER_PARAMETERS1 layerParams = { 0 };
    layerParams.contentBounds = cardRect;
    layerParams.geometricMask = pCardClip;
    layerParams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    layerParams.opacity = 1.0f;

    pDC->PushLayer(layerParams, nullptr);

    // 绘制模糊后的背景
    pDC->DrawImage(pBlurEffect, D2D1::Point2F(0.0f, 0.0f));

    pDC->PopLayer();

    // --- 第三层：半透明蒙版 ---
    ID2D1SolidColorBrush* pTintBrush = nullptr;
    pDC->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.25f),  // 25% 白色蒙版
        &pTintBrush
    );

    // 再次用图层裁切绘制蒙版
    pDC->PushLayer(layerParams, nullptr);
    pDC->FillRectangle(cardRect, pTintBrush);
    pDC->PopLayer();

    // --- 第四层：卡片边框 ---
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    pDC->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f),
        &pBorderBrush
    );
    pDC->DrawRoundedRectangle(roundedRect, pBorderBrush, 1.5f);

    // --- 第五层：卡片上的文字 ---
    pDC->DrawTextW(
        text, (UINT32)wcslen(text),
        g_pTextFormat,  // 预先创建的 IDWriteTextFormat
        D2D1::RectF(cardRect.left + 20, cardRect.top + 20,
                    cardRect.right - 20, cardRect.bottom - 20),
        pTextBrush
    );

    // 清理
    pBlurEffect->Release();
    pCardClip->Release();
    pTintBrush->Release();
    pBorderBrush->Release();

    pDC->EndDraw();
}
```

这段代码虽然看起来有点长，但逻辑是非常清晰的。我们分五个层次来绘制毛玻璃效果：先画原始背景，然后用 Effect Graph 对背景做高斯模糊，再通过图层裁切只在卡片区域显示模糊结果，接着叠加一层半透明白色蒙版，最后画边框和文字。

⚠️ 这里有个容易踩的坑：高斯模糊的 `D2D1_BORDER_MODE_SOFT` 选项。如果你用默认的 `D2D1_BORDER_MODE_HARD`，在图像边缘处会出现明显的接缝。`SOFT` 模式会让边缘自然过渡，这对于毛玻璃效果非常重要。

另外，我们在这个例子中 Push 了两次相同的图层（一次用于模糊绘制，一次用于蒙版绘制）。虽然看起来有点冗余，但这样做的好处是两次绘制的裁切区域完全一致，不会出现对齐偏差。

## 常见问题与踩坑总结

### ID2D1HwndRenderTarget 能用 Effect 吗？

不能。这是一个新手特别容易犯的错误。ID2D1Effect 是 D2D 1.1 的功能，只有 ID2D1DeviceContext 才支持 `CreateEffect` 和 `DrawImage`。如果你手头已有基于 ID2D1HwndRenderTarget 的代码，想要使用效果系统，就必须迁移到 ID2D1DeviceContext。好消息是，迁移的核心工作量就在创建设备资源那一步，绘图 API 大部分是兼容的。

### PushLayer 后没有 PopLayer 会怎样？

会崩溃，或者渲染结果完全错乱。PushLayer/PopLayer 是栈操作，Push 一次就必须 Pop 一次，不能嵌套不匹配。如果你在一个循环里 Push，确保循环的每个路径都能走到对应的 Pop。

### Effect 的 SetValue 类型搞错会怎样？

`SetValue` 是模板化的，但如果你传了错误类型的值（比如属性要求 float 你传了 int），编译可能能过（因为隐式转换），但效果的行为会和你预期的不一样。建议每次调用 `SetValue` 时都查一下文档，确认属性的类型。

### 高斯模糊效果会让图像变小吗？

会的。模糊效果默认会在输出图像的边缘"吃掉"一部分，因为模糊需要邻近像素参与计算。如果你发现模糊后图像边缘有黑边或者缺失，有两个解决方案：一是设置 `D2D1_BORDER_MODE_SOFT` 让边缘自然延展；二是让输入图像比实际需要的大一些，留出模糊所需的余量。

### 多个 Effect 的性能开销

每个 Effect 节点在 GPU 上都是一次绘制调用（Draw Call），如果你串联了十几个效果，就意味着十几次 GPU pass。对于高分辨率图像，这可能会成为性能瓶颈。建议在性能敏感的场景下尽量减少 Effect Graph 的深度，或者考虑把多个效果合并为一个自定义的 HLSL 着色器效果（这是后面文章的内容）。

## 总结

到这里我们已经把 Direct2D 的效果系统和图层系统过了一遍。从创建 ID2D1DeviceContext 这个必要的前置条件开始，到高斯模糊和阴影这两个最常用的内置效果，再到 Effect Graph 的链式连接，最后结合图层系统实现了毛玻璃卡片效果。

效果系统让你能在 GPU 上高效地做图像处理，图层系统让你能控制绘制内容的可见范围和透明度。这两套系统组合在一起，基本能覆盖现代 UI 中绝大部分视觉效果的实现需求。

下一篇我们要进入 DirectWrite 的世界——文字排版是 GUI 编程中另一个极其重要的领域，而 DirectWrite 提供了 Windows 平台上最高质量的文字渲染能力。从创建文字格式到精确的命中测试，从图文混排到代码高亮渲染器，这些都是下一篇文章的内容。

---

## 练习

1. **调整高斯模糊参数**：修改高斯模糊示例中的 `StandardDeviation` 值（尝试 2.0、5.0、15.0、30.0），观察不同模糊程度的效果差异。同时尝试切换 `D2D1_BORDER_MODE_HARD` 和 `D2D1_BORDER_MODE_SOFT`，对比边缘表现。

2. **实现色彩矩阵灰度化**：使用 `CLSID_D2D1ColorMatrix` 效果，通过设置合适的色彩矩阵将一张彩色位图转换为灰度图。提示：灰度转换的经典矩阵是 R = G = B = 0.299*R + 0.587*G + 0.114*B。

3. **多效果串联**：在毛玻璃卡片示例的基础上，添加一个亮度/对比度效果（`CLSID_D2D1Brightness`），让用户可以通过滑块控件调节卡片区域的亮度。

4. **形状蒙版动画**：创建一个圆形的几何裁切图层，让这个圆形从小到大做缩放动画（使用 D2D1_MATRIX_3X2_F 的 Scale 变换），模拟"聚焦展开"的视觉效果。

---

**参考资料**:
- [Built-in Effects - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/built-in-effects)
- [Gaussian blur effect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/gaussian-blur)
- [Shadow effect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/drop-shadow)
- [ID2D1DeviceContext::CreateEffect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_1/nf-d2d1_1-id2d1devicecontext-createeffect)
- [Effects overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/effects-overview)
- [Layers Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-layers-overview)
- [ID2D1RenderTarget::PushLayer - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-pushlayer(constd2d1_layer_parameters__id2d1layer))
- [ID2D1RenderTarget::PopLayer - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-poplayer)
- [How to Apply Effects to Primitives - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/how-to-apply-effects-to-primitives)
