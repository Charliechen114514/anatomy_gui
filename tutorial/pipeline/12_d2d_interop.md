---
title: 与 GDI 互操作
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 与 GDI 互操作

咱们在上一篇[《特效：Effect 图》](11_d2d_effects.md)里把特效也认过，Direct2D 的家当就算基本认齐了：目标、画刷、几何、位图、特效，一套画具从上到下过了一遍。可现实里的项目不长这样。书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[互操作一章](../native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md)开篇就提醒过咱们，现实世界不会给您一张白纸，让您从头开始写。更常见的处境，是您手头有一个攒了多年的 GDI 项目，几千行 `WM_PAINT` 绘制代码、一批自定义控件、几个只认 HDC 的第三方组件。想在一夜之间全部重写是不现实的，可您又想用上 Direct2D 的硬件加速和抗锯齿。本篇讲的就是这两套画具怎么同居：官方留的两条互操作通道，咱们一条一条认过去。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够。第四篇[GDI 对象模型](04_gdi_objects.md)认过的状态机、第六篇[工厂、设备与渲染目标](06_d2d_targets.md)搭好的对象模型，本篇咱们都直接拿来用。

## 为什么需要互操作

同居的需求从哪来？根子上是一件事：画布的归属早就定了，咱们动不了它。书架列过的场景里最常见的是遗留的绘制代码，旧的 `WM_PAINT` 里躺着几千行 GDI，咱们改不起，也一时半会儿是改不完的。只认 HDC 的第三方库也是书架点过名的，图表组件也好、老的字体引擎也好，它们的接口收的就是 HDC，别的输入一概不收。本篇再补一类书架没列的：打印管线，从 `StartDoc` 一路交到您手上的，是打印机的 HDC，D2D 的窗口目标插不进去。互操作（interop）解决的就是这个局面，两套 API 各给对方留了一道官方通道，让另一个画具能借用自家的画布。

咱们面前的通道有两条，方向恰好是一反一正的，分的依据是画布归谁。画布是 Direct2D 的渲染目标、GDI 来串门的，接口的名字叫 `ID2D1GdiInteropRenderTarget`，它在目标上临时开出了一张 HDC 交给 GDI。画布本身就是一块 HDC、Direct2D 来串门的，接口的名字叫 `ID2D1DCRenderTarget`，它把 D2D 的画面合到 HDC 上。两条道咱们各走一遍，选哪条道的判断，咱们留到后面讲工程取舍的地方再说。

## 方向一：把 D2D 的画布临时借给 GDI

### 门口的标志

第六篇咱们创建 `ID2D1HwndRenderTarget` 的时候，属性传的是无参的 `D2D1::RenderTargetProperties()`，默认的属性里并没有给 GDI 留门。互操作的入口，是从现成的渲染目标身上问出 `ID2D1GdiInteropRenderTarget`，用的还是 `QueryInterface`（COM 的对象一身能挂好几个接口，拿手头的接口问出另一个的正规通道就是它，上一篇从 Direct3D（D3D）的设备问出 DXGI（Windows 的图形基础设施层，Direct2D 底下管画面的就是它）的设备，走的也是它）。咱们创建目标的时候要是忘了带上 `D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE` 这个用法标志，问出来的结果就是 `E_NOINTERFACE`，接口压根就不在目标的身上。所以借道的头一步，是把创建的属性改齐：

```cpp
D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;  // opens the GDI door

HRESULT hr = g_pFactory->CreateHwndRenderTarget(
    rtProps,
    D2D1::HwndRenderTargetProperties(hwnd, size),
    &g_pHwndRenderTarget
);
```

咱们把标志设齐了，问接口这一步就成了例行公事：

```cpp
ID2D1GdiInteropRenderTarget* pGdiInterop = nullptr;
HRESULT hr = g_pHwndRenderTarget->QueryInterface(
    __uuidof(ID2D1GdiInteropRenderTarget), (void**)&pGdiInterop
);
```

书架还提了一句，主渲染框架要是已经换成了 D2D，不管用的是 `ID2D1HwndRenderTarget` 还是上一篇的 `ID2D1DeviceContext`，咱们借道的门路都不变。兼容的开关两代各有各的，这一句是本篇自己补的：1.0 路线用的就是上面这个渲染目标属性里的用法标志。上一篇 DeviceContext 的画布是 `CreateBitmapFromDxgiSurface` 出来的目标位图，管 GDI 兼容的是位图选项 `D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE`，咱们要是拿 1.0 的标志去配 1.1 的画布，是配不上的。`GdiInteropRenderTarget` 这个对象咱们不用单独创建，它跟目标是一体的，引用计数照 COM 的老章法走就行。

### GetDC 与 ReleaseDC 的一来一回

借画布的动作发生在 `BeginDraw` 和 `EndDraw` 的中间。`GetDC` 从目标的身上交出一个 HDC，您拿它跑 GDI，画完了用 `ReleaseDC` 还回去，一段 GDI 的客席就这么嵌进了 D2D 的绘制流程。咱们把书架示例的骨架抽出来看：

```cpp
pRenderTarget->BeginDraw();

// --- Direct2D drawing ---
pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
pRenderTarget->FillRoundedRectangle(card, pBlueBrush);

// --- hand the surface to GDI for a moment ---
D2D1_DC_INITIALIZE_MODE mode = D2D1_DC_INITIALIZE_MODE_COPY;
HDC hdc = nullptr;
HRESULT hr = pGdiInterop->GetDC(mode, &hdc);

if (SUCCEEDED(hr) && hdc != nullptr) {
    // GDI drawing on hdc goes here
    pGdiInterop->ReleaseDC(nullptr);
}

// --- Direct2D drawing continues ---
pRenderTarget->EndDraw();
```

书架的示例把头一个参数写成了 `&mode`，把枚举按指针传了。SDK 头文件 `d2d1.h` 里这个方法的 C++ 签名，收的是枚举值本身，咱们按 SDK 的签名修正过来。跟上一篇修书架的交换链指针类型比，算的是同一种出入：老教程落笔的年代和现在的头文件各有各的记法，您在别的资料里两种写法都会见到。

头一个参数定的是 HDC 拿到手的时候，上面带着的是什么内容。`D2D1_DC_INITIALIZE_MODE_COPY` 会把目标上已画好的 D2D 内容拷一份到 HDC 对应的表面上，您在 HDC 上落笔之前，底子上已经有了 D2D 画的图，GDI 的笔画叠在它上面。`D2D1_DC_INITIALIZE_MODE_CLEAR` 则反了过来，交出来的表面是清空的，底子上干干净净的。咱们打算接着 D2D 的内容往下画，咱们就用 COPY。要是您打算整块重画、不依赖底子，咱们就换用 CLEAR，它还能替咱们省掉一次内容拷贝。

借出去的这段时间里，目标本身是动不了的。`GetDC` 交出画布的那一刻起，到咱们调 `ReleaseDC` 收回来为止，渲染目标对 D2D 这边处于不可用的状态，这段窗口里落在渲染目标上的其他调用（绘制、`SetTransform` 这些）都会失败。`ReleaseDC` 把画布收了回来，D2D 那边才算恢复了可用，后面的绘制倒是照常。

还画布的那一次调用还带了一个 RECT 指针参数，它报告的是 GDI 到底改过了哪一块，Direct2D 拿这个信息去更新自己的表面。传空指针的意思是整块都改过了。报的改动区域越准，收尾的更新活就越省，咱们示例里图省事传的是空。

客席上能干什么？第四篇认过的那套 GDI 本事，在客席上全都是使得的。咱们把书架例子的客席部分展开看。头一段画的是一个老式按钮，`DrawEdge` 交出的是凸边框，`DrawTextW` 配的字就放在边框圈好的矩形里。后一段画分隔线的代码里，第四篇认过的那套进出各一件的节奏原样都在：

```cpp
if (SUCCEEDED(hr) && hdc != nullptr) {
    // an old-style raised button, drawn by GDI
    RECT btnRect = { 60, 320, 200, 360 };
    DrawEdge(hdc, &btnRect, EDGE_RAISED, BF_RECT | BF_ADJUST);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, L"GDI Button", -1, &btnRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // a separator line below the button
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, 60, 380, nullptr);
    LineTo(hdc, 390, 380);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    pGdiInterop->ReleaseDC(nullptr);
}
```

咱们看得出，画笔的创建、拧进、换回、删除这四步的顺序在客席上一步都不能乱。`GetDC` 交出来的 HDC 仍然是一块 GDI 的面板，第四篇讲过的配额与配对纪律，在客席上照旧压在咱们头上。咱们要是换成调用只认 HDC 的第三方图表库，或者老的 GDI 字体引擎，路数是一样的：只要它们要的是 HDC，咱们的客席就都能接待了。

### 借道的代价

性能的代价，咱们得如实说。`GetDC` 的内部会把 Direct2D 攒着的绘制命令一次性提交，命令队列跟着就被冲掉了。Flush 说的就是这个：平时 D2D 的命令是攒着批量交给 GPU 的，Flush 指的是一次强制提交，CPU 要一直等到 GPU 把手头的活干完了才返回。咱们等完这一场，GPU 的流水线就被打断了，而画布借出去的整段窗口里，D2D 那边是什么都不能画的，表面等于被占住了。咱们一帧里借几次，它的流水线就得跟着断几次。书架给的建议很直：把 GDI 的活集中成一段，咱们进出只来回一次，别在 D2D 的笔画之间来回穿插着借。书架还补了一句，上一篇的 DeviceContext 路线上，有些情况回来之后咱们还得手动调一次 `Flush()` 才稳妥。

## 方向二：把 D2D 的画面合到 HDC 上

### 创建：比上一篇轻多了

反过来的方向里，画布换成了 HDC 本身，来串门的是 Direct2D。接口的名字叫 `ID2D1DCRenderTarget`，创建它咱们只要工厂一个，第六篇的 `D2D1CreateFactory` 照旧。上一篇从 D3D 设备到交换链的五站链条，这边咱们一站都不用走，迁移的门槛一下子就低了。参数上要写准的是一份属性：

```cpp
D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1::PixelFormat(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED
    ),
    0.0f, 0.0f,                              // DPI: 0 follows the system default
    D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE  // the same GDI door
);

HRESULT hr = g_pFactory->CreateDCRenderTarget(&rtProps, &g_pDCRenderTarget);
```

像素格式给的是 `DXGI_FORMAT_B8G8R8A8_UNORM` 配 `D2D1_ALPHA_MODE_PREMULTIPLIED`。预乘 alpha（premultiplied）说的是这样一种格式，颜色分量是各自乘上透明度之后再存起来的。GDI 的 32 位位图用的正是 BGRA 字节序，两边的字节要对得上，咱们就得把格式写成这一副，书架把它称作 GDI 互操作的标准配置。

用法标志挂的还是方向一门口的同一块 `D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE`，咱们这边照样要挂上。这个标志在方向二漏了会怎么样，书架也交代过：目标咱们照样创建得出来，`BindDC` 咱们也照样绑得上，会出问题的是透明混合这类 GDI 侧的操作。DPI 两栏传 0 是第六篇讲过的省心传法，跟着系统默认走就行了。

画刷还是从目标的身上创建，设备相关的身份、跟着目标走的生死，咱们都照第六篇的资源分类算：

```cpp
g_pDCRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(0x0078D4), &g_pAccentBrush);  // the Windows blue
```

### BindDC 的顺序

咱们画到哪儿，由 `BindDC` 说了算。它把咱们手里的目标跟一块 HDC 连起来，头一个参数收的是 HDC，第二个参数圈出要绑的矩形。咱们拿一个 GDI 主体、D2D 画图表的 `WM_PAINT` 分支看全貌，上半截还是 GDI 自己的活：

```cpp
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // --- GDI part: background and title, create/use/delete pairing ---
    HBRUSH hbrBg = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &rcClient, hbrBg);
    DeleteObject(hbrBg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutW(hdc, 20, 20, L"GDI + D2D", 9);
```

下半截咱们把图表的矩形交给 D2D：

```cpp
    // --- D2D part: one anti-aliased chart region ---
    RECT rcChart = { 20, 80, rcClient.right - 20, rcClient.bottom - 20 };

    g_pDCRenderTarget->BindDC(hdc, &rcChart);

    g_pDCRenderTarget->BeginDraw();
    g_pDCRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));

    D2D1_RECT_F d2dRect = D2D1::RectF(
        10.0f, 10.0f,
        (rcChart.right - rcChart.left) - 10.0f,
        (rcChart.bottom - rcChart.top) - 10.0f);
    D2D1_ROUNDED_RECT rounded = { d2dRect, 12.0f, 12.0f };
    g_pDCRenderTarget->FillRoundedRectangle(rounded, g_pAccentBrush);

    HRESULT hr = g_pDCRenderTarget->EndDraw();
    if (FAILED(hr)) {
        // discard and rebuild the target side, the discard-rebuild loop
    }

    EndPaint(hwnd, &ps);
    return 0;
}
```

顺序写反了，这一轮就白画：`BindDC` 必须排在 `BeginDraw` 的前面，而且每一轮绘制咱们都得重新绑。原因出在 HDC 的寿命上：在两次 `WM_PAINT` 之间的空当里，手里的 HDC 可能早就没了，它是个短命的资源。第四篇讲 `BeginPaint` 交出的面板只在 `BeginPaint` 和 `EndPaint` 之间有效，讲的就是这个脾气。书架的作者自述在迁移项目里吃过一回亏：缓存了目标、忘了每帧重绑，画出来的东西全乱了，定位了半天。把顺序写反、`BeginDraw` 排在 `BindDC` 前面的写法，书架把它列为最常见的错误。

绘制的内容走了两步。DCRenderTarget 的内部养着一块自己的 D2D 位图，`BeginDraw` 到 `EndDraw` 之间您画的每一样，落的都是那块内部位图。等 `EndDraw` 收了尾，它再用 GDI 的 `BitBlt` 或 `AlphaBlend` 把整块合成到绑定的 HDC 上。之所以有 `AlphaBlend` 的份，原因在于标准 GDI 的函数基本不理透明通道，认半透明的只有它一个，咱们在方向二里画的透明背景，过桥的时候靠的就是它。所以 `EndDraw` 比别的目标多干一件活：一次实打实的位图搬运。

混着画还有一闪的问题，咱们第六篇记下过：咱们在 `WM_PAINT` 里混用 GDI 和 Direct2D 的话会闪，因为 `BeginPaint` 擦背景而 `BeginDraw` 不擦。解法书架也给过咱们，要么咱们只用一套画具，要么咱们在 `WM_ERASEBKGND` 里返回 1，把系统擦背景的那一步拦下来。方向二的示例里，HDC 上的底色由 GDI 自己铺，擦除的活本来就该咱们自己管，拦下来正合咱们的意。

坐标上还有一点值得咱们知道的小差异。两边的原点都在左上角，y 轴倒是都朝下，差别出的地方在精度：D2D 的浮点坐标能落到亚像素，GDI 的整数坐标要做像素对齐。书架提醒过咱们，互操作场景里 D2D 内容和 GDI 内容要是差着一两个像素的偏移，多半就是这个精度差导致的，咱们排查的时候值得想到它。

### EndDraw 那次搬运

方向二的性能代价，就落在 `EndDraw` 的那次位图搬运上。搬运的量跟绑定的矩形成正比，书架给过咱们一组估算：矩形要是圈住整个 1920×1080 的窗口，一次搬运就是大约 8MB 的数据，60 FPS 的动画一秒要搬 480MB。咱们还可以自己核一下这个数，咱们拿 1920 乘 1080 乘每像素 4 字节，得出的约是 8.3MB，咱们再按一秒六十帧乘上去，量级就对上了。办法跟着就出来了：`BindDC` 的矩形只圈真正要用 D2D 画的那块区域，图表就圈图表的框，您别贪整窗。

设备丢失的处置这边，走的还是第六篇那一套。要是 `EndDraw` 报回了 `D2DERR_RECREATE_TARGET`，第六篇的丢弃重建回路原样能用：咱们放掉目标一侧的资源，`InvalidateRect` 再登记一次重画的请求，咱们下一轮进门再重建。书架还给过一句宽心的话：DCRenderTarget 的内部走的是软件渲染的后备，设备丢失的概率比纯 D2D 场景低一些。不过处置的骨架咱们照旧备着，书架的意思也只是概率低一些，可没说咱们碰不上。

## 工程里怎么选

两条道都认齐了，落到真实的工程里，咱们见得最多的形态是这么分的。顶层已经换成 D2D、个别控件还留在 GDI 手里的程序，整窗走的是方向一：主画面是 D2D 的渲染目标，旧的 GDI 控件在 `GetDC` 开出来的客席上照旧画，两种笔画在一个窗口里相安无事地画着。打印和截图这类 GDI 传统的地盘，它们走的往往是方向二：打印管线从 `StartDoc` 起交到您手上的就是打印机的 HDC，截图的活路里，像素进的总是一块内存 DC，这些地盘的画布本身就是一块 HDC，咱们想让 D2D 插手，能走的就只有 `BindDC` 一条道。

书架还铺过一条渐进迁移的路子，主体仍是 GDI 的程序可以照着走：在现有的 `WM_PAINT` 里挑一块区域，咱们拿 DCRenderTarget 把它的绘制换成 D2D，其余的 GDI 代码一行不动。咱们每换一块就测一块，等大部分绘制都换完了，再把目标从 DCRenderTarget 换成 `ID2D1HwndRenderTarget` 或者上一篇的 DeviceContext，把中间的那一层整个去掉，搬运的代价也就跟着没了。

咱们把两条道的差别摆成一张表，您选的时候对着看：

| 对照的点 | 方向一 GdiInteropRenderTarget | 方向二 DCRenderTarget |
|:---|:---|:---|
| 画布归谁 | D2D 的渲染目标，GDI 是客人 | 本身就是一块 HDC，D2D 是客人 |
| 怎么到手 | 从现有目标 `QueryInterface` 问出来 | 工厂直接 `CreateDCRenderTarget` |
| 借道的动作 | `GetDC` 开客席，`ReleaseDC` 收回 | `BindDC` 绑矩形，每轮重绑 |
| 额外的开销 | `GetDC` 冲命令队列，表面被占住 | `EndDraw` 时一次位图搬运 |
| 典型场景 | 顶层 D2D，个别 GDI 控件 | 打印、截图、只认 HDC 的库 |

两条道的性能原则其实是同一句：减少切换的次数，同类的绘制咱们集中在一起画。咱们每切换一次，代价就是一次同步的等待，或者一次位图的搬运。切换多了，省下的开销就被抵掉了。书架的原话说得更干脆：互操作当的是迁移期的桥梁，等迁移完成了，咱们就该把 GDI 层去掉。

## 您可以动手的三处

1. 咱们把第六篇示例的创建属性补上 `D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE`，`QueryInterface` 就能问出接口。您再把标志拿掉编一遍，返回值里看到的 `E_NOINTERFACE`，就是门口那块牌子在起作用。
2. 咱们把 `GetDC` 的参数在 COPY 和 CLEAR 之间换着编两遍。COPY 那版里 D2D 画的底子还在，GDI 的笔画叠上去。CLEAR 那版里底子没了，画面上留下来的只有 GDI 自己的笔画。
3. 咱们把方向二示例里的 `BindDC` 挪到 `BeginDraw` 的后面去，编完看画面：画面上什么都不出现。`BindDC` 本身是会报失败的，只是示例没接它的返回值。往后排查画了却没显示的毛病，头一处查的就是这个顺序。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 创建渲染目标时漏了 `D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE` 标志，两个方向各会发生什么？
2. `D2D1_DC_INITIALIZE_MODE_COPY` 与 CLEAR 交出来的 HDC 差在哪？什么样的画面安排该选 CLEAR？
3. `GetDC` 到 `ReleaseDC` 的中间，为什么不能接着调 D2D 的绘制方法？
4. `BindDC` 为什么每一轮绘制都要重新调？HDC 的什么脾气决定了它？
5. DCRenderTarget 的 `EndDraw` 比 HwndRenderTarget 的多干了一件什么活？开销跟什么成正比？
6. 顶层用 D2D、个别控件还在 GDI 的程序选哪条道？往打印机的 HDC 上画 D2D 图形选哪条？

## 附录：方向一的完整示例

咱们把方向一的整条路拼成完整的函数。创建的骨架取的是第六篇 `CreateDeviceResources` 的模式，新添的活只有两样：属性里挂的是兼容标志，目标建好了之后咱们顺手问出互操作接口。错误判断照第六篇的 `FAILED` 宏走，中间省略的就是它：

```cpp
static ID2D1Factory*                g_pFactory = nullptr;
static ID2D1HwndRenderTarget*       g_pTarget  = nullptr;
static ID2D1GdiInteropRenderTarget* g_pGdi     = nullptr;
static ID2D1SolidColorBrush*        g_pBrush   = nullptr;

static HRESULT CreateDeviceResources(HWND hwnd)
{
    if (g_pTarget != nullptr) return S_OK;  // already there

    RECT rc;
    GetClientRect(hwnd, &rc);

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    HRESULT hr = g_pFactory->CreateHwndRenderTarget(
        props,
        D2D1::HwndRenderTargetProperties(
            hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
        &g_pTarget);
    if (FAILED(hr)) return hr;

    g_pTarget->QueryInterface(__uuidof(ID2D1GdiInteropRenderTarget),
                              (void**)&g_pGdi);
    g_pTarget->CreateSolidColorBrush(
        D2D1::ColorF(0x0078D4), &g_pBrush);
    return S_OK;
}
```

在 OnPaint 的绘制里，咱们把借道代价那一段的建议落了地：D2D 的笔画集中在头里，GDI 的活集中成一段，进出画布的那一对调用只跑一趟：

```cpp
static void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);
    if (g_pTarget == nullptr || g_pGdi == nullptr) return;

    g_pTarget->BeginDraw();
    g_pTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    D2D1_ROUNDED_RECT card = {
        D2D1::RectF(50.0f, 50.0f, 400.0f, 300.0f), 10.0f, 10.0f};
    g_pTarget->FillRoundedRectangle(card, g_pBrush);

    // the GDI guest seat: one trip, all GDI work together
    D2D1_DC_INITIALIZE_MODE mode = D2D1_DC_INITIALIZE_MODE_COPY;
    HDC hdc = nullptr;
    if (SUCCEEDED(g_pGdi->GetDC(mode, &hdc)) && hdc != nullptr) {
        RECT btn = { 60, 320, 200, 360 };
        DrawEdge(hdc, &btn, EDGE_RAISED, BF_RECT | BF_ADJUST);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        DrawTextW(hdc, L"GDI Button", -1, &btn,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        g_pGdi->ReleaseDC(nullptr);
    }

    HRESULT hr = g_pTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();  // release brush, interop, target
        InvalidateRect(hwnd, nullptr, FALSE);  // repaint to rebuild
    }
}
```

设备丢失的处置咱们认得：要是 `EndDraw` 报回了 `D2DERR_RECREATE_TARGET`，丢弃的名单里比第六篇多了一件 `g_pGdi`，它是跟着目标一起走的。`WM_PAINT` 分支的写法照的是第六篇的老样子：`ValidateRect` 销掉重画的登记，剩下的都交给了 OnPaint，咱们不在这里重贴了。

## 下一步

咱们从第六篇的 Direct2D 一路走到本篇，D2D 的一段就算收齐了。下一段咱们要换一套全新的 2D 库 Skia，也就是 Chrome 和 Flutter 画界面用的那套家什。它的接口全是另一家的名字：Surface、Canvas、Paint，对象模型跟 Direct2D 又正好构成了一副对照。紧挨着的下一篇是[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)，咱们过去看它跟 D2D 的目标模型哪里像、哪里换了路数。
