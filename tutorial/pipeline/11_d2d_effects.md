---
title: 特效：Effect 图
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 特效：Effect 图

上一篇[《位图与离屏渲染》](10_d2d_bitmaps.md)收尾的时候，咱们把一道题留给了今天：高斯模糊、投影阴影这些 Effect 拿的原料就是位图，出来的又是一种派生的图像。本篇咱们就来接它。Direct2D 把这些像素级的加工收进了一个叫 Effect 的系统里，咱们的主线很直：认清楚它凭什么跑在 GPU 上，再把阴影、模糊、颜色矩阵几个常用的效果亲手串成一张图。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够。排在本篇后面的一篇是[《与 GDI 互操作》](12_d2d_interop.md)，咱们到那边接着看。事实的底本主要取自书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)，[第六篇《工厂、设备与渲染目标》](06_d2d_targets.md)搭好的对象模型咱们也直接拿来用。

## Effect 为什么不在老目标上

书架特效章开头的警告写得很重：`ID2D1Effect` 是 Direct2D 1.1 的一部分，咱们必须改用 `ID2D1DeviceContext`，前几篇一直在用的 `ID2D1HwndRenderTarget` 到不了 Effect 的门口。咱们在 Windows 8 迎来这套效果系统，装了平台更新（Platform Update for Windows 7）的 Windows 7 上咱们也能用上它，头文件也从 `d2d1.h` 扩出了 `d2d1_1.h` 加 `d2d1effects.h`。所以第六篇搭的创建路线，到本篇要走另一条了。

凭什么绑得这么死？咱们看书架给的解释。`ID2D1Effect` 底层依赖 Direct3D 设备去做 GPU 加速的图像处理：每一个 Effect 节点，说穿了就是一段跑在 GPU 上的像素着色器（pixel shader 的直译，意思是对每个像素各算一遍的小程序），咱们把节点连成图，等于把一堆着色器连同它们的调度顺序登记在了设备上。`ID2D1HwndRenderTarget` 内部其实也连着 D3D 设备，但它没把这套创建和管理的接口暴露出来。`ID2D1DeviceContext` 就不同了，它直接关联的是一个 `ID2D1Device`，而 `ID2D1Device` 封装的正是底下的 DXGI 设备与 D3D 设备。Effect 的家安在 device context 线上，背后有实在的机制。

## 把创建链条走通

创建的链条总共有五站，咱们按书架的顺序走：D3D 设备、DXGI 设备、D2D 工厂、D2D 设备，最后落在了 DeviceContext。头文件与链接的清单如下：

```cpp
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>       // ID2D1DeviceContext and the effect API
#include <d2d1effects.h>  // built-in effect CLSIDs and property enums
#include <dxgi1_2.h>      // IDXGIFactory2, CreateSwapChainForHwnd
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
```

全局的家当咱们摆出来，您跟第六篇的三个指针一比，就能看出新路线重了多少：

```cpp
static ID3D11Device*        g_pD3DDevice  = nullptr;
static ID3D11DeviceContext* g_pD3DContext = nullptr;  // required out-param, kept
static IDXGIDevice*         g_pDXGIDevice = nullptr;
static ID2D1Factory*        g_pFactory    = nullptr;
static ID2D1Device*         g_pD2DDevice  = nullptr;
static ID2D1DeviceContext*  g_pDC         = nullptr;
static IDXGISwapChain1*     g_pSwapChain  = nullptr;
static ID2D1Bitmap1*        g_pTarget     = nullptr;
static ID2D1Bitmap1*        g_pBitmap     = nullptr;  // decoded content, guarded create
```

五站咱们一口气看完，错误判断的样板照第六篇的 `FAILED` 宏走，中间省掉的就是它：

```cpp
D3D_FEATURE_LEVEL fl{};
HRESULT hr = D3D11CreateDevice(
    nullptr,                          // default adapter
    D3D_DRIVER_TYPE_HARDWARE,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT, // D2D requires BGRA-capable buffers
    nullptr, 0, D3D11_SDK_VERSION,
    &g_pD3DDevice, &fl, &g_pD3DContext);

hr = g_pD3DDevice->QueryInterface(
    __uuidof(IDXGIDevice), (void**)&g_pDXGIDevice);

hr = D2D1CreateFactory(
    D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pFactory);

hr = g_pFactory->CreateDevice(g_pDXGIDevice, &g_pD2DDevice);

hr = g_pD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_pDC);
```

`D3D11CreateDevice` 的 `D3D11_CREATE_DEVICE_BGRA_SUPPORT` 标志是硬要求，书架的原话写得很明白：少了它，后面的 D2D 设备创建就会失败，因为 D2D 要的正是 BGRA 格式的缓冲区。接着的 `QueryInterface` 从 D3D 设备身上问出 DXGI 设备，书架打的比方是一座桥：DXGI 站在 Direct3D 和显卡驱动的中间，资源的呈现归它管。第三站的工厂咱们已经熟了，还是设备无关的老朋友，程序启动的时候建一次、从头活到尾，第六篇讲过的脾气都算数。第四站的 `CreateDevice` 拿 DXGI 设备换回一个 `ID2D1Device`，包装就在这里完成了。第五站的 `CreateDeviceContext` 交出来的，就是本篇真正画画的目标。

咱们跟第六篇对照着看，差别就清楚了。`CreateHwndRenderTarget` 是工厂一步造出连着窗口的目标，而新路线把设备一层一层显式地过手，目标也不再天生连着任何窗口了，画到哪儿、铺什么画布，由咱们用 `SetTarget` 说了算。咱们接下来要做的，就是给它配一块能上窗口的画布。

### 交换链与目标位图

配画布用的组件叫交换链（swap chain），它管的是一组轮换的后台缓冲区，双缓冲的动机咱们在[第一篇《从状态到像素》](01_status_to_pixels.md)就讲过。窗口画面的呈现，在 device context 的路线上也归它管：

```cpp
RECT rc;
GetClientRect(hwnd, &rc);

DXGI_SWAP_CHAIN_DESC1 sc{};
sc.Width       = rc.right - rc.left;
sc.Height      = rc.bottom - rc.top;
sc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;  // the format D2D wants
sc.SampleDesc.Count = 1;
sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
sc.BufferCount = 2;
sc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
sc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;

IDXGIFactory2* pDXGIFactory = nullptr;
CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pDXGIFactory);
hr = pDXGIFactory->CreateSwapChainForHwnd(
    g_pD3DDevice, hwnd, &sc, nullptr, nullptr, &g_pSwapChain);
pDXGIFactory->Release();
```

这里有一处与书架原文的出入要交代：书架把交换链指针声明成了 `IDXGISwapChain*`，而 `CreateSwapChainForHwnd` 按签名交还的是 `IDXGISwapChain1*`，照书架的写法编译不过，上面的代码已按 SDK 的签名修正。第十篇《位图与离屏渲染》里咱们也照 SDK 签名修过书架示例的参数与输出参数位置，同类出入您在别的资料里还会见到，动手前对照一遍签名也就踏实了。

后台缓冲区还只是 DXGI 的表面，要变成 D2D 能画的画布，咱们得再包上一层：

```cpp
static HRESULT CreateTargetBitmap(UINT width, UINT height)
{
    SafeRelease(&g_pTarget);  // the old size is obsolete

    g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    IDXGISurface* pBack = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(
        0, __uuidof(IDXGISurface), (void**)&pBack);
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED));
    hr = g_pDC->CreateBitmapFromDxgiSurface(pBack, props, &g_pTarget);
    pBack->Release();
    if (FAILED(hr)) return hr;

    g_pDC->SetTarget(g_pTarget);
    return S_OK;
}
```

`D2D1_BITMAP_OPTIONS_TARGET` 说的是它可以被画，`D2D1_BITMAP_OPTIONS_CANNOT_DRAW` 说的是它不能反过来当输入去画别处，咱们手里的后台缓冲区就是这样的单向画布。最后那句 `SetTarget` 是点睛的地方：从此 device context 画的就是这块位图，而这块位图背靠交换链的后台缓冲区，等 `Present` 换了页，画好的内容就上了窗口。

`Present` 正是书架没贴的一步，咱们把它补上：书架的片段到 `SetTarget` 就停了，完整的程序在每帧 `EndDraw` 之后还得调一句 `g_pSwapChain->Present(1, 0)`，不调的话画了也白画，屏幕上什么都不会有。参数传 1 的意思是等垂直同步，这层讲究咱们不展开，[第二篇《屏幕、帧与垂直同步》](02_screen_frame_vsync.md)讲过它的来路。

### 老路线怎么相处

路线换了，前几篇的家当并没有作废。`ID2D1DeviceContext` 本身就是 `ID2D1RenderTarget` 的派生接口，`BeginDraw` 与 `EndDraw` 的章法、DIP（设备无关像素，第六篇定的坐标单位）坐标、画刷、几何、世界变换，咱们在它身上都能照旧用，书架也提醒过迁移的主要工作量就在创建那一步。两条创建路线就这么并存在同一个程序的世界里：第六篇的示例不必返工，而 Effect、`DrawImage`、`SetTarget` 这些新面孔，只出现在 device context 的路线上。

窗口尺寸的处置，路数跟第六篇的是相同的，手势换了名字。窗口的尺寸一变，咱们要调交换链的 `ResizeBuffers`，旧的目标位图要丢弃重建，咱们把两步并进了上面的 `CreateTargetBitmap`，`WM_SIZE` 里咱们用 `SetTarget(nullptr)` 把旧画布摘下来再调它重建，末了登记一次重画。

设备丢失这边要防的码不止一个。老路线上常见的 `D2DERR_RECREATE_TARGET` 仍然要防，而在 device context 的路线上，设备丢失更多以 `DXGI_ERROR_DEVICE_REMOVED` 的面目从 `EndDraw` 报出来（D3D 侧的设备被移除时就是它，咱们还可以用 `ID2D1Device` 的 `GetDeviceRemovedReason` 查）。示例里咱们判 `FAILED` 再重建，两个码就都盖住了，丢弃的名单也从目标一个扩到了整条链，从 D3D 设备到交换链全都进了重来的名单。

## Effect：收图出图的节点

路线通了，咱们回头认主角。Effect 的用法是一个黑盒：您喂它输入图像，用属性调它的参数，它交还处理后的图像。创建的入口挂在 device context 身上，认的是 CLSID（COM 里的类标识符，每种效果各有自己的 GUID）：

```cpp
ID2D1Effect* pBlur = nullptr;
HRESULT hr = g_pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pBlur);
pBlur->SetInput(0, pBitmap);
pBlur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 8.0f);
```

内建的效果有几十种，完整的清单在[官方文档的 Built-in Effects 页](https://learn.microsoft.com/en-us/windows/win32/direct2d/built-in-effects)，咱们挑最常用的五个认脸。示意代码里的 `pBitmap` 指的是任一张建好的输入位图，创建那一步咱们不展开，正式示例里它的名字是 `g_pBitmap`。

`CLSID_D2D1GaussianBlur` 对应的是高斯模糊，属性 `D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION` 给的是标准差，给的数越大就越模糊，8.0 大概是柔和投影的量级。`CLSID_D2D1Shadow` 对应的是投影，咱们拿它当本篇示例的主力：`D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION` 管的是影子的软硬，`D2D1_SHADOW_PROP_COLOR` 管的是颜色。书架示例里颜色传的是 `D2D1::ColorF`，而官方文档给阴影颜色标的类型是四个浮点的向量，咱们示例里按文档的型号传 `D2D1::Vector4F`，免得碰上运行时的类型检查。

`CLSID_D2D1Composite` 管的活是把两张图叠在一起，怎么叠，由 `D2D1_COMPOSITE_PROP_MODE` 属性说了算，默认的 `D2D1_COMPOSITE_MODE_SOURCE_OVER` 就是常见的透明混合。它的两个输入是分层的，书架的原话是这么讲的：索引 0 放的是底层，索引 1 放的是上层，跟 Photoshop 的图层一个道理。`CLSID_D2D1ColorMatrix` 对应的是颜色矩阵，拿一个 5×4 的矩阵对每个像素的 RGBA 做线性变换，去饱和、调色调靠的都是它，咱们看一眼灰度化的写法：

```cpp
ID2D1Effect* pMatrix = nullptr;
g_pDC->CreateEffect(CLSID_D2D1ColorMatrix, &pMatrix);
pMatrix->SetInput(0, pBitmap);

// luminance weights from the bookshelf chapter's exercise
D2D1_MATRIX_5X4_F gray = D2D1::Matrix5x4F(
    0.299f, 0.299f, 0.299f, 0.0f,
    0.587f, 0.587f, 0.587f, 0.0f,
    0.114f, 0.114f, 0.114f, 0.0f,
    0.0f,   0.0f,   0.0f,   1.0f,
    0.0f,   0.0f,   0.0f,   0.0f);
pMatrix->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, gray);
```

系数 0.299、0.587、0.114 是书架练习里给的亮度加权，人眼对绿色是最敏感的，所以绿的权重最大。矩阵的排法笔者按结构体的行是输入通道、列是输出通道落的，依据是官方文档对 `D2D1_MATRIX_5X4_F` 的说明，三行同样的加权叠进红绿蓝，出来的就是一张灰度图。

最后咱们认的是 `CLSID_D2D1Flood`。它输出的是一整片单色的图，它的属性咱们数来数去只有 `D2D1_FLOOD_PROP_COLOR` 一个，是五个里最省事的效果，咱们常拿它当合成时的底色层或蒙版的原料。

属性的用法上还有一个书架提醒过的隐患。`SetValue` 是模板化的，属性要浮点、咱们传了整数的话，编译是能过的，出来的行为却跟咱们预期的不一样。所以每回设属性，咱们都对着文档核一遍类型，别去赌隐式转换的运气。

## ID2D1Image：图里流通的通用货币

三个 Effect 为什么能串起来？因为它们之间流通的货是同一种。`ID2D1Image` 是 D2D 1.1 里图像类的根：`ID2D1Bitmap` 和 `ID2D1Bitmap1` 都是它的派生类，Effect 的输出也是它，`GetOutput` 能从一个 Effect 的手里把它拿出来。您把 Effect 图想成一张管道图，`ID2D1Image` 就是在管道里流的东西，位图是它的一个来源，Effect 的输出是另一些来源。

输入的两种给法，顺着这个身份就好分了。`SetInput(0, pBitmap)` 收的是 `ID2D1Image*`，位图既然是它的派生类，直接喂进去就行了。`SetInputEffect(0, pShadow)` 收的是另一个 Effect，它自动把对方的输出接了过来，书架强调的就是这对区别：把一个效果的输出接到另一个效果的输入时，咱们用 `SetInputEffect` 更省事。画到目标的那一头也是同一个道理，`DrawImage` 吃的是 `ID2D1Image`，所以 Effect 的输出能直接画，而 `DrawBitmap` 只吃 `ID2D1Bitmap`，这是两条调用分工的边界。

## 串成图：模糊的阴影垫在原图下面

零件齐了，咱们拼书架的经典例子：给一张位图加柔和投影。它拼出来的就是一张真正的 Effect 图（书架叫它 Effect Graph，直译是有向无环图的意思，管道里的流向不绕圈）：

```text
pBitmap ──→ Shadow ──→ 2DAffineTransform ──→ Composite ──→ DrawImage
pBitmap ─────────────────────────────────────→ Composite input 1
```

咱们沿着管道读。`Shadow` 吃的是原图，交出的是一张比原图大一圈的影子图（官方文档 Output bitmap 一节写明，整根轴的外扩量按模糊标准差乘 6 再乘 DPI 与 96 之比估，宽高各算各的，每一边摊到的也就是乘 3 的量），颜色和模糊度是咱们调的属性。可影子图跟原图是重叠的，不挪窝的话正好被原图盖住，所以中间加了 `CLSID_D2D12DAffineTransform`，咱们拿一个平移矩阵把影子挪到右下角。最后的 `Composite` 收两路输入：索引 0 接挪好的影子当底层，索引 1 里喂的是原图、当上层，叠出来的就是垫着影子的成品。三段加工的全程都跑在 GPU 上，等咱们调 `DrawImage` 的时候，整张图才真正地跑起来。

Effect 图咱们不只能照着示例串，节点怎么连、串几层都由咱们自己定。书架的性能提醒咱们也原样记下：每一个 Effect 节点在 GPU 上都是一次 pass，串十几个就是十几次的 pass，性能敏感的场景，咱们宁可把图拼浅一点。它的另一条建议是把多个效果合并成自定义的 HLSL 着色器效果，那咱们要实现 `ID2D1EffectImpl` 自己去注册，属于进阶的活，本篇咱们只当知道它存在。

## 模糊的边

动手之前还有一个细节值得咱们停下来看：模糊的输出会在边上淡掉。书架的说法是输出会往里缩一圈，咱们按官方文档把它修正过来。边框模式的默认值就是 `D2D1_BORDER_MODE_SOFT`，它把输入边界之外的像素当作透明黑，输出的位图反而按模糊核的大小向外扩一圈，图像的边缘于是淡出到透明黑，看上去像被吃掉了一条。咱们再看另一个档位 `D2D1_BORDER_MODE_HARD`，它在边界处镜像取样、把输出的尺寸夹回跟输入一样。想让边缘的内容不淡掉，办法是让输入图比实际需要的大一圈，留出模糊的余量。咱们给投影用 `Shadow` 时倒不用操心，咱们的影子反正要被原图盖住大半，边上淡出去的部分根本看不见。

## 示例：给一张位图加柔和投影

咱们把整条路拼成程序。设备的骨架就是前面的那几段代码，咱们把它装进 `CreateDeviceResources` 里，指针判空的守卫照第六篇的写法，咱们不在这里重贴。画的内容咱们选一张从 PNG 文件解码来的位图，解码的链条跟第十篇《位图与离屏渲染》讲的完全一样：WIC 工厂、解码器、帧、格式转换器，一路归一到 PBGRA（预乘 alpha 的 BGRA 像素格式）。咱们把第十篇的解码链包成了一个函数 `CreateContentBitmap`，建好的位图交给全局的 `g_pBitmap`（家当清单里咱们新添的），它内部带指针为空才建的守卫，OnPaint 里咱们每帧调它也不会重复解码。差别只在收尾的那一步，位图建在 `g_pDC` 的身上，交还的类型是 `ID2D1Bitmap1`，书架特效章的 `LoadBitmapFromFile` 交还的正是它，咱们照它办。

每帧的重活都在 OnPaint 里，咱们分两段看。头一段建的是图：

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);
    CreateContentBitmap();  // WIC chain from article 10, built on g_pDC
    if (g_pDC == nullptr || g_pBitmap == nullptr) {
        return;
    }

    g_pDC->BeginDraw();
    g_pDC->Clear(D2D1::ColorF(0.10f, 0.12f, 0.18f, 1.0f));

    // node 1: a soft silhouette of the bitmap
    ID2D1Effect* pShadow = nullptr;
    g_pDC->CreateEffect(CLSID_D2D1Shadow, &pShadow);
    pShadow->SetInput(0, g_pBitmap);
    pShadow->SetValue(D2D1_SHADOW_PROP_COLOR,
                      D2D1::Vector4F(0.0f, 0.0f, 0.0f, 0.5f));
    pShadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, 8.0f);

    // node 2: nudge the shadow to the lower right
    ID2D1Effect* pOffset = nullptr;
    g_pDC->CreateEffect(CLSID_D2D12DAffineTransform, &pOffset);
    pOffset->SetInputEffect(0, pShadow);
    pOffset->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM,
                      D2D1::Matrix3x2F::Translation(12.0f, 16.0f));

    // node 3: shadow underneath, bitmap on top
    ID2D1Effect* pComposite = nullptr;
    g_pDC->CreateEffect(CLSID_D2D1Composite, &pComposite);
    pComposite->SetInputEffect(0, pOffset);
    pComposite->SetInput(1, g_pBitmap);
```

三个节点跟示意图是一一对应的。影子颜色的透明度咱们给了 0.5，半透明的黑影垫在深色背景上才不发死。平移量咱们给的是 12 和 16 个 DIP，方向朝的是右下，这是投影的惯例朝向。

咱们收尾的一段把图画出去、把家什收好：

```cpp
    g_pDC->DrawImage(pComposite, D2D1::Point2F(60.0f, 60.0f));

    HRESULT hr = g_pDC->EndDraw();

    pComposite->Release();
    pOffset->Release();
    pShadow->Release();

    if (FAILED(hr)) {  // covers D2DERR_RECREATE_TARGET and DXGI_ERROR_DEVICE_REMOVED
        DiscardDeviceResources();  // the whole chain from D3D device down
        InvalidateRect(hwnd, nullptr, FALSE);
        return;  // skip Present, the swap chain went with the device
    }

    g_pSwapChain->Present(1, 0);  // vsync-aligned page flip
}
```

Effect 的创建咱们跟书架一样，走的是每帧建、用完就释放的路子。好处是设备丢失的丢弃名单里不用记它们，重建时也不用管旧图怎么清理，代价是每帧多出几次创建的调用，对咱们单张图的示例毫无压力。窗口的消息分支补两条就齐了：

```cpp
case WM_PAINT:
    ValidateRect(hwnd, nullptr);  // article 06: no BeginPaint on this route
    OnPaint(hwnd);
    return 0;

case WM_SIZE:
    if (g_pSwapChain != nullptr) {
        g_pDC->SetTarget(nullptr);
        CreateTargetBitmap(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
```

`WM_PAINT` 里销重画登记的动作，第六篇讲过它的来路：Direct2D 不碰 HDC 的面板，登记下来的请求咱们自己用 `ValidateRect` 销掉。`WM_SIZE` 里的那句 `SetTarget(nullptr)`，干的活是把画布摘下来再重建，免得旧目标还挂在上下文的身上碍事。

## 您可以动手的三处

示例是笔者按书架的成稿拼的，咱们自己验自己。下面的三处，每一处咱们都只改一行、编一遍：

1. 咱们把影子的 `D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION` 从 8.0f 改成 2.0f，再改成 24.0f。影子从贴着原图的硬边，一路软成一片雾，标准差和观感的关系您就摸到了。
2. 咱们把 `Composite` 的两路输入对调，索引 0 喂原图、索引 1 接影子。图层顺序一换，半透明的黑影就糊在原图上面，压住的那层的概念您就亲眼看到了。
3. 咱们把平移矩阵改成 `D2D1::Matrix3x2F::Translation(0.0f, 0.0f)`。不透明的图会把影子挡得严严实实，您再换一张带透明通道的 PNG 进来，影子就从透明的窟窿里透出来了，Shadow 拿透明度当原料这件事，它自己会演示给咱们看。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `ID2D1HwndRenderTarget` 为什么用不了 Effect？差别落在哪个对象身上？
2. DeviceContext 的创建链条依次经过哪几个对象？`D3D11_CREATE_DEVICE_BGRA_SUPPORT` 漏了会怎样？
3. 交换链路线的窗口尺寸变了，咱们要补做哪几件事？`Present` 不调会怎样？
4. `SetInput` 与 `SetInputEffect` 各收什么类型的参数？什么时候用哪个？
5. `Composite` 的输入索引 0 和 1 各是什么位置？默认的叠法是哪一种？
6. `ID2D1Image` 在 Effect 图里扮演什么角色？哪几类对象能当图的输入？
7. 高斯模糊的输出在边缘为什么会淡出到透明黑？SOFT 与 HARD 两个边框模式各是什么行为？
8. 灰度矩阵里三行系数的来历是什么？为什么绿的权重最大？

## 下一步

颜色矩阵咱们只演示了灰度化，它还能做的远不止这些：去饱和、色相旋转、整体压低透明度，这些活用的都是同一张矩阵，您可以拿它当本篇的延长练习。书架特效章的后半场还讲了一套图层系统，`PushLayer` 与 `PopLayer` 管的是裁剪与透明度，跟 Effect 是两套分工不同的工具，本线的后面还排着《图层与裁剪》一篇，咱们到那边再接。紧挨着的下一篇是[《与 GDI 互操作》](12_d2d_interop.md)，讲的是 `ID2D1DCRenderTarget` 怎么把 Direct2D 的画面合进 GDI 的 HDC，咱们到那边去看它。
