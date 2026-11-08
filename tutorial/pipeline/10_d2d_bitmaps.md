---
title: 位图与离屏渲染
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 位图与离屏渲染

上一篇[《坐标变换》](09_d2d_transforms.md)的结尾咱们留过一个欠着的题目：`Clear` 铺的纯色和画刷的渐变都算现算出来的颜料，而从图片文件解码出来的现成像素怎么进画面，咱们当时没有展开。本篇咱们就来接它。本篇的主角是 `ID2D1Bitmap`，也就是 Direct2D 里的位图对象，咱们的主线也很直，就是把位图的来路和去路各走到头。

来路有三条：咱们可以从内存的像素数组直接建，可以从解码好的图片文件建，也可以在屏幕看不见的离屏画布上画出来。去路有两条：把成品位图一张张地贴上画面，或者把它当画刷的颜料铺出去。

读者设定照旧，您会 C++ 和 Win32 的基础用法就够了。本篇会提到排在后面的[《特效：Effect 图》](11_d2d_effects.md)一篇，细节您到那边读。

## 位图住在显存里

[第六篇《工厂、设备与渲染目标》](06_d2d_targets.md)把 Direct2D 的资源分成了设备无关与设备相关两类，判据咱们还记得：看创建函数挂在谁身上。位图的创建函数全都挂在渲染目标的身上，从 `CreateBitmap`、`CreateBitmapFromWicBitmap` 一路到本篇后半场的 `CreateCompatibleRenderTarget`，所以位图稳稳地归在设备相关的一侧。第六篇当时还给过一句直白的话：位图干脆就是一块显存。显存就是 GPU 自己的那块内存：CPU 的活是往里搬数据，而 GPU 用的时候直接从里面取。位图对象背后管的正是这样一块存储：像素数据进了显存，画它的时候 GPU 连着显存采样，中间也就不再劳驾 CPU 了。

设备相关的身份带来两条后果，咱们在画刷的身上都见过。位图生下来就绑在创建它的渲染目标对应的设备上，A 目标建的位图不能拿到 B 目标去画。另一条后果出在设备丢失的时刻：`EndDraw` 报回 `D2DERR_RECREATE_TARGET` 的时候，位图也就跟着渲染目标一起作废了，重建名单里也有它的一份。[第七篇《画刷：纯色、渐变、位图》](07_d2d_brushes.md)收尾的时候咱们留过话：位图怎么接进“指针为空才建”的守卫回路，咱们把活儿留给了位图篇，示例一节咱们再把它落到代码上。

## 三条来路

位图对象怎么到手，Direct2D 给的路有三条，咱们按像素的来处分头看。头两条搬的是现成的像素，第三条路连像素都是咱们自己画的。

### CreateBitmap：从内存数组建

最直接的一条路咱们其实已经走过。[第七篇](07_d2d_brushes.md)造棋盘格画刷的时候，`CreateBitmap` 收的原料就是一个 `UINT32` 数组：位图的尺寸、像素数据、每行占的字节数（pitch，咱们后文就管它叫行距），再配上了一份属性结构，位图就建出来了。像素格式那里咱们当时选了 `D2D1_ALPHA_MODE_IGNORE`，咱们只一句带过。属性结构里还装着什么，本篇在后面的章节里单独讲，它管的还不止格式。

### 从图片文件来：WIC 解码

图片文件到 `ID2D1Bitmap` 之间隔着一段解码的旅程，干活的组件叫 WIC（全称 Windows Imaging Component，也就是 Windows 自带的图像编解码组件），PNG、JPEG 这些格式的解码都归它管。咱们要认的链条在书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)里有现成的成稿，解码的几步咱们原样搬：

```cpp
IWICImagingFactory* pWICFactory = nullptr;
CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                 IID_PPV_ARGS(&pWICFactory));

IWICBitmapDecoder* pDecoder = nullptr;
HRESULT hr = pWICFactory->CreateDecoderFromFilename(uri, nullptr,
    GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);

IWICBitmapFrameDecode* pFrame = nullptr;
hr = pDecoder->GetFrame(0, &pFrame);

IWICFormatConverter* pConverter = nullptr;
hr = pWICFactory->CreateFormatConverter(&pConverter);
hr = pConverter->Initialize(pFrame,
    GUID_WICPixelFormat32bppPBGRA,  // D2D 最喜欢的格式
    WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
```

咱们把 WIC 这边的四站念一遍。`CoCreateInstance` 拿的是 WIC 的工厂，跟 Direct2D 的工厂一个道理，别的家伙的创建也都要从它的手里过，工厂本身还要求 COM 已经初始化过（就是 `CoInitializeEx` 的那一步，程序在启动的时候做一次）。`CreateDecoderFromFilename` 干的活是按文件名开一个解码器，它认的是文件的容器格式。`GetFrame(0)` 取的是头一帧，GIF 那样多帧的文件里，第 0 帧就是它的封面。`IWICFormatConverter` 做的是格式归一：PNG 解出来也许是别的位深，JPEG 解出来的往往是没带透明通道的 24 位 RGB，转换器把它们的像素统一到 `GUID_WICPixelFormat32bppPBGRA` 名下，书架的注释写得很直白，说它是 D2D 最喜欢的格式（D2D 就是 Direct2D 的缩写，后文偶尔这么写）。

PBGRA 对应的是 `DXGI_FORMAT_B8G8R8A8_UNORM` 加预乘 alpha。预乘说的是：进显存的像素，红绿蓝三个分量已经各自乘过了它自己的透明度，合成的时候 GPU 算得更省事。您不用自己动手去乘，WIC 的转换器在归一格式的那一步就办好了。

链条的最后一站是把转换好的像素交给 Direct2D，咱们看收尾的写法：

```cpp
D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                      D2D1_ALPHA_MODE_PREMULTIPLIED),
    0.0f, 0.0f);  // DPI 0: follow the WIC source's own resolution
hr = pRenderTarget->CreateBitmapFromWicBitmap(pConverter, props, &pBitmap);
```

这里有一处版本的事咱们得交代在前面。书架特效章的示例走的是 Direct2D 1.1 的路线：目标是 `ID2D1DeviceContext`，位图的类型换成了 `ID2D1Bitmap1`，属性的结构也换成了 `D2D1_BITMAP_PROPERTIES1`。本系列的主线是第六篇搭的 `ID2D1HwndRenderTarget`，它身上的 `CreateBitmapFromWicBitmap` 重载收的是转换好的 `IWICBitmapSource`（`pConverter` 就是它的派生接口），交还的是 `ID2D1Bitmap`。上面这段收尾的写法是笔者按 SDK 的接口签名落的，笔者的依据是官方文档。解码那几步跟书架特效章的成稿一致，只有开头 `CoCreateInstance` 建 WIC 工厂的几行是另借的，出自书架的[纹理与采样器一章](../native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md)，特效章自己是把工厂当参数传进来的。两代接口的名字并存，您在别的资料里两样都会见到，机制倒是同一套。

构建那边咱们还有要补的：头文件是 `wincodec.h`，要链接的库是 `windowscodecs.lib`，书架 D3D 纹理章（就是上面链接的那篇）的环境清单里写的就是它。

### 画出来：CreateCompatibleRenderTarget

第三条路跟头两条的都不一样：像素本身也是咱们画的。渲染目标的身上还备着一个 `CreateCompatibleRenderTarget`，它交还的也是一个能画图的目标，接口的名字是 `ID2D1BitmapRenderTarget`，咱们后文就叫它离屏目标。咱们在它上面照常 `BeginDraw`、画内容、`EndDraw`，画完了之后咱们一句 `GetBitmap`，整幅的结果就变成了一张现成的 `ID2D1Bitmap`。这就是离屏渲染：画在一块屏幕看不见的画布上。动机和用法咱们留到本篇后半场，到时候咱们用一整节来讲。

## D2D1_BITMAP_PROPERTIES：格式与 DPI

三条路里只有 `CreateBitmap` 要咱们必填一份属性结构，WIC 与离屏两条路把它省成了可选项，咱们带不带都行。`D2D1_BITMAP_PROPERTIES` 的成员一共三个，咱们按格式与 DPI 两块看。`pixelFormat` 管的是像素怎么存，它本身又是个两层的小结构：`format` 是 `DXGI_FORMAT` 枚举，说的是每个像素多少位、通道怎么排，咱们一路用的都是 `DXGI_FORMAT_B8G8R8A8_UNORM`（蓝绿红透明各自占 8 个位，凑成的是 32 位）。`alphaMode` 说的是透明通道怎么参与合成，常用的两个值咱们都见过面了：完全不透明的图用 `D2D1_ALPHA_MODE_IGNORE`，要透明的图用 `D2D1_ALPHA_MODE_PREMULTIPLIED`。`dpiX` 与 `dpiY` 管的是位图自带的分辨率，咱们马上讲它。

格式这里有一条实打实的后果：要是不支持的格式传了进去，创建的调用就直接失败，报回来的 `HRESULT` 是 `D2DERR_UNSUPPORTED_PIXEL_FORMAT`。渲染目标的类型不同，收的格式清单也不完全一样，咱们不去背清单，出了问题的时候拿错误码对着文档查就行。

属性里的 DPI 字段咱们单独看，接的是[第三篇《坐标、DIP 与 DPI》](03_dip_and_dpi.md)里讲过的换算。位图身上记着自己的一份 DPI，它决定着 `GetSize()` 与 `GetPixelSize()` 还回来的两个数：`GetPixelSize` 还的是纯像素个数，`GetSize` 还的是 DIP 尺寸，两者的换算咱们在第三篇里讲过，DIP 的数乘上 DPI 再除以 96，得的就是物理像素。帮手函数 `D2D1::BitmapProperties(pixelFormat)` 填的默认 DPI 是 96，此时的 DIP 尺寸和像素个数一样大。走 WIC 的来路时，属性里的 DPI 给 0，用的就是 WIC 来源自带的分辨率，图片元数据里写了多少就用多少，官方文档也写明了这一点。

## DrawBitmap：把位图贴上画面

位图到了手里，上画面的调用是 `DrawBitmap`。[第七篇](07_d2d_brushes.md)开场咱们说过它是“另一条路，本篇不展开”，咱们现在展开。它的参数表咱们摆开：

| 参数 | 管什么 |
|:---|:---|
| `bitmap` | 画的那张位图 |
| `destinationRectangle` | 贴到目标画面的哪块矩形，坐标单位是 DIP，与源矩形尺寸不一致时位图会被缩放 |
| `opacity` | 整张位图的不透明度，0.0 到 1.0，与每个像素自己的 alpha 相乘 |
| `interpolationMode` | 缩放时怎么采样：`NEAREST_NEIGHBOR` 取最近的像素，`LINEAR` 在邻近像素之间插值 |
| `sourceRectangle` | 只取位图的哪一块，裁子图用，传 `NULL` 就是整张 |

两个矩形是参数表的骨架，咱们多看两眼。源矩形裁的是位图本身：一张精灵图（精灵图的英文叫 sprite sheet，把许多小图拼在一张大图上的打包方式）里取左上角那 32×32 的一小块，靠的就是它。目标矩形定的是落点：尺寸给得跟源矩形的不一样，位图就会被缩放了再贴上去，插值模式管的就是缩放时怎么取色。`opacity` 的规则您在第七篇已经领教过：透明度是逐层相乘的，位图像素自带的 alpha 是一层，这里的 opacity 又是一层，两个 0.5 乘出来的只剩 0.25。

插值的两个值，第七篇在位图画刷的那边讲过同一对，道理也是相通的：`NEAREST_NEIGHBOR` 放大出来的是干脆的色块，`LINEAR` 放大出来的是柔和的过渡。像素风的小图要的就是锐利，照片缩放要的就是平滑，咱们选哪个，看的是内容。

还有一条跟上一篇的接缝值得点出来：`DrawBitmap` 也吃目标上挂着的世界变换。目标矩形在落到画面之前会过一遍 `SetTransform` 设上去的矩阵，所以咱们画一张朝向固定的位图、把转身的活交给矩阵就行，跟表盘指针用的是同一个思路，上一篇的变换机制在这边照常适用。

## 离屏渲染：画一次，贴每帧

`CreateCompatibleRenderTarget` 认完了，咱们回头补动机。您可能记得[第一篇《从状态到像素》](01_status_to_pixels.md)里的双缓冲：GDI 直画窗口 DC 的时候会露出“背景刷了一半”的中间态，所以整帧在内存位图里组好，再用一句 `BitBlt` 把整帧搬了过去，图的是防闪烁。到了 Direct2D 这边，`ID2D1HwndRenderTarget` 自己连着 DWM 的重定向表面（第一篇第六站讲的就是它），中间态咱们本来就看不见，防闪烁的动机在这儿也就没了用场。咱们给 DWM 补一句背景：它全称是桌面窗口管理器，Windows 里管各窗口最终合成上屏的就是这个组件。

离屏在 D2D 里的正当动机换成了省重画。界面里常有那么一大块内容，画起来是贵的，而内容又不怎么变：一张多段渐变的底图、一片带圆角和阴影的卡片背景。咱们每帧重画它，画出来的像素跟上一帧一模一样，贵的活就白白重复了。咱们把它画上一次、缓存成位图，帧循环里剩下的只是一句 `DrawBitmap`，贵的活就这么从每一帧里挪了出去，只在内容真变了的时候重画一回。

离屏的用法，书架的[互操作一章](../native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md)里有现成的成稿，它在书里的用场是给分层窗口供像素，咱们把骨架摘出来：

```cpp
D2D1_SIZE_F size = g_pRT->GetSize();
D2D1_PIXEL_FORMAT fmt = D2D1::PixelFormat(
    DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
g_pRT->CreateCompatibleRenderTarget(
    &size, nullptr, &fmt,
    D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
    &g_pOffscreenRT);

g_pOffscreenRT->BeginDraw();
g_pOffscreenRT->Clear(D2D1::ColorF(0, 0, 0, 0));  // transparent start
// ... draw the expensive content ...
g_pOffscreenRT->EndDraw();

ID2D1Bitmap* pBitmap = nullptr;
g_pOffscreenRT->GetBitmap(&pBitmap);
```

咱们把几处要紧的读一遍。尺寸参数的量纲是 DIP（`D2D1_SIZE_F`），书架传的就是创建它的那个渲染目标的 `GetSize()`。咱们省掉第三位的像素格式参数（`D2D1_PIXEL_FORMAT`）的时候，离屏目标沿用的就是创建它的那个渲染目标的格式，书架这里显式给了预乘 alpha 的格式，因为它的去处是 `UpdateLayeredWindow`，那边的硬需求就是透明。还有一处与书架原文的出入要交代：书架把输出参数 `g_pOffscreenRT` 写在了调用的头一位，还往参数里塞了一个位图属性结构，SDK 的签名里压根没有属性结构这个参数，参数的排布是尺寸、像素格式、选项在前，输出参数收在了末位，照书架那样写编译不过，上面的代码已按签名修正。`GetBitmap` 还有个容易漏的脾气：咱们每调它一次，交出来的都是一个新建的位图对象，引用计数记在咱们的头上，用完了得调一次 `Release`，书架的示例在收尾处就老老实实地释放了。

咱们跟 GDI 对照着看就更清楚了。同样的活在 GDI 里是个三件套：`CreateCompatibleDC` 造的是内存画板，`CreateCompatibleBitmap` 造的是内存位图，`SelectObject` 干的是把位图装进画板的活，画完了再拿 `BitBlt` 搬上窗口，书架的[GDI 位图一章](../native_win32/22_ProgrammingGUI_NativeWindows_GDI_Bitmap.md)和[双缓冲一章](../native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md)整篇讲的就是它。D2D 把内存画布和成品位图打包成了一个对象：离屏目标自己当的就是画板，`GetBitmap` 干的就是搬运，中间的配对纪律（GDI 那边选中的对象删不掉的限制）也就从语法上消失了。

生命周期上咱们照旧走设备资源的路数。`ID2D1BitmapRenderTarget` 和它交出来的位图都是设备相关资源，设备丢失的 `D2DERR_RECREATE_TARGET` 一报回来，两个都得进丢弃的名单，等下一轮的绘制再把它们建回来。还有一条独立的：离屏目标的尺寸在创建的时候就定下来了，窗口拉大了它不会自己跟上来，跟第六篇 `Resize` 的道理是同一个路数，咱们的做法是窗口一变就把缓存丢弃重建，示例里就是这么写的。

## 示例：渐变底图缓存，每帧只贴

咱们把机制拼成一个完整的小程序：一张两段渐变的底图离屏画好、缓存成位图，每帧咱们只贴它，上面再跑一个来回弹的小方块。设备一侧的骨架沿用第六篇那套 `CreateDeviceResources` 与 `DiscardDeviceResources` 回路，纯色刷（指针叫 `g_pSolid`）照第七篇的样子建在守卫里，丢弃它走的是 `DiscardBrushResources`，这两段咱们不重贴。新增的家当是两个指针：

```cpp
static ID2D1BitmapRenderTarget* g_pOffRT    = NULL;
static ID2D1Bitmap*             g_pBackdrop = NULL;
```

缓存的创建咱们单写成了一个函数，进门的地方照旧查指针：

```cpp
static void CreateBackdropResources(ID2D1RenderTarget* pRT, const D2D1_SIZE_F& size)
{
    if (g_pBackdrop != NULL) {
        return;  // cached already
    }

    HRESULT hr = pRT->CreateCompatibleRenderTarget(size, &g_pOffRT);
    if (FAILED(hr)) {
        return;
    }

    // Resources belong to the target they were created on, so the
    // gradient for the offscreen content is built on the offscreen
    // target itself.
    ID2D1GradientStopCollection* pStops = NULL;
    ID2D1LinearGradientBrush*    pGrad  = NULL;
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, D2D1::ColorF(0.10f, 0.12f, 0.18f, 1.0f) },
        { 1.0f, D2D1::ColorF(0.35f, 0.45f, 0.65f, 1.0f) },
    };
    g_pOffRT->CreateGradientStopCollection(
        stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStops);
    g_pOffRT->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(0.0f, 0.0f), D2D1::Point2F(0.0f, size.height)),
        pStops, &pGrad);

    g_pOffRT->BeginDraw();
    g_pOffRT->FillRectangle(
        D2D1::RectF(0.0f, 0.0f, size.width, size.height), pGrad);
    HRESULT hrDraw = g_pOffRT->EndDraw();

    pGrad->Release();
    pStops->Release();

    if (SUCCEEDED(hrDraw)) {
        g_pOffRT->GetBitmap(&g_pBackdrop);
    }
}
```

渐变轴咱们直接按整块底图摆正（起点选的是顶边、终点选的是底边），咱们在第七篇里领教过“轴不跟形状走”，它的教训在这里不成立，因为被填充的矩形就是整张画布，两边的坐标天然是对齐的。这段代码里最要紧的一行注释说的是画刷建在谁身上：画刷是设备相关资源，咱们给离屏内容配的渐变刷就建在离屏目标自己的身上，用完了就释放，不去做跨目标的串门。

丢弃的函数与窗口尺寸的处理，咱们一起看。释放用的 `SafeRelease` 是第六篇示例里就有的帮手，干的活是把指针释放掉并置成空：

```cpp
static void DiscardBackdropResources()
{
    SafeRelease(&g_pBackdrop);
    SafeRelease(&g_pOffRT);
}

// ...
case WM_SIZE:
    if (g_pRenderTarget != NULL) {
        g_pRenderTarget->Resize(
            D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
        DiscardBackdropResources();  // the cache holds the old size
        InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
```

`WM_SIZE` 里丢弃缓存是本示例的一个关键动作：底图的内容跟窗口尺寸相关，尺寸一变缓存就过期了，咱们宁可下一轮重画一次，也不留着一张尺寸不对的旧图。

每帧咱们要干的活就轻了：

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);   // same skeleton as article 06
    if (g_pRenderTarget == NULL) {
        return;
    }

    const D2D1_SIZE_F size = g_pRenderTarget->GetSize();
    CreateBackdropResources(g_pRenderTarget, size);

    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.08f, 0.08f, 0.10f, 1.0f));

    // The expensive backdrop is one call per frame:
    g_pRenderTarget->DrawBitmap(g_pBackdrop,
        D2D1::RectF(0.0f, 0.0f, size.width, size.height), 1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);

    // Cheap dynamic content on top: one bouncing square.
    g_pSolid->SetColor(D2D1::ColorF(D2D1::ColorF::CornflowerBlue));
    g_pRenderTarget->FillRectangle(
        D2D1::RectF(g_squareX, g_squareY,
                    g_squareX + 24.0f, g_squareY + 24.0f),
        g_pSolid);

    HRESULT hr = g_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardBrushResources();
        DiscardBackdropResources();
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}
```

`DrawBitmap` 的参数表咱们按新学的填全了：目标矩形铺的是整个画布，opacity 给的是 1.0，插值用的是 `LINEAR`，源矩形传的是 `NULL`、取整张。弹跳小方块的位置 `g_squareX` 与 `g_squareY` 由定时器推进，驱动跟第九篇表盘用的是同一个路数：

```cpp
SetTimer(hwnd, 1, 16, NULL);  // same SetTimer mechanism as article 09, 16 ms tempo

// ...
case WM_TIMER:
    // advance g_squareX / g_squareY and bounce at the edges here
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
```

设备丢失的处置也齐了，丢弃名单从第六篇的目标、第七篇的画刷，一路扩到了今天的离屏目标与缓存位图，咱们在每个丢弃函数的内部都按创建的逆序释放，一轮 OnPaint 就把它们全建了回来。

## 平铺归画刷，贴图归 DrawBitmap

位图有了两条去路，什么时候走哪一条，咱们把分工说定。位图画刷的路子是把位图当颜料使，第七篇的棋盘格就是现成的例子：`extendModeX` 和 `extendModeY` 给的都是 `WRAP`，位图就被当成瓷砖一块块地铺满被填充的区域。画刷的身上还带着自己的矩阵（第九篇里说过，目标矩阵挪的是画布，画刷矩阵挪的是颜料），平铺的图案还能整体挪窝和缩放。`DrawBitmap` 走的是一次性贴图的路：贴的图是一整张，落点定的是一个，连缩放带透明度也是一并给的。

判断法咱们这么记：位图要重复出现、填一片区域的时候，咱们就用画刷。位图本身是成品、一次贴到位的时候，咱们就用 `DrawBitmap`。今天的底图缓存正是后者的常客，而棋盘格要是只想在角落亮一次，它该走的同样是 `DrawBitmap`。

## 您可以动手的三处

示例是笔者写的，咱们自己验自己。下面的三处观察，每一处咱们只要改上一行、编上一遍：

1. 咱们把 `CreateCompatibleRenderTarget` 的尺寸参数改成 `D2D1::SizeF(size.width * 0.5f, size.height * 0.5f)`，缓存就只有半个窗口大了，`DrawBitmap` 也会把它拉伸了铺满。您再把插值模式换成 `NEAREST_NEIGHBOR`，渐变的过渡带上会显出一条一条的色阶，两种采样的差别就摆在眼前了。
2. 咱们把 `WM_SIZE` 里的 `DiscardBackdropResources()` 注释掉，再把窗口拉大。底图被拉伸到新的尺寸，渐变的整个跨度跟着被拽长了，咱们一眼就看出来缓存还停在旧窗口的大小上，尺寸过期还不丢弃的下场就是这样。
3. 咱们把 `DrawBitmap` 的 opacity 从 1.0f 改成 0.4f。`Clear` 铺的深色会从底下透上来，整个底图都变暗了，整张位图的透明度乘法，跟第七篇画刷那边的是同一条规则。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 位图归哪一类资源？判据是什么？
2. 位图的三条来路各是什么？各举一个创建函数。
3. WIC 的链条从工厂到转换器要走哪几步？像素格式统一到哪一个？
4. `D2D1_BITMAP_PROPERTIES` 的格式与 DPI 各管什么？走 WIC 的来路时 DPI 给 0，用的是什么分辨率？
5. `DrawBitmap` 的五个参数各管什么？源矩形和目标矩形尺寸不一致时会怎样？
6. 离屏缓存的动机与第一篇 GDI 双缓冲的动机差在哪儿？
7. `GetBitmap` 交出来的位图要不要 `Release`？窗口尺寸变了，离屏目标怎么办？
8. 平铺的背景和一次性的贴图，各该走哪条去路？

## 下一步

位图到了手、缓存好了之后，Direct2D 还能替咱们给这块像素再加工：高斯模糊、投影阴影这些 Effect 拿的原料就是位图，出来的又是一种派生的图像，书架特效章里 `CLSID_D2D1GaussianBlur` 的示例已经露过头。下一篇[《特效：Effect 图》](11_d2d_effects.md)就把模糊和阴影的接口接着讲下去。弹跳的小方块要是还没玩够，您可以给它接上今天学的源矩形，从一张拼好的精灵图里取帧，动画就真的一段一段跑起来了。
