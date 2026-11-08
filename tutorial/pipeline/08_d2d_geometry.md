---
title: 几何与路径
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 几何与路径

上一篇[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)的结尾咱们把话头留在了形状身上：`D2D1::RectF` 和圆角矩形用了不少次，但它们只是几何家族里最板正的两员，路径、椭圆、曲线的戏份都还没开场。本篇咱们就来补上。您已经会拿画刷上色了，剩下的问题是往哪儿上，也就是形状本身从哪儿来、怎么拼出来、画的时候有哪些讲究。要求跟前几篇是一样的：您不需要任何图形学的底子，会 C++ 和 Win32 的基础用法，您的进度跟到上一篇就够用，本篇直接拿来用的画刷机制那篇都讲过了。PathGeometry、GeometrySink 一类的名字，咱们都会配上白话再往下讲。

本篇的路线是从最简单的形状走到最自由的形状。开篇咱们就把纯数据与几何对象的分界画出来，中段的主力是路径几何和它的接收器。图形的开口闭口与 Close 的讲究，咱们走到半路再认。末尾咱们用一条手画的对勾把全部家当串起来，资源的归属也顺手收个尾。

咱们平时写的绘制代码里，形状大多是以纯数据的身份出现的。`D2D1_RECT_F` 是四个浮点的结构体，`D2D1_ROUNDED_RECT` 在它的基础上加了两个圆角半径，`D2D1_ELLIPSE` 装的是圆心加两个方向的半径。`FillRectangle`、`FillRoundedRectangle`、`DrawEllipse` 收的都是它们，咱们把参数一给，一笔也就画下去了。书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[Direct2D 几何章](../native_win32/30_ProgrammingGUI_Graphics_Direct2D_Geometry.md)把它们的局限说得很直白：这些方法只是即时绘制，画完就画完了，您没有任何办法再对形状本身做后续的操作。

## 形状的两条来路

想对形状做后续的操作，形状就不能只是一闪而过的参数，它得变成一个能拿在手里的对象。Direct2D 里干这件事的接口统称 `ID2D1Geometry`，咱们叫它几何对象，它描述的是形状长什么样，颜色的事它不管，画刷的事它也不管。渲染目标身上的 `FillRectangle` 一类快捷方法收的是纯数据，而 `FillGeometry` 和 `DrawGeometry` 收的就是 `ID2D1Geometry*`，这就是两条来路的分岔口。

咱们跟 GDI 对照一下，更能看清位置变了多少。第四篇[解剖过的那套旧画具](04_gdi_objects.md)里，形状从来都是绘制的附属品，`Rectangle`、`Ellipse`、`Polygon` 画完就散了，复杂的自定义形状，书架的说法是 GDI 那边得一个像素一个像素地手动去填。Direct2D 把形状提拔成了独立的对象，几何是能创建一次、画很多次、随时问它问题的，“问它问题”的本事咱们在进阶活儿那边兑现。

把纯数据升级成对象的入口挂在工厂身上，咱们在[《工厂、设备与渲染目标》](06_d2d_targets.md)的资源对照表里已经见过它们的名字。矩形、圆角矩形、椭圆各有一个对应的创建函数：`CreateRectangleGeometry` 收的是一个 `D2D1_RECT_F`，`CreateRoundedRectangleGeometry` 收的是一个 `D2D1_ROUNDED_RECT`，`CreateEllipseGeometry` 收的是一个 `D2D1_ELLIPSE`。咱们拿书架几何章里创建圆形的写法当例子：

```cpp
ID2D1EllipseGeometry* pCircle = NULL;
pFactory->CreateEllipseGeometry(
    D2D1::Ellipse(D2D1::Point2F(200.0f, 200.0f), 100.0f, 100.0f),
    &pCircle);
```

圆角矩形的两个半径咱们也顺嘴认一下：书架的示例里，radiusX 和 radiusY 给的都是 15。两个半径是要各填各的，一个管横向的弯、一个管纵向的弯，两个值填得不一致的时候，角落就压成扁的。多数场合咱们填一样的值，参数表却留着各自的自由。

创建出来的是各家的具体类型，咱们用的时候直接把它当 `ID2D1Geometry` 交给绘制方法就行。简单三件管得住的是板正的形状，您想要多边形、曲线、组合形状，它们就不够用了，咱们得请出本篇真正的主角。

## 描边与填充是两个动词

几何对象有了，画它的方法是两个。`FillGeometry` 的参数表是几何加画刷，管的是把形状的内部铺满。`DrawGeometry` 在这两样之外还多收了两个参数，管的是沿着形状的轮廓走一笔描边。咱们可以把它们跟上一篇的 `FillRoundedRectangle`、`DrawRoundedRectangle` 对着看，分工是一模一样的：填充是不问边的，描边是不管内部的，同一形状填一遍再描一遍是常见的组合。画刷从哪儿来、怎么换颜色，上一篇咱们已经铺好了，本篇也就不再重复了。

咱们要是又填又描，顺序就有讲究了：后画的压在前画的上面，所以常见的顺序是填充打底、描边收边，描出来的轮廓不会被填充盖住。这个次序是绘制调用的次序说了算的，跟几何是没有关系的，咱们记在调用侧就好。

咱们再凑近看一眼 `FillGeometry` 的参数表，头文件 `d2d1.h` 里它的第三个参数是个可选的 opacityBrush，省略的时候就是普通的填充。您要是把一支画刷递了进去，它就被当成了一张透明度蒙版，罩在整片填充的上面。头文件里的注释还给了一个限制：蒙版一旦指定了，填充用的画刷就得是位图画刷、延展方式还得是 `D2D1_EXTEND_MODE_CLAMP`。透明度在这里的算法，跟上一篇讲的 opacity 乘法是同一家子，咱们不重复展开，您只要记得入口在参数表里就行。

`DrawGeometry` 多收的第一个参数是 strokeWidth，它的单位是 DIP（设备无关像素，Direct2D 的坐标单位，换算的内情在[第三篇《坐标、DIP 与 DPI》](03_dip_and_dpi.md)）。线宽是每一次描边调用自己的参数，同一个几何拿不同线宽画几遍是完全正当的用法，咱们后面的示例就要这么干。再往后的 strokeStyle 参数要是省略了，描边出来的就是实线配平头尖角的默认样子，您想改它，咱们就得把样式对象认下来，路径的讲法放完咱们再到它那儿去。

## 路径几何：把形状逐段说出来

路径几何的接口是 `ID2D1PathGeometry`，在几何家族里它是最能装的一个，咱们想把直线段、贝塞尔曲线段和弧线段怎么混着拼都行，装出的形状想多复杂都可以。创建还是要找工厂的，但工厂给的只是一个空壳，真正的形状得咱们另走一道手续填进去：

```cpp
ID2D1PathGeometry* pPath = NULL;
pFactory->CreatePathGeometry(&pPath);

ID2D1GeometrySink* pSink = NULL;
pPath->Open(&pSink);
```

`Open` 交到咱们手上的是一个 `ID2D1GeometrySink`，中文的名字叫几何体接收器。咱们可以把它想成一个漏斗：形状的数据从这儿倒进去，倒完了它替咱们把路径组装好。倒数据的章法是这样的：一个路径由一个或多个图形（figure）组成，一个图形指的是一段连续的笔画。`BeginFigure` 起的是一个图形的头，头一个参数给的是起点坐标，第二个参数咱们留到下一节专门说。

之后的每一段都从“当前位置”接着往下走：`AddLine` 走的是直线，一头扎到指定的终点，`AddBezier` 走的是曲线，`AddArc` 走的是一段椭圆弧。`EndFigure` 收掉的是当前图形的尾巴，之后咱们还能再 `BeginFigure` 起下一段，同一个路径里装几个图形都随咱们的便。

接收器身上还有两个批量款的帮手，头文件里跟在单个版的后头：`AddLines` 收的是点数组，一口气接上去的是多条直线，`AddBeziers` 收的是 `D2D1_BEZIER_SEGMENT` 数组，一口气接上去的是多条曲线。书架的五角星用的就是循环里逐段的 `AddLine`，咱们本篇的示例也是逐段的写法，您要是手里已经有了成排的点，批量款连咱们自己的循环都省了。

三种段里最值得咱们认识的是贝塞尔曲线段。`AddBezier` 收的结构体叫 `D2D1_BEZIER_SEGMENT`，头文件 `d2d1.h` 里给它配的注释就一句，说的是“描述路径里的一条三次贝塞尔”。它的三个点里，point1 和 point2 干的是控制点的活，point3 管的是终点。起点是不写在结构体里的，因为起点就是图形当前走到的位置，曲线是从那儿出发的。三次贝塞尔的脾气咱们用白话讲：曲线从起点奔终点去，两个控制点好比是两只拉线的手，各自拉着曲线朝自己的方向拽，控制点的位置放得越远，拽出来的弯也就越狠。SVG、PDF 这些矢量格式的曲线都建立在贝塞尔曲线家族上，三次款是其中的主力，书架说贝塞尔曲线是矢量图形的基石。

曲线其实还有一个二次款的表亲，`AddQuadraticBezier` 带的是单个控制点，适合的是弯得不复杂的弧，书架的示例里它也有出镜。头文件的注释把二次贝塞尔也列进了路径能装的家当，咱们本篇不展开它，三次款对咱们够用了。

`AddArc` 的参数表咱们照书架的代码念：`D2D1::ArcSegment` 里装的是弧的终点、椭圆的两个半径、旋转角，还有扫过的方向和弧的大小。咱们要注意，弧的描述里没有圆心，也没有起始角和扫过的角度，Direct2D 要的是“到哪儿去、坐多大的椭圆、朝哪边弯”，符合条件的弧由它自己解出来。方向枚举 `D2D1_SWEEP_DIRECTION` 挑的是顺时针还是逆时针，大小枚举 `D2D1_ARC_SIZE` 挑的是小弧还是大弧。本篇的示例用不上它，您真要用的时候，拿书架那五行参数对一遍就有感觉了。

## 图形的开口与闭口

`BeginFigure` 的第二个参数和 `EndFigure` 的参数是一对开关，咱们把组合摆开了说。begin 那一侧的两个值是 `D2D1_FIGURE_BEGIN_FILLED` 和 `D2D1_FIGURE_BEGIN_HOLLOW`：拿 FILLED 起头的图形是参与填充的，拿 HOLLOW 起头的图形就是空心的了，填充的时候会被整个跳过，描边倒是照常。end 那一侧的两个值是 `D2D1_FIGURE_END_OPEN` 和 `D2D1_FIGURE_END_CLOSED`：拿 OPEN 收尾的图形，描边走到终点就停了，不会补一条回起点的线，拿 CLOSED 收尾的图形，Direct2D 会替咱们补一条直线回到起点，描边连补出来的线也一起画上。

开了口的图形要是被 `FillGeometry` 填了会怎样呢？官方文档的说法是它会被当作闭合的来填，缺口上等于压着一条隐形的直线。咱们在这句话上留个验证的口子，动手清单的第二处专门看它。还有一个组合咱们顺口记下：FILLED 配 OPEN 是合法的，begin 管的只是填充时算不算数，end 管的只是描边闭不闭口，它们管的是两件不相干的事。

咱们拿两个真实的形状对号入座。书架的五角星走的是 FILLED 配 CLOSED 的组合，十个顶点是首尾相接的，填出来是实心的星，描边在收尾处是严丝合缝的。咱们本篇的对勾走的是另一个极端，HOLLOW 配 OPEN 的组合，描边是有的，填充是没有的，两端也是不封口的。剩下的两个组合咱们也补齐：FILLED 配 OPEN 填出来照样是实心的，缺口在填充里是看不出来的，HOLLOW 配 CLOSED 的描边会多出一条回起点的直线，填充却是空的。

一个路径里装多个图形的时候，begin 的记号是跟着图形各记各的：咱们可以一个图形拿 FILLED、另一个拿 HOLLOW，填的时候 Direct2D 按各自的记号分别对待。书架五角星的十个顶点装的是单个图形，真要拼组合的标志图形，多个图形共用一条路径的写法就有用武之地了。

## 忘了 Close 的下场

数据倒完了之后还有一步，书架几何章用加粗警告标出来的就是它：`pSink->Close()`。咱们要是忘了调 `Close`，几何也就白建了，拿去画的时候什么都不画，报错是没有的，崩溃也是没有的，书架作者的原话是排查它花掉了大半天，因为调试器里单步是走得干干净净的，一点提示也不给咱们。所以咱们得把肌肉记忆定成一对：`Open` 的时候拿接收器，倒完了数据就 `Close` 交货，然后咱们再 `SafeRelease(&pSink)` 把接收器放走。

`Close` 的返回值是 `HRESULT`，咱们是可以拿 `SUCCEEDED` 去判它的，一整串的创建顺不顺，答案就落在它的返回里。笔者的习惯是判创建函数的返回，Close 这儿咱们也顺手判一下，多花的只是一行。

书架还有一处提醒咱们得接住：它的五角星示例把几何的创建写在了 OnPaint 里，咱们每画一帧它就重建一遍。书架自己注明的是，写法图的是演示从简，实际项目里的几何体是设备无关资源，应该在初始化的时候创建并缓存下来，每帧建了毁、毁了建的话，性能会大幅地下跌。书架的警告为什么下得这么重，咱们讲完资源归属就明白了。

## 描边的讲究：宽度、线帽与虚线

线宽咱们已经认了，剩下的讲究都装在一个叫 `ID2D1StrokeStyle` 的对象里，咱们按它管的事逐个看。线帽 cap 是线段两端收边的形状，枚举 `D2D1_CAP_STYLE` 里有四个值：FLAT 是齐头切断的，SQUARE 是方形地往外扩一截的，ROUND 是半圆的头，TRIANGLE 是三角的头。

转角 join 是两条线段相接处的形状，`D2D1_LINE_JOIN` 里 MITER 是尖的角，BEVEL 是斜切的角，ROUND 是圆的角，MITER_OR_BEVEL 的章法是有条件地保尖，太尖了就自动降成斜切，分寸的松紧由 miterLimit 把着，书架示例里给的 10.0f 就是它的常用值。虚线 dash 是实线切成一段一段的排法，咱们直接看真迹。

虚线的完整写法，书架[自绘控件章](../native_win32/49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md)给按钮焦点框画虚线矩形的代码就是现成的例子，咱们原样搬过来：

```cpp
float dashes[] = { 2.0f, 2.0f };
ComPtr<ID2D1StrokeStyle> pStrokeStyle;
pFactory->CreateStrokeStyle(
    D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_FLAT,
        D2D1_CAP_STYLE_FLAT,
        D2D1_CAP_STYLE_FLAT,
        D2D1_LINE_JOIN_MITER,
        10.0f,
        D2D1_DASH_STYLE_CUSTOM,
        0.0f),
    dashes, 2, &pStrokeStyle
);
```

书架在那段代码的上头还配了一行中文注释，说的是实际项目里应该把样式缓存下来，绘制的时候不要每次都创建。创建的入口您也看到了，走的还是 `pFactory->CreateStrokeStyle`，它挂在的是工厂身上而不是渲染目标的身上。第六篇的资源对照表里，`ID2D1StrokeStyle` 就登记在设备无关的那一栏。

属性结构体 `D2D1_STROKE_STYLE_PROPERTIES` 的栏位一共有七个，`D2D1::StrokeStyleProperties` 帮咱们按顺序填。咱们把七栏分两拨认：头三栏是管线帽的，管的分别是起头、收尾和虚线段自己的收边，后面四栏装的分别是转角、miterLimit、虚线样式和虚线偏移。

dashStyle 给了 `D2D1_DASH_STYLE_CUSTOM`，后面跟着的数组才真正生效：数组里的数，一个记的是实段长，一个记的是空段长，两个数是交替着用的，一路循环到路径的末尾，单位用的还是 DIP。末位的 dashOffset 说的是排样从哪一步开始画，咱们把它挪一挪，虚线的断口就会沿着轮廓滑动。

样式对象跟几何一样是能复用的，一支样式是可以配着矩形、圆角矩形和路径一起用的，`DrawRectangle` 一类快捷方法的参数表里同样有它的位置。经典的角色是选框用的“蚂蚁线”，也就是虚线沿着轮廓爬动的效果，动画的每一帧咱们把 dashOffset 往前挪一点，虚线看起来就是在爬动的。咱们本篇只在动手清单的第三处挪一步，动画的完整做法归后面的[《动画与帧调度》](34_animation_frames.md)。

## 示例：手画一个对勾

咱们把本篇的新家当拼成一个完整的小段：一条手画感的对勾路径，配两副描边的样子。设备一侧的骨架（工厂的出生、渲染目标的创建回路、纯色刷）沿用前两篇的代码，咱们不重贴。几何和描边样式都是设备无关的资源，咱们给它们的待遇是建一次用到底：建在启动的时候，释放在退出的时候，设备丢失的回路里轮不到它们。

```cpp
static ID2D1PathGeometry* g_pCheck     = NULL;
static ID2D1StrokeStyle*  g_pInkStyle  = NULL;
static ID2D1StrokeStyle*  g_pDashStyle = NULL;

static void CreateGeometryResources(ID2D1Factory* pFactory)
{
    if (g_pCheck != NULL) {
        return;  // device-independent: built once for the whole run
    }

    pFactory->CreatePathGeometry(&g_pCheck);

    ID2D1GeometrySink* pSink = NULL;
    g_pCheck->Open(&pSink);

    // One figure: a short straight arm down to the corner,
    // then one cubic bezier sweeping up to the tip.
    pSink->BeginFigure(
        D2D1::Point2F(48.0f, 104.0f),
        D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddLine(D2D1::Point2F(86.0f, 142.0f));
    pSink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(104.0f, 124.0f),   // control point 1
        D2D1::Point2F(142.0f, 84.0f),    // control point 2
        D2D1::Point2F(196.0f, 46.0f)));  // end point
    pSink->EndFigure(D2D1_FIGURE_END_OPEN);

    pSink->Close();  // forget this and nothing draws - no error at all
    SafeRelease(&pSink);

    // Pen-like stroke: round caps and joins at 9 DIP width.
    pFactory->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0f,
            D2D1_DASH_STYLE_SOLID, 0.0f),
        NULL, 0, &g_pInkStyle);

    // Thin dashed underlay: same geometry, different clothes.
    const FLOAT dashes[2] = { 2.0f, 2.0f };
    pFactory->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
            D2D1_LINE_JOIN_MITER, 10.0f,
            D2D1_DASH_STYLE_CUSTOM, 0.0f),
        dashes, 2, &g_pDashStyle);
}
```

咱们把倒数据的代码读一遍。对勾装在了一个图形里：起点是在左边落笔的，一条短线压到下面的拐角，再拿一条贝塞尔甩上去收了尾。begin 给的是 HOLLOW，因为咱们只想描它的边，填充是没打算做的。end 给的是 OPEN，因为手画的勾，两端是不该被连上的。线帽和转角都取了 ROUND，9 个 DIP 宽的笔配上圆头，才有笔锋圆收的味道，您要是把 join 换回 MITER，宽笔之下拐角会扎出一个方的刺，对比是一眼就看得出来的。

控制点的坐标咱们也交代一下思路：头一个控制点摆在拐角的外下方，曲线离开拐角的时候会朝外鼓一下再往上走，第二个控制点压在长臂的上沿，把甩上去的势头收住。手画的勾好看就好看在带一点弯，直上直下的两条直线就成打印体了。

调用顺序咱们再数一遍：`CreatePathGeometry`、`Open`、`BeginFigure`、两段、`EndFigure`、`Close`，一步都是跳不得的，动手清单的第一处就让咱们亲手忘它一回。

画它的部分很短，咱们拿同一个几何画两遍：

```cpp
    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));

    g_pSolid->SetColor(D2D1::ColorF(0.30f, 0.85f, 0.40f, 1.0f));
    g_pRenderTarget->DrawGeometry(g_pCheck, g_pSolid, 9.0f, g_pInkStyle);

    g_pSolid->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.35f));
    g_pRenderTarget->DrawGeometry(g_pCheck, g_pSolid, 1.5f, g_pDashStyle);

    HRESULT hr = g_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardBrushResources();
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);  // repaint to rebuild
    }
```

两笔 `DrawGeometry` 递进去的是同一个 `g_pCheck`，换掉的只是画刷、线宽和样式。形状与它的两种画法分开放着，咱们前面说的“形状是对象”到这儿落了地：对象是建一次的，怎么画是每一笔自己的事。`EndDraw` 之后的设备丢失处置跟前两篇是一样的，咱们回头多看一眼就会看到，两个 Discard 的名单里没有今天的任何新家当。

咱们编出来跑一下，您看到的是深灰的底上一条墨绿色的粗对勾，笔头是带着圆角的，底下还压着一条半透明白的细虚线，同一条路径的两种气质。您要是把对勾的 HOLLOW 改成 FILLED，画面上倒是看不出变化的，因为咱们只描了边，填不填的差别，得等您把画笔递过去才算数。

## 几何是设备无关资源

前两篇咱们把资源分成了设备相关与设备无关的两栏，画刷住的是相关那一栏：它生下来就绑着创建它的渲染目标，设备丢失的 `D2DERR_RECREATE_TARGET` 一报回来，它就跟着目标一起作废了。几何和描边样式住的是另一栏：形状的坐标和曲线跟哪块 GPU 都没有关系，所以它们的创建挂在工厂的名下，建一次就够用了，能从程序的开头一直活到结尾。上一篇的 `DiscardBrushResources` 名单里放的是画刷和 stop 集合（渐变里逐个登记的颜色断点），本篇的几何与两个样式都不进名单，设备丢了它们照活着，重建的活儿一件都落不到它们的头上，程序退出的时候 `Release` 一次就算收尾。

创建的位置咱们也定得干脆：工厂在程序启动的时候建好（第六篇的章法），几何和样式是跟着工厂走的，也建在启动的时候，一份是常驻的。您把它们建在第一次绘制的时候也不算错，守卫保证了只建一次，只是咱们读代码的时候，启动处的一份名单比散在绘制里的创建好认得多。

至于书架为什么要警告每帧创建的写法，第六篇咱们坦白过一笔：几何的创建贵、路径要逐点算曲线的说法，是笔者按常规实现补的理解，咱们没有量过数。但方向是稳的：设备无关的活物建一次用到底，本篇的示例里那个“指针非空就返回”的守卫，给几何与样式配上的就是跟画刷同款的待遇，帧里做的只剩画了，建对象的活一件都没有。

收尾的代码咱们照着名单写全，落在窗口销毁的地方：

```cpp
case WM_DESTROY:
    DiscardBrushResources();      // device-dependent: gone with the target
    DiscardDeviceResources();
    SafeRelease(&g_pDashStyle);   // device-independent: live until exit
    SafeRelease(&g_pInkStyle);
    SafeRelease(&g_pCheck);
    SafeRelease(&g_pFactory);     // the factory outlives everything
    PostQuitMessage(0);
    return 0;
```

三个 SafeRelease 摆在两个 Discard 的后面，顺序咱们一眼就看得懂：设备相关的跟着设备走，几何、样式和工厂陪到程序的最后一刻。您拿它跟前两篇的销毁代码一比，名单的边界就是资源的边界。

## 几何身上的进阶活儿

`ID2D1Geometry` 的基类身上还有一批方法，本篇是不逐个展开的，咱们一件一句话带过，您知道柜子里存了什么就行。`Widen` 把描边本身膨胀成一块新的填充几何，书架拿它当给描边做渐变填充的入口。`CombineWithGeometry` 做的是两个几何的布尔运算，`D2D1_COMBINE_MODE` 的四种模式并、交、异或、差各算一种，咱们把两个圆错开着一叠就能看到形状。书架的示例用的是两个同大的圆，圆心是错开 80 个 DIP 的，并集的形状像一枚胶囊，交集的形状像一枚透镜。

`Simplify` 干的是拍平的活，咱们用 `D2D1_GEOMETRY_SIMPLIFICATION_OPTION` 挑的是档位：一档会把曲线全部拍平成直线段，另一档则给咱们留下三次贝塞尔和直线。`GetBounds` 和 `GetWidenedBounds` 算的是边界矩形，一个是不含线宽的，一个把线宽也算进去了，脏区登记和布局的活儿它们都接得住。

书架的常见问题一节还提过精度的旋钮：命中测试还带的是一个可选的 flatteningTolerance 参数，默认的取值是 `D2D1_DEFAULT_FLATTENING_TOLERANCE`，书架给的说法是约 0.25 像素，咱们觉得判断不够精细的时候可以调小它，书架也说默认的值通常已经够用了。

真正替咱们答上“鼠标点没点中”这个问题的是命中测试。`FillContainsPoint` 问的是一个点在不在形状的内部，`StrokeContainsPoint` 问的是一个点在不在描边的附近，线宽在这里是当容差用的，以描边的中心线为轴、5 个 DIP 宽的一条带里都算命中。书架几何章后半段的可点击五角星，整段用的就是它：悬停的状态一变，就拿 `InvalidateRect` 请了一回重画，那套状态驱动重画的机制，咱们在[骨架线](../gui/06_property_repaint.md)（仓库里另一条 GUI 教程线）造过，对应的那一篇讲的就是这套机制怎么搭。有了它，形状就不光是能画给人看的了，鼠标的命中它也接得住。

## 动手清单

动手的三处观察，每一处咱们改的都不出两三行、编一遍，本篇最难缠的机关就都在里面了：

1. 咱们把 `CreateGeometryResources` 里那句 `pSink->Close()` 注释掉，编出来对勾消失，没有报错，调试器也走得干干净净。书架作者找了半天的静默失败，咱们花两分钟亲眼看一遍。
2. 咱们把 `EndFigure` 的参数换成 `D2D1_FIGURE_END_CLOSED`，勾的起笔和收笔之间多出一条直线，闭口画进了描边。您再把 begin 换成 FILLED、`DrawGeometry` 换成 `FillGeometry`，开口的勾照样被填出一整块形状，缺口那条边就是官方文档说的隐形直线。
3. 咱们把 `g_pDashStyle` 里的 dashOffset 从 0.0f 改成 1.5f，底下那条虚线沿着轮廓滑了一步。排样从哪一步开始，就是这个参数在管。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 简单几何三件的创建函数各收什么参数？它们挂在谁的身上？
2. `AddBezier` 收的 `D2D1_BEZIER_SEGMENT` 里，起点在哪儿？两个控制点又管着什么？
3. FILLED 配 OPEN 的图形拿去填充，Direct2D 按什么来填？
4. 忘了调 `Close`，您拿几何去画会看到什么？
5. miterLimit 把着的是什么的分寸？
6. `D2DERR_RECREATE_TARGET` 报回来之后，几何和描边样式为什么不用重建？
7. 同一个几何用两种线宽画两遍，咱们用到的调用长什么样？
8. 虚线的断口想沿着轮廓滑动，咱们该动哪个参数？

## 下一步

本篇的形状都活在渲染目标的坐标系里，坐标本身怎么动还没有讲过：平移、旋转、缩放怎么用一套矩阵说清，压在后续绘制身上的 `SetTransform` 状态第六篇提过一嘴。按[篇目索引](index.md)的登记，下一篇讲的就是坐标变换，咱们到那边把画布搬来搬去的机制接上，对勾也就可以转起来了。
