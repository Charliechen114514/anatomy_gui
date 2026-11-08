---
title: 工厂、设备与渲染目标
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 工厂、设备与渲染目标

第四篇[GDI 对象模型](04_gdi_objects.md)的结尾，咱们留过一张对照表，说 Direct2D 会把模型整个地翻过来：画刷不再是拧在面板槽位上的零件，而是每次调用自己带着的参数。当时的承诺还悬在半空，因为咱们手里没有一行真的 Direct2D 代码。上一篇《失效、脏区与重绘》收尾时咱们说好接下来就动手换画具，本篇咱们就来兑现。咱们从零开始认 Direct2D 的对象模型，头一段路要认的是三样东西：工厂、渲染目标、资源。您不需要任何图形学的底子，会 C++ 和 Win32 的基础用法，跟第四篇站在同一个位置上就够了。

名字上的事咱们放在开头交代。Direct2D 是微软在 Windows 7 时代推出的二维绘图接口，底下的活借道 Direct3D 10.1 交给了 GPU，CPU 管的只是下达指令。书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[Direct2D 架构章](../native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture.md)把它跟 GDI 的差别讲得很直白：GDI 画的每一条线、每一个像素都由 CPU 逐个去算，而 GPU 闲在一边，Direct2D 则把这些活全交给了 GPU 去算。性能上的对比咱们不急着做，本篇要认的是它的对象模型。模型变了，咱们写代码的手势也就跟着全变了。

模型变在哪儿？咱们拿第四篇收尾时的话来对照。GDI 是状态保留的：您调 `TextOutW` 的时候，参数表里是找不到颜色的，颜色拧在 `SetTextColor` 的旋钮上，谁拧的最后一把，决定了画出来的是什么。Direct2D 却是反着来的，样式是跟着调用走的，画刷就写在 `FillRectangle` 的参数表里，画完了也不留。第四篇咱们管 GDI 叫标本，本篇的主角就是那个把标本翻转过来的活物。

## COM 的地面规则

咱们进 Direct2D 的门之前，还得认一位它自带的门房。Direct2D 是建立在 COM 之上的接口，COM 的全称是 Component Object Model，中文的名字叫组件对象模型，这是微软的一套对象规范：所有接口的名字都以 `I` 开头，对象的生命周期不靠 `new` 和 `delete`，靠的是引用计数。您拿到一个接口指针，用完之后调它的 `Release()` 方法、把引用减一，等引用减到零的时候，对象就自己销毁了。您要是顺手写了个 `delete`，行为就是未定义的，书架架构章在开头就用了整段警告提醒过这一点。

释放的样板代码，书架给的是一个小模板，咱们原样借来，后面的代码全靠它：

```cpp
template<class T> void SafeRelease(T** ppT)
{
    if (*ppT) { (*ppT)->Release(); *ppT = NULL; }
}
```

指针非空的时候才释放，释放完了还顺手置空，后面的代码就能拿空指针当“还没创建”的凭据。构建方面要交代的是链接：头文件是 `<d2d1.h>`，要链接的是 `d2d1.lib`，示例里用的是 `#pragma comment(lib, "d2d1.lib")` 这一句。COM 更深的部分咱们一概不碰，您不用在双接口与套间线程这些名词上花功夫。Direct2D 的日常用法里，您只要把“用完调 `Release`，创建完判返回值”练成手上的习惯，这一层就算铺平了。

## 工厂：一切的出生地

Direct2D 的入口是一个工厂对象，接口的名字是 `ID2D1Factory`。它是全链条的头一站：渲染目标是从它手里创建的，几何、线条样式这些资源也从它的手里出来，咱们说它是工厂，意思就是别的东西都从它的生产线上下来。它的创建不靠 `new`，靠的是模块级函数 `D2D1CreateFactory`：

```cpp
ID2D1Factory* pFactory = NULL;
HRESULT hr = D2D1CreateFactory(
    D2D1_FACTORY_TYPE_SINGLE_THREADED,  // single-threaded mode
    &pFactory
);
if (FAILED(hr)) {
    return;
}
```

这里出现了 `HRESULT`，咱们补一句白话：它是 Windows 里表示调用结果的一个 32 位整数，负值代表的是失败，零和非负代表的是成功，咱们判定时不念魔法数，用的是 `FAILED` 和 `SUCCEEDED` 两个宏。往后本篇的每一处创建调用，您都拿它这么判。

`D2D1CreateFactory` 的头一个参数是工厂类型，枚举里只有两个可选的值，值得咱们停下来认一认。`D2D1_FACTORY_TYPE_SINGLE_THREADED` 声明的是您的程序只在一个线程里使用 Direct2D，书架架构章说这是最常见的情形，性能也是最好的。`D2D1_FACTORY_TYPE_MULTI_THREADED` 声明的则是多线程的用法，此时 Direct2D 的内部会自己加锁保护，代价就是锁带来的开销。书架给的建议很干脆：除非您确实要从多个线程同时调用 Direct2D，不然就老老实实地用单线程模式。咱们示例里的用法全是单线程的，您直接复制这个参数不会错。

工厂还有一条性质值得咱们现在记下，本篇后面的部分要用到它：工厂是设备无关的，咱们可以把它理解成不绑任何一块 GPU 的常驻对象，程序启动的时候创建一次，退出的时候释放一次，中间从头活到了尾。等咱们讲到设备丢失的地方，您会看到它替咱们省了多大的事。

## 两类资源，两个出生地

Direct2D 把资源分成了两类，分界线就画在离 GPU 设备远近的地方，咱们把两边的名单摆出来：

| | 设备无关资源 | 设备相关资源 |
|:---|:---|:---|
| 典型成员 | 几何 `ID2D1Geometry` 一类：矩形、椭圆、路径几何，加 `ID2D1StrokeStyle` 线条样式 | 渲染目标本身，`ID2D1Brush` 画刷（纯色、渐变、位图），`ID2D1Bitmap` 位图 |
| 创建函数挂在谁身上 | `ID2D1Factory` | 渲染目标 |
| 生命周期 | 启动时创建，退出时释放 | 跟着目标走，设备丢了就全体作废 |

名单的依据都在书架上。几何的创建函数挂在工厂身上，书架的[几何一章](../native_win32/30_ProgrammingGUI_Graphics_Direct2D_Geometry.md)里满页都是 `pFactory->CreatePathGeometry(...)`、`pFactory->CreateEllipseGeometry(...)` 的调用，线条样式的创建走的同样是 `pFactory->CreateStrokeStyle(...)`，出自书架自绘控件章的示例。画刷和位图倒是反过来的，书架架构章里创建纯色画刷的写法是 `pRenderTarget->CreateSolidColorBrush(...)`，入口就在渲染目标的身上。所以您拿到陌生的 Direct2D 对象、想知道它归哪一类的时候，看它的创建函数挂在谁身上就够了。有一条例外咱们得说在前面：渲染目标咱们也是从工厂手里创建的，咱们却把它归在设备相关一侧。所以咱们只把判据用在资源身上。

为什么要把线画在这儿？咱们分别看两边的脾气。几何描述的是形状长什么样，一条路径的坐标和曲线，其实跟哪块 GPU 都没有关系，书架架构章的说法是它们不依赖特定的硬件或渲染配置，创建一次就可以反复地用。而它们的创建又贵，路径几何要逐点地算曲线，您要是每帧画同一个齿轮都重建一遍几何，重复的功夫就全白花了。画刷和位图倒是另一种脾气：它们背后是真实的 GPU 资源，纯色画刷在显存里有自己的一份颜色对象，而位图干脆就是一块显存，这些东西生下来就绑在创建它们的那个渲染目标对应的设备上，换个目标就用不成了。所以分界线是自然形成的：创建一次就处处能复用的东西归工厂那侧，跟设备绑死的那批就归目标那侧，它们的生死跟着目标走。

上一段有一处得向您坦白：创建贵的说法，书架原文用的是“可以重复使用”，重复创建的代价是笔者按几何的常规实现补的理解，笔者没有量过数。您要是较真，咱们拿一个路径几何在循环里创建一万次、再复用一万次各测一轮，答案就落在您自己的机器上了。

## 渲染目标：把窗口交给 Direct2D

有了工厂，咱们就能创建本篇真正的主角：渲染目标（render target）。它回答的是“画往哪儿去”的问题，跟 GDI 那边的 HDC 坐在同一个位置上。HDC 是 GDI 绘图用的设备上下文句柄，咱们每次画图都向系统借它，您用完了也得还回去。渲染目标不是借来的面板，它是一个沉甸甸的 COM 对象，背后连着的可是 GPU 设备、后备缓冲区一整套家当。落在窗口上用的类型叫 `ID2D1HwndRenderTarget`，创建它要填的是两份属性结构：

```cpp
RECT rc;
GetClientRect(hwnd, &rc);

ID2D1HwndRenderTarget* pRenderTarget = NULL;
hr = pFactory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),  // general properties: format, DPI, usage
    D2D1::HwndRenderTargetProperties(
        hwnd,                                    // the window to draw onto
        D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)  // pixel size
    ),
    &pRenderTarget
);
if (FAILED(hr)) {
    return;
}
```

两份结构管的各是一层。`D2D1_HWND_RENDER_TARGET_PROPERTIES` 是窗口专用的，里面装的东西一共有三样，示例里咱们要填的是头两样：画到哪个窗口上的句柄，和目标的初始尺寸，第三样呈现选项咱们按默认值走。尺寸的量纲要留意，书架架构章专门强调过它量的是物理像素，逻辑坐标的事它不管，所以代码里现取了一次 `GetClientRect`。`D2D1_RENDER_TARGET_PROPERTIES` 管的是更一般的事：像素格式、用法标志、DPI。咱们示例传的是无参的 `D2D1::RenderTargetProperties()`，默认值给的是 32 位 BGRA 格式、系统默认 DPI、硬件渲染，书架说大多数情况下默认的就够。

DPI 那一栏值得咱们单独停一停，因为它接的是[第三篇](03_dip_and_dpi.md)的线。属性结构里有专门的 dpiX、dpiY 两栏，传 0 的意思是跟着系统默认走，书架里 Direct2D 的示例用的就是省心的传法。目标创建好了之后，它的身上就带着一份 DPI 设置，此后您在它上面写的所有坐标，用的单位都是 DIP，DIP 的全称是 device-independent pixel，中文的名字叫设备无关像素，换算成物理像素的活由 Direct2D 在内部完成。第三篇快收尾的地方咱们预告过这一幕：画一个 200.0f 宽的矩形，在 96 DPI 的目标上占 200 个物理像素，在 240 DPI 的目标上占 500 个，咱们的代码一行都不用改。所以您在 Direct2D 里看到的满眼浮点坐标，跟精度的讲究没有关系，单位其实就是 DIP：GDI 那边布局常数要咱们自己乘 DPI 的苦活，这边的 API 替咱们扛了。

窗口的尺寸一变，您就得调目标的 `Resize` 方法，书架架构章给的写法就挂在 `WM_SIZE` 里：

```cpp
case WM_SIZE:
{
    if (pRenderTarget != NULL) {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        pRenderTarget->Resize(D2D1::SizeU(width, height));
        InvalidateRect(hwnd, NULL, FALSE);  // schedule a repaint
    }
    return 0;
}
```

不调 `Resize` 的后果，书架也写明了：后备缓冲区的尺寸不会自己跟上来，窗口变大之后有内容的就只有一块区域，变小了则白白浪费内存。您把这句跟第三篇 demo 里的 `layout_controls` 对照着看，会发现姿态换了但活没换，GDI 那边是咱们按客户区宽度重算控件的位置，这边是咱们通知目标改尺寸，窗口变了总得有人招呼一声。

## BeginDraw 与 EndDraw 之间

目标有了，画画的章法是下一件要认的事。Direct2D 的所有绘制调用，必须夹在 `BeginDraw` 和 `EndDraw` 的中间，这是硬性的要求。咱们看书架架构章里 OnPaint 函数的样子：

```cpp
pRenderTarget->BeginDraw();

pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::DarkSlateGray));

D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
    D2D1::RectF(50.0f, 50.0f, 350.0f, 200.0f),
    10.0f, 10.0f
);

pBrush->SetColor(D2D1::ColorF(0.2f, 0.6f, 0.9f, 1.0f));
pRenderTarget->FillRoundedRectangle(roundedRect, pBrush);

HRESULT hr = pRenderTarget->EndDraw();
```

`BeginDraw` 把目标标成了“正在绘制”，中间的每个调用都是一条下给 GPU 的命令，`EndDraw` 把整批命令提交了上去，把结果呈现了出来。这套姿态的名字叫立即模式：每次调用当场就变成一条命令记下来，画完了也不留在任何对象里，目标的参数表之外没有暗处的状态。跟它相对的就是咱们第四篇解剖过的状态机，两边的轻重，咱们留到本篇结尾再算。

不过章法之外还有一处等着咱们当心：绘制方法要是调在 `BeginDraw` 外面，书架给的原话叫“静默失败”，咱们在画面上什么也看不到、报错也收不到，程序也懒得崩给咱们看。您要是哪天发现 `DrawRectangle` 明明调了却什么都不画，头一个该查的就是它有没有被夹在那对调用中间。颜色的事也有一处要认：`D2D1::ColorF` 的分量是 0.0 到 1.0 的浮点数，跟 GDI 那套 0 到 255 的整数不一样，您手里要是捏着 `RGB(255, 128, 0)` 的旧值，您就得自己除以 255 换算过去。

错误从哪儿出？这是这对调用最要紧的讲究。咱们看中间那些绘制方法，您看它们的返回值，大多返回的是 `void`，错误根本不从它们的身上出。一整轮绘制的成败，集中在 `EndDraw` 返回的一个 `HRESULT` 上。设计成这样的道理跟 GPU 的工作方式有关：命令是攒着批量下发的，单条命令在当时还没有真的执行，真正的执行和出错都发生在提交的时刻，所以检查点也就设在了提交的门口。您拿到 `hr` 之后拿 `FAILED` 判，而其中一个特殊的返回值，值得咱们用一整节来讲。

## D2DERR_RECREATE_TARGET：设备丢了的时候

那个特殊的返回值叫 `D2DERR_RECREATE_TARGET`。书架架构章列举的触发场景都挺日常：GPU 驱动崩溃之后的恢复、笔记本外接显示器的热插拔、远程桌面连接的断开，书架的[互操作一章](../native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md)还补了系统休眠唤醒和显存不足。这些场景的共同点是，渲染目标背后的 GPU 设备没了，渲染目标自己和画刷、位图这样的设备相关资源，就全体一起作废了。您要是收到错误之后还接着用旧指针，轻一点的话画面消失，重一点的话程序崩溃。

处置的办法，书架给的是一个流传很广的经典模式，咱们把它整个搬过来看。全局的姿势长这样：

```cpp
ID2D1Factory* g_pFactory = NULL;
ID2D1HwndRenderTarget* g_pRenderTarget = NULL;
ID2D1SolidColorBrush* g_pBrush = NULL;
```

三样东西摆在了一起，命运却分成了两拨，咱们前面讲资源分类的地方，用武之地就在这儿。设备的活儿收拢在两个函数里，头一个管的是创建，进门查指针、目标还在就什么都不做：

```cpp
void CreateDeviceResources(HWND hwnd)
{
    if (g_pRenderTarget != NULL) return;  // already there

    RECT rc;
    GetClientRect(hwnd, &rc);

    HRESULT hr = g_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            hwnd,
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
        ),
        &g_pRenderTarget
    );
    if (FAILED(hr)) return;

    g_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), &g_pBrush);
}
```

第二个管的是丢弃，咱们按创建的逆序释放，两个 Release 之外再没有别的动作：

```cpp
void DiscardDeviceResources()
{
    SafeRelease(&g_pBrush);
    SafeRelease(&g_pRenderTarget);
}
```

串起两个函数的是 OnPaint。咱们在开头调一次创建，画完拿到 `EndDraw` 的返回值再判：

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);

    if (g_pRenderTarget == NULL) return;

    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));

    // ... drawing calls ...

    HRESULT hr = g_pRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);  // repaint now to rebuild
    }
}
```

咱们把整条回路数一遍。设备丢失的那一刻，`EndDraw` 报回来的是 `D2DERR_RECREATE_TARGET`，咱们的代码当场把设备相关资源全丢掉、再补一句 `InvalidateRect` 请求重画。下一轮 OnPaint 进门的时候，目标指针已经是空的了，`CreateDeviceResources` 于是把目标和画刷从头再建了一遍，画面接着就回来了。而整条回路里工厂一下都没动，因为它是设备无关的资源，设备丢了也牵不到它的身上，这也正是工厂从头活到尾的好处兑现的时刻。咱们留着工厂不动，重建的活儿就只剩目标那一侧了。

同样的处置，咱们在书架的别处也见得到：[几何一章](../native_win32/30_ProgrammingGUI_Graphics_Direct2D_Geometry.md)和[互操作一章](../native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md)的示例里，`EndDraw` 之后判的都是同一个错误码。互操作一章还补了一句自己的情况：DCRenderTarget 用的是软件渲染的后备，设备丢失的概率比纯 D2D 场景低一些，处置的骨架却没有变。

细心的您会发现，这里的 `InvalidateRect` 是咱们的熟面孔。[第五篇](05_invalidation_dirty_region.md)整篇讲的都是失效登记，您再看看第三个参数，传的还是 `FALSE`，因为 `Clear` 会把整幅的画全重铺一遍，再安排背景擦除就白干了。

销登记的动作也换了地方。咱们看书架完整示例的 `WM_PAINT` 分支：画完调的是 `ValidateRect(hwnd, NULL)`，用的不是 `BeginPaint`。这个动作从 `BeginPaint` 的内部挪到了明面上，因为 Direct2D 压根不碰 HDC 的面板，`BeginPaint` 交面板的那套在这里没有了用武之地，但登记下来的重画请求总得有人销掉，`ValidateRect` 干的就是原来那件事。

书架的常见问题一节还提醒过咱们一件事：在 `WM_PAINT` 里混用 GDI 和 Direct2D 的话会闪，因为 `BeginPaint` 擦背景而 `BeginDraw` 不擦。书架给的解法是：要么只用一套画具，要么在 `WM_ERASEBKGND` 里返回 TRUE、把擦除拦下来。这跟第五篇讲 `bErase` 时的机关是同一家。

## 回看 GDI：状态去了哪儿

现在咱们手里有真的 Direct2D 代码了，第四篇结尾的对照表咱们可以一行行对回来，咱们再补上坐标单位、生命周期、设备出事几行新的。咱们看最要紧的一行：画刷在哪。GDI 的画刷拧在 DC 的槽位上，换进换出靠的是 `SelectObject`，Direct2D 的画刷则是 `FillRoundedRectangle` 的最后一个参数，调用完了也就完了。槽位没有了，换回也没有了，被选删不掉的对象更是没有了。第四篇咱们领教过的整套配对纪律，进出各一件、换回再删除、删除失败的无声泄漏，在这儿从语法上就没了发生的条件。您就算漏掉恢复也漏不成，咱们没有状态可漏。忘了删除也一样，画刷的释放跟所有 COM 对象一样走 `Release`，跟选中与否都不相干了。

文字颜色的一行也对得上。GDI 里 `TextOutW` 的参数表没有颜色，颜色拧在 `SetTextColor` 的旋钮上，Direct2D 里的画刷自己带着颜色，您还看到了 `pBrush->SetColor(...)`，可以在两笔之间改一个画刷的颜色接着用。画刷是个活的对象，却不是暗处的全局状态，谁用的时候就传给谁。

不过咱们也得把话说全，别把新模型夸成了完人。Direct2D 不是完全无状态的，第四篇咱们就说过，坐标变换这样的状态仍设在目标上，`SetTransform` 调完了之后，变换会一直压在后面的绘制身上，直到您再调一次为止。所以准确的说法是：绘制属性基本是随调用走的，留在目标上的状态咱们只数得出变换这类少数几处。咱们拿它跟 GDI 一比，它把会忘掉的恢复错误从十几个旋钮缩到了变换等几处，这是它省心的部分。

| 对照的点 | GDI（第四篇） | Direct2D（本篇） |
|:---|:---|:---|
| 画刷在哪 | 拧在 DC 的槽位上，`SelectObject` 换进换出 | 每次调用的参数 |
| 文字颜色在哪 | `SetTextColor` 一类的旋钮 | 画刷自己带着，`SetColor` 可改 |
| 忘了恢复的后果 | 删除失败，配额悄悄地漏 | 绘制属性无可忘，变换等少数状态除外 |
| 坐标单位 | 整数像素，换算归咱们 | DIP 浮点，换算归 API |
| 生命周期 | 句柄配对，`DeleteObject` | COM 引用计数，`Release` |
| 设备出事 | 没有这个概念，GDI 不碰 GPU | `D2DERR_RECREATE_TARGET`，重建目标一侧 |

表里最后一行讲的是 GDI 那边没有的概念：GPU 进来了之后，设备丢失的处置就成了新的功课。模型的翻转从来就不是白翻的，省下的配对纪律，换回来的是 COM 的引用计数和设备丢失的预案，两边的得失摆在同一张桌上，您选画具的时候心里才有数。

书架上还有一件跟本篇沾亲带故的家当值得指给您：渲染目标的种类不止 `ID2D1HwndRenderTarget` 一种，`ID2D1DCRenderTarget` 是摆在旁边的另一种，它把 Direct2D 的绘制结果合到 GDI 的 HDC 上，给存量的 GDI 程序留了渐进迁移的路，书架的[互操作一章](../native_win32/33_ProgrammingGUI_Graphics_Direct2D_GDIInterop.md)整篇讲它。本系列的后面还排着一篇[《与 GDI 互操作》](12_d2d_interop.md)，咱们到那边再接着看。

## 您可以动手的三处

书架架构章附了一个完整的最小程序：旋转的三角形，咱们靠 `WM_TIMER` 每隔 16 毫秒让它转一个角度。咱们把下面三处观察都放在它搭的台子上：

1. 咱们把完整的示例原样编起来跑。三角形转起来了，说明工厂、目标、画刷的一条链全通了。您再开任务管理器确认一下，自己进程在 GDI 那列没有漏东西，Direct2D 的资源不走 GDI 的配额。
2. 咱们把 `WM_SIZE` 分支里的 `Resize` 那一行注释掉，编完了再把窗口拉大。右边新露出来的区域没有内容，后备缓冲区还停在旧的尺寸上，书架写的后果您就亲眼看到了。
3. 咱们把一句 `FillRoundedRectangle` 挪到 `BeginDraw` 的前面去，编完了看画面。它什么都不画，也不给咱们报错，您在调试器里单步，它会正常地走过去，这就是静默失败的现场。往后排查画了却没显示的毛病，头一站查的就是夹没夹对。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `D2D1_FACTORY_TYPE_SINGLE_THREADED` 和多线程的款差在哪？书架建议咱们怎么选？
2. 路径几何、线条样式归哪类资源？它们的创建函数挂在谁的身上？
3. 画刷、位图为什么不能跨渲染目标复用？
4. `CreateHwndRenderTarget` 的 pixelSize 用的是什么量纲？属性里的 DPI 两栏传 0 是什么意思？
5. 在 `BeginDraw` 外面调绘制方法，您会看到什么？
6. 一整轮绘制的错误，从哪个调用的返回值出？为什么？
7. `EndDraw` 报回 `D2DERR_RECREATE_TARGET` 之后，哪些对象要重建？工厂要不要动？
8. GDI 里 `SelectObject` 干的活，在 Direct2D 里换成了什么？

## 下一步

您可能也注意到了，画刷在本篇里一直是个工具人：咱们创建它、传它、看它当参数，咱们还没有正眼看过它。它还是纯色的，可 `CreateSolidColorBrush` 只是画刷家族里最朴素的一员，渐变的、拿位图当颜料的、能在两笔之间换颜色的，都排在它的后面。下一篇[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)讲的就是画刷，咱们在那里把纯色、渐变、位图三种画刷逐个细看，`D2D1::ColorF` 那套 0 到 1 的浮点颜色也会在那边展开。
