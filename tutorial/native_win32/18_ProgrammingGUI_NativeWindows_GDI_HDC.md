# 通用GUI编程技术——Win32 原生编程实战（十八）——GDI 设备上下文（HDC）完全指南

> 前面一系列文章我们聊了对话框、控件、资源这些内容，我们的窗口已经能够显示各种控件了。但你可能已经发现了一个问题：我们所有的"绘图"操作都是在控件内部进行的，如果我想在窗口的客户区直接画一条线、画一个圆、或者显示一张图片，该怎么处理？这就涉及到 Win32 编程中一个核心但容易被初学者忽视的主题——GDI 设备上下文（HDC）。今天我们要深入的就是这个让无数新手踩坑的话题。

---

## 前言：从 WM_PAINT 说起的绘图需求

说实话，在我刚开始学 Win32 的时候，对 GDI 绘图这件事其实有点抗拒。那时候觉得现代框架随便拖个控件就能实现大部分需求，为什么要去折腾这些底层的绘图 API？但很快我就发现，这种想法是 naive 的。当你需要实现一个自定义的图表控件、需要在窗口上实时显示数据曲线、或者想做点炫酷的动画效果时，你会发现控件完全帮不上忙，必须亲自下场画。

更现实的是：很多看似简单的需求背后都需要 GDI 知识。比如你想在窗口背景上画一个渐变色，想在状态栏显示一个进度图标，或者想把控件的外观改成非标准样式，这些都需要直接操作设备上下文。而如果你不理解 HDC 的本质和正确的使用方式，你的程序要么画不出东西，要么画出来一闪一闪的，更糟糕的是——悄无声息地泄漏资源，直到系统提示你" GDI 对象不足"。

另一个让新手头疼的问题是：获取 HDC 的方式有好几种，BeginPaint、GetDC、GetWindowDC、CreateCompatibleDC，它们各自适用什么场景？什么时候该用哪个？用错了会有什么后果？官方文档虽然写了，但说实话那些描述对于初学者来说有点抽象，很多坑只能自己踩过才知道。

这篇文章会带着你从 HDC 的本质开始，把四种获取方式、状态管理、GDI 对象生命周期这些核心问题彻底搞清楚。我们不只是知道"怎么用"，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 2000+ 都支持 GDI）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）
* **Windows SDK**：任何最新版本即可

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本窗口、怎么处理消息、什么是窗口过程函数。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——HDC 的本质：什么是设备上下文

### HDC 是什么

HDC（Handle to Device Context）是 Windows GDI 中最核心的概念之一。官方定义是"设备上下文"，一个结构体，定义了一组图形对象及其关联属性，以及影响输出的图形模式。这个定义听起来有点抽象，我们换个角度理解。

你可以把 HDC 想象成一张"画布"的句柄。就像画家需要画布来作画一样，在 Windows 里绘图，你需要先拿到一个 HDC，然后才能在上面画线、画圆、写字。但这个画布不只是屏幕，也可以是打印机、内存中的位图，甚至是元文件。HDC 就是 Windows 给你提供的一个抽象层，让你用同一套 API 就能在不同设备上绘图。

### HDC 包含什么

一个 HDC 内部包含了很多东西，你可以把它理解成当前绘图状态的一个快照。里面有什么呢？有当前选中的画笔（决定线的颜色和粗细）、画刷（决定填充颜色）、字体（决定文字样式）、位图（用于图像操作）、调色板（定义可用颜色），还有一堆绘图属性比如背景模式、文本对齐方式、当前坐标位置等等。

这些东西在 Windows 内部是怎么组织的呢？其实 HDC 背后对应的是一段内核管理的内存结构。当你调用 GetDC 或 BeginPaint 时，Windows 会为你准备好这个结构，填充好默认值。你可以修改这些值，你的修改会立即影响后续的绘图操作。当你 ReleaseDC 或 EndPaint 时，Windows 会把这个 HDC 标记为可用，其他地方可以复用。

### 为什么需要 HDC

你可能会问：为什么不直接在窗口上画，非要通过 HDC 这个中间层？核心原因是抽象和隔离。Windows 需要支持很多种输出设备——显示器、打印机、绘图仪、内存位图等等。每种设备的特性都不一样，分辨率的差异、色彩深度的差异、支持绘图原语的差异。如果没有 HDC 这个抽象层，你的程序就需要针对每种设备写不同的代码，这是不可想象的。

有了 HDC，你可以用同一套代码在不同设备上绘图。你只需要说"画一条红色直线"，Windows 会根据当前设备的特性，把这条指令转换成对应的设备操作。显示器上是画像素，打印机上是喷墨，内存位图上是修改像素数据。你的代码不需要关心这些细节。

另一个重要的原因是多任务环境下的资源管理。Windows 是多任务系统，很多程序可能同时想绘图。如果没有 HDC 的概念，程序之间就会互相干扰——你的程序可能把另一个程序的绘图给覆盖了。通过 HDC，Windows 可以管理绘图上下文，确保每个程序只能操作自己的区域。

---

## 第二步——获取 HDC 的四种方式

现在我们进入正题。Windows 提供了几种获取 HDC 的方式，每种方式都有自己的适用场景和注意事项。用错了不仅可能导致绘图失败，还可能造成资源泄漏。我们一个个来看。

### BeginPaint/EndPaint（WM_PAINT 中专用）

这是最常见的获取方式，也是最容易用错的方式。BeginPaint 和 EndPaint 是专门为处理 WM_PAINT 消息设计的，它们必须成对使用，而且只能在 WM_PAINT 消息处理中调用。

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 所有绘图操作在这里进行
    Rectangle(hdc, 50, 50, 200, 150);

    EndPaint(hwnd, &ps);
    return 0;
}
```

这里有个重要的细节：BeginPaint 返回的 HDC 只能用于绘制窗口的"无效区域"。什么是无效区域？就是 Windows 标记为需要重绘的部分。当你调用 InvalidateRect 时，Windows 会把指定区域标记为无效，然后在下一个消息循环时发送 WM_PAINT 消息。BeginPaint 会自动把无效区域的矩形信息填充到 PAINTSTRUCT 结构的 rcPaint 字段里，你的绘图操作应该限制在这个区域内，这样效率最高。

更重要的是，BeginPaint 会自动验证无效区域。什么叫验证？就是告诉 Windows"这个区域我已经画好了，不需要再发 WM_PAINT 了"。如果你在 WM_PAINT 里不用 BeginPaint/EndPaint，而是用 GetDC/ReleaseDC，无效区域就永远不会被验证，Windows 会一直发送 WM_PAINT 消息，导致你的程序陷入无限重绘循环，CPU 飙升。

⚠️ 注意

千万别在 WM_PAINT 以外的地方调用 BeginPaint。BeginPaint 的实现依赖于 WM_PAINT 消息的内部机制，在其他消息里调用会导致未定义行为。如果你在非 WM_PAINT 消息里需要绘图，用 GetDC/ReleaseDC。

还有一个常见误区：BeginPaint 返回的 HDC 可以缓存起来后续使用。不行！BeginPaint 返回的 HDC 只在 BeginPaint 和 EndPaint 之间有效，一旦调用了 EndPaint，这个 HDC 就失效了。你必须在每次处理 WM_PAINT 时重新获取。

### GetDC/ReleaseDC（随时获取）

当你需要在非 WM_PAINT 消息里绘图时，应该使用 GetDC/ReleaseDC。这对函数的适用范围更广，可以随时调用，不限于特定消息。

```cpp
// 在任何地方都可以调用
HDC hdc = GetDC(hwnd);

// 绘图操作
Ellipse(hdc, 10, 10, 100, 100);

// 必须配对调用
ReleaseDC(hwnd, hdc);
```

GetDC 返回的 HDC 覆盖整个窗口客户区，不像 BeginPaint 只覆盖无效区域。这意味着你可以用 GetDC 在窗口的任意位置绘图，不受无效区域的限制。但这也意味着你需要注意坐标计算，确保画在你想要的位置。

与 BeginPaint 不同，GetDC/ReleaseDC 不会影响无效区域的验证。你在 WM_PAINT 里用 GetDC 绘图后，无效区域仍然存在，Windows 会继续发送 WM_PAINT 消息。所以在 WM_PAINT 里必须用 BeginPaint/EndPaint，这是铁律。

GetDC 有个兄弟叫 GetWindowDC，我们稍后会讲到。GetDC 还有个变体是 GetDCEx，可以指定更多选项，比如是否包含子窗口、是否拦截绘图操作等。但对于大多数场景，普通的 GetDC 就够用了。

⚠️ 注意

GetDC 和 ReleaseDC 必须成对调用，而且必须在同一线程内配对。你不能在一个线程里 GetDC，然后在另一个线程里 ReleaseDC。这会导致引用计数混乱，最终造成资源泄漏。

频繁调用 GetDC/ReleaseDC 会影响性能，因为每次调用都需要内核介入。如果你需要在短时间内多次绘图，可以考虑缓存 HDC，但缓存时要非常小心——窗口销毁前必须释放，而且窗口大小改变后缓存的 HDC 可能失效。

### GetWindowDC（包含非客户区）

GetWindowDC 与 GetDC 类似，但它返回的 HDC 覆盖整个窗口，包括非客户区（标题栏、边框、菜单栏等）。这意味着你可以在窗口的标题栏上画东西，或者实现自定义的窗口边框。

```cpp
HDC hdc = GetWindowDC(hwnd);

// 可以在整个窗口区域绘图，包括标题栏
TextOut(hdc, 10, 5, L"Custom Title", 13);

ReleaseDC(hwnd, hdc);
```

这个功能在某些特殊场景下很有用，比如你想实现一个自定义标题栏，或者在窗口边框上画一些装饰。但大多数情况下，你不需要操作非客户区，GetDC 就足够了。

⚠️ 注意

在非客户区绘图时要非常小心。Windows 有自己的非客户区绘制逻辑，你的自定义绘图可能会与系统的绘制冲突。而且非客户区的尺寸和样式在不同 Windows 版本上可能有差异，你的程序需要做好兼容性测试。

GetWindowDC 返回的 HDC 也用 ReleaseDC 释放，而不是 DeleteDC 或 EndPaint。这点和 GetDC 一样，它们获取的都是"借来的"DC，用完要还。

### CreateCompatibleDC（内存 DC）

这是最特殊的一个获取方式。CreateCompatibleDC 创建的是一个"内存设备上下文"，它不对应任何真实设备，而是在内存中创建一个虚拟画布。内存 DC 主要用于离屏绘图和双缓冲技术，这是实现平滑动画的基础。

```cpp
// 获取窗口 DC 作为参考
HDC hdcWindow = GetDC(hwnd);

// 创建兼容的内存 DC
HDC hdcMem = CreateCompatibleDC(hdcWindow);

// 创建一个位图并选入内存 DC
HBITMAP hbmMem = CreateCompatibleBitmap(hdcWindow, 400, 300);
HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

// 现在可以在内存 DC 上绘图了
Rectangle(hdcMem, 0, 0, 400, 300);
TextOut(hdcMem, 10, 10, L"Hello Memory DC", 15);

// 把内存 DC 的内容复制到窗口
BitBlt(hdcWindow, 0, 0, 400, 300, hdcMem, 0, 0, SRCCOPY);

// 清理
SelectObject(hdcMem, hbmOld);
DeleteObject(hbmMem);
DeleteDC(hdcMem);
ReleaseDC(hwnd, hdcWindow);
```

这段代码展示了内存 DC 的典型用法。我们先创建一个内存 DC，然后在内存里画好所有东西，最后一次性复制到屏幕上。这样做的好处是避免闪烁——因为所有绘图操作都在内存中完成，用户看到的是最终结果，不会看到中间过程。

⚠️ 注意

新创建的内存 DC 默认只有 1×1 像素的单色位图，你必须先选入一个合适尺寸的位图才能正常绘图。这个陷阱让无数新手踩过坑，你创建内存 DC 后如果不选入位图，画什么都看不见。

CreateCompatibleDC 创建的 DC 必须用 DeleteDC 释放，不是 ReleaseDC。这是因为它是你"创建"的，不是"借用"的。而选入的位图在删除 DC 前必须先选出来，否则位图会泄漏——这个我们后面会详细讲。

---

## 第三步——设备上下文状态管理

### 什么是 DC 状态

我们在前面提到，HDC 内部包含了很多属性：当前画笔、画刷、字体、背景色、文本颜色、绘图模式等等。所有这些属性构成了 DC 的当前"状态"。当你用 SelectObject 选入一个新画笔时，DC 的状态就改变了。后续的绘图操作都会使用新画笔。

问题来了：如果你临时改了一下 DC 状态，画完东西后想恢复原来的状态，该怎么办？一种方法是记住原来的值，然后手动恢复。但这很麻烦，因为 DC 状态包含很多属性，你要全部记住并恢复很不现实。

Windows 提供了一个更好的解决方案：SaveDC 和 RestoreDC。

### SaveDC/RestoreDC 的使用

SaveDC 会把当前 DC 的状态保存到一个内部栈里，RestoreDC 则从栈里恢复之前保存的状态。这样你就可以随时"备份"当前状态，折腾完之后一键恢复。

```cpp
// 保存当前状态
int savedState = SaveDC(hdc);

// 修改 DC 状态
HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
SetTextColor(hdc, RGB(255, 255, 255));
SetBkColor(hdc, RGB(0, 0, 0));

// 绘图操作
Rectangle(hdc, 10, 10, 100, 100);

// 恢复之前保存的状态
RestoreDC(hdc, savedState);

// 现在 DC 状态已经恢复，不需要手动恢复原来的画笔、颜色等
DeleteObject(hPen);  // 但别忘了删除创建的对象
```

SaveDC 返回一个整数，标识这次保存的状态。你可以把它传给 RestoreDC 来恢复这个特定的状态。RestoreDC 有两种调用方式：传入正数恢复到指定的保存点，传入负数恢复相对位置的状态。传入 -1 表示恢复到最近一次保存的状态，最常用。

⚠️ 注意

SaveDC 和 RestoreDC 必须在同一 DC 上配对调用。你不能在一个 DC 上 SaveDC，然后在另一个 DC 上 RestoreDC。这会导致状态栈混乱，可能引发未定义行为。

### 状态栈的深度限制

SaveDC 使用栈来保存状态，那栈的深度有限制吗？有的，但很大。Windows 保证至少支持 16 层嵌套，实际实现通常支持更多。对于大多数应用程序来说，这个深度绰绰有余。但如果你写的是深度递归的绘图代码，还是要注意不要过度嵌套。

一个常见的模式是在函数入口 SaveDC，出口 RestoreDC，这样函数内部可以随意修改 DC 状态，不用担心影响调用者。这种模式在复杂的绘图代码中非常有用，可以避免手动管理状态的麻烦。

---

## 第四步——GDI 对象生命周期

### SelectObject 的返回值

SelectObject 是 GDI 编程中最常用的函数之一，但很多新手忽略了它的返回值。SelectObject 选入一个新对象时，会返回之前选入的同类型对象。这个返回值非常重要，你必须保存它，以便后续恢复。

```cpp
HPEN hPenRed = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPenRed);

// 使用红色画笔绘图
Ellipse(hdc, 10, 10, 100, 100);

// 恢复原来的画笔
SelectObject(hdc, hOldPen);

// 现在可以安全删除红色画笔了
DeleteObject(hPenRed);
```

为什么要这样麻烦？因为 GDI 对象在被选入 DC 时不能被删除。如果你选入了红色画笔，然后在没恢复的情况下调用 DeleteObject，删除操作会失败，画笔会泄漏。这是因为 DC 内部还持有这个对象的引用。

### DeleteObject 的调用时机

GDI 对象（画笔、画刷、字体、位图等）在创建后必须手动删除，否则会泄漏。但删除的前提是对象没有被任何 DC 选入。正确的删除时机是：先从所有 DC 中选出对象，然后删除。

```cpp
// 创建对象
HPEN hPen = CreatePen(PS_DASH, 1, RGB(128, 128, 128));
HBRUSH hBrush = CreateSolidBrush(RGB(0, 128, 255));

// 选入 DC
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

// 绘图
Rectangle(hdc, 10, 10, 200, 100);

// 恢复原始对象
SelectObject(hdc, hOldBrush);
SelectObject(hdc, hOldPen);

// 现在可以安全删除
DeleteObject(hBrush);
DeleteObject(hPen);
```

对于内存 DC，还有一个额外步骤：在删除 DC 前，必须先选出你选入的位图。内存 DC 创建时自带一个 1×1 的单色位图，你在删除 DC 前必须恢复这个原始位图。

```cpp
HDC hdcMem = CreateCompatibleDC(hdc);
HBITMAP hbmMem = CreateCompatibleBitmap(hdc, 400, 300);
HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

// ... 使用内存 DC ...

// 恢复原始位图
SelectObject(hdcMem, hbmOld);

// 先删除位图
DeleteObject(hbmMem);

// 再删除 DC
DeleteDC(hdcMem);
```

### 常见资源泄漏陷阱

GDI 资源泄漏是 Win32 程序中最隐蔽也最危险的 bug 之一。Windows 对每个进程有 GDI 对象数量限制（大约 10,000 个），泄漏的对象会累积，直到达到上限，然后你的程序再也无法创建任何 GDI 对象，绘图功能全面崩溃。

最常见的问题是忘记调用 DeleteObject。每次 CreatePen、CreateBrush、CreateFont 都要配对 DeleteObject，没例外。另一个常见问题是 SelectObject 后忘记恢复就 DeleteObject，导致删除失败。

还有一种更隐蔽的泄漏：在错误处理路径上忘记清理。如果你的函数有多个返回点，每个返回点都要确保释放已创建的对象。使用 RAII 风格的包装类可以大大减少这类问题。

```cpp
// RAII 风格的 GDI 对象包装
class GDIObjectGuard {
public:
    GDIObjectGuard(HDC hdc, HGDIOBJ obj) : m_hdc(hdc), m_obj(obj) {}
    ~GDIObjectGuard() { if (m_obj) DeleteObject(m_obj); }
    operator HGDIOBJ() { return m_obj; }
private:
    HDC m_hdc;
    HGDIOBJ m_obj;
};

// 使用示例
void DrawSomething(HDC hdc) {
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    GDIObjectGuard penGuard(hdc, hPen);  // 自动管理生命周期

    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    // ... 绘图操作 ...
    SelectObject(hdc, hOldPen);

    // 函数退出时自动调用 DeleteObject
}
```

---

## 第五步——完整示例：一个简单的绘图程序

我们来看一个完整的示例，把今天讲的知识都用上。这个程序会在窗口上画一些图形，展示基本的 GDI 绘图操作。

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"GDI Drawing Window";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"GDI Drawing Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 保存 DC 状态
        int savedDC = SaveDC(hdc);

        // 创建并选入红色画笔
        HPEN hPenRed = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenRed);

        // 创建并选入蓝色画刷
        HBRUSH hBrushBlue = CreateSolidBrush(RGB(0, 0, 255));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrushBlue);

        // 画矩形
        Rectangle(hdc, 50, 50, 250, 150);

        // 画椭圆
        HBRUSH hBrushGreen = CreateSolidBrush(RGB(0, 255, 0));
        SelectObject(hdc, hBrushGreen);
        Ellipse(hdc, 300, 50, 500, 150);
        DeleteObject(hBrushGreen);

        // 画文字
        SetTextColor(hdc, RGB(128, 0, 128));
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, 50, 200, L"Hello GDI!", 10);

        // 恢复 DC 状态
        RestoreDC(hdc, savedDC);

        // 删除创建的对象（恢复后已安全）
        DeleteObject(hBrushBlue);
        DeleteObject(hPenRed);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 响应鼠标点击，触发重绘
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

编译运行这个程序，你会看到一个窗口，里面画着一个蓝色填充的矩形、一个绿色填充的椭圆，还有一行紫色的文字。每次你在窗口上点击鼠标左键，窗口会重新绘制。

这个示例展示了几个关键点：在 WM_PAINT 中使用 BeginPaint/EndPaint、正确保存和恢复 DC 状态、创建和删除 GDI 对象、使用 InvalidateRect 触发重绘。这些都是 GDI 编程的基础模式。

---

## 第六步——调试技巧：如何检测 GDI 泄漏

GDI 泄漏很难发现，因为初期没有任何症状，直到对象数量累积到临界值才会爆发。我们需要一些工具和技巧来及早发现问题。

### 任务监控器

最简单的检测工具是 Windows 任务监控器。打开任务监控器，在"详细信息"选项卡右键列标题，选择"选择列"，勾选"GDI 对象"。现在你可以看到每个进程的 GDI 对象数量。

如果你的程序的 GDI 对象数量持续增长，且不随操作回落，那很可能就是泄漏了。正常情况下，GDI 对象数量应该在一定范围内波动，不会无限增长。

### GDI 对象计数

在代码里，你可以用 GetGuiResources 函数查询当前进程的 GDI 对象数量：

```cpp
DWORD gdiCount = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
DWORD userCount = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);

wchar_t msg[256];
swprintf_s(msg, L"GDI Objects: %u\nUser Objects: %u", gdiCount, userCount);
OutputDebugString(msg);
```

在关键操作前后调用这个函数，可以看到对象数量的变化。如果你创建了一个对象，数量应该加 1；删除后应该减 1。如果只增不减，就是泄漏了。

### Application Verifier

对于更复杂的场景，可以使用 Windows Application Verifier。这是一个微软提供的调试工具，可以检测各种资源泄漏，包括 GDI 对象。启用 GDI 泄漏检测后，Application Verifier 会在程序退出时报告所有未释放的 GDI 对象及其分配调用栈。

### 代码审查

除了工具，代码审查也很重要。几个常见检查点：每次 Create 都有对应的 Delete、每个 SelectObject 的返回值都保存并恢复、BeginPaint/EndPaint 和 GetDC/ReleaseDC 正确配对、内存 DC 的位图正确恢复和删除。

---

## 后续可以做什么

到这里，GDI 设备上下文的基础知识就讲完了。你现在应该能够理解 HDC 的本质，知道在什么场景下用什么方式获取 HDC，掌握状态管理和 GDI 对象生命周期的基本规则。但 GDI 的世界远不止这些，还有很多高级主题等着我们去探索。

下一篇文章，我们会继续深入 **GDI 绘图 API**——学习各种绘图函数的具体用法，包括画线、画矩形、画椭圆、画多边形，以及文本输出和位图操作。我们还会介绍双缓冲技术，这是实现平滑动画的关键。

在此之前，建议你先把今天的内容消化一下。写一些小练习，巩固一下知识：

1. 修改示例程序，让它在不同的位置绘制不同的图形
2. 实现一个简单的绘图程序，让用户用鼠标在窗口上画图
3. 使用双缓冲技术消除重绘时的闪烁
4. 用任务监控器观察你的程序的 GDI 对象数量，确保没有泄漏

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。特别是最后一个，养成监控 GDI 对象数量的习惯，能帮你避免很多难以排查的问题。

好了，今天的文章就到这里，我们下一篇再见！

---

## 相关资源

- [Device Contexts - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/device-contexts)
- [Saving, Restoring, and Resetting a Device Context - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/saving--restoring--and-resetting-a-device-context)
- [Windows GDI - Win32 apps | Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/gdi/windows-gdi)
- [GDI Objects - Win32 apps | Microsoft Learn](https://learn.microsoft.com/fi-fi/windows/win32/sysinfo/gdi-objects)
- [BeginPaint function (Winuser.h) - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-beginpaint)
- [GetDC function (Winuser.h) - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdc)
- [SelectObject function (wingdi.h) - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectobject)
