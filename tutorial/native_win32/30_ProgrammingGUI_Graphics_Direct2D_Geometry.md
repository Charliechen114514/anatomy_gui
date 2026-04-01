# 通用GUI编程技术——图形渲染实战（三十）——Direct2D几何体系统：从路径到命中测试

> 在上一篇文章中，我们搭建了 Direct2D 的基础框架——创建工厂、创建渲染目标、处理设备丢失，并且成功在窗口中绘制了一个旋转的三角形。如果你跟着做了，应该已经感受到了 Direct2D 的渲染质量：线条平滑、没有锯齿、GPU 加速让 60fps 动画毫无压力。
>
> 今天我们要深入的是 Direct2D 的几何体系统。GDI 时代我们只有 `Rectangle`、`Ellipse`、`Polygon` 这几个简单的图形函数，想做复杂一点的自定义形状就得手动逐像素填充。Direct2D 的几何体系统要强大得多——它支持贝塞尔曲线、路径组合的布尔运算、描边扩展，以及最重要的：精确的命中测试。这些能力是构建交互式图形应用的基础，不管是矢量绘图工具还是数据可视化图表，都离不开几何体系统。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形库**: Direct2D（链接 `d2d1.lib`）
- **前置知识**: 文章 29（Direct2D 架构与初始化）

## 内置几何体：矩形、圆角矩形和椭圆

Direct2D 的 `ID2D1RenderTarget` 提供了直接绘制基本形状的方法，不需要显式创建几何体对象：

```cpp
// 绘制矩形（描边）
pRenderTarget->DrawRectangle(
    D2D1::RectF(50.0f, 50.0f, 300.0f, 200.0f),  // 矩形区域
    pBrush,                                         // 画刷
    2.0f                                            // 线宽
);

// 填充圆角矩形
D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
    D2D1::RectF(50.0f, 50.0f, 300.0f, 200.0f),
    15.0f,  // X 圆角半径
    15.0f   // Y 圆角半径
);
pRenderTarget->FillRoundedRectangle(rr, pBrush);

// 绘制椭圆
D2D1_ELLIPSE ellipse = D2D1::Ellipse(
    D2D1::Point2F(400.0f, 150.0f),  // 中心点
    100.0f,  // X 半径
    80.0f    // Y 半径
);
pRenderTarget->DrawEllipse(ellipse, pBrush, 2.0f);
```

这些方法直接在 `BeginDraw`/`EndDraw` 之间调用即可。但它们只是"即时绘制"，没有办法对形状进行后续操作（比如命中测试、布尔运算）。如果你需要这些高级功能，就必须使用独立的几何体对象。

## 路径几何体（PathGeometry）：自由形状的核心

`ID2D1PathGeometry` 是 Direct2D 中最灵活的几何体类型。它由一系列线段和曲线段组成，可以表达任意复杂的 2D 形状。

### 绘制五角星的完整示例

我们来用 `PathGeometry` 绘制一个五角星，这是理解路径几何体工作方式的最佳示例：

```cpp
ID2D1PathGeometry* CreateStarGeometry(ID2D1Factory* pFactory,
    float cx, float cy, float outerR, float innerR)
{
    ID2D1PathGeometry* pPath = NULL;
    pFactory->CreatePathGeometry(&pPath);

    ID2D1GeometrySink* pSink = NULL;
    pPath->Open(&pSink);

    // 五角星有 10 个顶点：5 个外顶点 + 5 个内顶点
    pSink->BeginFigure(
        D2D1::Point2F(cx, cy - outerR),  // 从顶部外顶点开始
        D2D1_FIGURE_BEGIN_FILLED);

    for (int i = 1; i <= 9; i++) {
        float angle = -3.14159f / 2.0f + i * (3.14159f / 5.0f);
        float r = (i % 2 == 0) ? outerR : innerR;
        pSink->AddLine(D2D1::Point2F(
            cx + r * cosf(angle),
            cy + r * sinf(angle)));
    }

    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pSink->Close();  // ⚠️ 必须调用 Close！

    SafeRelease(&pSink);
    return pPath;
}
```

这段代码有几个关键点需要注意。

`Open` 方法返回一个 `ID2D1GeometrySink`（几何体接收器），你通过这个接收器来定义路径的形状。路径由一个或多个"图形"（Figure）组成，每个图形由 `BeginFigure` 开始、`EndFigure` 结束。

`BeginFigure` 的第一个参数是起始点坐标，第二个参数指定填充模式：`D2D1_FIGURE_BEGIN_FILLED` 表示这个图形参与填充计算，`D2D1_FIGURE_BEGIN_HOLLOW` 表示只做描边不填充。

⚠️ 注意一个非常重要的坑：`GeometrySink` 使用完毕后**必须调用 `Close()`**。未 Close 的几何体在绘制时会静默忽略——不报错，不崩溃，但什么都不画。这个坑花了我大半天时间才排查出来，因为调试器不会给你任何提示。

### 在 WM_PAINT 中使用

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);
    if (!g_pRT) return;

    // 创建五角星几何体（只需创建一次，建议缓存）
    ID2D1PathGeometry* pStar = CreateStarGeometry(g_pFactory, 300, 250, 100, 40);

    g_pRT->BeginDraw();
    g_pRT->Clear(D2D1::ColorF(0.1f, 0.1f, 0.12f, 1.0f));

    // 填充五角星
    g_pBrush->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.0f, 1.0f));  // 金色
    g_pRT->FillGeometry(pStar, g_pBrush);

    // 描边
    g_pBrush->SetColor(D2D1::ColorF(0.8f, 0.6f, 0.0f, 1.0f));  // 深金色
    g_pRT->DrawGeometry(pStar, g_pBrush, 2.0f);

    HRESULT hr = g_pRT->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);
    }

    SafeRelease(&pStar);
}
```

⚠️ 注意，上面的示例每次 `OnPaint` 都创建几何体，这只是为了演示简洁。实际项目中，几何体是设备无关资源，应该在初始化时创建并缓存。如果你每帧都创建和销毁几何体对象，性能会大幅下降。

## 贝塞尔曲线段

路径几何体不仅支持直线段（`AddLine`），还支持贝塞尔曲线：

```cpp
// 三次贝塞尔曲线（Cubic Bezier）
pSink->AddBezier(D2D1::BezierSegment(
    D2D1::Point2F(100, 50),    // 控制点1
    D2D1::Point2F(200, 50),    // 控制点2
    D2D1::Point2F(250, 150)    // 终点
));

// 二次贝塞尔曲线（Quadratic Bezier）
pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
    D2D1::Point2F(150, 50),    // 控制点
    D2D1::Point2F(250, 150)    // 终点
));

// 弧线段
pSink->AddArc(D2D1::ArcSegment(
    D2D1::Point2F(300, 150),   // 弧线终点
    D2D1::SizeF(80, 80),       // 椭圆半径
    0.0f,                       // 旋转角度
    D2D1_SWEEP_DIRECTION_CLOCKWISE,
    D2D1_ARC_SIZE_SMALL
));
```

贝塞尔曲线是矢量图形的基石。三次贝塞尔有两个控制点，可以表达 S 形曲线；二次贝塞尔有一个控制点，适合表达简单的弧线。所有主流矢量图形格式（SVG、PDF、AI）的曲线都基于贝塞尔曲线。

## 几何体的布尔运算

Direct2D 支持对几何体进行布尔运算——并集（Union）、交集（Intersect）、差集（Xor/Exclude）。这些操作通过 `CombineWithGeometry` 方法实现：

```cpp
// 创建两个圆形
ID2D1EllipseGeometry* pCircle1 = NULL;
ID2D1EllipseGeometry* pCircle2 = NULL;

g_pFactory->CreateEllipseGeometry(
    D2D1::Ellipse(D2D1::Point2F(200, 200), 100, 100), &pCircle1);
g_pFactory->CreateEllipseGeometry(
    D2D1::Ellipse(D2D1::Point2F(280, 200), 100, 100), &pCircle2);

// 求并集
ID2D1PathGeometry* pUnion = NULL;
g_pFactory->CreatePathGeometry(&pUnion);
ID2D1GeometrySink* pUnionSink = NULL;
pUnion->Open(&pUnionSink);

pCircle1->CombineWithGeometry(
    pCircle2,
    D2D1_COMBINE_MODE_UNION,  // 并集
    NULL,                       // 变换矩阵（可选）
    pUnionSink
);

pUnionSink->Close();
SafeRelease(&pUnionSink);
```

`D2D1_COMBINE_MODE` 枚举提供了四种布尔运算：`UNION`（并集，两个形状合并）、`INTERSECT`（交集，只保留重叠部分）、`XOR`（异或，重叠部分被去掉）、`EXCLUDE`（差集，从第一个形状中去掉第二个形状）。这些操作在构建复杂的自定义形状时非常有用。

## 命中测试：让图形可交互

几何体系统最强大的功能之一是精确的命中测试。GDI 时代你只能用 `PtInRect` 做矩形范围判断，对于不规则形状无能为力。Direct2D 的 `FillContainsPoint` 和 `StrokeContainsPoint` 方法可以精确判断一个点是否在形状内部或描边上。

### 实现可点击的五角星

```cpp
// 全局变量
ID2D1PathGeometry* g_pStarGeometry = NULL;
D2D1_POINT_2F g_starCenter = D2D1::Point2F(300, 250);
float g_starOuterR = 100.0f;
float g_starInnerR = 40.0f;
BOOL g_isStarHovered = FALSE;

// 在 WM_MOUSEMOVE 中进行命中测试
case WM_MOUSEMOVE:
{
    float x = (float)GET_X_LPARAM(lParam);
    float y = (float)GET_Y_LPARAM(lParam);

    if (g_pStarGeometry) {
        BOOL contains = FALSE;
        g_pStarGeometry->FillContainsPoint(
            D2D1::Point2F(x, y),
            NULL,       // 变换矩阵
            &contains   // 输出结果
        );

        if (contains != g_isStarHovered) {
            g_isStarHovered = contains;
            InvalidateRect(hwnd, NULL, FALSE);  // 状态变化时触发重绘
        }
    }
    return 0;
}
```

`FillContainsPoint` 判断点是否在几何体的填充区域内，`StrokeContainsPoint` 判断点是否在描边线段上（可以指定线宽和容差）。命中测试的精确度非常高——即使鼠标指针在五角星的两个角之间（凹陷处），也能正确判断为"不在形状内部"。

### 描边命中测试

```cpp
// 判断点是否在描边线段上（指定线宽容差）
BOOL isOnStroke = FALSE;
g_pStarGeometry->StrokeContainsPoint(
    D2D1::Point2F(x, y),
    5.0f,       // 描边线宽（命中容差）
    NULL,       // 描边样式
    NULL,       // 变换矩阵
    &isOnStroke
);
```

描边命中测试的"线宽"参数实际上充当了命中容差——鼠标指针离描边线段多远以内算"命中"。设为 5.0f 意味着鼠标离描边 5 像素以内就会被检测到。这在做交互式编辑器时特别有用，用户不需要精确地点击在线段上。

## 几何体的其他操作

### Widen：将描边转换为填充区域

`Widen` 方法可以将一个几何体的描边"膨胀"为一个新的填充几何体。这在需要给描边做渐变填充或裁切时很有用：

```cpp
ID2D1PathGeometry* pWidened = NULL;
g_pFactory->CreatePathGeometry(&pWidened);
ID2D1GeometrySink* pWidenSink = NULL;
pWidened->Open(&pWidenSink);

g_pStarGeometry->Widen(
    8.0f,       // 扩展宽度
    NULL,       // 描边样式
    NULL,       // 变换矩阵
    pWidenSink
);
pWidenSink->Close();
SafeRelease(&pWidenSink);
```

### GetBounds 和 GetWidenedBounds：获取边界矩形

```cpp
D2D1_RECT_F bounds;
g_pStarGeometry->GetBounds(NULL, &bounds);

// 获取描边后的边界（考虑线宽）
D2D1_RECT_F widenedBounds;
g_pStarGeometry->GetWidenedBounds(3.0f, NULL, NULL, &widenedBounds);
```

边界矩形在布局计算和脏区刷新时非常有用。比如你可以只在鼠标移动到几何体的边界矩形内时才做精确的命中测试，避免不必要的计算开销。

## 常见问题与调试

### 问题1：几何体绘制不出来，没有报错

首先检查你是否调用了 `GeometrySink::Close()`。未 Close 的几何体在绘制时静默忽略，不会给你任何错误提示。这是 Direct2D 中最隐蔽的坑之一。

### 问题2：CombineWithGeometry 结果为空

如果两个几何体没有重叠，`INTERSECT` 运算会返回一个空几何体。同样，如果第一个几何体完全被第二个包含，`EXCLUDE` 也会返回空。确保你的输入几何体确实有预期的重叠关系。

### 问题3：命中测试精度不够

`FillContainsPoint` 和 `StrokeContainsPoint` 有一个可选的 `flatteningTolerance` 参数，默认值为 `D2D1_DEFAULT_FLATTENING_TOLERANCE`（约 0.25 像素）。如果你发现命中测试不够精确，可以尝试减小这个值，但通常默认值已经足够好了。

## 总结

Direct2D 的几何体系统是一个完整的 2D 矢量图形引擎。从简单的矩形和椭圆到复杂的贝塞尔曲线路径，从布尔运算组合到精确的命中测试，它提供了构建交互式图形应用所需的全部工具。和 GDI 的 `Polygon`/`PolyBezier` 相比，Direct2D 的几何体是独立于渲染的对象——你可以创建一次、多次绘制、随时做命中测试，不需要每次重绘时重新定义形状。

下一步，我们要进入 Direct2D 的效果（Effects）系统。这是 Direct2D 1.1（Windows 8+）引入的图像处理管线——通过连接多个效果节点，你可以实现高斯模糊、阴影、色彩矩阵等高级视觉效果。配合图层（Layer）系统，这些效果可以精确地应用到画面的局部区域，而不是整个窗口。

---

## 练习

1. 用 `PathGeometry` 和贝塞尔曲线绘制一个心形，填充为红色，并在鼠标悬停时变为亮红色。
2. 实现两个圆形的交集、并集、差集和异或可视化：四个按钮切换不同的布尔运算模式。
3. 实现矢量图形编辑器原型：支持点击添加矩形和椭圆，拖拽移动已有图形（使用命中测试），Delete 键删除选中图形。
4. 用 `Widen` 方法将一个复杂路径的描边扩展为填充区域，然后用渐变画刷填充扩展后的形状。

---

**参考资料**:
- [ID2D1PathGeometry interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1pathgeometry)
- [ID2D1GeometrySink interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1geometrysink)
- [ID2D1Geometry::FillContainsPoint - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1geometry-fillcontainspoint)
- [ID2D1Geometry::CombineWithGeometry - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1geometry-combinewithgeometry)
- [Geometries Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-geometries-overview)
- [ID2D1Geometry::Widen - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1geometry-widen)
