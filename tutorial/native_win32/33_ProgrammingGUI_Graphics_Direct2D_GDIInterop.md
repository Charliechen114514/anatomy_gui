# 通用GUI编程技术——图形渲染实战（三十三）——Direct2D与Win32/GDI互操作：渐进迁移实战

> 上一篇我们花了大量篇幅啃下了 DirectWrite 的三层 API 架构，从 IDWriteTextFormat 到 IDWriteTextLayout 到自定义 TextRenderer，代码高亮渲染器也跑起来了。至此，Direct2D 生态中渲染相关的核心能力我们基本都覆盖了。但现实世界不会给你一张白纸让你从头开始写——更多的情况是，你手头已经有一个积累了多年的 GDI 项目，里面有几千行 WM_PAINT 处理代码，有各种自定义控件，有第三方的 GDI 组件。你不可能一夜之间全部重写成 D2D，但你又想享受 D2D 的硬件加速、抗锯齿、效果系统这些好处。今天这篇就来解决这个问题：如何在 GDI 和 Direct2D 之间做互操作，实现渐进式迁移。

## 环境说明

本篇开发环境如下：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **目标平台**: Win32 API 原生开发
- **图形库**: Direct2D（d2d1.h）+ GDI（wingdi.h），互操作接口
- **关键头文件**: d2d1.h, d2d1_1.h, windows.h, wingdi.h

互操作涉及两个方向的桥接：在 GDI 的 HDC 上使用 D2D 绘制，和在 D2D 的渲染目标上使用 GDI 绘制。两个方向有不同的接口和适用场景，我们逐一展开。

## 互操作的两个方向

在深入代码之前，先搞清楚互操作的两条路。

第一条路是 D2D 绘制到 GDI 的 DC 上。这种方式的接口是 [ID2D1DCRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1dcrendertarget)，通过 [ID2D1Factory::CreateDCRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createdcrendertarget) 创建。它适用于你在现有 GDI 代码中想插入一些 D2D 绘制的场景——比如你的 WM_PAINT 处理函数里大部分是 GDI 代码，但某个复杂的图表你想用 D2D 来画。DCRenderTarget 会把 D2D 的绘制内容先渲染到内部位图上，然后通过 GDI 的 BitBlt 合成到目标 HDC 上。

第二条路是 GDI 绘制到 D2D 的渲染目标上。这种方式的接口是 [ID2D1GdiInteropRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1gdiinteroprendertarget)，通过 `QueryInterface` 从 ID2D1HwndRenderTarget 获取。它适用于你的主渲染框架已经是 D2D，但偶尔需要调用一些只有 GDI 能做的事情——比如使用旧的 GDI 字体引擎、调用第三方 GDI 组件等。它通过 [GetDC](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1gdiinteroprendertarget-getdc) 方法临时获取一个 HDC，你可以在上面进行 GDI 绘制，完成后通过 [ReleaseDC](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1gdiinteroprendertarget-releasedc) 释放。

理解了这两条路之后，我们就可以根据实际的迁移方向选择合适的接口了。下面我们分步骤来实战。

## 第一步——DCRenderTarget：在 GDI 环境中使用 D2D

这是渐进迁移最实用的方式。你的程序主体仍然是 GDI，但在某些地方你想用 D2D 的高质量渲染。DCRenderTarget 就是为此而生的。

### 创建 DCRenderTarget

创建 DCRenderTarget 只需要 D2D Factory，不需要 D3D 设备和 SwapChain 那套复杂的流程。这降低了迁移的初始成本：

```cpp
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

ID2D1Factory*            g_pD2DFactory = nullptr;
ID2D1DCRenderTarget*     g_pDCRenderTarget = nullptr;
ID2D1SolidColorBrush*    g_pBlackBrush = nullptr;
ID2D1SolidColorBrush*    g_pAccentBrush = nullptr;

HRESULT CreateD2DResources()
{
    // 创建 D2D Factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
    if (FAILED(hr)) return hr;

    // 创建 DCRenderTarget
    // 像素格式必须和 GDI 兼容——BGRA + Premultiplied Alpha
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED
        ),
        0.0f,   // DPI，0 表示使用系统默认 DPI
        0.0f,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE  // 关键标志！
    );

    hr = g_pD2DFactory->CreateDCRenderTarget(&rtProps, &g_pDCRenderTarget);
    if (FAILED(hr)) return hr;

    // 创建一些画刷
    g_pDCRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &g_pBlackBrush);
    g_pDCRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0x0078D4), &g_pAccentBrush);  // Windows 蓝

    return hr;
}
```

这里有一个非常关键的细节：`D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE` 标志。这个标志告诉 Direct2D，这个渲染目标需要支持 GDI 互操作。如果你不设置这个标志，后续创建的 DCRenderTarget 虽然可以绑定到 GDI 的 HDC 上，但在某些 GDI 操作（比如透明度混合）上可能出现问题。

像素格式 `DXGI_FORMAT_B8G8R8A8_UNORM` 配合 `D2D1_ALPHA_MODE_PREMULTIPLIED` 是 GDI 互操作的标准配置。GDI 的 32 位位图使用的是 BGRA 字节序，和 Direct2D 的这个格式完全匹配。

### 绑定到 HDC 并绘制

DCRenderTarget 的核心方法是 `BindDC`——它把 D2D 渲染目标和 GDI 的 HDC 关联起来。你需要在每次绘制前调用 BindDC，传入 HDC 和需要绑定的矩形区域：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // ===== GDI 绘制部分 =====
    // 先用 GDI 填充背景
    HBRUSH hbrBg = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &rcClient, hbrBg);
    DeleteObject(hbrBg);

    // 用 GDI 绘制标题文字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    HFONT hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TextOutW(hdc, 20, 20, L"GDI + D2D 混合绘制", 13);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    // ===== D2D 绘制部分 =====
    // 在窗口下半部分用 D2D 绘制高质量图形
    RECT rcD2D = { 20, 80, rcClient.right - 20, rcClient.bottom - 20 };

    // 将 D2D 渲染目标绑定到 HDC 的指定矩形区域
    g_pDCRenderTarget->BindDC(hdc, &rcD2D);

    g_pDCRenderTarget->BeginDraw();

    // D2D 绘制的内容会自动合成到 HDC 上
    // 先填充 D2D 区域的背景
    g_pDCRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));

    // 用 D2D 绘制高质量抗锯齿的圆角矩形和圆形
    D2D1_RECT_F d2dRect = D2D1::RectF(
        10.0f, 10.0f,
        (float)(rcD2D.right - rcD2D.left - 10),
        (float)(rcD2D.bottom - rcD2D.top - 10)
    );

    D2D1_ROUNDED_RECT roundedRect = { d2dRect, 12.0f, 12.0f };

    // 绘制填充的圆角矩形
    g_pDCRenderTarget->FillRoundedRectangle(roundedRect, g_pAccentBrush);

    // 绘制抗锯齿的圆形
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F(
            (float)(rcD2D.right - rcD2D.left) / 2,
            (float)(rcD2D.bottom - rcD2D.top) / 2
        ),
        60.0f, 60.0f
    );

    ID2D1SolidColorBrush* pWhiteBrush = nullptr;
    g_pDCRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White, 0.8f), &pWhiteBrush);

    g_pDCRenderTarget->FillEllipse(ellipse, pWhiteBrush);
    pWhiteBrush->Release();

    HRESULT hr = g_pDCRenderTarget->EndDraw();
    if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET) {
        // 需要重新创建渲染资源
        // 实际项目中这里应该调用重建资源的函数
    }

    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码展示了一个典型的 GDI + D2D 混合绘制的 WM_PAINT 处理函数。上半部分用 GDI 画标题文字，下半部分用 D2D 画高质量的抗锯齿图形。

⚠️ 关于 `BindDC` 有一个重要的注意事项：每次调用 BeginDraw 之前都必须重新调用 BindDC。这是因为 HDC 在两次 WM_PAINT 之间可能已经失效了（Windows 的 HDC 是短生命周期的资源）。如果你缓存了 DCRenderTarget 但忘记在每次绘制前重新绑定 HDC，你会看到绘制结果完全错乱或者什么都不显示。这个坑我在迁移项目的时候踩过，找了好久才定位到。

另外注意 `EndDraw` 返回 `D2DERR_RECREATE_TARGET` 的情况。当 Direct3D 设备丢失（Device Lost）时，这个错误码会被返回。你需要在收到这个错误时释放所有 D2D 资源并重新创建它们。在 GDI 互操作场景下，设备丢失的概率比纯 D2D 场景要低一些（因为 DCRenderTarget 使用的是软件渲染后备），但仍然需要处理。

## 第二步——GdiInteropRenderTarget：在 D2D 中使用 GDI

现在我们看反方向的情况：你的主渲染框架已经是 D2D（使用 ID2D1HwndRenderTarget 或 ID2D1DeviceContext），但你需要在某些地方临时调用 GDI 函数。

### 获取 GdiInteropRenderTarget

GdiInteropRenderTarget 不是通过 Create 函数创建的，而是通过 QueryInterface 从现有的 D2D 渲染目标获取的：

```cpp
ID2D1GdiInteropRenderTarget* g_pGdiInterop = nullptr;

void InitGdiInterop(ID2D1HwndRenderTarget* pHwndRenderTarget)
{
    // 从 HwndRenderTarget 获取 GDI 互操作接口
    HRESULT hr = pHwndRenderTarget->QueryInterface(
        __uuidof(ID2D1GdiInteropRenderTarget),
        (void**)&g_pGdiInterop
    );

    if (FAILED(hr)) {
        // QueryInterface 失败意味着渲染目标不支持 GDI 互操作
        // 确保创建 HwndRenderTarget 时设置了 GDI_COMPATIBLE 标志
    }
}
```

⚠️ 这里有一个前提条件：你的 ID2D1HwndRenderTarget 在创建时必须指定了 `D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE` 用法标志。如果你没有设置这个标志，QueryInterface 会返回 E_NOINTERFACE。具体的创建代码看起来是这样的：

```cpp
D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;  // 必须设置！

HRESULT hr = g_pD2DFactory->CreateHwndRenderTarget(
    rtProps,
    D2D1::HwndRenderTargetProperties(hwnd, size),
    &g_pHwndRenderTarget
);
```

### 在 D2D 绘制流程中插入 GDI 操作

下面是一个完整的例子——在 D2D 绘制流程中临时切换到 GDI 画一些旧控件：

```cpp
void RenderWithGdiInterop(ID2D1HwndRenderTarget* pRT,
                          ID2D1GdiInteropRenderTarget* pGdiInterop)
{
    pRT->BeginDraw();

    // --- D2D 绘制部分 ---
    pRT->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 用 D2D 画一些漂亮的图形
    ID2D1SolidColorBrush* pBlueBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0x0078D4), &pBlueBrush);

    D2D1_ROUNDED_RECT card = {
        D2D1::RectF(50.0f, 50.0f, 400.0f, 300.0f),
        10.0f, 10.0f
    };
    pRT->FillRoundedRectangle(card, pBlueBrush);

    // --- 临时切换到 GDI ---
    // 获取 HDC 用于 GDI 绘制
    HDC hdc = nullptr;
    D2D1_DC_INITIALIZE_MODE initMode = D2D1_DC_INITIALIZE_MODE_COPY;

    HRESULT hr = pGdiInterop->GetDC(&initMode, &hdc);
    if (SUCCEEDED(hr) && hdc != nullptr)
    {
        // 现在你可以在这个 HDC 上使用 GDI 函数了
        // 例如：绘制一些旧的 GDI 控件效果

        // 用 GDI 画一个带边框的按钮
        RECT btnRect = { 60, 320, 200, 360 };
        DrawEdge(hdc, &btnRect, EDGE_RAISED, BF_RECT | BF_ADJUST);

        // 用 GDI 画按钮文字
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        DrawTextW(hdc, L"GDI Button", -1, &btnRect,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 画一条分隔线
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 60, 380, nullptr);
        LineTo(hdc, 390, 380);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        // 必须调用 ReleaseDC 释放 HDC
        // RECT 参数指定 GDI 修改过的区域，nullptr 表示整个区域
        pGdiInterop->ReleaseDC(nullptr);
    }

    // --- 继续用 D2D 绘制 ---
    // GetDC 调用会自动 Flush D2D 命令，所以这里可以继续 D2D 绘制
    pBlueBrush->Release();

    pRT->EndDraw();
}
```

这段代码中有几个非常关键的点需要你注意。

首先是 `D2D1_DC_INITIALIZE_MODE_COPY`。这个参数告诉 Direct2D，在返回 HDC 之前先把当前 D2D 的渲染结果拷贝到 HDC 对应的表面。这样你在 HDC 上看到的就是 D2D 已经绘制的内容，GDI 的绘制会覆盖在上面。如果你用 `D2D1_DC_INITIALIZE_MODE_CLEAR`，HDC 的内容会被清空，你就看不到之前 D2D 画的东西了。

其次是 `GetDC` 的副作用。根据 [ID2D1GdiInteropRenderTarget::GetDC - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1gdiinteroprendertarget-getdc) 的文档，调用 GetDC 会自动 Flush 渲染目标——也就是说，所有尚未执行的 D2D 绘制命令会被立即提交到 GPU 执行。这个操作是有性能成本的，所以不要在每一帧都频繁调用 GetDC/ReleaseDC。

### ⚠️ Flush 同步的铁律

这里有一个铁律必须遵守：GDI 和 D2D 在同一个表面上的绘制必须通过 Flush 来同步。

D2D 的绘制命令是延迟执行的——你调用 DrawRectangle 的时候，命令只是被放进了一个命令队列，并不会立即执行。而 GDI 的绘制是立即模式的——调用 Rectangle 马上就会画上去。如果你在 D2D 绘制和 GDI 绘制之间不进行同步，就会出现绘制顺序错乱的问题。

GetDC 内部会自动调用 Flush，这保证了 D2D 的绘制结果在 GDI 获取 HDC 之前已经完成。但反过来呢？如果你在 ReleaseDC 之后又进行 D2D 绘制，D2D 是否能正确看到 GDI 的绘制结果？

答案是肯定的——ReleaseDC 会通知 D2D 渲染目标，GDI 已经修改了内容，D2D 后续的绘制会基于最新的表面状态。但如果你在使用 ID2D1DeviceContext（D2D 1.1），在某些情况下你可能需要手动调用 `Flush()` 来确保命令同步。

```cpp
// 如果你在 D2D 和 GDI 之间交替绘制，确保每次切换都进行同步
pRT->DrawRectangle(rect, brush);    // D2D 绘制
pRT->Flush();                        // 强制提交 D2D 命令

pGdiInterop->GetDC(&initMode, &hdc);
// ... GDI 绘制 ...
pGdiInterop->ReleaseDC(nullptr);

pRT->DrawEllipse(ellipse, brush);   // 继续D2D绘制
```

在性能敏感的场景中，你应该尽量减少 D2D/GDI 之间的切换次数。每次 GetDC/ReleaseDC 对都会触发一次 Flush，而 Flush 是一个同步 GPU 的操作，会打断渲染管线的并行性。理想的做法是把所有 D2D 绘制放在一起，所有 GDI 绘制放在一起，只切换一次。

## 第三步——渐进迁移策略

有了上面两个互操作接口的武器，我们就可以制定实际的渐进迁移策略了。

### 自顶向下：从 WM_PAINT 开始

最实用的迁移策略是"自顶向下"——从 WM_PAINT 处理函数开始，逐步把 GDI 代码替换为 D2D 代码。

迁移的第一阶段是创建 DCRenderTarget 并集成到现有的 WM_PAINT 流程中。这时候你不需要修改任何现有的 GDI 代码，只是在 WM_PAINT 的某个位置插入 D2D 绘制。你可以在一个特定的矩形区域内用 D2D 绘制，GDI 的内容不受影响。

```cpp
// 阶段一：在现有 WM_PAINT 中插入 D2D 绘制
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 现有的 GDI 绘制代码（不动）
    DrawBackground_GDI(hdc);
    DrawStaticElements_GDI(hdc);

    // 新增：用 D2D 绘制图表区域
    RECT rcChart = { 20, 100, 500, 400 };
    g_pDCRenderTarget->BindDC(hdc, &rcChart);
    g_pDCRenderTarget->BeginDraw();
    DrawChart_D2D(g_pDCRenderTarget);  // 新的 D2D 绘制函数
    g_pDCRenderTarget->EndDraw();

    // 更多现有的 GDI 代码（不动）
    DrawStatusBar_GDI(hdc);
    DrawToolbar_GDI(hdc);

    EndPaint(hwnd, &ps);
    return 0;
}
```

迁移的第二阶段是把越来越多的 GDI 绘制函数替换为 D2D 版本。每次只替换一个函数，替换完就测试，确保没有引入回归问题。DCRenderTarget 的好处是你可以逐区域替换，不需要一次性改完。

迁移的第三阶段是当你大部分绘制代码都已经迁移到 D2D 后，考虑把渲染目标从 DCRenderTarget 切换为 ID2D1HwndRenderTarget 或 ID2D1DeviceContext。这样可以完全消除 GDI 中间层，获得更好的性能。

### 分层窗口的迁移

分层窗口（Layered Window）是 GDI 时代实现自定义窗口形状和半透明效果的常用技术。它的核心是 `UpdateLayeredWindow` 函数，它接受一个 32 位 ARGB 位图作为窗口内容。

如果你有一个现有的分层窗口项目想要迁移到 D2D，有一个非常优雅的方案：使用 ID2D1BitmapRenderTarget（D2D 1.0）或 ID2D1Bitmap1（D2D 1.1）作为中间渲染目标，渲染完成后把结果传给 UpdateLayeredWindow。

```cpp
// 分层窗口 + D2D 的实现

ID2D1HwndRenderTarget*  g_pRT = nullptr;
ID2D1BitmapRenderTarget* g_pOffscreenRT = nullptr;
HWND g_hwndLayered = nullptr;

void RenderLayeredWindow()
{
    // 确保离屏渲染目标存在
    if (!g_pOffscreenRT) {
        D2D1_SIZE_F size = g_pRT->GetSize();
        D2D1_BITMAP_PROPERTIES bmpProps = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                             D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        g_pRT->CreateCompatibleRenderTarget(
            &g_pOffscreenRT, size, &bmpProps);
    }

    // 在离屏渲染目标上绘制
    g_pOffscreenRT->BeginDraw();
    g_pOffscreenRT->Clear(D2D1::ColorF(0, 0, 0, 0));  // 透明背景

    // --- D2D 绘制内容 ---
    DrawLayeredContent_D2D(g_pOffscreenRT);

    g_pOffscreenRT->EndDraw();

    // 获取渲染结果的位图
    ID2D1Bitmap* pBitmap = nullptr;
    g_pOffscreenRT->GetBitmap(&pBitmap);

    // 将 D2D 位图转换为 GDI 位图用于 UpdateLayeredWindow
    // 需要获取位图像素数据
    D2D1_SIZE_U bitmapSize = pBitmap->GetPixelSize();
    UINT32 pixelCount = bitmapSize.width * bitmapSize.height;
    UINT32* pixels = new UINT32[pixelCount];

    D2D1_RECT_U rect = D2D1::RectU(0, 0, bitmapSize.width, bitmapSize.height);
    pBitmap->CopyPixels(rect, bitmapSize.width * 4, (BYTE*)pixels,
                        pixelCount * 4);

    // 创建 GDI 位图
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bitmapSize.width;
    bmi.bmiHeader.biHeight = -(LONG)bitmapSize.height;  // 自顶向下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    VOID* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS,
                                       &pBits, nullptr, 0);
    memcpy(pBits, pixels, pixelCount * 4);

    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // 使用 UpdateLayeredWindow 更新分层窗口
    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { (LONG)bitmapSize.width, (LONG)bitmapSize.height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    UpdateLayeredWindow(g_hwndLayered, hdcScreen, nullptr, &sizeWnd,
                       hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    // 清理
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    delete[] pixels;
    pBitmap->Release();
}
```

这段代码展示了一个完整的分层窗口 D2D 渲染流程。核心思路是：先用 D2D 在离屏渲染目标上绘制内容，然后把像素数据拷贝出来创建 GDI 位图，最后通过 UpdateLayeredWindow 更新到屏幕上。

中间的像素拷贝步骤确实有性能开销，但在实际测试中，对于 800x600 分辨率的窗口，这个开销大约在 1-2ms，完全可以接受。如果你对性能有极致要求，可以考虑使用 DXGI Surface 直接和 D2D 共享，但这会增加代码复杂度。

⚠️ 这里的一个关键点是 `BITMAPINFOHEADER` 的 `biHeight` 必须设为负值，表示自顶向下的位图。D2D 的坐标系是自顶向下的（y 轴向下），而 GDI 的 DIB 默认是自底向上的（biHeight > 0）。如果符号搞反了，你的图像会上下翻转。

另外注意 `BLENDFUNCTION` 结构的 `AlphaFormat` 字段必须设为 `AC_SRC_ALPHA`，这告诉 UpdateLayeredWindow 使用源位图中的 Alpha 通道进行混合。如果你忘记设这个标志，半透明效果会完全不工作。

## 第四步——互操作的性能考量

互操作虽然方便，但它确实引入了一些性能开销。我们来分析一下主要的开销来源和优化策略。

### DCRenderTarget 的性能特征

DCRenderTarget 的工作原理是：它在内部维护一个 D2D 位图，你在 D2D 上画的所有东西都先画到这个内部位图上，然后在 EndDraw 的时候，通过 GDI 的 BitBlt 或 AlphaBlend 把内部位图合成到目标 HDC 上。

这意味着每次 EndDraw 都会发生一次额外的位图拷贝。对于小面积区域来说这个开销可以忽略不计，但如果你把 BindDC 的矩形设为整个窗口大小（比如 1920x1080），每次拷贝就要移动大约 8MB 的数据。在 60 FPS 的动画场景下，这每秒就要 480MB 的带宽占用。

所以优化策略很明确：尽量缩小 BindDC 的矩形范围。只把你真正需要用 D2D 绘制的区域绑定到 DCRenderTarget，不要贪大。

### GetDC/ReleaseDC 的性能特征

每次 GetDC 都会 Flush D2D 的命令队列，而 Flush 意味着 CPU 要等待 GPU 完成所有待处理的绘制命令。这是一个同步点，会中断 GPU 的命令流水线。

优化策略是减少 GetDC/ReleaseDC 的调用次数。把所有 GDI 绘制操作集中在一起执行，而不是在 D2D 绘制之间反复切换。

### 纯 D2D 的终极方案

如果你的迁移已经完成，所有 GDI 代码都已经替换为 D2D，那就应该考虑去掉互操作层，直接使用 ID2D1HwndRenderTarget 或 ID2D1DeviceContext。这样可以完全消除 GDI/D2D 之间的数据拷贝和同步开销。

## 常见问题与踩坑总结

### BindDC 后什么都看不到

这是一个非常常见的问题。检查清单如下：确认你调用了 BeginDraw 和 EndDraw；确认 EndDraw 返回的是 S_OK；确认你在 BeginDraw 之前调用了 BindDC（不能在 BeginDraw 之后调用）；确认 BindDC 的矩形区域不是空矩形。

我见过最多的错误是在 WM_PAINT 处理函数中，先调用了 BeginDraw 再调用 BindDC。正确的顺序是 BindDC 在 BeginDraw 之前。

### GDI 和 D2D 的坐标系差异

D2D 使用浮点坐标，原点在左上角，y 轴向下。GDI 使用整数坐标，原点也在左上角，y 轴向下。坐标系本身是一致的，但精度不同——D2D 的浮点坐标可以精确到亚像素级别，而 GDI 的整数坐标会做像素对齐。在互操作场景下，如果你发现 D2D 绘制的内容和 GDI 绘制的内容有 1-2 像素的偏移，通常就是这个精度差异导致的。

### Alpha 混合的陷阱

GDI 对 Alpha 通道的支持非常有限。标准的 GDI 函数（FillRect、BitBlt 等）基本上都忽略 Alpha 通道，只有使用 AlphaBlend 函数才能正确处理半透明。而 D2D 从设计之初就全面支持 Alpha 通道。

在互操作场景下，如果你在 DCRenderTarget 上使用半透明画刷绘制内容，然后通过 BindDC 合成到 GDI 的 HDC 上，你需要确保合成方式正确。DCRenderTarget 的 EndDraw 会使用 AlphaBlend 来合成，所以半透明效果应该是正确的。但如果你的 GDI 代码在 D2D 之后又覆盖了那个区域，半透明效果就会被破坏。

### 设备丢失处理

在使用 DCRenderTarget 时，EndDraw 可能返回 D2DERR_RECREATE_TARGET。这通常发生在显卡驱动更新、系统休眠恢复、或者显存不足等场景。你的代码需要能够检测到这个错误并重建所有 D2D 资源。

一个简单的处理模式是：在检测到 D2DERR_RECREATE_TARGET 时，释放所有 D2D 资源（Factory、RenderTarget、Brush 等），设置一个标志，然后在下一帧重新创建。由于 GDI 的绘制不受影响，所以即使 D2D 暂时不可用，程序也不会崩溃，只是 D2D 绘制的部分会暂时空白。

### 多显示器和 DPI 缩放

如果你的应用需要支持多显示器和 DPI 缩放，互操作会变得更加复杂。GDI 对 Per-Monitor DPI 的支持很差（基本不支持），而 D2D 原生支持。在 DPI 缩放的显示器上，DCRenderTarget 绑定的 HDC 可能已经被系统缩放过，你需要确保 D2D 的 DPI 设置和 HDC 的 DPI 匹配，否则绘制内容会出现缩放偏差。

## 总结

这一篇我们覆盖了 Direct2D 和 GDI 互操作的两种方式和三个实战场景。

DCRenderTarget 是"在 GDI 中使用 D2D"的方式，它非常适合渐进迁移——你可以在现有的 WM_PAINT 处理函数中，选择特定的区域用 D2D 绘制，其余部分保持 GDI 不变。每次只替换一小块区域，逐步完成整个迁移。

GdiInteropRenderTarget 是"在 D2D 中使用 GDI"的方式，它适用于主框架已经迁移到 D2D 但偶尔需要调用 GDI 的场景。通过 GetDC/ReleaseDC 临时获取 HDC，完成 GDI 绘制后再回到 D2D。

两种方式的核心性能原则都是一样的：减少 D2D/GDI 之间的切换次数，尽量把同类绘制操作集中在一起。每次切换都会触发 Flush 和数据拷贝，频繁切换会严重影响性能。

互操作是迁移的桥梁，不是终点。当你完成迁移后，应该最终去掉 GDI 层，纯用 D2D 渲染。下一篇我们将进入 HLSL 的世界——Direct2D 的自定义效果让你能写自己的 GPU 着色器，这是 D2D 效果系统的终极扩展方式。

---

## 练习

1. **渐进迁移一个 GDI 程序**：找一个你之前写的使用 GDI 的 Win32 程序（或者写一个简单的），创建 DCRenderTarget，选择其中一个绘制区域（比如一个图表或一个自定义控件），用 D2D 重新实现那个区域的绘制，其余部分保持 GDI 不变。

2. **混合绘制性能对比**：分别用纯 GDI、纯 D2D（DCRenderTarget）和 GDI+D2D 混合三种方式绘制相同的复杂图形（比如包含多个圆角矩形、渐变、透明度的仪表盘），用 `QueryPerformanceCounter` 测量每种方式的帧时间，对比差异。

3. **分层窗口迁移**：创建一个 WS_EX_LAYERED 分层窗口，用 D2D 绘制一个带圆角和半透明效果的浮动面板，通过 UpdateLayeredWindow 显示。要求面板边缘有抗锯齿效果，半透明区域能看到底下的桌面。

4. **双向互操作演示**：创建一个窗口，上半部分用 D2D 绘制（包含渐变和抗锯齿图形），下半部分用 GDI 绘制（包含传统控件效果），中间用一条分隔线隔开。用 GdiInteropRenderTarget 在 D2D 区域绘制一个 GDI 边框，验证双向互操作都能正常工作。

---

**参考资料**:
- [Direct2D and GDI Interoperability Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-gdi-interoperation-overview)
- [ID2D1DCRenderTarget - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1dcrendertarget)
- [ID2D1Factory::CreateDCRenderTarget - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createdcrendertarget)
- [ID2D1GdiInteropRenderTarget - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1gdiinteroprendertarget)
- [ID2D1GdiInteropRenderTarget::GetDC - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1gdiinteroprendertarget-getdc)
- [ID2D1GdiInteropRenderTarget::ReleaseDC - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1gdiinteroprendertarget-releasedc)
- [Interoperability Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/interoperability-overview)
- [Windows with C++: Layered Windows with Direct2D - Microsoft Learn](https://learn.microsoft.com/en-us/archive/msdn-magazine/2009/december/windows-with-c-layered-windows-with-direct2d)
