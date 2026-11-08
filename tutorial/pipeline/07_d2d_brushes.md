---
title: 画刷：纯色、渐变、位图
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 画刷：纯色、渐变、位图

上一篇[《工厂、设备与渲染目标》](06_d2d_targets.md)的结尾咱们说过，画刷在示例里的身份一直是工具人：咱们创建它、传它、看它当参数，咱们却始终没有正眼看过它。本篇咱们就来补上这一课。

要问的问题其实很具体：画面上的颜色是从哪儿来的？您把 Direct2D 绘制方法的参数表挨个翻一遍，咱们能看出一条清晰的分界：直接收颜色值的方法只有 `Clear` 一个，而 `FillRectangle`、`DrawLine`、`DrawText` 这些真正往画面上落颜色的方法，收的全是 `ID2D1Brush*`。想画带颜色的形状、线条与文字，咱们手里都得有一把画刷。直接把位图像素铺上画面的 `DrawBitmap` 是另一条路，咱们本篇不展开它。

本篇沿着颜料从简单到复杂的顺序走，从纯色的画刷一路讲到位图的画刷，径向渐变咱们在半路上只带一句，末尾再收 opacity 的乘法关系与画刷作为设备相关资源的复用策略。上一篇搭好的 `CreateDeviceResources` 与 `DiscardDeviceResources` 回路咱们直接搬来用，画刷正好是住进这套回路的第一批新住客。

## 颜色怎么写：四个浮点

画刷装的是颜色，所以咱们从颜色的写法说起。Direct2D 里的颜色类型叫 `D2D1_COLOR_F`，它是一个只有四个浮点字段的结构体，四个字段的名字分别是 `r`、`g`、`b`、`a`，每个分量的取值范围都是 0.0 到 1.0。这个范围跟 GDI（Windows 的旧绘图接口）那套 0 到 255 的整数不一样，上一篇咱们已经提过换算：您手里要是捏着 `RGB(255, 128, 0)` 的旧值，换算的活儿您得自己干，具体的方法就是除以 255。

直接填结构体的写法几乎没人用，大家用的都是帮手类 `D2D1::ColorF`。它是个带构造函数的小类，能隐式地转成 `D2D1_COLOR_F`，所以您能把它直接塞进任何要颜色的参数位。书架（本系列引用的一套 Win32 参考教程，就是链接里的 native_win32 各章）的[Direct2D 架构章](../native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture.md)给过前两种写法，第三种十六进制写法是笔者按 SDK 里 `ColorF(UINT32, FLOAT)` 构造重载补上的，咱们一起列在这里：

```cpp
D2D1::ColorF(D2D1::ColorF::White)        // 预定义常量
D2D1::ColorF(0.85f, 0.90f, 0.95f, 1.0f)  // r, g, b, alpha
D2D1::ColorF(0x0078D4)                   // 十六进制 0xRRGGBB
```

头一种是查表：`ColorF::White`、`ColorF::CornflowerBlue` 这些常量都是从 CSS 那边借来的命名色，书架的[自绘控件章](../native_win32/49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md)里满页都是它们的身影。第二种的写法是逐分量给浮点，末位的参数是 alpha，省了它的话默认值就是 1.0。第三种的写法最像老习惯，`0x0078D4` 的前两位是红、中间两位是绿、末尾两位是蓝，跟 `RGB` 宏的数字是同一套，只是咱们不用再除以 255 了。

alpha 这个分量值得咱们单独停一停，它管的是透明。咱们用白话讲：alpha 管的是不透明度，1.0 表示完全遮住底下的东西，0.0 表示完全透过去了，0.5 的话就是一半对一半。您在后面的示例里会看到一个 0.35 的 alpha 拿去画按钮上的高光线，透出来的底色会让那道线看起来像光泽，而不是一道生硬的白线。透明合成的时候，底下已经画过的颜色会按比例透上来参与运算，所以“画了但看得见底下”的能力是颜色天生就有的，不需要咱们额外开什么开关。

## 纯色画刷：SetColor 的价值

咱们要认的纯色画刷接口叫 `ID2D1SolidColorBrush`，创建的入口挂在渲染目标的身上，书架架构章给的写法是：

```cpp
ID2D1SolidColorBrush* pBrush = NULL;
hr = pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::White),  // 白色
    &pBrush
);
```

画刷创建出来了之后，它身上最值得咱们认识的方法是 `SetColor`。书架架构章的 OnPaint 示例里就有它的身影：一把白刷子创建在门外，进了 `BeginDraw` 之后调 `pBrush->SetColor(...)` 换成蓝色，再拿去当 `FillRoundedRectangle` 的参数。画完了之后，刷子还留在咱们手里，下一笔咱们可以再 Set 成别的颜色接着用。

您可能会觉得奇怪：既然 `D2D1::ColorF` 就是个轻飘飘的四个浮点，绘制方法为什么不直接收颜色值呢，反倒要咱们绕一道手、去创建一个 COM 对象当画刷？答案要到篇尾的复用一节才能给全，咱们在这里交代半截：画刷是设备相关资源，它的背后是 GPU 那一侧的真实对象，创建是有成本的，所以它的正确用法是创建一次、反复使用。`SetColor` 换颜色只是改一个现成对象身上的参数，代价跟零也差不了多少。绘制方法收的是画刷而不是颜色，等于用参数表的类型提醒咱们，颜色得装进画刷才进得了绘制方法。

按状态建多把刷子也是合法的用法，书架自绘控件章的 `CustomButton::CreateBrushes` 就是现成的例子：它一口气创建了七把纯色画刷，普通态用的白色，悬停用的浅灰，按下用的天蓝，禁用态换的是极浅灰，再加上边框、文字、焦点框用的各一把，绘制的时候按当前状态挑一把用。咱们要注意它的创建位置：七把刷子全在设备资源创建的函数里一次性建好，绘制循环里做的只有挑选。书架自己在正文里也跟咱们说过，实际项目里的画刷还可以再往上收，做成共享的管理方式。两种做法咱们都别学成教条：一把刷子靠 `SetColor` 切换也行，按用途各建一把也是可以的，真正要避免的做法只有一种，也就是每一帧都 Create 一遍、再 Release 一遍的写法。

## 线性渐变：stop 集合与渐变轴

颜色一旦从单色升级成按位置排好的一组，画刷就进入了渐变的世界。线性渐变的接口是 `ID2D1LinearGradientBrush`，它的创建比纯色多一道手续：咱们得攒出一个 stop 集合，之后咱们拿这个集合去换画刷。

stop 的类型是 `D2D1_GRADIENT_STOP`，一个只有两个字段的小结构体：`position` 是 0.0 到 1.0 的浮点，表示这个颜色停靠在渐变的哪个位置，`color` 就是咱们刚认识的 `D2D1_COLOR_F`。咱们最少也得给一个渐变摆两个 stop，起点和终点各摆一个是它们的底线，中间想插几个都随咱们的便。咱们看一个三段式的例子：

```cpp
D2D1_GRADIENT_STOP stops[3] = {
    { 0.0f, D2D1::ColorF(0.28f, 0.52f, 0.95f, 1.0f) },  // 顶部：亮蓝
    { 0.5f, D2D1::ColorF(0.20f, 0.40f, 0.85f, 1.0f) },  // 中段：过渡色
    { 1.0f, D2D1::ColorF(0.12f, 0.28f, 0.70f, 1.0f) },  // 底部：深蓝
};
```

把 stop 数组变成集合的活儿，咱们交给渲染目标身上的 `CreateGradientStopCollection`：

```cpp
ID2D1GradientStopCollection* pStops = NULL;
pRenderTarget->CreateGradientStopCollection(
    stops, 3,
    D2D1_GAMMA_2_2,                // 颜色插值的 gamma 空间
    D2D1_EXTEND_MODE_CLAMP,        // 渐变区间之外的延展方式
    &pStops
);
```

`D2D1_GAMMA_2_2` 说的是颜色在哪个 gamma 空间里做插值，官方示例里用的都是它，咱们照着走就好，真要计较颜色的精确过渡时您再去查它的另一个取值。`D2D1_EXTEND_MODE` 管的是渐变区间之外的地方怎么办：`CLAMP` 用两端的颜色守住边界，出了 0 之前还是 0 号色，过了 1 还是 1 号色，`WRAP` 会把整段渐变当成壁纸重复地铺，`MIRROR` 的接法是镜像着折回去，接缝处不会出现颜色的跳变。

在咱们有了集合之后，画刷本身的创建反而简单。线性渐变自己的属性只有一条渐变轴，起点和终点两个坐标就够了：

```cpp
ID2D1LinearGradientBrush* pGradBrush = NULL;
pRenderTarget->CreateLinearGradientBrush(
    D2D1::LinearGradientBrushProperties(
        D2D1::Point2F(0.0f, 0.0f),    // startPoint
        D2D1::Point2F(0.0f, 100.0f)   // endPoint
    ),
    pStops,
    &pGradBrush
);
```

渐变轴怎么读？咱们把起点到终点想成一条连线，颜色就是沿着连线从 0 号 stop 过渡到 1 号 stop 的。画面上的任何一个点，看的是它投影到连线上的位置，投影落在 0.5 的位置，它拿到的就是 0.5 那个 stop 的颜色。由此有个好用的推论：跟渐变轴垂直的方向上颜色不变。起点和终点连成竖线的时候，渐变就是从上到下的横条带，连成横线的时候，就是从左到右的竖条带。

这里有一处笔者必须交代的话：书架的 Direct2D 各章里没有渐变画刷的示例代码，本节的代码是笔者按 SDK 的接口签名写的，接口名、参数与行为的依据都是官方文档，咱们没有在书架里找到能对照的成稿。好在验证的门槛不高，篇末的动手清单里咱们列了两处专门针对渐变的观察点，您跑一遍就都落实了。

渐变轴也不是创建时定死的，画刷的身上还有 `SetStartPoint` 和 `SetEndPoint` 两个方法，运行的时候随时都能改。它马上就会在咱们的示例里派上用场，因为渐变轴有一个新手最容易栽进去的误会，咱们到示例里指着代码说。

## 径向渐变：一句话的表亲

径向渐变咱们照篇首路线的安排只带一句。它的接口是 `ID2D1RadialGradientBrush`，创建的函数是 `CreateRadialGradientBrush`，用的还是同一个 stop 集合，换掉的只是几何：属性结构体 `D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES` 里装的是 `center`（圆心）、`radiusX` 与 `radiusY`（两个方向的半径），外加一个 `gradientOriginOffset`（渐变原点相对圆心的偏移）。颜色是从中心向圆周过渡的，到了半径处收尾。`gradientOriginOffset` 不为 0 的时候，渐变的起点会挪出圆心，光晕偏心的效果靠的就是它。它适合画光晕、球面高光这类从一点向外扩散的效果，用法跟线性款是同构的，咱们就不再展开代码了。

## 示例：一个渐变按钮底

现在咱们把三样东西拼成一个完整的小段：一把线性渐变刷当按钮的底色，一把靠 `SetColor` 身兼两职的纯色刷，外加一道用 alpha 画出来的高光。设备资源的管理沿用上一篇的骨架：渲染目标的创建代码跟上一篇相同，工厂照旧建在启动处、设备丢失的回路里一下都不动，这两段咱们省掉不贴。咱们没搬的是上一篇 `CreateDeviceResources` 末尾顺手建的那把白色纯色刷，本篇把画刷的创建挪进了自己的 `CreateBrushResources`。`SafeRelease` 模板也是从上一篇借来的，这里只看画刷一侧新增的部分。

```cpp
static ID2D1GradientStopCollection* g_pStops     = NULL;
static ID2D1LinearGradientBrush*    g_pGradBrush = NULL;
static ID2D1SolidColorBrush*        g_pSolid     = NULL;

static void CreateBrushResources(ID2D1RenderTarget* pRT)
{
    if (g_pGradBrush != NULL) {
        return;  // already built
    }

    D2D1_GRADIENT_STOP stops[3] = {
        { 0.0f, D2D1::ColorF(0.28f, 0.52f, 0.95f, 1.0f) },
        { 0.5f, D2D1::ColorF(0.20f, 0.40f, 0.85f, 1.0f) },
        { 1.0f, D2D1::ColorF(0.12f, 0.28f, 0.70f, 1.0f) },
    };

    pRT->CreateGradientStopCollection(
        stops, 3, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &g_pStops);

    pRT->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(0.0f, 0.0f), D2D1::Point2F(0.0f, 1.0f)),
        g_pStops, &g_pGradBrush);

    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_pSolid);
}

static void DiscardBrushResources()
{
    SafeRelease(&g_pSolid);
    SafeRelease(&g_pGradBrush);
    SafeRelease(&g_pStops);
}
```

创建的部分咱们扫一眼就知道没什么意外：三样家当依次进了门，全都站在指针为空才创建的守卫后面，跟渲染目标走的是同一条设备资源的回路。创建时给渐变轴填的 `0.0f, 0.0f` 到 `0.0f, 1.0f` 只是占位，真正的轴咱们在绘制时再摆。

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);   // target，同上一篇（factory 在启动处建）
    if (g_pRenderTarget == NULL) {
        return;
    }
    CreateBrushResources(g_pRenderTarget);

    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));

    RECT rc;
    GetClientRect(hwnd, &rc);
    const float kWidth  = 220.0f;
    const float kHeight = 44.0f;
    const float kLeft   = (rc.right - kWidth) / 2.0f;
    const float kTop    = 80.0f;

    D2D1_RECT_F button =
        D2D1::RectF(kLeft, kTop, kLeft + kWidth, kTop + kHeight);

    // The gradient axis lives in target coordinates - it does NOT
    // follow the filled shape. Point it at the button's top and
    // bottom edges every frame:
    g_pGradBrush->SetStartPoint(D2D1::Point2F(button.left, button.top));
    g_pGradBrush->SetEndPoint(D2D1::Point2F(button.left, button.bottom));

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(button, 6.0f, 6.0f);
    g_pRenderTarget->FillRoundedRectangle(rr, g_pGradBrush);

    // One solid brush, two jobs: a border, then a translucent
    // highlight line near the top (alpha = 0.35 at work):
    g_pSolid->SetColor(D2D1::ColorF(D2D1::ColorF::CornflowerBlue));
    g_pRenderTarget->DrawRoundedRectangle(rr, g_pSolid, 1.0f);

    g_pSolid->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.35f));
    g_pRenderTarget->DrawLine(
        D2D1::Point2F(button.left + 8.0f, button.top + 6.0f),
        D2D1::Point2F(button.right - 8.0f, button.top + 6.0f),
        g_pSolid, 1.5f);

    HRESULT hr = g_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardBrushResources();
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);  // repaint to rebuild
    }
}
```

注释里全大写的那句就是咱们要指给您看的误会：渐变轴活在渲染目标的坐标系里，它不会自动跟着被填充的形状走。您要是创建时把轴定在了 0 到 1，然后指望它自己相对于矩形、从矩形的顶边渐到矩形的底边，画出来的会是一条只有 1 个 DIP（设备无关像素，Direct2D 的坐标单位）高的渐变带，带子以下的区域全靠 `CLAMP` 用端点颜色填满。所以咱们的代码在每一帧绘制时都用按钮当前的上下边缘调 `SetStartPoint` 和 `SetEndPoint`，把轴的位置重新摆正。按钮的位置要是接入了布局逻辑，这一步就是必须的：轴追的是矩形，矩形追的是布局。

纯色刷那边的两连调咱们也看一眼：同一把 `g_pSolid`，头一笔被咱们 Set 成矢车菊蓝画边框，第二笔 Set 成 alpha 0.35 的白画高光。一把刷子在一个帧里换了两次颜色，这就是 `SetColor` 的日常。要是您把状态的切换也接进来，比如鼠标悬停的时候换一套 stop 颜色，思路也是一样的：资源在门外建好，帧里改的只是参数。状态驱动重画的那套机制，咱们在[骨架线](../gui/06_property_repaint.md)（GUI 系列里讲属性变化触发重画的那篇）已经造过一遍，您直接拿来用就好。

## 位图画刷：拿像素当颜料

颜料的最高形态是直接搬像素。位图画刷的接口是 `ID2D1BitmapBrush`，创建的函数是 `CreateBitmapBrush`，它的原料是一个 `ID2D1Bitmap` 对象。位图本身怎么来是个不小的话题，从图片文件走到可画位图的一整段路，要经过 WIC（Windows 的图像编解码组件，负责读图片文件）解码的旅程，咱们系列按[篇目索引](index.md)的排法后面还有位图与离屏渲染一篇，到时候咱们专门讲。这里咱们用最省事的来源：渲染目标身上的 `CreateBitmap`，咱们直接从像素数组把它建出来。

咱们造一个 8×8 的棋盘格小位图当颜料：

```cpp
static ID2D1Bitmap* g_pBitmap = NULL;

// 8x8 checkerboard: 4x4 blocks of two colors, 0xAARRGGBB
// (little-endian bytes land as B, G, R, A - matching BGRA)
static void CreateBitmapResource(ID2D1RenderTarget* pRT)
{
    if (g_pBitmap != NULL) {
        return;
    }

    UINT32 pixels[8 * 8];
    for (UINT32 i = 0; i < 8 * 8; ++i) {
        UINT32 blockX = (i % 8) / 4;
        UINT32 blockY = (i / 8) / 4;
        pixels[i] = (blockX ^ blockY) ? 0xFFDDDDDD : 0xFF666666;
    }

    pRT->CreateBitmap(
        D2D1::SizeU(8, 8),
        pixels,
        sizeof(pixels[0]) * 8,   // row pitch in bytes: 8 pixels * 4
        D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_IGNORE)),
        &g_pBitmap);
}
```

像素格式那里咱们选了 `D2D1_ALPHA_MODE_IGNORE`，因为棋盘格是完全不透明的，咱们让 Direct2D 别费心管 alpha。然后是画刷本身的创建：

```cpp
static ID2D1BitmapBrush* g_pBmpBrush = NULL;

// ... inside CreateBitmapResource, after the bitmap exists:
    pRT->CreateBitmapBrush(
        g_pBitmap,
        D2D1::BitmapBrushProperties(
            D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR),
        D2D1::BrushProperties(1.0f),
        &g_pBmpBrush);
```

`D2D1_BITMAP_BRUSH_PROPERTIES` 里的三样各管一件事。`extendModeX` 和 `extendModeY` 管的是被填充的区域比位图大的时候怎么办：`WRAP` 是平铺，位图会被当成瓷砖一块块地铺过去，咱们的棋盘格要的就是它。`MIRROR` 的做法是镜像着接，相邻的两块互为倒影，接缝处没有颜色的跳变，适合连续的纹理。`CLAMP` 的做法是把边缘像素拉出去一直延伸，位图之外的区域全被边缘的颜色铺满，咱们要填多大的区域它都接得住。`interpolationMode` 管的是位图被放大时怎么采样：`NEAREST_NEIGHBOR` 取最近的那个像素，放大后是锐利的色块，`LINEAR` 会在邻近的像素之间做插值，放大后是柔和的过渡。棋盘格咱们选了 NEAREST，格子的边缘才是干脆的。

咱们拿这把刷子去 `FillRectangle` 一块大区域，出来的就是棋盘格铺满的样子。Windows 里您见过的那些花纹填充，思路跟这里是相通的。

## opacity 是乘法

透明还剩最后一层窗户纸：画刷身上除了颜色自带的 alpha，还有一份自己的 opacity。接口是 `ID2D1Brush` 基类身上的 `SetOpacity`，取值的范围是 0.0 到 1.0，创建时也能通过 `D2D1::BrushProperties(0.5f)` 这样的参数带上，上面位图画刷的代码里您已经见过它了。

最终落到屏幕上的透明度是逐层相乘出来的。咱们把乘法链从头数一遍：颜色自带的 alpha 是一层，画刷的 opacity 是一层，再往上的话，如果绘制发生在 `PushLayer` 推进去的图层里，图层的 opacity 又是一层。咱们拿数字看最直观：颜色的 alpha 给 0.5，画刷的 opacity 再给 0.5，乘出来的却是 0.25，只剩四分之一的不透明度，透出去的比咱们可能预想的一半还要多，咱们要是以为两层半透明叠起来正好得到一半，那就想岔了。图层的用法咱们不用展开，书架的[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)里有现成的代码：`layerParams.opacity` 那里填的是一个浮点，`layerParams.opacityBrush` 填的是 `nullptr`，意思是用一个固定透明度罩住图层里画的所有内容。乘法的关系对这几层是一视同仁的，您叠几层它就乘几次。

咱们把 opacity 和 alpha 分放在两处，这样的分法看着虽然像冗余，其实分工清楚：alpha 是颜色的一部分，是跟着 `SetColor` 走的，opacity 是画刷的整体开关，改它的时候不用动颜色。咱们手里同一把渐变刷，想整体淡出的时候调一下 opacity 就行，stop 的序列一个都不用动。

## 画刷是设备相关资源

最后咱们把篇中欠的话还上：画刷为什么是 COM 对象，它的创建为什么值得当成正经的资源来管理。

上一篇咱们把 Direct2D 的资源分成了设备无关与设备相关两类，画刷在设备相关的那一侧：它的背后是 GPU 侧的真实对象，生下来就绑在创建它的渲染目标上。由此带来的后果，一样是咱们没法跨目标使用画刷，A 目标建的刷子不能拿去 B 目标上画。另一样咱们已经用代码对付过了：`EndDraw` 报回 `D2DERR_RECREATE_TARGET` 的时候，画刷也跟着渲染目标一起作废了，`DiscardBrushResources` 里两把画刷带着渐变的 stop 集合按创建时的逆序 `Release`。stop 集合也是从渲染目标身上创建出来的设备相关资源，所以丢弃的名单里它跟画刷排在一起。下一轮 OnPaint 进门的时候咱们再把它们建回来。

创建的成本到底有多大？笔者没有量过数，只把官方的定性说给您：画刷的创建是一次要过 GPU 的对象创建，可比不上四个浮点的赋值那么轻。所以复用的策略咱们可以这样定：颜色常变的场景，咱们用一把纯色刷靠 `SetColor` 打天下，渐变按渐变的样子各建一把，反正一个界面上像样的渐变也就那么几种，位图的画刷按原料各建一把。纯色与渐变这几样都住进本文那套为空才建的守卫回路里，帧内改的只有参数，建对象的活儿一件都没有。位图与位图画刷在上面的片段里只示意了创建，照同样的守卫把它们接进回路、加进丢弃名单的活儿，咱们留给后面的位图篇一并料理。

## 您可以动手的三处

书架没有渐变的成稿，咱们自己的示例就得自己来验。三处观察的每一处都只要改一行、编一遍：

1. 咱们把 `SetEndPoint` 里的 `button.bottom` 改成 `button.top`，渐变的方向就整个翻了过来，亮的跑到了下面。轴是有方向的，起点终点一互换就是反向。
2. 咱们把渐变轴的终点改到 `button.top + 20.0f`，让轴比按钮矮一截。`CLAMP` 之下，20 个 DIP 以下全是端点色的纯色带。您再把 `CreateBrushResources` 里 `CreateGradientStopCollection` 的 `D2D1_EXTEND_MODE_CLAMP` 换成 `D2D1_EXTEND_MODE_WRAP`，同一段渐变会重复出现。延展方式的区别，看一眼就忘不掉了。
3. 咱们把高光线的 alpha 从 0.35 改成 1.0，那道“光泽”立刻变成一道实心白线。alpha 在合成里干的活，这个对照比任何解释都直白。

## 下一步

画刷解决的是“上什么色”的问题，接下来的问题是“往哪儿上”。咱们这几篇里 `D2D1::RectF` 和圆角矩形已经用了不少次，但它们只是几何家族里最板正的两员，路径、椭圆、曲线的戏份都还没开场。按[篇目索引](index.md)的排法，下一篇讲的就是几何与路径，咱们到那边把 `FillRoundedRectangle` 身后的那套形状机制接着挖下去。
