# Win32 GDI 图形绘制：线条、矩形与多边形完全指南

> 在之前的文章里，我们聊过怎么创建一个基本的 Win32 窗口，也讲过图标、光标这些资源的使用。但如果你仔细观察那些示例程序，会发现它们有一个共同特点——界面都是"空"的。窗口里除了默认的白色背景，什么都没有。这就像你买了一张画布，但从来不动笔画画一样。今天我们要做的就是拿起画笔，在 Win32 的画布上真正画出点东西来。

---

## 从手动画图到程序绘图

说实话，我刚开始接触图形编程的时候，脑子里完全是乱的。为什么要在代码里画图？有 Photoshop 不香吗？后来才发现，程序绘图的本质不是"画一张好看的图"，而是"让程序能够动态地生成图形"。你想做一个数据可视化图表？需要程序绘图。你想做一个简单的画图工具？需要程序绘图。你想在窗口里显示一个进度条或者状态指示器？还是需要程序绘图。

Win32 的图形绘制API叫做 GDI（Graphics Device Interface），这是一个从 Windows 1.0 就存在的古老API。虽然现在有了 Direct2D、DirectX 这些更现代的图形库，但 GDI 依然有几个无可替代的优势：它简单、稳定、不需要额外的运行时，而且对于大多数2D绘图任务来说完全够用。

很多朋友可能会问："现在都2025年了，还学GDI会不会太老？"我的观点是：技术老不老不重要，重要的是它能不能解决问题。GDI 就像一把老式的瑞士军刀，虽然不像现代刀具那么炫酷，但当你需要快速切个苹果或者拧个螺丝的时候，它依然是最可靠的选择。而且理解了 GDI 的工作原理，再去学 Direct2D 或者其他图形库会容易得多——它们的很多概念都是相通的。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 2000+ 都支持）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的窗口、什么是 WM_PAINT 消息、怎么获取和使用 HDC。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——理解绘图前的准备工作

就像你画画前要准备好画笔、颜料、画布一样，Win32 绘图也需要做一些准备工作。这里有三个核心概念你需要理解：设备上下文（HDC）、画笔（HPEN）、画刷（HBRUSH）。

### 设备上下文 HDC 是什么

HDC（Handle to Device Context）是 Windows 绘图的核心概念。你可以把它理解为一个"绘图环境"——它知道你画在什么地方（屏幕、打印机、内存位图），用什么样的颜色和线宽，当前坐标在哪里等等。所有的 GDI 绘图函数都需要一个 HDC 参数，就是这个原因。

在 WM_PAINT 消息处理中，我们通过 BeginPaint 获取 HDC：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 所有绘制操作都在这里进行

    EndPaint(hwnd, &ps);
    return 0;
}
```

⚠️ 注意

千万别忘了 EndPaint！如果你只调用 BeginPaint 而不调用 EndPaint，窗口的更新区域不会被清除，会导致 WM_PAINT 消息不断发送，程序会陷入一个无限的重绘循环。这就像你告诉操作系统"我准备好画了"，然后永远不告诉它"我画完了"，操作系统会一直傻傻地等你。

### 创建和使用画笔

画笔（Pen）用来画线条和轮廓。创建画笔的函数是 CreatePen 或 ExtCreatePen：

```cpp
HPEN CreatePen(
    int      iStyle,     // 画笔样式
    int      cWidth,     // 画笔宽度
    COLORREF color       // 画笔颜色
);
```

画笔样式有几种常用选项：

```cpp
PS_SOLID      // 实线（最常用）
PS_DASH       // 虚线
PS_DOT        // 点线
PS_DASHDOT    // 点划线
PS_NULL       // 空笔（不画线）
```

使用画笔的标准流程是这样的：

```cpp
// 创建一支红色实线画笔，宽度为2
HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));

// 把画笔选入 DC，同时保存旧画笔
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

// 使用新画笔绘制...
LineTo(hdc, 100, 100);

// 恢复旧画笔
SelectObject(hdc, hOldPen);

// 删除我们创建的画笔
DeleteObject(hPen);
```

这里有个很重要的细节：SelectObject 返回的是之前选入 DC 的对象。你必须保存这个返回值，并在绘图完成后恢复它。如果你不恢复，DC 会一直持有你的画笔，当你再次尝试 DeleteObject 时会出问题。更糟糕的是，如果这个 DC 被其他代码使用，它会错误地使用你的画笔而不是系统默认的。

### 创建和使用画刷

画刷（Brush）用来填充封闭图形的内部。创建画刷最常用的函数是 CreateSolidBrush：

```cpp
HBRUSH CreateSolidBrush(COLORREF crColor);
```

使用画刷的流程和画笔类似：

```cpp
// 创建一个蓝色画刷
HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255));

// 选入 DC 并保存旧画刷
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

// 绘制填充图形...
Rectangle(hdc, 10, 10, 100, 100);

// 恢复和清理
SelectObject(hdc, hOldBrush);
DeleteObject(hBrush);
```

还有一个常用的画刷是库存对象（Stock Object），它们不需要你创建也不需要删除：

```cpp
// 使用库存的空心画刷（不填充）
SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

// 或者使用库存的白色画刷
SelectObject(hdc, GetStockObject(WHITE_BRUSH));
```

⚠️ 注意

千万别 DeleteObject 库存对象！这些是系统预定义的全局对象，删除它们会导致整个系统的绘图行为异常。如果你不确定一个对象是不是库存对象，那就不要删除它，或者只删除你自己创建的对象。

### 背景模式：OPAQUE vs TRANSPARENT

当你在窗口上画文字或者虚线的时候，GDI 会在字符或线条间隙处绘制背景色。这个行为由背景模式控制：

```cpp
int SetBkMode(HDC hdc, int iBkMode);
```

两种模式的区别：

```cpp
// OPAQUE 模式（默认）：字符间隙用背景色填充
SetBkMode(hdc, OPAQUE);
SetBkColor(hdc, RGB(255, 255, 0));  // 背景色设为黄色
TextOut(hdc, 10, 10, L"Hello", 5);  // 文字后面的空隙会被填成黄色

// TRANSPARENT 模式：字符间隙透明，显示原有内容
SetBkMode(hdc, TRANSPARENT);
TextOut(hdc, 10, 10, L"Hello", 5);  // 文字后面的空隙保持原样
```

这在绘制重叠图形时特别有用。如果你想让文字"浮"在已有的图形上而不遮挡它，就用 TRANSPARENT 模式。

---

## 第二步——线条绘制详解

线条是所有图形的基础。Win32 提供了几种画线的方式，从最简单的 LineTo 到复杂的 PolyBezier，各有各的用途。

### MoveToEx + LineTo 的基础用法

MoveToEx 设置"当前位置"（Current Position），LineTo 从当前位置画一条线到指定位置：

```cpp
BOOL MoveToEx(
    HDC     hdc,     // 设备上下文
    int     x,       // 新位置的 X 坐标
    int     y,       // 新位置的 Y 坐标
    LPPOINT lppt     // 可选：接收之前的位置
);

BOOL LineTo(
    HDC hdc,
    int  x,          // 终点 X 坐标
    int  y           // 终点 Y 坐标
);
```

一个简单的例子：

```cpp
// 移动到起点
MoveToEx(hdc, 10, 10, NULL);

// 画线到 (100, 100)
LineTo(hdc, 100, 100);

// 当前的位置现在是 (100, 100)，继续画线
LineTo(hdc, 200, 50);

// 再画线回到起点附近
LineTo(hdc, 10, 10);
```

这里有个容易踩的坑：LineTo 会更新当前位置。所以连续调用 LineTo 会画出一条折线，而不是多条独立的线。如果你想要从不同的位置画线，每次都要先调用 MoveToEx。

MoveToEx 的第四个参数（LPPOINT）可以用来获取之前的位置：

```cpp
POINT ptPrev;
MoveToEx(hdc, 50, 50, &ptPrev);

// ptPrev 现在包含了移动之前的位置
// 后续可以恢复：
MoveToEx(hdc, ptPrev.x, ptPrev.y, NULL);
```

这在需要临时改变绘图位置然后恢复的场景下很有用。

### Polyline 绘制折线

如果你有一系列点需要连续连接成折线，Polyline 比 MoveToEx+LineTo 更高效：

```cpp
BOOL Polyline(
    HDC         hdc,      // 设备上下文
    const POINT *apt,     // 点数组
    int         cpt       // 点的数量
);
```

```cpp
// 定义一组点
POINT points[] = {
    {10, 10},
    {50, 50},
    {100, 20},
    {150, 80},
    {200, 10}
};

// 一次性绘制连接所有点的折线
Polyline(hdc, points, sizeof(points) / sizeof(points[0]));
```

Polyline 和多次 LineTo 的主要区别是：Polyline 不会修改当前位置。这在某些需要保持当前绘图状态的场景下很重要。

另一个区别是性能。Polyline 只需要一次 API 调用，而多次 LineTo 需要多次调用。在绘制大量线段时，Polyline 会明显更快。

### PolyBezier 绘制贝塞尔曲线

贝塞尔曲线是计算机图形学中最重要的曲线类型之一。Win32 的 PolyBezier 函数可以绘制三次贝塞尔曲线：

```cpp
BOOL PolyBezier(
    HDC         hdc,      // 设备上下文
    const POINT *apt,     // 控制点和端点数组
    DWORD       cpt       // 点的数量
);
```

三次贝塞尔曲线需要4个点：起点、两个控制点、终点。如果要画多条连续的贝塞尔曲线，后面曲线的起点就是前一条曲线的终点，所以每增加一条曲线只需要3个新点。

```cpp
// 一条贝塞尔曲线：4个点
POINT bezier1[] = {
    {10, 100},    // 起点
    {50, 10},     // 控制点1
    {150, 10},    // 控制点2
    {200, 100}    // 终点
};
PolyBezier(hdc, bezier1, 4);

// 两条连续的贝塞尔曲线：7个点
POINT bezier2[] = {
    {10, 100},    // 第一条曲线的起点
    {50, 10},     // 第一条曲线的控制点1
    {100, 10},    // 第一条曲线的控制点2
    {150, 100},   // 第一条曲线的终点 = 第二条曲线的起点
    {200, 10},    // 第二条曲线的控制点1
    {250, 10},    // 第二条曲线的控制点2
    {300, 100}    // 第二条曲线的终点
};
PolyBezier(hdc, bezier2, 7);
```

贝塞尔曲线的控制点就像"磁铁"，曲线会被吸引向控制点的方向移动，但不一定经过控制点。这需要一点实践才能掌握。

如果你想让曲线从当前位置开始，而不是指定新的起点，可以使用 PolyBezierTo：

```cpp
// 设置起点
MoveToEx(hdc, 10, 100, NULL);

// 从当前起点开始，用后续点画贝塞尔曲线
POINT controls[] = {
    {50, 10},     // 控制点1
    {150, 10},    // 控制点2
    {200, 100}    // 终点
};
PolyBezierTo(hdc, controls, 3);
```

### 当前的位置的概念

"当前位置"（Current Position）是 GDI 维护的一个状态，被多个绘图函数使用。影响当前位置的函数包括：

| 函数 | 是否更新当前位置 | 备注 |
|------|------------------|------|
| MoveToEx | 是 | 设置新位置 |
| LineTo | 是 | 更新到线的终点 |
| Polyline | 否 | 不影响当前位置 |
| PolyBezier | 否 | 不影响当前位置 |
| LineTo | 是 | 更新到终点 |
| Rectangle | 否 | 不影响当前位置 |
| Ellipse | 否 | 不影响当前位置 |
| TextOut | 否 | 不影响当前位置 |

理解这些函数对当前位置的影响很重要，否则你可能会遇到"为什么我的线没有从我想的地方开始"这种困惑。

---

## 第三步——封闭图形绘制

封闭图形是指有明确的边界的图形，它们可以被填充。Win32 提供了多种封闭图形的绘制函数。

### Rectangle 绘制矩形

Rectangle 是最简单的封闭图形：

```cpp
BOOL Rectangle(
    HDC hdc,
    int left,     // 左边界 X 坐标
    int top,      // 上边界 Y 坐标
    int right,    // 右边界 X 坐标
    int bottom    // 下边界 Y 坐标
);
```

```cpp
// 画一个矩形
Rectangle(hdc, 10, 10, 200, 100);
```

这个矩形会用当前画笔画边框，用当前画刷填充内部。如果你只想要边框不填充，可以选入空心画刷：

```cpp
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
Rectangle(hdc, 10, 10, 200, 100);
SelectObject(hdc, hOldBrush);
```

⚠️ 注意

千万别把 right 和 bottom 的值想成"宽度和高度"。它们是绝对坐标。Rectangle(hdc, 10, 10, 200, 100) 画出的矩形宽度是 190（200-10），高度是 90（100-10）。如果你传了 (10, 10, 100, 100)，会得到一个 90×90 的正方形，而不是你预期的 100×100。

### Ellipse 绘制椭圆

Ellipse 绘制一个椭圆，它被限定在一个矩形边界框内：

```cpp
BOOL Ellipse(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom
);
```

```cpp
// 画一个椭圆
Ellipse(hdc, 10, 10, 200, 100);
```

椭圆的中心点是边界框的中心。如果你传的边界框是一个正方形（比如 10, 10, 110, 110），画出来就是一个正圆。

```cpp
// 画一个正圆
Ellipse(hdc, 10, 10, 110, 110);
```

### RoundRect 绘制圆角矩形

RoundRect 是矩形的变体，它的角是圆的：

```cpp
BOOL RoundRect(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom,
    int width,    // 圆角椭圆的宽度
    int height    // 圆角椭圆的高度
);
```

```cpp
// 画一个圆角矩形，圆角大小为 20×10
RoundRect(hdc, 10, 10, 200, 100, 20, 10);
```

圆角的形状是通过在每个角画一个小椭圆实现的。width 和 height 指定了这个椭圆的尺寸。width 越大，圆角越宽；height 越大，圆角越高。

如果你想要完全圆角的矩形（比如胶囊形状），可以让圆角的尺寸等于矩形尺寸的一半：

```cpp
// 胶囊形状
RoundRect(hdc, 10, 10, 210, 60, 100, 25);
```

### Pie 绘制饼图扇形

Pie 画一个饼图的扇形，它是由椭圆和两条半径围成的区域：

```cpp
BOOL Pie(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom,
    int xr1,       // 半径1终点的 X 坐标
    int yr1,       // 半径1终点的 Y 坐标
    int xr2,       // 半径2终点的 X 坐标
    int yr2        // 半径2终点的 Y 坐标
);
```

```cpp
// 画一个从12点钟方向到3点钟方向的扇形
Pie(hdc, 10, 10, 210, 210,
    110, 10,   // 第一条半径终点（12点钟方向）
    210, 110); // 第二条半径终点（3点钟方向）
```

这个函数的参数设计有点反直觉。xr1, yr1 和 xr2, yr2 不是角度，而是从椭圆中心出发的两条射线的终点。你需要自己计算这些点的坐标。

计算这些坐标的一个小技巧：

```cpp
// 计算扇形半径终点的辅助函数
void GetPieRadiusPoint(int cx, int cy, int radius, double angleDegrees,
                       int* x, int* y)
{
    double angleRad = angleDegrees * 3.1415926535 / 180.0;
    *x = cx + (int)(radius * cos(angleRad));
    *y = cy - (int)(radius * sin(angleRad));  // Y轴向下为正，所以要减
}

// 画一个从 30° 到 120° 的扇形
int cx = 110, cy = 110, radius = 100;
int x1, y1, x2, y2;
GetPieRadiusPoint(cx, cy, radius, 30, &x1, &y1);
GetPieRadiusPoint(cx, cy, radius, 120, &x2, &y2);
Pie(hdc, cx - radius, cy - radius, cx + radius, cy + radius,
    x1, y1, x2, y2);
```

### Chord 绘制弓形

Chord 和 Pie 类似，但它画的是"弓形"——椭圆被一条弦截断后形成的区域：

```cpp
BOOL Chord(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom,
    int xr1,
    int yr1,
    int xr2,
    int yr2
);
```

```cpp
// 画一个弓形
Chord(hdc, 10, 10, 210, 210,
      10, 110,   // 第一个点
      210, 110); // 第二个点
```

Pie 和 Chord 的区别可以这样理解：Pie 是"披萨切片"，Chord 是"月牙形"。Pie 会连接两个点到圆心，Chord 直接连接两个点。

---

## 第四步——多边形绘制

多边形是由多条线段围成的封闭图形。Win32 提供了两个函数：Polygon 和 PolyPolygon。

### Polygon vs PolyPolygon

Polygon 画一个多边形：

```cpp
BOOL Polygon(
    HDC         hdc,
    const POINT *apt,
    int         cpt
);
```

```cpp
// 画一个五边形
POINT pentagon[] = {
    {100, 10},
    {190, 75},
    {155, 170},
    {45, 170},
    {10, 75}
};
Polygon(hdc, pentagon, 5);
```

Polygon 会自动把最后一个点连接回第一个点，形成封闭图形。

PolyPolygon 可以一次画多个多边形：

```cpp
BOOL PolyPolygon(
    HDC         hdc,
    const POINT *apt,
    const INT   *aiCounts,  // 每个多边形的点数数组
    int         csz         // 多边形的数量
);
```

```cpp
// 画两个多边形：一个三角形和一个矩形
POINT points[] = {
    // 三角形的三个点
    {10, 10},
    {50, 50},
    {10, 50},
    // 矩形的四个点
    {100, 10},
    {150, 10},
    {150, 60},
    {100, 60}
};

int counts[] = {3, 4};  // 第一个多边形3个点，第二个4个点
PolyPolygon(hdc, points, counts, 2);
```

PolyPolygon 的效率比多次调用 Polygon 高，尤其是在绘制大量多边形时。

### 填充模式：ALTERNATE vs WINDING

当多边形自相交（比如五角星）时，填充模式就很重要了。你可以用 SetPolyFillMode 设置填充模式：

```cpp
int SetPolyFillMode(HDC hdc, int mode);
```

两种模式的区别：

**ALTERNATE 模式（默认）**：从一点向任意方向画一条射线，数它和多边形边界的交点数。如果是奇数，该点在内部（填充）；如果是偶数，该点在外部（不填充）。

```cpp
// 使用 ALTERNATE 模式
SetPolyFillMode(hdc, ALTERNATE);
```

**WINDING 模式**：考虑边的方向。从一点向任意方向画射线，计算"绕数"（winding number）。如果绕数非零，该点在内部。

```cpp
// 使用 WINDING 模式
SetPolyFillMode(hdc, WINDING);
```

用一个五角星作为例子：

```cpp
// 五角星的点（注意这个点序列会产生自相交）
POINT star[] = {
    {100, 10},   // 顶点
    {120, 80},
    {190, 80},
    {130, 120},
    {150, 190},
    {100, 140},
    {50, 190},
    {70, 120},
    {10, 80},
    {80, 80}
};

// ALTERNATE 模式：中间的五边形是空的
SetPolyFillMode(hdc, ALTERNATE);
Polygon(hdc, star, 10);

// WINDING 模式：整个五角星都是填充的
SetPolyFillMode(hdc, WINDING);
Polygon(hdc, star, 10);
```

⚠️ 注意

千万别以为这两种模式只是"有点不一样"。对于自相交多边形，它们产生的结果可能完全相反。ALTERNATE 更符合直觉的"镂空"效果，而 WINDING 则会填充所有区域。根据你的需求选择合适的模式。

### 自相交多边形的行为差异

为了更清楚地展示两种模式的区别，我们来画一个"回字形"多边形：

```cpp
// 画一个回字形（内圈和外圈方向相反）
POINT donut[] = {
    // 外圈（逆时针）
    {10, 10},
    {190, 10},
    {190, 190},
    {10, 190},
    // 内圈（顺时针，与外圈相反）
    {50, 50},
    {50, 150},
    {150, 150},
    {150, 50}
};

// ALTERNATE 模式：中心是空的（环形）
SetPolyFillMode(hdc, ALTERNATE);
Polygon(hdc, donut, 8);

// WINDING 模式：整个都是填充的（外圈和内圈绕数相抵消）
SetPolyFillMode(hdc, WINDING);
Polygon(hdc, donut, 8);
```

在 WINDING 模式下，如果你想让中心是空的，需要让内外圈的方向一致：

```cpp
POINT donut2[] = {
    // 外圈（逆时针）
    {10, 10},
    {190, 10},
    {190, 190},
    {10, 190},
    // 内圈（也逆时针，与外圈一致）
    {50, 50},
    {150, 50},
    {150, 150},
    {50, 150}
};

// WINDING 模式下中心是空的
SetPolyFillMode(hdc, WINDING);
Polygon(hdc, donut2, 8);
```

---

## 第五步——绘图属性控制

除了基本的图形绘制，GDI 还提供了很多控制绘图行为的属性。

### SetROP2 的光栅操作模式

SetROP2（Set Raster Operation 2）设置"前景混合模式"，决定了新绘制的像素和屏幕上已有的像素如何混合：

```cpp
int SetROP2(HDC hdc, int rop2);
```

常用的模式：

| 模式 | 效果 |
|------|------|
| R2_COPYPEN（默认） | 直接用画笔颜色覆盖 |
| R2_NOT | 反转屏幕颜色 |
| R2_XORPEN | XOR 操作 |
| R2_BLACK | 总是画黑色 |
| R2_WHITE | 总是画白色 |

一个有趣的例子：用 XOR 模式实现"可擦除"的绘图：

```cpp
// 保存旧的绘图模式
int oldROP = SetROP2(hdc, R2_XORPEN);

// 画一条线
MoveToEx(hdc, 10, 10, NULL);
LineTo(hdc, 200, 200);

// 再画一次同一条线，就"擦除"了（因为 XOR 两次等于原值）
MoveToEx(hdc, 10, 10, NULL);
LineTo(hdc, 200, 200);

// 恢复旧模式
SetROP2(hdc, oldROP);
```

这在实现拖动预览时特别有用——你可以先画一个预览形状，移动时再画一次原来的位置擦除它，然后在新位置画新的预览。

### 线条连接样式

当你画折线或多边形时，线段连接处的形状可以控制。这需要使用 ExtCreatePen：

```cpp
HPEN ExtCreatePen(
    DWORD         dwPenStyle,
    DWORD         dwWidth,
    const LOGBRUSH *lplb,
    DWORD         dwStyleCount,
    const DWORD  *lpStyle
);
```

连接样式通过 PS_JOIN 标志指定：

```cpp
// 创建一支带特定连接样式的画笔
LOGBRUSH lb = { BS_SOLID, RGB(255, 0, 0) };
HPEN hPen = ExtCreatePen(
    PS_SOLID | PS_JOIN_MITER,  // 或 PS_JOIN_BEVEL, PS_JOIN_ROUND
    5,                          // 线宽
    &lb,
    0, NULL
);
```

三种连接样式：

* **PS_JOIN_MITER（默认）**：尖角，线条端点延长相交
* **PS_JOIN_BEVEL**：斜角，连接处是平的
* **PS_JOIN_ROUND**：圆角，连接处是圆弧

```cpp
// 尖角连接
HPEN hPenMiter = ExtCreatePen(PS_SOLID | PS_JOIN_MITER, 10, &lb, 0, NULL);

// 斜角连接
HPEN hPenBevel = ExtCreatePen(PS_SOLID | PS_JOIN_BEVEL, 10, &lb, 0, NULL);

// 圆角连接
HPEN hPenRound = ExtCreatePen(PS_SOLID | PS_JOIN_ROUND, 10, &lb, 0, NULL);
```

### 端点样式

线条的端点样式也可以控制，同样通过 ExtCreatePen：

```cpp
// 端点样式
PS_ENDCAP_ROUND    // 圆头（默认）
PS_ENDCAP_SQUARE   // 方头
PS_ENDCAP_FLAT     // 平头
```

```cpp
HPEN hPenRound = ExtCreatePen(PS_SOLID | PS_ENDCAP_ROUND, 10, &lb, 0, NULL);
HPEN hPenSquare = ExtCreatePen(PS_SOLID | PS_ENDCAP_SQUARE, 10, &lb, 0, NULL);
HPEN hPenFlat = ExtCreatePen(PS_SOLID | PS_ENDCAP_FLAT, 10, &lb, 0, NULL);
```

ROUND 和 SQUARE 的区别是：ROUND 端点是半圆形，SQUARE 端点是方形但会延伸出半个线宽的距离，FLAT 端点是平的且正好结束在指定位置。

---

## 第六步——实战示例：绘制一个简单的图表

让我们把以上知识整合成一个完整的示例。这个程序会绘制一个带有坐标轴、折线图和图例的简单图表。

```cpp
#include <windows.h>
#include <vector>

// 窗口类名
static const wchar_t g_szClassName[] = L"GDIChartDemo";

// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 绘制坐标轴
void DrawAxes(HDC hdc, const RECT& rcClient)
{
    // 创建黑色画笔
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // 创建灰色画刷
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    // 边距
    const int MARGIN = 50;
    int graphLeft = MARGIN;
    int graphTop = MARGIN;
    int graphRight = rcClient.right - MARGIN;
    int graphBottom = rcClient.bottom - MARGIN;

    // 绘制背景
    Rectangle(hdc, graphLeft, graphTop, graphRight, graphBottom);

    // 绘制网格线（虚线）
    HPEN hDashPen = CreatePen(PS_DASH, 1, RGB(200, 200, 200));
    SelectObject(hdc, hDashPen);

    // 水平网格线
    for (int y = graphTop; y <= graphBottom; y += 50) {
        MoveToEx(hdc, graphLeft, y, NULL);
        LineTo(hdc, graphRight, y);
    }

    // 垂直网格线
    for (int x = graphLeft; x <= graphRight; x += 50) {
        MoveToEx(hdc, x, graphTop, NULL);
        LineTo(hdc, x, graphBottom);
    }

    // 恢复黑色画笔，绘制坐标轴
    SelectObject(hdc, hPen);
    MoveToEx(hdc, graphLeft, graphTop, NULL);
    LineTo(hdc, graphLeft, graphBottom);  // Y轴
    LineTo(hdc, graphRight, graphBottom); // X轴

    // 清理
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hDashPen);
    DeleteObject(hBrush);
}

// 绘制折线图
void DrawLineChart(HDC hdc, const RECT& rcClient)
{
    // 数据点
    std::vector<POINT> dataPoints = {
        {50, 250}, {100, 200}, {150, 180}, {200, 220},
        {250, 150}, {300, 170}, {350, 100}, {400, 120},
        {450, 80}, {500, 100}, {550, 60}
    };

    // 绘制折线
    HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 100, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    Polyline(hdc, dataPoints.data(), dataPoints.size());

    // 绘制数据点（小圆圈）
    for (const auto& pt : dataPoints) {
        Ellipse(hdc, pt.x - 4, pt.y - 4, pt.x + 4, pt.y + 4);
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

// 绘制图例
void DrawLegend(HDC hdc, const RECT& rcClient)
{
    const int LEGEND_X = rcClient.right - 150;
    const int LEGEND_Y = 60;

    // 绘制图例背景
    RECT rcLegend = {LEGEND_X - 10, LEGEND_Y - 10,
                     LEGEND_X + 130, LEGEND_Y + 60};

    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    Rectangle(hdc, rcLegend.left, rcLegend.top,
              rcLegend.right, rcLegend.bottom);

    // 绘制折线图例
    SelectObject(hdc, CreatePen(PS_SOLID, 3, RGB(0, 100, 200)));
    MoveToEx(hdc, LEGEND_X, LEGEND_Y + 15, NULL);
    LineTo(hdc, LEGEND_X + 40, LEGEND_Y + 15);

    // 图例文字
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, LEGEND_X + 50, LEGEND_Y + 8, L"销售额", 6);

    // 恢复和清理
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// 主绘制函数
void DrawChart(HWND hwnd, HDC hdc)
{
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 清空背景
    FillRect(hdc, &rcClient, (HBRUSH)(COLOR_WINDOW + 1));

    // 绘制各部分
    DrawAxes(hdc, rcClient);
    DrawLineChart(hdc, rcClient);
    DrawLegend(hdc, rcClient);
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"窗口类注册失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0, g_szClassName, L"GDI 图表绘制示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"窗口创建失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        DrawChart(hwnd, hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
```

编译运行这个程序，你会看到一个带有坐标轴、网格线、折线图和图标的完整图表。

---

## 第七步——常见问题与调试

在实际开发中，你可能会遇到各种 GDI 绘图的问题。这里分享一些常见的坑和调试方法。

### GDI 对象泄漏怎么检测

GDI 对象泄漏不像内存泄漏那么明显，但累积多了会导致系统绘图性能下降甚至崩溃。Windows 任务管理器的"详细信息"选项卡可以显示"GDI 对象"数量。

正常情况下，一个窗口程序的 GDI 对象数量应该在几十到一两百之间。如果发现这个数字一直在增长，说明有对象泄漏。

泄漏的常见原因：

```cpp
// 错误1：忘记 DeleteObject
HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
SelectObject(hdc, hPen);
// ... 使用后忘记 DeleteObject(hPen);

// 错误2：选入对象后直接删除，没有恢复旧对象
HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
DeleteObject(hPen);  // 错误！hPen 还在被 DC 使用
SelectObject(hdc, hOldPen);  // 这时候 hOldPen 已经是无效的了

// 正确做法
HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
// ... 使用 ...
SelectObject(hdc, hOldPen);  // 先恢复旧对象
DeleteObject(hPen);  // 再删除新对象
```

### 绘图闪烁怎么消除

当你频繁重绘窗口时（比如拖动滑块、动画），可能会看到闪烁。这是因为 WM_PAINT 处理中先擦除背景再绘制新内容，两个操作之间有延迟。

解决方法是双缓冲：先在内存 DC 上画完，然后一次性复制到窗口：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 创建内存 DC
    HDC hdcMem = CreateCompatibleDC(hdc);

    // 创建内存位图
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // 在内存 DC 上绘制
    FillRect(hdcMem, &rcClient, (HBRUSH)(COLOR_WINDOW + 1));
    // ... 其他绘制操作 ...

    // 一次性复制到窗口
    BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom,
           hdcMem, 0, 0, SRCCOPY);

    // 清理
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(hwnd, &ps);
    return 0;
}
```

### 为什么我的图形没有显示

这是新手最常遇到的问题。可能的原因：

1. **忘记调用 InvalidateRect**：修改数据后需要触发重绘
```cpp
// 数据更新后
g_dataPoints.push_back(newPoint);
InvalidateRect(hwnd, NULL, TRUE);  // 触发重绘
```

2. **WM_PAINT 中没有真正绘制**：BeginPaint/EndPaint 之间没有调用任何绘制函数
```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    // 如果这里什么都不画，窗口会保持空白
    EndPaint(hwnd, &ps);
    return 0;
}
```

3. **绘制到更新区域之外**：只绘制 ps.rcPaint 指定的区域
```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 只绘制更新区域内的内容
    FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

    EndPaint(hwnd, &ps);
    return 0;
}
```

如果更新区域很小，而你在区域外绘制，那些内容不会显示出来。

### 颜色不对怎么办

GDI 使用 RGB 颜色，每个颜色分量 0-255。但要注意显示器的颜色深度设置。如果你的显示器是 16 位色（65536 色），一些颜色可能会显示不准确。

另一个常见问题是文本颜色和背景色：

```cpp
// 设置文本颜色
COLORREF oldTextColor = SetTextColor(hdc, RGB(255, 0, 0));

// 设置背景模式
int oldBkMode = SetBkMode(hdc, TRANSPARENT);

// 绘制文本
TextOut(hdc, 10, 10, L"红色文字", 8);

// 恢复
SetTextColor(hdc, oldTextColor);
SetBkMode(hdc, oldBkMode);
```

---

## 后续可以做什么

到这里，Win32 GDI 的基本图形绘制知识就讲完了。你现在应该能够：
- 创建和使用画笔、画刷
- 绘制线条、折线、贝塞尔曲线
- 绘制各种封闭图形
- 理解多边形填充模式的区别
- 控制绘图属性（光栅操作、连接样式、端点样式）

但这些只是基础，GDI 还有更多内容值得探索：

1. **路径（Path）**：使用 BeginPath/EndPath 创建复杂形状
2. **区域（Region）**：用于裁剪和点击检测
3. **坐标变换**：SetWorldTransform 实现旋转、缩放
4. **字体和文本**：更丰富的文本绘制功能
5. **位图操作**：BitBlt、StretchBlt 等

下一步，我们可以探讨 Win32 的字体与文本绘制、GDI 路径与区域、或者双缓冲动画技术。这些都是让你的程序更加专业和生动的关键技能。

---

## 相关资源

- [MoveToEx 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-movetoex)
- [LineTo 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-lineto)
- [Polyline 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-polyline)
- [PolyBezier 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-polybezier)
- [Rectangle 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-rectangle)
- [Ellipse 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-ellipse)
- [RoundRect 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-roundrect)
- [Pie 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-pie)
- [Chord 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-chord)
- [Polygon 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-polygon)
- [PolyPolygon 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-polypolygon)
- [SetPolyFillMode 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-setpolyfillmode)
- [SetROP2 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-setrop2)
- [Filled Shape Functions - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/gdi/filled-shape-functions)
- [Windows GDI API 参考主页 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/_gdi/)
