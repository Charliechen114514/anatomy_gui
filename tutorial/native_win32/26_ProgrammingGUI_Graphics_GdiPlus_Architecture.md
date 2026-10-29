# 通用GUI编程技术——图形渲染实战（二十六）——GDI+与GDI架构差异：抗锯齿与渐变

> 在上一篇文章中，我们折腾了 Alpha 混合——用 GDI 的 `AlphaBlend` 函数手动拼装源混合因子和目标混合因子，一行一行地控制每个像素的透明度。说实话，用纯 GDI 做渐变、做半透明叠加，体验就像拿着螺丝刀拧螺丝——能用，但拧完一整面墙的螺丝之后，你的手腕会告诉你这不是长久之计。GDI 在 1985 年被设计出来的时候，图形编程的需求远没有今天这么复杂。没有抗锯齿，没有渐变画刷，没有浮点坐标——所有东西都被限定在像素级整数网格上。
>
> 这就引出了今天的主题：GDI+。微软在 Windows XP 时代引入了 GDI+，目的很明确——给 GDI 打补丁，让它能做抗锯齿、渐变填充、矩阵变换和多种图像格式支持。GDI+ 不是 GDI 的替代品，而是一层建在 GDI 之上的 C++ 封装。今天我们就来拆解 GDI+ 和 GDI 的架构差异，看看它到底解决了什么问题，又带来了什么新的坑。

## 环境说明

在开始之前，先交代一下我们这次的开发环境：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x 工具集)
- **目标平台**: Win32 原生桌面应用
- **图形库**: GDI+（需要链接 `gdiplus.lib`，头文件 `<gdiplus.h>`）
- **字符集**: Unicode（所有字符串使用宽字符）

⚠️ 注意，GDI+ 的头文件是 `<gdiplus.h>`，但它实际上会引入一系列子头文件（`gdiplusgraphics.h`、`gdiplusbrush.h`、`gdipluspen.h` 等）。你只需要 `#include <gdiplus.h>` 一行就够了。但链接器那边你必须手动加上 `gdiplus.lib`，否则会收获一大堆 unresolved external symbol 的链接错误。在 Visual Studio 里可以通过"项目属性 → 链接器 → 输入 → 附加依赖项"添加，也可以直接在代码里用 `#pragma comment(lib, "gdiplus.lib")`。

## GDI+ 的设计目标：GDI 做不到什么

在动手写代码之前，我们得先搞清楚一个问题：GDI+ 到底解决什么问题？或者说，GDI 哪些事情做不好？

如果你跟着前面的文章一路走过来，你应该已经感受到了 GDI 的几个痛点。GDI 绘制直线时，像素要么亮要么灭，没有中间态，斜线看起来像锯齿一样；GDI 的画刷只有实心色（`CreateSolidBrush`）、阴影线（`CreateHatchBrush`）和位图纹理（`CreatePatternBrush`），没有渐变画刷；GDI 的坐标系统只有整数，你没法表达 0.5 个像素的位置；GDI 的图像支持只有 BMP 格式，想显示 PNG 或 JPEG 得自己找第三方库解码。

GDI+ 的设计目标就是解决这些痛点。它提供了抗锯齿渲染，让你的线条和曲线不再像锯齿；它提供了渐变画刷（线性渐变和路径渐变），让你一行代码就能画出从红色到蓝色的平滑过渡；它支持浮点坐标和矩阵变换，你可以对整个坐标系做旋转、缩放和平移；它内置了 BMP、PNG、JPEG、GIF、TIFF 等多种图像格式的编解码器，加载和保存图片都不需要第三方库。

你可以把 GDI+ 理解为 GDI 之上的一个"增强层"——它底层还是通过 GDI 的 HDC 来与设备打交道，但在应用层提供了更高级的抽象。这个增强层是用 C++ 类来实现的，所有核心功能都封装在 `Graphics`、`Pen`、`Brush`、`Image`、`Matrix` 等类中。

### GDI vs GDI+ 的核心架构对比

我们先从架构层面看一下两者的区别，因为这个区别会直接影响你写代码的方式。

GDI 是一个纯粹的 C 语言 API，所有操作都围绕 HDC（设备上下文句柄）展开。你创建画笔用 `CreatePen`，返回一个 `HPEN` 句柄；你选入画笔用 `SelectObject`；你画线用 `MoveToEx` + `LineTo`；你清理资源用 `DeleteObject`。整个编程模型是"句柄 + 函数调用"的形式，没有面向对象的概念。

GDI+ 则采用 C++ 类封装的方式。核心类 `Graphics` 取代了 HDC 的角色——准确地说，`Graphics` 对象内部持有一个 HDC，你在构造 `Graphics` 时把 HDC 传进去，之后就通过 `Graphics` 的成员函数来执行所有绘制操作。画笔是 `Pen` 类，画刷是 `Brush` 类（下面派生出 `SolidBrush`、`LinearGradientBrush`、`TextureBrush` 等），图像是 `Image` 类，变换矩阵是 `Matrix` 类。每个类都自己管理资源，析构函数自动释放底层 GDI 对象，你不用再操心 `DeleteObject` 的问题。

这里有一个很关键的架构差异值得强调：GDI 是"状态机"模型，你通过 `SelectObject` 把画笔、画刷等选入 DC，之后的绘制操作就用当前选入的对象；而 GDI+ 是"参数化"模型，你每次绘制时把 `Pen` 或 `Brush` 作为参数传入函数。这意味着你可以在同一次绘制中随时切换不同的画笔和画刷，而不需要频繁地"选入-选出"。

另外一个重要的区别是 GDI+ 支持 ARGB 颜色。GDI 的颜色是 24 位 RGB（用 `RGB()` 宏），没有 alpha 通道；GDI+ 的 `Color` 类支持 32 位 ARGB，alpha 值范围 0-255，0 表示完全透明，255 表示完全不透明。这意味着你可以在 GDI+ 中直接使用半透明颜色进行绘制，不需要像在 GDI 中那样手动调用 `AlphaBlend`。

## 从零开始：初始化 GDI+ 运行时

好了，概念说完了，现在我们开始动手。第一步是初始化 GDI+ 运行时。这件事听起来简单，但根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusinit/nf-gdiplusinit-gdiplusstartup)，`GdiplusStartup` 必须在你调用任何其他 GDI+ 函数之前执行，而 `GdiplusShutdown` 必须在你不再使用 GDI+ 之后执行。这个"之前"和"之后"的时序看起来很明确，但实际操作中坑不少。

### 初始化模板

我们来看一个最小化的 GDI+ 初始化模板：

```cpp
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// 全局变量：GDI+ 的初始化令牌
ULONG_PTR g_gdiplusToken = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    // 第一步：初始化 GDI+
    GdiplusStartupInput gdiplusStartupInput;
    Status status = GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    if (status != Ok) {
        MessageBox(NULL, L"GDI+ 初始化失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 注册窗口类
    const wchar_t CLASS_NAME[] = L"GdiPlusDemo";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName  = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"GDI+ 渐变与抗锯齿演示",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 最后一步：关闭 GDI+
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
```

这段代码的初始化逻辑非常直接。`GdiplusStartupInput` 是一个结构体，用来告诉 GDI+ 你需要哪些功能。它有几个成员，但默认构造函数已经把所有东西都设置好了——`GdiplusVersion` 设为 1，`SuppressBackgroundThread` 和 `SuppressExternalCodecs` 都设为 FALSE，`DebugEventCallback` 设为 NULL。在绝大多数场景下，你不需要修改这些默认值，直接声明一个 `GdiplusStartupInput` 变量然后传给 `GdiplusStartup` 就行。

`GdiplusStartup` 的第一个参数是一个指向 `ULONG_PTR` 的指针，GDI+ 会往里面写入一个"令牌"（token）。这个令牌是一个不透明的标识符，你不需要关心它的值是什么，只需要在程序结束时把它传给 `GdiplusShutdown` 就行。

第三个参数是 `GdiplusStartupOutput`，用来接收 GDI+ 返回的钩子函数指针。只有当你设置 `SuppressBackgroundThread` 为 TRUE 时才需要用到它，一般情况下传 NULL 就好。

⚠️ 这里有一个非常容易踩的坑。`GdiplusStartup` 的返回值类型是 `Gdiplus::Status`，这是一个枚举。`Ok` 表示成功，其他值表示各种错误。很多新手不检查返回值就直接往下走，结果后面调用 GDI+ 函数时全部返回 `WrongState` 或 `GenericError`，怎么排查都找不到原因——其实根子就在初始化失败了。

### 初始化和关闭的位置问题

现在我们来聊聊 `GdiplusStartup` 和 `GdiplusShutdown` 的放置位置。这看似是个小事，实际上是个大坑。

最基本的原则是：`GdiplusStartup` 必须在 `WinMain` 的最开头调用，`GdiplusShutdown` 必须在消息循环结束之后调用。这个顺序保证了在窗口过程（`WndProc`）中使用 GDI+ 对象时，GDI+ 运行时已经初始化完毕。

但真正的坑在于对象的生命周期管理。GDI+ 的 C++ 类（如 `Graphics`、`Pen`、`Brush`、`Image` 等）在析构时会自动释放底层资源。如果 `GdiplusShutdown` 在这些对象析构之前就被调用了，析构函数里的清理操作就会失败，可能导致崩溃或内存泄漏。

考虑一个常见场景：你有一个全局的 `Image` 对象（比如程序启动时加载的背景图），这个对象的析构发生在程序退出时，而全局对象的析构顺序是由 C++ 运行时决定的，通常在 `WinMain` 返回之后。如果你的 `GdiplusShutdown` 调用在 `WinMain` 的末尾，那么在全局 `Image` 对象析构时，GDI+ 已经关闭了——这就炸了。

解决办法很简单：避免使用全局 GDI+ 对象。把需要长期持有的 GDI+ 对象（如 `Image`、`Bitmap`）包装成动态分配的对象，在 `WM_DESTROY` 中手动 `delete` 它们。或者更优雅的做法是用 `std::unique_ptr` 来管理它们的生命周期，确保在 `GdiplusShutdown` 之前释放。

```cpp
// 正确的做法：在 WM_CREATE 中创建，在 WM_DESTROY 中销毁
std::unique_ptr<Image> g_pBackground;

case WM_CREATE:
{
    // 创建窗口时加载图片
    g_pBackground = std::make_unique<Image>(L"background.png");
    if (g_pBackground->GetLastStatus() != Ok) {
        MessageBox(hwnd, L"无法加载背景图片", L"警告", MB_OK | MB_ICONWARNING);
        g_pBackground.reset();
    }
    return 0;
}

case WM_DESTROY:
{
    // 销毁窗口时释放图片——这在 GdiplusShutdown 之前发生
    g_pBackground.reset();
    PostQuitMessage(0);
    return 0;
}
```

这里的关键点是 `WM_DESTROY` 消息在窗口销毁时发送，此时消息循环还在运行（`GetMessage` 还没返回 FALSE），所以 `GdiplusShutdown` 还没被调用。当你在 `WM_DESTROY` 中 `delete` GDI+ 对象时，GDI+ 运行时仍然处于活动状态，析构函数可以正常清理资源。

## Graphics 对象：GDI+ 的核心入口

初始化搞定之后，接下来我们看 GDI+ 绘制的核心——`Graphics` 类。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nl-gdiplusgraphics-graphics)，`Graphics` 类是所有绘制操作的入口点，你可以把它理解为 GDI+ 版本的 HDC。但它比 HDC 高级得多——它封装了绘制状态、变换矩阵、裁剪区域、渲染质量等所有信息。

### 从 HDC 创建 Graphics 对象

在 Win32 应用中使用 GDI+，最常见的做法是在 `WM_PAINT` 处理函数中从 HDC 创建 `Graphics` 对象：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 从 HDC 创建 Graphics 对象
    Graphics graphics(hdc);

    // 设置渲染质量——开启抗锯齿
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    // ... 在 graphics 上执行绘制操作

    EndPaint(hwnd, &ps);
    return 0;
}
```

这里有几个值得注意的点。首先，`Graphics` 对象的构造函数接受一个 HDC 参数，它会在内部引用这个 DC 进行绘制。当 `Graphics` 对象析构时（在这里是 `WM_PAINT` 处理函数返回时），它不会销毁这个 HDC——它只是"借用"而已。所以你仍然需要在合适的地方调用 `EndPaint` 或 `ReleaseDC` 来释放 HDC。

其次，`SetSmoothingMode(SmoothingModeAntiAlias)` 这行代码开启了抗锯齿渲染。在 GDI 中，你画的任何线条都有锯齿——因为 GDI 只支持像素对齐的渲染，要么亮要么灭，没有中间态。GDI+ 的抗锯齿通过在像素边界处混合前景色和背景色来实现平滑效果。如果你不调用 `SetSmoothingMode`，默认值是 `SmoothingModeNone`，也就是不抗锯齿——绘制效果和纯 GDI 一样。

`SmoothingMode` 枚举有几个可选值。`SmoothingModeInvalid` 是无效值，通常用于错误检测。`SmoothingModeDefault` 等同于 `SmoothingModeNone`，不抗锯齿。`SmoothingModeHighSpeed` 优先速度，效果接近不抗锯齿。`SmoothingModeHighQuality` 优先质量，使用更精确的抗锯齿算法。`SmoothingModeAntiAlias` 使用标准的 8x4 盒式滤波抗锯齿。`SmoothingModeAntiAlias8x8` 使用 8x8 盒式滤波，质量更高但速度更慢。在实际应用中，`SmoothingModeAntiAlias` 是最常用的选择，它在质量和性能之间取得了不错的平衡。

### Graphics 对象与 HDC 的关系

你可能会问：我能不能在一个 HDC 上同时使用 GDI 和 GDI+ 进行绘制？答案是可以的，但需要注意调用顺序。

GDI+ 的 `Graphics` 对象在创建时可能会改变 HDC 的某些状态（比如映射模式），在析构时又会尝试恢复这些状态。如果你在 `Graphics` 对象存活期间通过 GDI 函数直接操作同一个 HDC，可能会导致不可预期的行为。

最佳实践是：如果需要在同一个 DC 上混合使用 GDI 和 GDI+，先用 GDI 完成所有操作，然后再创建 `Graphics` 对象进行 GDI+ 操作。或者反过来——先把 GDI+ 的 `Graphics` 对象销毁，再用 GDI 函数操作 DC。不要交叉使用。

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 先用 GDI 绘制
    HBRUSH hbr = CreateSolidBrush(RGB(200, 200, 200));
    FillRect(hdc, &ps.rcPaint, hbr);
    DeleteObject(hbr);

    // GDI 操作完成后再创建 GDI+ Graphics 对象
    {
        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        // 用 GDI+ 继续绘制
        Pen pen(Color(255, 0, 0, 255), 2.0f);
        graphics.DrawEllipse(&pen, 50, 50, 200, 150);
    }
    // Graphics 对象在这里析构

    EndPaint(hwnd, &ps);
    return 0;
}
```

用大括号限定 `Graphics` 对象的作用域是一个好习惯——它保证了 `Graphics` 在 `EndPaint` 之前析构，避免了 GDI+ 析构时与 `EndPaint` 产生冲突。

## 渐变填充：从 SolidBrush 到 LinearGradientBrush

现在我们进入 GDI+ 的重头戏——渐变填充。在 GDI 中，如果你想画一个从红色渐变到蓝色的矩形，你需要手动逐行计算颜色值，逐行填充——代码量感人。在 GDI+ 中，你只需要创建一个 `LinearGradientBrush` 对象就行了。

### SolidBrush 的基本用法

在讲渐变之前，先快速过一下实心画刷 `SolidBrush`。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusbrush/nl-gdiplusbrush-solidbrush)，`SolidBrush` 类似于 GDI 的 `CreateSolidBrush`，但支持 ARGB 颜色：

```cpp
// 创建一个半透明的红色画刷
SolidBrush brush(Color(128, 255, 0, 0));  // alpha=128, R=255, G=0, B=0

// 用它填充一个矩形
graphics.FillRectangle(&brush, 10, 10, 200, 100);
```

`Color` 构造函数的参数顺序是 `(alpha, red, green, blue)`。注意这不是 GDI 的 `RGB()` 宏——`RGB()` 的参数顺序是 `(red, green, blue)`，没有 alpha。在 GDI+ 中，alpha=255 表示完全不透明，alpha=0 表示完全透明。当你使用半透明画刷绘制时，GDI+ 会自动进行 Alpha 混合，不需要你手动调用任何混合函数。

这就是 GDI+ 的巨大优势之一：你再也不需要像在前一篇文章中那样手动管理源混合因子和目标混合因子了。只需要设置 alpha 值，GDI+ 自动搞定一切。

### LinearGradientBrush 渐变画刷

好了，现在进入正题。根据 [Microsoft Learn 的文档](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusbrush/nl-gdiplusbrush-lineargradientbrush)，`LinearGradientBrush` 可以创建从一个点到另一个点的线性渐变。最简单的用法：

```cpp
// 创建从红色到蓝色的水平渐变画刷
LinearGradientBrush brush(
    Point(0, 0),            // 渐变起始点
    Point(300, 0),          // 渐变结束点
    Color(255, 255, 0, 0),  // 起始颜色（红色，完全不透明）
    Color(255, 0, 0, 255)   // 结束颜色（蓝色，完全不透明）
);

// 用渐变画刷填充矩形
graphics.FillRectangle(&brush, 0, 0, 300, 200);
```

这段代码会画出一个 300 像素宽、200 像素高的矩形，颜色从左侧的红色平滑过渡到右侧的蓝色。`Point(0, 0)` 到 `Point(300, 0)` 定义了渐变的方向和范围——从左到右，跨越 300 像素。

你可能会发现这里用的坐标类型是 `Point` 而不是 GDI 的 `POINT`。GDI+ 有自己的坐标类型：`Point`（整数坐标）和 `PointF`（浮点坐标）。对于大多数场景，`Point` 就够了。但如果你需要精确的亚像素渐变控制，可以使用 `PointF`。

渐变方向不限于水平。你可以通过改变起始点和结束点来创建任意方向的渐变——对角线渐变、垂直渐变，甚至是非常倾斜的渐变：

```cpp
// 对角线渐变：从左上角到右下角
LinearGradientBrush diagonalBrush(
    Point(0, 0),
    Point(300, 200),
    Color(255, 255, 255, 0),  // 黄色
    Color(255, 128, 0, 128)   // 紫色
);

graphics.FillRectangle(&diagonalBrush, 0, 0, 300, 200);
```

### 渐变画刷的高级用法

`LinearGradientBrush` 还支持一些高级功能，比如设置中间颜色点。默认情况下，渐变只在起始颜色和结束颜色之间进行线性插值。但你可以用 `SetInterpolationColors` 方法添加多个中间颜色，创建更丰富的渐变效果：

```cpp
LinearGradientBrush brush(
    Point(0, 0), Point(400, 0),
    Color(255, 255, 0, 0),    // 红色
    Color(255, 0, 0, 255)     // 蓝色
);

// 添加中间颜色
Color colors[] = {
    Color(255, 255, 0, 0),    // 红色
    Color(255, 255, 255, 0),  // 黄色
    Color(255, 0, 255, 0),    // 绿色
    Color(255, 0, 0, 255)     // 蓝色
};
REAL positions[] = { 0.0f, 0.33f, 0.66f, 1.0f };

brush.SetInterpolationColors(colors, positions, 4);

graphics.FillRectangle(&brush, 0, 0, 400, 200);
```

这里 `positions` 数组中的值范围是 0.0 到 1.0，表示渐变的位置比例。0.0 对应起始点，1.0 对应结束点。通过设置多个颜色和位置，你可以创建彩虹效果、热力图等各种复杂渐变。

还有一个实用的功能是 `SetGammaCorrection`，它启用伽马校正来让渐变看起来更自然。人类视觉对亮度的感知不是线性的，开启伽马校正后，渐变过渡会看起来更加均匀：

```cpp
brush.SetGammaCorrection(TRUE);
```

这个小细节在实际应用中很有效果，尤其是深色到浅色的渐变——不开启伽马校正时，中间部分看起来会比预期暗很多。

## 实战演练：用 GDI+ 绘制带渐变填充的饼图

好了，理论铺垫够了，现在我们来做一个完整的实战练习：绘制一个带渐变填充的饼图。通过这个例子，我们能把今天讲的所有知识点串起来——GDI+ 初始化、Graphics 对象、抗锯齿、渐变画刷。

先来看看最终的 `WndProc` 实现：

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

        // 获取客户区尺寸
        RECT rc;
        GetClientRect(hwnd, &rc);
        int cx = rc.right - rc.left;
        int cy = rc.bottom - rc.top;

        // 饼图参数
        REAL cxCenter = cx / 2.0f;
        REAL cyCenter = cy / 2.0f;
        REAL radius = (cx < cy ? cx : cy) * 0.35f;

        // 饼图数据（百分比）
        struct Slice {
            REAL percentage;
            Color color1;
            Color color2;
            const wchar_t* label;
        };

        Slice slices[] = {
            { 0.35f, Color(255, 220, 50, 50),   Color(255, 255, 120, 120), L"产品A 35%" },
            { 0.25f, Color(255, 50, 120, 220),   Color(255, 120, 180, 255), L"产品B 25%" },
            { 0.20f, Color(255, 50, 180, 80),    Color(255, 120, 240, 160), L"产品C 20%" },
            { 0.12f, Color(255, 220, 180, 50),   Color(255, 255, 230, 120), L"产品D 12%" },
            { 0.08f, Color(255, 160, 80, 200),   Color(255, 220, 160, 240), L"产品E 8%"  },
        };
        int sliceCount = sizeof(slices) / sizeof(slices[0]);

        // 绘制每个扇形
        REAL startAngle = -90.0f;  // 从12点钟方向开始

        for (int i = 0; i < sliceCount; i++) {
            REAL sweepAngle = slices[i].percentage * 360.0f;

            // 为每个扇形创建径向渐变效果
            // 用从中心到边缘的线性渐变模拟
            LinearGradientBrush brush(
                PointF(cxCenter - radius, cyCenter - radius),
                PointF(cxCenter + radius, cyCenter + radius),
                slices[i].color1,
                slices[i].color2
            );

            // 绘制扇形
            graphics.FillPie(
                &brush,
                cxCenter - radius, cyCenter - radius,
                radius * 2, radius * 2,
                startAngle, sweepAngle
            );

            // 绘制扇形边框
            Pen pen(Color(255, 255, 255, 255), 2.0f);
            graphics.DrawPie(
                &pen,
                cxCenter - radius, cyCenter - radius,
                radius * 2, radius * 2,
                startAngle, sweepAngle
            );

            startAngle += sweepAngle;
        }

        // 绘制图例
        Font font(L"微软雅黑", 12);
        SolidBrush textBrush(Color(255, 60, 60, 60));
        REAL legendX = cxCenter + radius + 40;
        REAL legendY = cyCenter - sliceCount * 12;

        for (int i = 0; i < sliceCount; i++) {
            // 绘制图例色块
            SolidBrush legendBrush(slices[i].color1);
            graphics.FillRectangle(&legendBrush, legendX, legendY + i * 28, 18, 18);
            graphics.DrawRectangle(
                &Pen(Color(200, 120, 120, 120), 1.0f),
                legendX, legendY + i * 28, 18, 18
            );

            // 绘制图例文字
            PointF textPos(legendX + 26, legendY + i * 28);
            graphics.DrawString(
                slices[i].label, -1, &font, textPos, &textBrush
            );
        }

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

我们来逐段拆解这段代码。

饼图的核心数据定义在 `Slice` 结构体数组中，每个元素包含一个百分比、两个渐变颜色和一个标签字符串。百分比用浮点数表示，所有百分比加起来应该等于 1.0。渐变的两个颜色分别作为对角线渐变的起始色和结束色——这样每个扇形都会有一个从深到浅的过渡效果。

绘制扇形用的是 `FillPie` 和 `DrawPie` 函数。这两个函数的参数和 GDI 的 `Pie` 函数类似，但有重要区别：GDI 的 `Pie` 使用矩形边界和两个径向端点来定义扇形，而 GDI+ 的 `FillPie`/`DrawPie` 使用矩形边界、起始角度和扫描角度。角度单位是度数（不是弧度），正方向是顺时针，0 度指向 3 点钟方向。我们设置 `startAngle = -90.0f`，也就是从 12 点钟方向开始，这符合饼图的常见布局。

`FillPie` 的参数依次是：画刷指针、包围椭圆的矩形左上角 x、y、宽、高、起始角度、扫描角度。扫描角度是扇形的角度跨度，不是终止角度——如果扫描角度为负数，会逆时针绘制。

每个扇形使用独立的 `LinearGradientBrush`，从深色渐变到浅色。白色边框 `Pen` 用来分隔相邻的扇形，让每个区域看起来更清晰。

图例部分用 `Font` 和 `DrawString` 绘制文字。GDI+ 的 `DrawString` 比 GDI 的 `DrawText`/`TextOut` 灵活得多——它支持抗锯齿文字渲染、自动换行、精确的浮点坐标定位等特性。`Font` 构造函数的第一个参数是字体族名称，第二个是字号（以 UnitPixel 为默认单位）。

⚠️ 注意，如果你在编译时遇到了关于 `GDIPLUS::Font` 的链接错误，检查一下 `gdiplus.lib` 是否正确链接。GDI+ 的所有类都在 `Gdiplus` 命名空间下，所以 `using namespace Gdiplus;` 是省事的做法，但在大型项目中建议显式使用命名空间前缀以避免名称冲突。

### 对比 GDI 实现的代码量

现在我们来做一个有趣的对比。如果要实现同样效果的饼图——带渐变填充和抗锯齿——用纯 GDI 需要多少代码？

在 GDI 中，渐变效果需要逐像素处理。你需要先创建一个内存位图，获取它的像素缓冲区，然后对每个像素计算它在扇形中的位置、对应的渐变颜色值，再写入像素。抗锯齿更是需要你手动实现亚像素渲染——通常是通过多重采样或者预计算 AA 查找表来实现。保守估计，纯 GDI 版本的代码量是 GDI+ 版本的 5-8 倍，而且运行效果可能还不如 GDI+ 的好。

这就是 GDI+ 的核心价值——它不是什么革命性的新技术，而是把 GDI 中需要大量手动实现的常见图形效果封装成了简单易用的 API。你用更少的代码实现了更好的效果，降低了出错的概率。

## 抗锯齿深入理解

既然我们已经在饼图例子中用到了抗锯齿，那就趁机深入了解一下它的工作原理。

### SmoothingMode 的不同级别

GDI+ 的 `SmoothingMode` 枚举定义了不同的抗锯齿级别。前面提到过可选值，现在我们来看看它们之间的实际差异。

`SmoothingModeNone` 完全不做抗锯齿处理。线条的像素要么是前景色要么是背景色，没有中间态。绘制斜线时会有明显的锯齿。性能最好，效果最差。

`SmoothingModeAntiAlias` 使用 8x4 盒式滤波器进行抗锯齿。对于每个输出像素，在 8 个水平方向和 4 个垂直方向的子像素位置进行采样，计算覆盖比例，然后混合前景色和背景色。这是最常用的模式，在质量和性能之间取得了不错的平衡。

`SmoothingModeAntiAlias8x8` 使用 8x8 盒式滤波器，在水平和垂直方向都使用 8 个子像素采样。质量更高，但计算量也更大。

在实际应用中，`SmoothingModeAntiAlias` 已经足够好了。只有在需要极高绘制质量的场景（比如打印输出或高质量截图）中，才有必要使用 `SmoothingModeAntiAlias8x8`。

### 抗锯齿与文字渲染

需要注意的是，`SmoothingMode` 只影响图形绘制（线条、曲线、填充形状），不影响文字渲染。文字的抗锯齿由 `SetTextRenderingHint` 单独控制：

```cpp
graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
```

`TextRenderingHint` 枚举也有多个级别。`TextRenderingHintSystemDefault` 使用系统默认的文字渲染方式。`TextRenderingHintSingleBitPerPixel` 使用单色渲染，无抗锯齿。`TextRenderingHintAntiAlias` 使用标准的灰度抗锯齿。`TextRenderingHintClearTypeGridFit` 使用 ClearType 亚像素渲染——这是 LCD 显示器上效果最好的文字渲染方式。

```cpp
graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
```

如果你在程序中需要绘制文字，强烈建议使用 `ClearTypeGridFit`——它利用了 LCD 显示器的亚像素结构来提供更清晰锐利的文字渲染效果。

## Pen 类：线条的艺术

在 GDI+ 中，`Pen` 类取代了 GDI 的 `HPEN`。虽然前面已经用了 `Pen`，但我们还没有深入看它的功能。

### 基本画笔

最简单的用法是创建一个指定颜色和宽度的画笔：

```cpp
Pen pen(Color(255, 0, 0, 0), 2.0f);  // 黑色，2像素宽
graphics.DrawLine(&pen, 10, 10, 300, 200);
```

画笔宽度支持浮点数——这是 GDI 做不到的。GDI 的 `CreatePen` 只接受整数宽度。在 GDI+ 中，0.5 像素宽度的线条配合抗锯齿会呈现出非常细腻的效果。

### 虚线样式

`Pen` 类支持丰富的虚线样式：

```cpp
Pen dashPen(Color(255, 0, 0, 255), 1.5f);
dashPen.SetDashStyle(DashStyleDash);
graphics.DrawLine(&dashPen, 10, 50, 300, 50);

// 自定义虚线模式
REAL dashValues[] = { 5.0f, 2.0f, 1.0f, 2.0f };
dashPen.SetDashPattern(dashValues, 4);
graphics.DrawLine(&dashPen, 10, 70, 300, 70);
```

`SetDashPattern` 让你可以定义任意虚线模式。数组中的值交替表示"画"和"不画"的长度，单位是像素。上面的 `dashValues` 表示：画 5 像素、跳 2 像素、画 1 像素、跳 2 像素，然后循环。

### 线帽和连接

`Pen` 还支持自定义线帽（LineCap）和线条连接样式（LineJoin）：

```cpp
Pen capPen(Color(255, 0, 100, 0), 8.0f);
capPen.SetStartCap(LineCapRound);
capPen.SetEndCap(LineCapArrowAnchor);
capPen.SetLineJoin(LineJoinRound);

graphics.DrawLine(&capPen, 50, 150, 250, 150);
```

`LineCapRound` 让线条的起始端呈圆形，`LineCapArrowAnchor` 让结束端呈箭头形状。`LineJoinRound` 让折线的连接处呈圆弧过渡，而不是尖锐的角。这些功能在 GDI 中要么不支持，要么需要非常复杂的代码才能实现。

## 常见问题与调试

在 GDI+ 的实际开发中，有一些常见的问题值得提前了解。

### 问题一：GdiplusStartup 返回错误

`GdiplusStartup` 可能因为多种原因失败。最常见的原因是传递了无效的 `GdiplusStartupInput` 结构——比如手动修改了 `GdiplusVersion` 字段为不支持的版本号。默认构造函数已经把版本设为 1，这是目前唯一支持的版本。如果你手动改成了 2 或者 0，`GdiplusStartup` 会返回 `UnsupportedGdiplusVersion`。

另一个可能的错误是系统资源不足。GDI+ 初始化时需要分配一些内部资源，如果内存不足可能返回 `OutOfMemory`。

### 问题二：Graphics 对象创建失败

从 HDC 创建 `Graphics` 对象时，如果 HDC 无效（比如已经被删除的 DC），构造不会崩溃但后续所有绘制操作都会返回错误状态。你可以用 `GetLastStatus` 方法检查：

```cpp
Graphics graphics(hdc);
if (graphics.GetLastStatus() != Ok) {
    // Graphics 对象创建失败
    EndPaint(hwnd, &ps);
    return 0;
}
```

### 问题三：渐变画刷方向不对

`LinearGradientBrush` 的渐变方向由起始点和结束点决定，但很多人会误以为渐变会自动填充整个目标矩形。实际上，渐变只在起始点和结束点定义的范围内有效——超出范围的部分会使用起始或结束颜色填充（取决于哪个方向超出了）。如果你发现渐变只覆盖了矩形的一部分，检查一下起始点和结束点是否覆盖了你想要的范围。

### 问题四：Alpha 混合不生效

如果你创建了半透明颜色的画刷或画笔，但绘制出来的效果完全不透明，可能的原因是窗口没有启用分层（layered）支持。在标准窗口中，GDI+ 的 Alpha 混合通常可以正常工作，但如果你是在双缓冲的内存 DC 上绘制然后再 `BitBlt` 到屏幕，需要注意 `BitBlt` 不支持 Alpha 混合——你需要用 `AlphaBlend` 函数来传输带 alpha 通道的内容。

## 总结

到这里，我们已经把 GDI+ 和 GDI 的架构差异完整梳理了一遍。我们从 GDI+ 的设计目标出发，理解了它在抗锯齿、渐变、浮点坐标和多图像格式支持方面相比 GDI 的优势；我们搭建了完整的 GDI+ 初始化框架，包括 `GdiplusStartup`/`GdiplusShutdown` 的正确使用位置和全局对象的生命周期管理陷阱；我们通过 `Graphics` 对象了解了 GDI+ 的参数化绘制模型，用 `LinearGradientBrush` 实现了渐变填充，最后把这些知识点组合成了一个带渐变填充的饼图。

GDI+ 的 C++ 封装让图形编程的门槛降低了不少，但底层机制仍然建立在 GDI 之上。理解了 GDI 的工作原理，再回头看 GDI+，你会发现它做的事情其实并不神秘——只是把那些原本需要你手动实现的复杂操作，封装成了简洁的 C++ 类。

下一篇文章我们将进入 GDI+ 的坐标变换世界。坐标变换是图形编程中最优雅也最容易让人困惑的部分之一——世界坐标、页面坐标、设备坐标的三级转换，矩阵乘法的非交换性，Save/Restore 的状态管理——每一个都是值得深入理解的专题。到时候见。

---

**练习**

1. **渐变色轮**：用 `LinearGradientBrush` 和 `FillPie` 绘制一个彩虹色轮——每个扇形 30 度，共 12 个扇形，颜色依次为红、橙、黄、绿、青、蓝、紫及其过渡色。要求使用 `SetInterpolationColors` 实现更丰富的色彩过渡。

2. **对比实验**：将本文的饼图示例用纯 GDI 实现一遍（不使用 GDI+），体验一下渐变填充和抗锯齿在 GDI 中的实现难度。记录两个版本的代码行数和视觉效果差异。

3. **半透明叠加**：修改饼图示例，让每个扇形使用半透明颜色（alpha=200），在饼图下方先绘制一个棋盘格背景，观察 GDI+ 的自动 Alpha 混合效果。尝试不同的 alpha 值，理解透明度对视觉效果的影响。

4. **画笔实验室**：创建一个演示程序，展示 `Pen` 类的各种特性——不同宽度（0.5f、1.0f、3.0f、5.0f）、不同虚线样式（Dash、Dot、DashDot、DashDotDot、自定义模式）、不同线帽（Flat、Round、Square、ArrowAnchor）和不同连接样式（Miter、Bevel、Round）。用表格式的布局展示所有组合。

---

**参考资料**

- [GdiplusStartup function (gdiplusinit.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusinit/nf-gdiplusinit-gdiplusstartup)
- [GdiplusShutdown function (gdiplusinit.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusinit/nf-gdiplusinit-gdiplusshutdown)
- [Graphics class (gdiplusgraphics.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nl-gdiplusgraphics-graphics)
- [LinearGradientBrush class (gdiplusbrush.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusbrush/nl-gdiplusbrush-lineargradientbrush)
- [SolidBrush class (gdiplusbrush.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusbrush/nl-gdiplusbrush-solidbrush)
- [The Structure of the Class-Based Interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-the-structure-of-the-class-based-interface-about)
- [Creating a Linear Gradient - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-creating-a-linear-gradient-use)
- [Graphics::Save method - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-save)
