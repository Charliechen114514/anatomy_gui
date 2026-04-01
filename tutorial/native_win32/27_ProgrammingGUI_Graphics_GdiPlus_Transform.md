# 通用GUI编程技术——图形渲染实战（二十七）——坐标变换与矩阵：三级坐标系

> 在上一篇文章中，我们正式进入了 GDI+ 的世界——从 `GdiplusStartup` 初始化到 `Graphics` 对象创建，从 `SolidBrush` 实心画刷到 `LinearGradientBrush` 渐变填充，我们用几行代码就实现了 GDI 中需要几十行才能达到的渐变效果和抗锯齿渲染。但 GDI+ 的能力远不止于画几个好看的矩形和饼图。今天我们要进入 GDI+ 最强大也最容易让人迷糊的功能领域——坐标变换。
>
> 你可能觉得"坐标变换"听起来像数学课本里的内容，跟实际编程关系不大。但实际上，坐标变换是图形编程中最实用的工具之一。没有坐标变换，你绘制一个钟表的刻度需要手动计算 60 个刻度线的端点坐标；有了坐标变换，你只需要算一次刻度线，然后旋转坐标系 60 次就行。没有坐标变换，实现一个"拖动旋转图片"的功能需要你自己处理旋转矩阵；有了坐标变换，一个 `RotateTransform` 调用就搞定了。

## 环境说明

继续沿用上一篇的配置：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x 工具集)
- **目标平台**: Win32 原生桌面应用
- **图形库**: GDI+（链接 `gdiplus.lib`，头文件 `<gdiplus.h>`）
- **字符集**: Unicode

⚠️ 本文的代码建立在上一篇的 GDI+ 初始化框架之上。`GdiplusStartup`/`GdiplusShutdown` 的调用位置和全局对象生命周期管理不再重复说明。如果你直接跳到了这篇，请先回顾上一篇文章的"从零开始：初始化 GDI+ 运行时"章节。

## 三级坐标系：世界坐标、页面坐标、设备坐标

在正式动手之前，我们必须先搞清楚 GDI+ 的三级坐标系模型。这是理解坐标变换的基础，也是很多人迷迷糊糊的根源。

根据 Microsoft Learn 的文档，GDI+ 在将图形绘制到屏幕上的过程中，经过了三个坐标空间的转换：世界坐标（World Coordinate）到页面坐标（Page Coordinate）再到设备坐标（Device Coordinate）。这三个坐标空间各有不同的用途。

世界坐标是你代码中直接使用的坐标。当你在代码里写 `graphics.DrawRectangle(&pen, 100, 100, 200, 150)` 时，那个 `(100, 100)` 和 `(200, 150)` 就是世界坐标。你不用关心屏幕的物理尺寸或像素密度，直接用你逻辑上方便的数值就行。

页面坐标是经过世界变换（World Transform）之后得到的坐标。世界变换就是你通过 `RotateTransform`、`TranslateTransform`、`ScaleTransform` 等函数设置的变换矩阵。如果你没有设置任何世界变换，页面坐标就等于世界坐标。但如果你设置了旋转 45 度，那么世界坐标中的一个点 `(100, 0)` 在页面坐标中就变成了约 `(70.7, 70.7)`——因为整个坐标系旋转了。

设备坐标是最终映射到物理设备（屏幕或打印机）上的坐标。页面坐标到设备坐标的转换由页面变换（Page Transform）控制，主要包括 `SetPageUnit`（设置坐标单位）和 `SetPageScale`（设置缩放比例）。默认情况下，页面单位是像素（`UnitPixel`），页面缩放为 1.0，所以页面坐标就等于设备坐标。但你可以把页面单位设成毫米或英寸，GDI+ 会自动根据设备的 DPI 进行换算。

这个三级模型的设计目的是把"逻辑绘制"和"物理输出"解耦。你只需要在世界坐标中思考你的图形布局，GDI+ 负责处理旋转、缩放和设备映射。

### 一个直观的类比

你可以把三级坐标系类比为三层地图。世界坐标是你的手绘草图——你在上面用自己方便的方式标注位置和距离。页面坐标是标准化后的地图——所有位置都被统一到标准的坐标系中。设备坐标是最终打印出来的地图——根据打印机的分辨率和纸张大小进行了适配。你的工作只需要在草图上画画，后续的转换都由系统自动完成。

## 2D 变换矩阵基础

在写代码之前，我们先快速过一下 2D 变换矩阵的数学基础。这块内容不会太深，但足以让你理解后面代码中发生的事情。

### 矩阵的表示方式

GDI+ 使用 3x3 矩阵来表示 2D 仿射变换。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusmatrix/nl-gdiplusmatrix-matrix)，这个矩阵的结构如下：

```
| M11  M12  0 |
| M21  M22  0 |
| DX   DY   1 |
```

其中 `M11` 和 `M22` 控制 X 和 Y 方向的缩放，`M12` 和 `M21` 控制旋转和倾斜，`DX` 和 `DY` 控制平移。第三列是 `[0, 0, 1]`，这是固定的，因为 GDI+ 只支持仿射变换（平行线变换后仍然平行的变换）。

虽然这个矩阵有 9 个元素，但因为第三列固定为 `[0, 0, 1]`，实际只需要存储 6 个值。GDI+ 的 `Matrix` 类就是这样做的——构造函数接受 6 个参数：`m11, m12, m21, m22, dx, dy`。

### 三种基本变换

2D 空间中有三种基本变换：平移、旋转和缩放。每种变换都对应一个特定的矩阵形式。

**平移**（Translation）把所有点在 X 和 Y 方向上移动固定距离：

```
| 1  0  0 |
| 0  1  0 |
| dx dy 1 |
```

这个矩阵的含义很直观——X 坐标加上 `dx`，Y 坐标加上 `dy`，缩放和旋转不变。

**旋转**（Rotation）围绕原点旋转指定角度（弧度）：

```
| cos(θ)   sin(θ)  0 |
| -sin(θ)  cos(θ)  0 |
| 0        0       1 |
```

旋转矩阵让 X 轴方向旋转了 θ 角度。注意 GDI+ 使用的是行向量右乘矩阵的约定——也就是 `[x, y, 1] * Matrix`。这意味着矩阵的排列方式可能和你在线性代数课上学到的列向量约定不同。不过好消息是，你不需要手算这些——GDI+ 的 `RotateTransform`、`TranslateTransform`、`ScaleTransform` 函数已经帮你封装好了。

**缩放**（Scaling）改变坐标系的尺度：

```
| sx  0   0 |
| 0   sy  0 |
| 0   0   1 |
```

`sx` 是 X 方向的缩放因子，`sy` 是 Y 方向的缩放因子。如果 `sx = sy = 2`，所有坐标都放大一倍；如果 `sx = 0.5`，X 方向缩小一半。

### 矩阵乘法的顺序问题

这里有一个非常重要的概念：矩阵乘法不满足交换律。`A * B` 不等于 `B * A`。在坐标变换的语境下，这意味着"先旋转再平移"和"先平移再旋转"的结果通常不同。

考虑一个简单的例子。假设你想把一个图形先平移 (100, 0)，再旋转 45 度。如果先平移，图形移到了 (100, 0) 的位置；然后旋转，围绕原点旋转 45 度，图形的位置变成了约 (70.7, 70.7)。反过来，如果先旋转，图形还在原位但旋转了 45 度；然后平移 (100, 0)，图形移到了 (100, 0) 的位置——但方向是斜的。

这两种结果完全不同。而在 GDI+ 中，变换的叠加是通过矩阵乘法实现的——每次调用 `RotateTransform` 或 `TranslateTransform` 时，GDI+ 实际上是在当前的变换矩阵上乘以一个新的矩阵。乘法的顺序决定了变换的效果。

根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-why-transformation-order-is-significant-use)，GDI+ 的变换函数有两个附加模式：`MatrixOrderPrepend`（前置）和 `MatrixOrderAppend`（后置）。默认是 `MatrixOrderPrepend`——新变换被乘在前面。这意味着你先调用的变换后执行，后调用的变换先执行——这个行为可能和直觉相反。

不过说实话，绝大多数情况下你不需要纠结这个顺序问题。最简单的策略是使用 `Graphics::Save` 和 `Graphics::Restore` 来管理变换状态——每次需要变换时，先 Save 当前的状态，设置变换，绘制，然后 Restore 恢复到之前的状态。这样每次变换都是从干净的状态开始，不会产生累积效应。

## Graphics 的变换方法

理论铺垫够了，现在我们来看看 GDI+ 提供的变换 API。

### TranslateTransform：平移坐标系

`TranslateTransform` 在当前的变换矩阵上叠加一个平移变换。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-translatetransform)，函数签名如下：

```cpp
Status TranslateTransform(REAL dx, REAL dy, MatrixOrder order);
```

`dx` 和 `dy` 是平移的距离，`order` 是矩阵乘法顺序，默认为 `MatrixOrderPrepend`。

```cpp
// 将坐标系原点移动到 (200, 150)
graphics.TranslateTransform(200.0f, 150.0f);

// 此时 (0, 0) 实际上对应屏幕的 (200, 150)
graphics.DrawRectangle(&pen, -50, -50, 100, 100);
// 上面这行在屏幕上绘制一个以 (200, 150) 为中心的正方形
```

平移是最简单的变换，也是复合变换中通常需要最先考虑的部分。当你想围绕某个点旋转时，你需要先平移让该点到达原点，然后旋转，最后平移回去。

### RotateTransform：旋转坐标系

`RotateTransform` 在当前的变换矩阵上叠加一个旋转变换。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-rotatetransform)，函数签名如下：

```cpp
Status RotateTransform(REAL angle, MatrixOrder order);
```

`angle` 是旋转角度，单位是度数（不是弧度），正方向是顺时针。

```cpp
// 旋转 45 度
graphics.RotateTransform(45.0f);

// 此时绘制的所有图形都会被旋转 45 度
Pen pen(Color(255, 255, 0, 0), 2.0f);
graphics.DrawRectangle(&pen, -50, -50, 100, 100);
```

⚠️ 注意，旋转是围绕原点进行的。如果你想围绕一个特定的点旋转，需要用"平移-旋转-平移"三步操作。这是一个非常常见的模式，后面在仪表盘的例子中我们会大量使用。

### ScaleTransform：缩放坐标系

`ScaleTransform` 在当前的变换矩阵上叠加一个缩放变换。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-scaletransform)，函数签名如下：

```cpp
Status ScaleTransform(REAL sx, REAL sy, MatrixOrder order);
```

`sx` 和 `sy` 分别是 X 和 Y 方向的缩放因子。1.0 表示不缩放，2.0 表示放大一倍，0.5 表示缩小一半。

```cpp
// X 方向放大 2 倍，Y 方向不变
graphics.ScaleTransform(2.0f, 1.0f);

Pen pen(Color(255, 0, 0, 255), 2.0f);
graphics.DrawEllipse(&pen, -50, -50, 100, 100);
// 这个圆会被水平拉伸成椭圆
```

缩放同样是以原点为中心进行的。缩放因子可以是负数——负数会产生镜像翻转效果。比如 `sx = -1` 会让 X 坐标取反，产生水平翻转。

### SetTransform：直接设置变换矩阵

前面三个方法都是"叠加"变换——它们在当前变换矩阵上乘以新的矩阵。但有时候你想直接设置整个变换矩阵，跳过累积过程。这时候就用 `SetTransform`：

```cpp
Matrix matrix(1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 50.0f);
graphics.SetTransform(&matrix);
```

`SetTransform` 会完全替换当前的世界变换矩阵。它不会和之前的矩阵相乘——不管之前设置过什么变换，调用 `SetTransform` 后都会被覆盖。这在你需要从一个干净的状态开始设置复杂变换时很有用。

### ResetTransform：重置为默认

如果你想把变换矩阵重置为单位矩阵（也就是没有任何变换），用 `ResetTransform`：

```cpp
graphics.ResetTransform();
```

这等同于 `SetTransform(&单位矩阵)`。在每次绘制循环开始时重置变换是一个好习惯——防止上一帧的变换累积到这一帧。

## Save 和 Restore：状态管理利器

变换矩阵最让人头疼的问题就是"累积"。每次调用 `RotateTransform` 或 `TranslateTransform`，变换效果会叠加到当前矩阵上。如果你不管理这个状态，画完一个旋转的元素后，后续所有元素都会被旋转。

GDI+ 提供了 `Graphics::Save` 和 `Graphics::Restore` 来解决这个问题。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-save)，`Save` 把当前 Graphics 对象的所有状态（变换矩阵、裁剪区域、渲染质量等）保存到一个栈中，返回一个 `GraphicsState` 标识符。`Restore` 则根据这个标识符恢复之前的状态：

```cpp
// 保存当前状态
GraphicsState state = graphics.Save();

// 设置变换、绘制
graphics.TranslateTransform(200.0f, 200.0f);
graphics.RotateTransform(30.0f);
graphics.DrawRectangle(&pen, -25, -25, 50, 50);

// 恢复到之前的状态——变换矩阵被还原
graphics.Restore(state);

// 现在绘制的图形不受前面的变换影响
graphics.DrawRectangle(&pen, 10, 10, 50, 50);
```

`Save`/`Restore` 可以嵌套使用——你可以在一个 Save/Restore 对内部再 Save 一个新的状态。这就像函数调用栈一样，每一层都独立管理自己的状态。

⚠️ 注意，`Save` 和 `Restore` 是成对使用的。如果你 Save 了但忘了 Restore，变换矩阵的状态就会一直累积下去。而且 `Restore` 必须用对应的 `GraphicsState` 值——如果你用一个过期的或者不匹配的 `GraphicsState` 来 Restore，结果是不可预期的。最安全的做法是在同一个作用域内完成 Save 和 Restore：

```cpp
{
    GraphicsState state = graphics.Save();
    // ... 变换和绘制 ...
    graphics.Restore(state);
}
```

用大括号限定作用域可以确保即使你忘了 Restore，编译器也会提醒你。

## 实战演练：指针式仪表盘刻度绘制

好了，理论和 API 都讲完了。现在我们来做一个真正有意思的实战项目——绘制一个指针式仪表盘的刻度。这个例子完美展示了坐标变换的威力：如果没有变换，你需要手动计算 60 个刻度线（分针刻度）和 12 个主刻度线（时刻度）的端点坐标，每个都需要用到三角函数；有了变换，你只需要计算一次刻度线相对于 12 点钟方向的位置，然后旋转坐标系就行。

### 设计思路

我们的仪表盘设计如下：一个圆形表盘，60 个小刻度（每 6 度一个），12 个大刻度（每 30 度一个），加上数字标注。刻度线从圆心向外辐射，大刻度比小刻度更长更粗。

用坐标变换的思路是：对于每个刻度，先平移坐标系到表盘中心，然后旋转到对应角度，最后在旋转后的坐标系中画一条简单的水平线（或者垂直线，取决于你的设计）。由于坐标系已经旋转了，"画一条垂直线"实际上就是在原始坐标系中画了一条指向特定角度的刻度线。

### 完整代码

先看完整的 `WndProc`：

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        // 仪表盘参数
        REAL cx = width / 2.0f;
        REAL cy = height / 2.0f;
        REAL outerRadius = (cx < cy ? cx : cy) * 0.85f;
        REAL innerRadius = outerRadius * 0.80f;
        REAL majorOuterRadius = outerRadius;
        REAL majorInnerRadius = outerRadius * 0.70f;
        REAL labelRadius = outerRadius * 0.60f;

        // 绘制表盘背景
        SolidBrush bgBrush(Color(255, 245, 245, 245));
        graphics.FillEllipse(&bgBrush,
            cx - outerRadius - 10, cy - outerRadius - 10,
            (outerRadius + 10) * 2, (outerRadius + 10) * 2);

        // 绘制外圈
        Pen outerPen(Color(255, 80, 80, 80), 3.0f);
        graphics.DrawEllipse(&outerPen,
            cx - outerRadius, cy - outerRadius,
            outerRadius * 2, outerRadius * 2);

        // 平移坐标系到表盘中心
        graphics.TranslateTransform(cx, cy);

        // ---- 绘制60个小刻度 ----
        Pen minorPen(Color(255, 150, 150, 150), 1.0f);
        Pen majorPen(Color(255, 50, 50, 50), 2.5f);

        for (int i = 0; i < 60; i++) {
            GraphicsState state = graphics.Save();

            // 旋转到对应角度（-90度偏移让0刻度在12点钟方向）
            REAL angle = i * 6.0f - 90.0f;
            graphics.RotateTransform(angle);

            if (i % 5 == 0) {
                // 每5个刻度是一个大刻度
                graphics.DrawLine(&majorPen,
                    0, -majorInnerRadius, 0, -majorOuterRadius);
            } else {
                // 小刻度
                graphics.DrawLine(&minorPen,
                    0, -innerRadius, 0, -outerRadius);
            }

            graphics.Restore(state);
        }

        // ---- 绘制数字标注 ----
        Font font(L"Arial", 14.0f, FontStyleBold);
        SolidBrush textBrush(Color(255, 40, 40, 40);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);

        for (int i = 0; i < 12; i++) {
            GraphicsState state = graphics.Save();

            // 计算数字位置
            REAL angle = (i * 30.0f - 90.0f) * 3.14159265f / 180.0f;
            REAL labelX = labelRadius * cos(angle);
            REAL labelY = labelRadius * sin(angle);

            // 数字值：12点方向显示12，其余依次
            int number = (i == 0) ? 12 : i;

            wchar_t text[8];
            swprintf_s(text, L"%d", number);

            PointF origin(labelX, labelY);
            graphics.DrawString(text, -1, &font, origin, &format, &textBrush);

            graphics.Restore(state);
        }

        // ---- 绘制指针 ----
        // 时针（指向10点方向）
        {
            GraphicsState state = graphics.Save();
            graphics.RotateTransform(-90.0f + 10 * 30.0f);

            Pen hourPen(Color(255, 30, 30, 30), 6.0f);
            hourPen.SetEndCap(LineCapRound);
            graphics.DrawLine(&hourPen, 0, 0, 0, -labelRadius * 0.65f);
            graphics.Restore(state);
        }

        // 分针（指向2点方向）
        {
            GraphicsState state = graphics.Save();
            graphics.RotateTransform(-90.0f + 2 * 30.0f + 20 * 6.0f);

            Pen minutePen(Color(255, 30, 30, 30), 3.0f);
            minutePen.SetEndCap(LineCapRound);
            graphics.DrawLine(&minutePen, 0, 0, 0, -labelRadius * 0.85f);
            graphics.Restore(state);
        }

        // 中心圆点
        SolidBrush centerBrush(Color(255, 200, 50, 50));
        graphics.FillEllipse(&centerBrush, -6, -6, 12, 12);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这段代码比较长，我们来逐段拆解。

### 刻度绘制详解

刻度绘制的核心逻辑在这段代码中：

```cpp
for (int i = 0; i < 60; i++) {
    GraphicsState state = graphics.Save();

    REAL angle = i * 6.0f - 90.0f;
    graphics.RotateTransform(angle);

    if (i % 5 == 0) {
        graphics.DrawLine(&majorPen,
            0, -majorInnerRadius, 0, -majorOuterRadius);
    } else {
        graphics.DrawLine(&minorPen,
            0, -innerRadius, 0, -outerRadius);
    }

    graphics.Restore(state);
}
```

循环 60 次，每次绘制一个刻度。`i * 6.0f` 计算每个刻度对应的角度（360 度 / 60 = 6 度）。`-90.0f` 是偏移量——因为 GDI+ 的 0 度指向 3 点钟方向（正右方），而钟表的 0 刻度在 12 点钟方向（正上方），所以需要减 90 度。

每次循环内部，先 Save 当前状态，然后 RotateTransform 旋转坐标系，再画线，最后 Restore。注意画线时坐标是 `(0, -innerRadius)` 到 `(0, -outerRadius)`——在旋转后的坐标系中，这是一条从内圈到外圈的垂直线。但在原始坐标系中，这条线已经被旋转到了正确的角度。

这就是坐标变换的魔力——你在"局部坐标系"中用最简单的方式描述形状（一条垂直线），坐标变换自动把它映射到"全局坐标系"中的正确位置和角度。

每 5 个刻度（`i % 5 == 0`）是大刻度，使用更粗的画笔和更大的半径。60 / 5 = 12 个大刻度正好对应钟表的 12 个小时。

### 数字标注的放置

数字标注的绘制方式和刻度线稍有不同。我们没有用旋转坐标系的方式来放数字——因为数字不应该跟着坐标系旋转，否则"12"会变成倒着的或者歪着的。取而代之的是用三角函数直接计算每个数字在圆上的位置：

```cpp
REAL angle = (i * 30.0f - 90.0f) * 3.14159265f / 180.0f;
REAL labelX = labelRadius * cos(angle);
REAL labelY = labelRadius * sin(angle);
```

这里将角度转换为弧度（乘以 π/180），然后用 `cos` 和 `sin` 计算圆上的坐标。`labelRadius` 是数字到圆心的距离，比刻度线内圈更靠内。

`StringFormat` 的 `SetAlignment` 和 `SetLineAlignment` 都设为 `StringAlignmentCenter`，让文字以给定点为中心绘制——这样数字会精确地居中在计算出的坐标位置。

这里仍然使用了 `Save`/`Restore`，虽然我们只改变了坐标系原点（通过 `TranslateTransform`），没有额外旋转，但保持这个习惯可以防止意外累积。

### 指针的旋转

时针和分针的绘制是坐标变换最直接的应用。我们用 `RotateTransform` 将一条垂直线旋转到指向特定时刻的方向：

```cpp
// 时针指向10点方向
graphics.RotateTransform(-90.0f + 10 * 30.0f);
graphics.DrawLine(&hourPen, 0, 0, 0, -labelRadius * 0.65f);
```

`-90.0f` 是偏移量（让 0 度对应 12 点钟方向），`10 * 30.0f` 是 10 点钟对应的角度（10 小时 * 30 度/小时）。线条从原点 (0, 0) 向上画（Y 轴负方向），在旋转后就指向了 10 点钟方向。时针长度是 `labelRadius * 0.65f`——比数字标注半径短一些，不会遮住数字。

分针类似，但角度更精细：`2 * 30.0f + 20 * 6.0f` 表示 2 小时 20 分。分针更长（`labelRadius * 0.85f`），画笔更细。

### 为什么用 Save/Restore

你可能在想，为什么每个刻度和每根指针都要 `Save`/`Restore`？能不能一次旋转画完所有刻度？

答案是：不能。因为 `RotateTransform` 是累积的。如果你在循环中不用 `Save`/`Restore`，第一次循环旋转 6 度，第二次旋转 12 度（在第一次的基础上再旋转 6 度），第三次 18 度...但这里用的是绝对角度而不是增量角度，所以累积会让旋转角不对。更关键的是，如果你在循环外还有其他绘制操作（比如数字和指针），它们的坐标系会被循环内的旋转累积影响。

`Save`/`Restore` 保证了每次绘制操作都在一个独立的、干净的变换状态下进行。这是使用坐标变换时最重要的原则：**每次独立的绘制操作都应该从已知的变换状态开始**。

## Matrix 类：直接操作矩阵

`TranslateTransform`、`RotateTransform`、`ScaleTransform` 是便捷方法，它们在 `Graphics` 对象内部的变换矩阵上进行操作。但有时候你需要更精细的控制——比如构建一个复杂的复合变换，然后在多个 `Graphics` 对象之间共享。

GDI+ 提供了 `Matrix` 类来直接操作变换矩阵。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusmatrix/nl-gdiplusmatrix-matrix)，`Matrix` 类可以独立于 `Graphics` 对象创建和操作，然后通过 `SetTransform` 应用到 `Graphics` 上。

### 创建和组合矩阵

```cpp
// 创建一个平移矩阵
Matrix translateMatrix;
translateMatrix.Translate(200.0f, 150.0f);

// 创建一个旋转矩阵
Matrix rotateMatrix;
rotateMatrix.Rotate(45.0f);

// 组合：先平移再旋转
Matrix combinedMatrix;
combinedMatrix.Multiply(&translateMatrix, MatrixOrderAppend);
combinedMatrix.Multiply(&rotateMatrix, MatrixOrderAppend);

// 应用到 Graphics 对象
graphics.SetTransform(&combinedMatrix);
```

`Matrix::Multiply` 的第二个参数控制乘法顺序。`MatrixOrderAppend` 意味着新矩阵乘在右边，效果是"按书写顺序执行变换"——先平移，后旋转。`MatrixOrderPrepend` 则相反。

### 围绕任意点旋转

使用 `Matrix` 类可以很方便地实现围绕任意点的旋转。原理是三步操作：先把旋转中心平移到原点，然后旋转，最后平移回去：

```cpp
Matrix matrix;
matrix.Translate(centerX, centerY, MatrixOrderAppend);  // 第三步：平移回去
matrix.Rotate(angle, MatrixOrderAppend);                  // 第二步：旋转
matrix.Translate(-centerX, -centerY, MatrixOrderAppend);  // 第一步：移到原点
```

⚠️ 注意这里矩阵乘法的顺序。因为我们用 `MatrixOrderAppend`，矩阵是按书写顺序从上到下相乘的。但矩阵乘法应用到点上时，是从右到左执行的。所以代码的执行顺序实际上是：先 `Translate(-centerX, -centerY)`，然后 `Rotate`，最后 `Translate(centerX, centerY)`。这正是我们想要的"平移到原点-旋转-平移回去"的三步操作。

如果你用 `MatrixOrderPrepend`，顺序就反过来了——先执行的在前面。很多新手在这里被绕晕。最简单的办法是：统一使用一种顺序模式，不要混用。本文的示例统一使用 `MatrixOrderAppend`。

### 矩阵的逆矩阵

有时候你需要"撤销"一个变换——比如你已经知道了一个点在变换后的坐标系中的位置，想知道它在原始坐标系中的位置。这就需要逆矩阵：

```cpp
Matrix matrix;
matrix.Rotate(30.0f);
matrix.Translate(100.0f, 50.0f, MatrixOrderAppend);

// 计算逆矩阵
Matrix inverseMatrix;
matrix.Invert(&inverseMatrix);

// 用逆矩阵变换一个点
PointF transformedPoint(200.0f, 100.0f);
inverseMatrix.TransformPoints(&transformedPoint, 1);
// transformedPoint 现在是原始坐标系中的位置
```

`Matrix::Invert` 计算当前矩阵的逆矩阵。如果一个矩阵表示"从原始坐标到变换坐标"的映射，那么它的逆矩阵就表示"从变换坐标回到原始坐标"的映射。这在实现鼠标交互（比如用户点击了屏幕上的一个点，你需要知道它在未变换的坐标系中的位置）时非常有用。

## 变换的实际应用场景

坐标变换不只是画钟表刻度的"技巧"——它在实际图形编程中有大量应用场景。

### 场景一：UI 组件的旋转和缩放

如果你需要实现一个可以被用户拖动旋转的 UI 组件（比如一个旋转的拨盘或者一个可以倾斜的卡片），坐标变换是核心机制。用户的拖动操作被转换为旋转角度，然后通过 `RotateTransform` 应用到绘制过程中。

### 场景二：2D 地图和图表的视口管理

在 2D 地图应用中，用户可以平移和缩放地图。这可以通过 `TranslateTransform`（平移）和 `ScaleTransform`（缩放）来实现。所有的地图元素都使用世界坐标绘制，变换矩阵负责把世界坐标映射到屏幕坐标。用户缩放时改变 `ScaleTransform` 的参数，平移时改变 `TranslateTransform` 的参数。

### 场景三：动画中的运动轨迹

在动画中，对象的运动可以用变换矩阵来描述。每帧更新变换矩阵的参数（平移位置、旋转角度、缩放比例），然后重新绘制。这种方式比直接操作每个像素的坐标要优雅得多。

## 常见问题与调试

### 问题一：变换累积导致图形飞出屏幕

这是最常见的错误。如果你在一个绘制循环中不断调用 `RotateTransform` 而不重置，旋转角度会不断累积，图形会越转越快直到完全看不到。解决办法是在每次绘制操作后用 `Save`/`Restore` 恢复状态，或者在每帧开始时调用 `ResetTransform`。

### 问题二：旋转中心不对

`RotateTransform` 围绕原点旋转，但很多人以为它围绕图形中心旋转。如果你的图形不在原点附近，旋转后图形会飞到意想不到的位置。解决办法是用"平移到原点-旋转-平移回去"的三步操作：

```cpp
graphics.TranslateTransform(cx, cy);      // 移到图形中心
graphics.RotateTransform(angle);          // 旋转
graphics.TranslateTransform(-cx, -cy);    // 移回去
```

或者更简单的方式是先平移坐标系到图形中心，然后在以原点为中心的局部坐标系中绘制图形。

### 问题三：矩阵乘法顺序搞反了

`Matrix::Multiply` 的 `MatrixOrder` 参数让很多人困惑。记住这个规则：`MatrixOrderAppend` 按代码书写顺序执行变换（直觉友好），`MatrixOrderPrepend` 按逆序执行（数学友好）。建议统一使用一种模式。

### 问题四：文字跟着变换旋转了

如果你用 `RotateTransform` 旋转了坐标系，后面绘制的文字也会被旋转。但通常你不希望文字旋转——比如仪表盘上的数字应该始终保持正向。解决办法是在绘制文字之前 `Save` 状态，绘制完文字后 `Restore`——或者干脆用三角函数计算文字位置，不用坐标系旋转来放置文字（就像我们在仪表盘例子中做的那样）。

## 总结

这篇文章我们从三个层面拆解了 GDI+ 的坐标变换系统。首先我们理解了三级坐标系模型——世界坐标、页面坐标、设备坐标——以及它们之间的转换关系。然后我们学习了三种基本变换（平移、旋转、缩放）的矩阵表示和对应的 GDI+ API。最后通过指针式仪表盘的实战演练，我们把所有知识点串联起来，看到了 `Save`/`Restore` 如何管理变换状态、`RotateTransform` 如何简化重复图形的绘制、以及 `Matrix` 类如何实现更精细的变换控制。

坐标变换是图形编程的核心工具。它不仅让代码更简洁（不需要手动计算每个点的坐标），还让代码更具表达力（你描述的是"变换什么"而不是"变换后的结果是什么"）。掌握了坐标变换，你才能在后续的图形编程中得心应手。

下一篇文章我们将转向 GDI+ 的另一个重要功能——图像格式与编解码。GDI+ 内置支持 BMP、PNG、JPEG、GIF、TIFF 等多种格式，我们将学习如何加载、转换和保存这些格式的图片，包括 JPEG 质量参数的设置和 `GetImageEncoders` 的使用。到时候见。

---

**练习**

1. **动态时钟**：将本文的仪表盘示例改造成一个实时时钟——用 `WM_TIMER` 每秒更新一次，根据系统时间计算时针和分针的角度。提示：用 `GetLocalTime` 获取当前时间。

2. **可旋转图片查看器**：实现一个简单的图片查看器，支持鼠标拖动旋转和滚轮缩放。提示：在 `WM_MOUSEMOVE` 中记录鼠标移动的角度，在 `WM_MOUSEWHEEL` 中调整缩放比例，在 `WM_PAINT` 中用 `RotateTransform` 和 `ScaleTransform` 应用变换。用矩阵维护变换状态。

3. **万花尺**：用坐标变换实现一个万花尺效果。一个圆在另一个圆内滚动，笔尖的轨迹形成复杂的几何图案。提示：使用 `Matrix` 的复合变换来计算每一步的旋转和平移。

4. **变换顺序实验**：创建一个演示程序，让用户可以交互式地调整三种基本变换的参数和顺序。提供一个界面来添加"平移→旋转→缩放"或"缩放→旋转→平移"等不同的变换组合，用同一个图形（比如一个带标记的矩形或箭头）展示不同变换顺序的结果差异。

---

**参考资料**

- [Graphics::RotateTransform - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-rotatetransform)
- [Graphics::TranslateTransform - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-translatetransform)
- [Graphics::ScaleTransform - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-scaletransform)
- [Matrix class (gdiplusmatrix.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusmatrix/nl-gdiplusmatrix-matrix)
- [Graphics::Save - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-save)
- [Why Transformation Order Is Significant - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-why-transformation-order-is-significant-use)
- [Matrix Representation of Transformations - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-matrix-representation-of-transformations-about)
- [Graphics::SetTransform - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-settransform)
