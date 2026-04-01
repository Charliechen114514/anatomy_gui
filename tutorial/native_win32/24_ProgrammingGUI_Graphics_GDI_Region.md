# 通用GUI编程技术——图形渲染实战（二十四）——GDI Region与裁切：不规则窗口与可视化控制

> 上一篇文章我们搞定了 GDI 双缓冲技术，用内存 DC 一次性拷贝解决了绘制闪烁的老大难问题。双缓冲让画面变得流畅了，但我们的窗口形状始终被限制在矩形的世界里——标准的 `WS_OVERLAPPEDWINDOW` 样式给出的永远是一个方方正正的盒子。如果我们想要一个圆角窗口呢？或者一个星形窗口？甚至是一个完全自定义形状的悬浮控件？这就涉及到了 Windows GDI 中一个极其重要但经常被忽视的概念：Region（区域）。

Region 是 GDI 中描述"一块形状"的基础数据结构。它可以描述矩形、椭圆、多边形，甚至通过布尔运算组合出任意复杂的形状。Windows 系统在两个层面使用 Region：一是作为绘图时的裁切区域（Clipping Region），控制哪些像素可以画上去；二是作为窗口的可视区域（Window Region），直接决定窗口在屏幕上呈现什么形状。今天我们就要把 Region 的这两种用法彻底搞明白，最后实现两个实战案例——一个圆角窗口和一个星形异形窗口。

## 环境说明

在开始之前，先确认一下我们这次的开发环境：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022)
- **目标平台**: Win32 API 原生开发
- **图形库**: GDI (Graphics Device Interface)
- **链接库**: 除了常规的 `gdi32.lib`、`user32.lib` 之外，本篇不需要额外的库

这次不需要像后续文章那样链接 `msimg32.lib`，因为 Region 相关的 API 全都在 `gdi32.lib` 和 `user32.lib` 里，属于最基础的 GDI 功能。

## Region 的本质：GDI 中的"形状描述器"

在深入代码之前，我们得先搞清楚 Region 到底是什么东西。从底层机制上说，Region 是一个 GDI 对象，句柄类型是 `HRGN`，它内部维护的是一系列扫描线（scanline）的集合。你可以把它理解为一张"蒙版"——在这张蒙版覆盖的范围内是"有东西"的，范围之外是"空白"的。

### Region 的三种基本形态

GDI 提供了多个创建 Region 的函数，但归根结底，Region 只有三种基本形态。

最简单的是矩形 Region，用 `CreateRectRgn` 创建。它只需要两个坐标点就能定义一个矩形的区域。如果你想用 RECT 结构体来创建，还有一个等价的 `CreateRectRgnIndirect` 函数。

然后是椭圆 Region，用 `CreateEllipticRgn` 创建。注意这里说的是"椭圆"，虽然你传进去的参数是一个矩形的左上角和右下角坐标，但 GDI 会在那个矩形内接一个椭圆来创建区域。

最后是圆角矩形 Region，用 `CreateRoundRectRgn` 创建。它的函数签名是这样的：

```cpp
HRGN CreateRoundRectRgn(
    int x1,  // 左上角 x
    int y1,  // 左上角 y
    int x2,  // 右下角 x
    int y2,  // 右下角 y
    int w,   // 圆角椭圆的宽度
    int h    // 圆角椭圆的高度
);
```

前四个参数定义了一个矩形边界，`w` 和 `h` 定义了圆角处那个椭圆的宽和高。圆角就是用椭圆的四分之一弧线来拟合的——`w` 越大圆角越"扁平"，`h` 越大圆角越"圆滑"。当 `w` 和 `h` 等于矩形宽高时，整个 Region 就退化成了一个椭圆。

还有一种更灵活的创建方式是多边形 Region。`CreatePolygonRgn` 可以用一组点来创建一个任意多边形的区域，而 `CreatePolyPolygonRgn` 甚至可以一次创建多个多边形组成的区域。这两个函数的最后一个参数 `iMode` 指定填充模式，可选 `ALTERNATE`（交替填充）或 `WINDING`（环绕填充），对于凸多边形两者效果一样，但对于有交叉的复杂多边形会有区别。

### Region 的布尔运算：CombineRgn

单个基本形态有时候不够用。比如我们想要一个"两个圆的并集"，或者一个"矩形减去一个圆"——这就需要布尔运算了。GDI 提供了 `CombineRgn` 函数来完成这件事：

```cpp
int CombineRgn(
    HRGN hrgnDest,      // 结果 Region
    HRGN hrgnSrc1,      // 源 Region 1
    HRGN hrgnSrc2,      // 源 Region 2
    int  fnCombineMode  // 组合模式
);
```

`fnCombineMode` 支持以下模式：

- `RGN_AND`：两个 Region 的交集——只保留两者重叠的部分
- `RGN_OR`：两个 Region 的并集——两个 Region 覆盖的所有区域
- `RGN_XOR`：两个 Region 的异或——并集减去交集
- `RGN_DIFF`：Region1 减去 Region2——只保留在 Region1 中但不在 Region2 中的部分
- `RGN_COPY`：只复制 Region1——Region2 被忽略

这个函数的返回值也有讲究：`SIMPLEREGION` 表示结果是一个简单区域（没有重叠边框），`COMPLEXREGION` 表示结果是一个复杂区域，`NULLREGION` 表示结果为空，`ERROR` 表示出错了。在调试 Region 相关代码时，检查这个返回值是个好习惯。

有了布尔运算，我们可以把简单的 Region 组合出极其复杂的形状。比如要做一个"甜甜圈"形状的窗口，可以用一个大圆 Region 和一个小圆 Region 做 `RGN_DIFF` 运算。

## 裁切区域（Clipping Region）：控制绘制范围

讲完了 Region 的创建和组合，我们先来看看 Region 在绘图层面的第一个用途——裁切区域。

### 什么是裁切区域

裁切区域是 DC（设备上下文）的一个属性，它决定了后续绘制操作"允许画到哪些地方"。当你在 DC 上设置了裁切区域后，所有落在裁切区域之外的绘制操作都会被系统自动丢弃——不是画成透明，而是完全不画。这比你自己去判断像素是否在范围内要高效得多，因为 GDI 在底层做了硬件级别的优化。

你可以把裁切区域理解为一张"镂空模板"——你往上面涂颜料，只有模板镂空的地方才能涂上去，其他地方全被挡住了。

### SelectClipRgn：设置裁切区域

设置裁切区域最直接的方法是调用 `SelectClipRgn`：

```cpp
int SelectClipRgn(
    HDC  hdc,   // 目标 DC
    HRGN hrgn   // 要设置的 Region
);
```

这个函数的行为有一个很重要的细节：它使用的是 Region 的**副本**。也就是说，`SelectClipRgn` 内部会复制一份你的 Region 然后设置为裁切区域，所以调用之后你仍然持有原始 Region 的所有权，仍然需要自己 `DeleteObject` 释放它。这一点和 `SetWindowRgn`（后面会讲）的所有权转移行为完全不同，千万别搞混了。

`SelectClipRgn` 的返回值和 `CombineRgn` 类似，也是 `SIMPLEREGION`、`COMPLEXREGION`、`NULLREGION` 或 `ERROR`。

### SelectClipPath：用路径作为裁切区域

除了用现成的 Region，你还可以用 GDI 的路径（Path）来创建裁切区域。路径的创建方式是先用 `BeginPath(hdc)` 开启路径模式，然后在 DC 上执行一系列绘制操作（这些操作不会真正画到屏幕上，而是被记录为路径），最后用 `EndPath(hdc)` 结束路径。之后调用 `SelectClipPath` 就可以把这条路径转化为裁切区域：

```cpp
// 用路径创建一个椭圆形的裁切区域
BeginPath(hdc);
Ellipse(hdc, 50, 50, 300, 200);
EndPath(hdc);
SelectClipPath(hdc, RGN_AND);  // 与现有裁切区域求交集
```

`SelectClipPath` 的第二个参数是组合模式，和 `CombineRgn` 的模式参数含义相同。这种方式的优点是你可以用 GDI 的所有绘图函数来"画"出裁切区域的形状，比手动计算多边形顶点要方便得多。

### 实际应用场景

裁切区域在实际开发中有很多用途。比如我们要实现一个进度条控件，进度条外面的部分不应该画出来——这时候就可以用一个矩形 Region 作为裁切区域，宽度设为"当前进度百分比 × 控件宽度"。再比如实现一个圆形头像显示，可以用一个椭圆 Region 来裁切方形的位图。

下面我们写一个简单的演示，看看裁切区域的实际效果：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int cx = rcClient.right;
    int cy = rcClient.bottom;

    // 创建一个星形裁切区域
    POINT star[10];
    int cxCenter = cx / 2;
    int cyCenter = cy / 2;
    int outerR = min(cx, cy) / 2 - 20;
    int innerR = outerR * 38 / 100;  // 内半径约为外半径的 38%

    for (int i = 0; i < 10; i++) {
        double angle = (i * 36.0 - 90.0) * 3.14159265 / 180.0;
        int r = (i % 2 == 0) ? outerR : innerR;
        star[i].x = cxCenter + (int)(r * cos(angle));
        star[i].y = cyCenter + (int)(r * sin(angle));
    }

    HRGN hrgnClip = CreatePolygonRgn(star, 10, WINDING);
    SelectClipRgn(hdc, hrgnClip);

    // 在整个客户区绘制彩色渐变条纹
    for (int x = 0; x < cx; x += 10) {
        HBRUSH hbr = CreateSolidBrush(RGB(x * 255 / cx, 128, 255 - x * 255 / cx));
        RECT rcFill = { x, 0, x + 10, cy };
        FillRect(hdc, &rcFill, hbr);
        DeleteObject(hbr);
    }

    // 清理：取消裁切区域
    SelectClipRgn(hdc, NULL);
    DeleteObject(hrgnClip);

    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码创建了一个五角星形状的裁切区域，然后在整个客户区绘制彩色渐变条纹。由于裁切区域的存在，只有落在五角星范围内的条纹才会显示出来，最终你看到的就是一个彩虹色的五角星。

注意这里清理的时候，我们先调用 `SelectClipRgn(hdc, NULL)` 把裁切区域取消掉，然后才 `DeleteObject` 释放 Region。虽然 `SelectClipRgn` 用的是副本，释放时机不太敏感，但保持良好的资源管理习惯总是好的。

## SetWindowRgn：异形窗口的核心

裁切区域只是控制绘制范围，窗口本身的形状并没有改变。如果我们想让窗口在屏幕上呈现出不规则的形状——比如圆角、圆形、星形——就需要用到 `SetWindowRgn` 了。

### 窗口 Region vs 裁切 Region

这里有一个很重要的概念区分。裁切区域（Clipping Region）是 DC 的属性，它只影响绘制操作——你设置了裁切区域后，画不出去的部分不显示，但窗口本身还是矩形。窗口 Region（Window Region）则是窗口的属性，它直接决定了窗口在屏幕上的可见形状。设置窗口 Region 后，系统不会显示窗口 Region 之外的任何部分，包括边框、标题栏、客户区——全部被裁掉。

你可以把窗口 Region 理解为"物理层面"的裁切，而裁切区域是"绘制层面"的裁切。窗口 Region 的优先级更高——如果窗口 Region 把某个角落裁掉了，不管你的绘制代码怎么画，那个角落都不会出现在屏幕上。

### SetWindowRgn 函数详解

`SetWindowRgn` 的函数签名如下：

```cpp
int SetWindowRgn(
    HWND hWnd,     // 窗口句柄
    HRGN hRgn,     // Region 句柄
    BOOL bRedraw   // 是否重绘窗口
);
```

三个参数都很直观。`bRedraw` 通常设为 `TRUE`，让系统立即重新绘制窗口以反映新的形状。

但是这里有一个极其重要的细节，也是很多人踩的坑：**调用 `SetWindowRgn` 成功后，系统接管了 Region 的所有权，你不能再对它调用 `DeleteObject`**。根据 [Microsoft Learn 的官方文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn)，系统不会复制 Region，而是直接使用你传入的句柄。当窗口销毁或者下次 `SetWindowRgn` 时，系统会自动释放旧的 Region。

如果你手滑在 `SetWindowRgn` 之后又 `DeleteObject` 了同一个 Region，后果是不确定的——可能马上崩溃，可能过一会儿才崩溃，也可能在某些 Windows 版本上看起来"没事"。这是一个典型的"未定义行为"陷阱，调试起来非常痛苦。

### 坐标系的坑

`SetWindowRgn` 的 Region 坐标是相对于窗口的左上角的，但不是客户区的左上角，而是整个窗口（包括标题栏和边框）的左上角。也就是说，坐标 `(0, 0)` 对应的是窗口边框的最左上角。如果你的窗口有标题栏（比如 `WS_OVERLAPPEDWINDOW` 样式），那客户区的起始 Y 坐标大约是 30 多像素往下。这意味着你在计算 Region 的坐标时需要考虑非客户区的高度和宽度。

不过在实际开发中，异形窗口通常会去掉标题栏和边框（使用 `WS_POPUP` 样式），这样窗口坐标和客户区坐标就一致了，省去了不少麻烦。

## 实战一：圆角窗口完整实现

理论讲了不少，现在我们开始动手。第一个实战案例是实现一个圆角窗口，这是一个非常常见的需求——很多现代应用的窗口都有圆角效果，即使 Windows 11 本身也给窗口加了圆角。

我们的计划是：创建一个无边框的窗口，用 `CreateRoundRectRgn` 创建一个圆角矩形 Region，然后通过 `SetWindowRgn` 应用到窗口上。由于去掉了标题栏，我们需要自己实现窗口拖动和关闭功能。

### 窗口创建部分

首先是窗口类注册和窗口创建：

```cpp
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"RoundedWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = CLASS_NAME;
    wc.hbrBackground  = CreateSolidBrush(RGB(45, 45, 45));  // 深色背景
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // 创建无边框窗口
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"圆角窗口",
        WS_POPUP,            // 无边框、无标题栏
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
```

这里我们用 `WS_POPUP` 样式创建了一个完全无边框的窗口。`hbrBackground` 设为深灰色，这样窗口会有一个深色调的外观。

### WM_CREATE：设置圆角 Region

窗口创建后，我们需要立即设置圆角 Region：

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 获取窗口尺寸（注意：这里用的是窗口尺寸，不是客户区尺寸）
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // 创建圆角矩形 Region
        // 圆角半径设为 20 像素
        HRGN hrgn = CreateRoundRectRgn(0, 0, w, h, 20, 20);

        if (hrgn != NULL) {
            // SetWindowRgn 成功后，系统接管 hrgn 的所有权
            // 不能再 DeleteObject(hrgn)！
            SetWindowRgn(hwnd, hrgn, TRUE);
        }
        return 0;
    }
```

⚠️ 这段代码里有一个非常重要的细节。`CreateRoundRectRgn` 的坐标是 `(0, 0, w, h)` 而不是 `(0, 0, w-1, h-1)`。虽然很多 GDI 函数（比如 `Rectangle`）画的矩形右下边界是"排除"的（不画最后一个像素），但 `CreateRoundRectRgn` 的坐标定义的是一个完整的区域边界。如果你用 `(0, 0, w-1, h-1)`，Region 会比窗口实际尺寸小一像素，右边和下边会露出一条线。

另外一个细节：这里用的是 `GetWindowRect` 而不是 `GetClientRect`。因为我们用的是 `WS_POPUP` 样式（没有边框和标题栏），两者返回的尺寸实际上是一样的。但如果你的窗口有边框，`GetWindowRect` 返回的是包含边框的整个窗口尺寸，而 `GetClientRect` 只返回客户区尺寸。对于 `SetWindowRgn` 来说，Region 坐标是相对于整个窗口左上角的，所以应该使用窗口尺寸。

### WM_NCHITTEST：实现窗口拖动

去掉了标题栏之后，窗口默认就没法拖动了。我们需要自己处理 `WM_NCHITTEST` 消息来实现拖动功能：

```cpp
    case WM_NCHITTEST:
    {
        // 获取鼠标位置（相对于窗口）
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 顶部条形区域作为"标题栏"——允许拖动
        if (pt.y < 40) {
            return HTCAPTION;  // 系统会把这个区域当作标题栏处理
        }

        // 其余区域正常处理
        return HTCLIENT;
    }
```

`WM_NCHITTEST` 是 Windows 非客户区命中测试消息。当鼠标在窗口上移动或点击时，系统会发送这个消息来询问"鼠标现在在窗口的哪个部位"。返回 `HTCAPTION` 告诉系统"鼠标在标题栏上"，系统就会允许拖动操作。返回 `HTCLIENT` 告诉系统"鼠标在客户区"，系统就按正常方式处理。

这样处理之后，窗口顶部 40 像素的区域充当了"虚拟标题栏"，用户可以拖动它来移动窗口。

### 绘制窗口内容

现在我们来给圆角窗口画点内容，让它看起来不那么单调：

```cpp
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cx = rc.right;
        int cy = rc.bottom;

        // 使用双缓冲绘制
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, cx, cy);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

        // 填充深色背景
        HBRUSH hbrBg = CreateSolidBrush(RGB(45, 45, 45));
        FillRect(hdcMem, &rc, hbrBg);
        DeleteObject(hbrBg);

        // 绘制顶部"标题栏"区域
        HBRUSH hbrTitle = CreateSolidBrush(RGB(55, 55, 55));
        RECT rcTitle = { 0, 0, cx, 40 };
        FillRect(hdcMem, &rcTitle, hbrTitle);
        DeleteObject(hbrTitle);

        // 绘制标题文字
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(220, 220, 220));
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontOld = (HFONT)SelectObject(hdcMem, hFont);
        TextOut(hdcMem, 15, 10, L"圆角窗口示例", 7);
        SelectObject(hdcMem, hFontOld);
        DeleteObject(hFont);

        // 绘制关闭按钮（右上角的小叉）
        HPEN hPenClose = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
        HPEN hPenOld = (HPEN)SelectObject(hdcMem, hPenClose);
        int closeX = cx - 30;
        int closeY = 13;
        MoveToEx(hdcMem, closeX, closeY, NULL);
        LineTo(hdcMem, closeX + 14, closeY + 14);
        MoveToEx(hdcMem, closeX + 14, closeY, NULL);
        LineTo(hdcMem, closeX, closeY + 14);
        SelectObject(hdcMem, hPenOld);
        DeleteObject(hPenClose);

        // 在中央绘制一个圆形装饰
        HPEN hPenCircle = CreatePen(PS_SOLID, 3, RGB(100, 180, 255));
        HBRUSH hbrCircle = CreateSolidBrush(RGB(60, 80, 120));
        SelectObject(hdcMem, hPenCircle);
        SelectObject(hdcMem, hbrCircle);
        Ellipse(hdcMem, cx/2 - 60, cy/2 - 60, cx/2 + 60, cy/2 + 60);

        // 清理 GDI 对象
        SelectObject(hdcMem, (HBRUSH)GetStockObject(NULL_BRUSH));
        DeleteObject(hbrCircle);
        DeleteObject(hPenCircle);

        // 一次性拷贝到屏幕
        BitBlt(hdc, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbm);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        return 0;
    }
```

这里我们结合了上一篇文章学到的双缓冲技术。可以看到，在圆角窗口中绘制内容的方式和普通窗口完全一样——Region 只影响窗口的形状，不影响绘制的代码逻辑。

### 处理窗口大小变化

如果我们的窗口需要支持大小调整（比如通过代码调用 `SetWindowPos` 改变大小），那每次大小变化后都需要重新创建 Region 并重新设置：

```cpp
    case WM_SIZE:
    {
        // 窗口大小变了，重新设置圆角 Region
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HRGN hrgn = CreateRoundRectRgn(0, 0, w, h, 20, 20);
        if (hrgn != NULL) {
            SetWindowRgn(hwnd, hrgn, TRUE);
            // 同样不需要 DeleteObject(hrgn)
        }
        return 0;
    }
```

每次调用 `SetWindowRgn` 时，系统会自动释放旧的 Region，然后接管新的。所以你不用担心旧的 Region 泄漏。

### 关闭按钮和清理

最后，我们需要处理关闭按钮的点击和窗口的清理：

```cpp
    case WM_LBUTTONUP:
    {
        // 检查是否点击了关闭按钮区域
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cx = rc.right;

        if (xPos >= cx - 35 && xPos <= cx - 10 && yPos >= 5 && yPos <= 35) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

到这里，一个完整的圆角窗口就实现了。编译运行后你会看到一个圆角的深色窗口，可以通过顶部区域拖动，右上角有关闭按钮。这就是 `SetWindowRgn` 配合 `CreateRoundRectRgn` 的基本用法。

## 实战二：星形异形窗口

圆角窗口只是 Region 最简单的应用。接下来我们来做一个更有挑战性的——用多边形 Region 创建一个五角星形状的窗口。这个案例会用到 `CreatePolygonRgn`，同时我们还要处理鼠标拖动和自定义绘制。

### 计算五角星顶点

五角星的顶点计算有一套固定的数学公式。一个五角星有 10 个顶点——5 个外顶点和 5 个内顶点交替排列。每个外顶点之间的角度是 72 度（360 / 5），内外顶点之间的角度是 36 度。

```cpp
// 根据中心和半径计算五角星的顶点
void CalcStarPoints(POINT* pts, int cx, int cy, int outerR, int innerR)
{
    for (int i = 0; i < 10; i++) {
        // 从正上方（-90度）开始，每个顶点间隔 36 度
        double angle = (i * 36.0 - 90.0) * 3.14159265 / 180.0;
        int r = (i % 2 == 0) ? outerR : innerR;
        pts[i].x = cx + (int)(r * cos(angle));
        pts[i].y = cy + (int)(r * sin(angle));
    }
}
```

内半径和外半径的比例决定了五角星的"尖锐程度"。经典的五角星内半径大约是外半径的 38%（更精确地说是 `1 / (2 * sin(72°) * cos(36°))`，约 0.382）。这个比例来自黄金分割——五角星是一个和黄金分割有深刻联系的几何图形。

### 完整的星形窗口代码

下面是完整的星形窗口实现。我们会创建一个五角星形状的窗口，用渐变色填充，支持鼠标拖动：

```cpp
#include <windows.h>
#include <cmath>

// 全局变量：用于窗口拖动
POINT g_ptDragStart = {0};
bool g_bDragging = false;

void CalcStarPoints(POINT* pts, int cx, int cy, int outerR, int innerR)
{
    for (int i = 0; i < 10; i++) {
        double angle = (i * 36.0 - 90.0) * 3.14159265 / 180.0;
        int r = (i % 2 == 0) ? outerR : innerR;
        pts[i].x = cx + (int)(r * cos(angle));
        pts[i].y = cy + (int)(r * sin(angle));
    }
}

LRESULT CALLBACK StarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 窗口尺寸
        int winW = 500;
        int winH = 500;
        int cxCenter = winW / 2;
        int cyCenter = winH / 2;
        int outerR = 220;
        int innerR = (int)(outerR * 0.382);

        // 计算五角星顶点
        POINT starPts[10];
        CalcStarPoints(starPts, cxCenter, cyCenter, outerR, innerR);

        // 创建多边形 Region
        HRGN hrgnStar = CreatePolygonRgn(starPts, 10, WINDING);
        if (hrgnStar != NULL) {
            SetWindowRgn(hwnd, hrgnStar, TRUE);
        }
        return 0;
    }
```

### 鼠标拖动实现

由于星形窗口没有标题栏，我们需要自己处理鼠标事件来实现拖动：

```cpp
    case WM_LBUTTONDOWN:
    {
        // 开始拖动
        g_ptDragStart.x = LOWORD(lParam);
        g_ptDragStart.y = HIWORD(lParam);
        g_bDragging = true;
        SetCapture(hwnd);  // 捕获鼠标，确保拖动期间不会丢失消息
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (g_bDragging) {
            // 计算偏移量并移动窗口
            int dx = LOWORD(lParam) - g_ptDragStart.x;
            int dy = HIWORD(lParam) - g_ptDragStart.y;

            RECT rc;
            GetWindowRect(hwnd, &rc);
            MoveWindow(hwnd, rc.left + dx, rc.top + dy,
                       rc.right - rc.left, rc.bottom - rc.top, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_bDragging) {
            g_bDragging = false;
            ReleaseCapture();  // 释放鼠标捕获
        }
        return 0;
    }
```

这里我们用了 `SetCapture` / `ReleaseCapture` 来确保在拖动过程中，即使鼠标移出了窗口范围，窗口仍然能收到 `WM_MOUSEMOVE` 消息。如果不这样做，当鼠标移动过快、超出窗口边界时，拖动就会中断。`SetCapture` 是实现拖动功能的关键——它告诉系统"把所有鼠标消息都发给我"，直到 `ReleaseCapture` 为止。

### 绘制五角星内容

现在我们来绘制五角星的填充内容，让它看起来漂亮一些：

```cpp
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cx = rc.right;
        int cy = rc.bottom;

        // 双缓冲
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, cx, cy);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

        // 填充整个缓冲区为透明色（因为 Region 会裁掉窗口外的部分）
        HBRUSH hbrBg = CreateSolidBrush(RGB(30, 30, 80));
        FillRect(hdcMem, &rc, hbrBg);
        DeleteObject(hbrBg);

        // 绘制星形渐变效果——用同心五角星模拟
        int cxCenter = cx / 2;
        int cyCenter = cy / 2;

        for (int i = 10; i >= 0; i--) {
            double scale = i / 10.0;
            int outerR = (int)(220 * scale);
            int innerR = (int)(outerR * 0.382);

            POINT pts[10];
            CalcStarPoints(pts, cxCenter, cyCenter, outerR, innerR);

            // 颜色从深蓝渐变到亮蓝
            BYTE r = (BYTE)(30 + 170 * (1.0 - scale));
            BYTE g = (BYTE)(60 + 140 * (1.0 - scale));
            BYTE b = (BYTE)(180 + 75 * (1.0 - scale));

            HBRUSH hbr = CreateSolidBrush(RGB(r, g, b));
            HRGN hrgn = CreatePolygonRgn(pts, 10, WINDING);
            FillRgn(hdcMem, hrgn, hbr);
            DeleteObject(hrgn);
            DeleteObject(hbr);
        }

        // 中央绘制文字
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(255, 255, 255));
        HFONT hFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontOld = (HFONT)SelectObject(hdcMem, hFont);

        const wchar_t* text = L"Star Window";
        SIZE sz;
        GetTextExtentPoint32(hdcMem, text, lstrlenW(text), &sz);
        TextOut(hdcMem, cxCenter - sz.cx / 2, cyCenter - sz.cy / 2, text, lstrlenW(text));

        SelectObject(hdcMem, hFontOld);
        DeleteObject(hFont);

        // 拷贝到屏幕
        BitBlt(hdc, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbm);
        DeleteDC(hdcMem);

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

这段绘制代码用了 11 层同心五角星来模拟渐变效果。`FillRgn` 是一个很有用的函数——它直接用指定的画刷填充一个 Region，不需要先选入 DC。每一层的颜色从深蓝过渡到亮蓝，最终呈现出一个有立体感的五角星。

### 入口函数

最后是 `wWinMain`：

```cpp
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"StarWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = StarWndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = CLASS_NAME;
    wc.hbrBackground  = NULL;  // 不需要默认背景
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Star Window",
        WS_POPUP,  // 无边框
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
```

编译运行后你会看到一个五角星形状的窗口浮在桌面上，内部有蓝白渐变效果，可以任意拖动。这就是 `CreatePolygonRgn` + `SetWindowRgn` 的威力——任意多边形都可以变成窗口的形状。

## 常见问题与调试

在实现 Region 相关功能时，有几个常见问题值得单独拿出来说。

### SetWindowRgn 后窗口边框残留

这个问题通常出现在你给一个有 `WS_OVERLAPPEDWINDOW` 样式的窗口设置 Region 时。Region 裁掉了一部分窗口，但标题栏和边框的渲染逻辑还在运行，结果就会出现一些奇怪的视觉残留。解决办法很简单——异形窗口统一用 `WS_POPUP` 样式，去掉所有非客户区元素。

### Region 边缘锯齿

GDI Region 的坐标精度是像素级别的整数，所以圆形和圆角的边缘不可避免地会出现锯齿。这是因为 Region 内部用的是扫描线表示法，每条扫描线的起点和终点都是整数坐标，无法表达亚像素精度。在 DWM（桌面窗口管理器）时代，Windows 会对窗口边缘做一些抗锯齿处理来减轻这个问题，但 Region 本身是无法做到亚像素精度的。如果你需要平滑的异形窗口，后续文章会讲到用 Alpha 混合和分层窗口来实现。

### CombineRgn 的结果为空

当你用 `CombineRgn` 做交集运算时，如果两个 Region 没有重叠区域，结果会是 `NULLREGION`。这在调试复杂的组合运算时特别容易踩坑——建议每次 `CombineRgn` 后都检查返回值，如果返回 `NULLREGION` 就说明你的 Region 位置或大小计算有问题。

### GDI 对象泄漏

Region 也是 GDI 对象，每个 `Create*Rgn` 都要对应一个 `DeleteObject`——唯一的例外是已经传给 `SetWindowRgn` 的 Region。如果你在任务管理器中看到 GDI 对象数量持续增长，大概率就是 Region 泄漏了。一个好的编码习惯是：创建 Region 后立即检查是否为 NULL，使用完毕后确保释放，传给 `SetWindowRgn` 后就不要再碰了。

## 总结

到这里我们已经把 GDI Region 的核心用法全部走了一遍。从 Region 的三种基本形态（矩形、椭圆、多边形），到布尔运算（`CombineRgn`）组合复杂形状，再到裁切区域控制绘制范围，最后用 `SetWindowRgn` 实现了圆角窗口和星形异形窗口两个实战案例。

Region 是 Win32 GUI 编程中实现不规则形状的基础工具，但它有一个根本性的局限——只支持"硬边界"裁切，不支持半透明。你的窗口要么"有"要么"没有"某个像素，无法做到"这个像素 50% 透明"。这在很多现代 UI 场景中是不够的。下一篇文章我们就要引入 Alpha 混合技术，用 `WS_EX_LAYERED` 分层窗口和 `UpdateLayeredWindow` 来实现真正的逐像素透明效果——做出那种带有柔和阴影和半透明渐变的漂亮界面。

---

## 练习

1. **实现一个圆形窗口**：用 `CreateEllipticRgn` 创建一个完美的圆形窗口，在窗口中显示一张图片（可以用 `LoadImage` 加载一张位图），图片应该被裁切成圆形显示。

2. **实现环形窗口**：用两个不同大小的椭圆 Region 通过 `CombineRgn` 的 `RGN_DIFF` 模式创建一个"甜甜圈"形状的窗口，环形中间应该是透明的（可以看到桌面）。

3. **实现支持大小调整的圆角窗口**：在圆角窗口的四个角和四条边添加拖拽热区（通过 `WM_NCHITTEST` 返回 `HTTOPLEFT`、`HTRIGHT` 等值），使得用户可以通过拖拽边缘来调整窗口大小。注意每次大小变化后需要重新创建 Region。

4. **用 Region 布尔运算创建十字形窗口**：创建两个细长的矩形 Region（一个水平、一个垂直），用 `CombineRgn` 的 `RGN_OR` 合并成一个十字形窗口。给这个十字形窗口加上红色填充和拖动功能。

---

**参考资料**:
- [CreateRoundRectRgn function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createroundrectrgn)
- [CombineRgn function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-combinergn)
- [SetWindowRgn function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn)
- [SelectClipRgn function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectcliprgn)
- [Region Functions overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/region-functions)
- [Window Regions - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/window-regions)
- [CreatePolygonRgn function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createpolygonrgn)
