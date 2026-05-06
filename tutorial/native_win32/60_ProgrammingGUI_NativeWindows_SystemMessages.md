# 通用GUI编程技术——Win32 原生编程实战（六十）——常用系统消息补充：滚轮、命中测试、窗口限制与背景擦除

> 上一篇文章我们深入了消息机制的底层——SendMessage 和 PostMessage 的区别、跨线程通信模式、消息队列优先级。有了这些知识，你已经可以在多线程环境下安全地操作 UI 了。但 Win32 的消息世界里还有一些"高频但容易被忽略"的消息，它们在你做实际项目的时候几乎一定会遇到：鼠标滚轮、无边框窗口拖拽、限制窗口最小尺寸、消除闪烁。今天我们就来把这些常用的系统消息一次性讲清楚。

---

## 为什么要补充这些系统消息

前面讲系统消息的那篇文章（第二篇）覆盖了 WM_KEYDOWN、WM_CHAR、WM_LBUTTONDOWN、WM_MOUSEMOVE、WM_PAINT 等基础消息。但有些消息在那篇文章里被刻意跳过了，因为它们要么需要更多前置知识（比如 WM_NCHITTEST 需要理解窗口非客户区的概念），要么在简单示例里用不到（比如 WM_GETMINMAXINFO 只在你需要限制窗口大小时才有用）。

然而，一旦你开始做"像样的"桌面程序，这些消息就变成了必须掌握的东西。举个例子：

* **WM_MOUSEWHEEL**：你的列表不支持滚轮滚动？用户会觉得你的程序是上个世纪的。
* **WM_NCHITTEST**：想做一个无边框窗口但还要能拖动？必须处理这个消息。
* **WM_GETMINMAXINFO**：你的窗口能被缩到一个像素？用户体验灾难。
* **WM_ERASEBKGND**：你的窗口每帧都在闪烁？多半是没处理好这个消息。

这篇文章会用简洁实用的方式，把这四个消息以及它们相关的内容讲透。每个消息都会给出完整的代码示例，你可以直接复制到自己的项目中。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道消息循环怎么写、窗口过程怎么处理消息、基本的 WM_PAINT 绘制。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——WM_MOUSEWHEEL：鼠标滚轮

### 消息参数

```cpp
case WM_MOUSEWHEEL:
{
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);  // 滚动距离
    WORD keys = GET_KEYSTATE_WPARAM(wParam);      // 键盘状态
    POINTS pt = MAKEPOINTS(lParam);                // 鼠标屏幕坐标
    // ...
}
```

| 参数 | 含义 |
|------|------|
| `delta` | 滚轮滚动的距离。正值表示向前滚（远离用户），负值表示向后滚。一个"咔嗒"通常是 120（WHEEL_DELTA） |
| `keys` | 键盘状态标志：MK_CONTROL、MK_SHIFT、MK_LBUTTON 等 |
| `pt` | 鼠标的**屏幕坐标**（注意不是客户区坐标！） |

### 关键细节：WHEEL_DELTA

Windows 定义 `WHEEL_DELTA = 120` 作为一次"标准滚轮咔嗒"的单位。为什么是 120 而不是 1？因为有些高精度鼠标（比如罗技的无极滚轮）可以在一次滚动中生成小于 120 的 delta 值。你应该累加 delta，当累加值达到 WHEEL_DELTA 时再执行一次滚动：

```cpp
static int g_wheelAccum = 0;

case WM_MOUSEWHEEL:
{
    g_wheelAccum += GET_WHEEL_DELTA_WPARAM(wParam);
    int linesToScroll = 0;

    while (g_wheelAccum >= WHEEL_DELTA)
    {
        g_wheelAccum -= WHEEL_DELTA;
        linesToScroll++;
    }
    while (g_wheelAccum <= -WHEEL_DELTA)
    {
        g_wheelAccum += WHEEL_DELTA;
        linesToScroll--;
    }

    // 用 linesToScroll 执行滚动
    if (linesToScroll > 0)
    {
        // 向上滚动 linesToScroll 行
    }
    else if (linesToScroll < 0)
    {
        // 向下滚动 -linesToScroll 行
    }

    return 0;
}
```

### 坐标转换

WM_MOUSEWHEEL 的鼠标坐标是屏幕坐标，而大多数绘图和命中测试使用客户区坐标。需要转换：

```cpp
case WM_MOUSEWHEEL:
{
    POINTS ptScreen = MAKEPOINTS(lParam);
    POINT ptClient = { ptScreen.x, ptScreen.y };
    ScreenToClient(hwnd, &ptClient);
    // 现在 ptClient 是客户区坐标
    // ...
}
```

⚠️ 注意

如果你的窗口有 DPI 缩放（Per-Monitor DPI），ScreenToClient 的结果可能需要额外处理。在高 DPI 场景下，建议使用 `WM_MOUSEWHEEL` 的坐标仅做判断"鼠标在不在我的窗口上"，具体滚动量根据控件自身逻辑处理。

### 系统滚动行数

用户可以在系统设置中调整"每次滚轮滚动的行数"。你可以通过 `SystemParametersInfo` 获取：

```cpp
UINT scrollLines = 3;  // 默认值
SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);

if (scrollLines == WHEEL_PAGESCROLL)
{
    // 用户设置为"一次滚动一页"
}
else
{
    // scrollLines 是每次咔嗒滚动的行数（通常是 3）
}
```

---

## 第二步——WM_NCHITTEST：命中测试与自定义拖拽区域

WM_NCHITTEST 是 Win32 中最灵活的消息之一。每当鼠标在你的窗口上移动、点击，或者有其他需要判断"鼠标在窗口的哪个部位"的操作时，系统都会发送这个消息。

### 消息参数

```cpp
case WM_NCHITTEST:
{
    POINTS pt = MAKEPOINTS(lParam);  // 屏幕坐标
    // 返回一个命中测试值，告诉系统鼠标在窗口的哪个部位
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

lParam 是鼠标的**屏幕坐标**（不是客户区坐标）。

### 返回值含义

返回值告诉系统"鼠标当前在窗口的哪个区域"，系统据此决定后续行为：

| 返回值 | 含义 | 系统行为 |
|--------|------|---------|
| HTCLIENT | 客户区 | 发送 WM_MOUSEMOVE 等客户区消息 |
| HTCAPTION | 标题栏 | 允许拖动窗口 |
| HTSYSMENU | 系统菜单 | 点击弹出系统菜单 |
| HTMINBUTTON / HTMAXBUTTON | 最小化/最大化按钮 | 对应的按钮操作 |
| HTCLOSE | 关闭按钮 | 关闭窗口 |
| HTLEFT / HTRIGHT / HTTOP / HTBOTTOM | 边缘 | 对应方向的缩放 |
| HTTOPLEFT / HTTOPRIGHT / HTBOTTOMLEFT / HTBOTTOMRIGHT | 角落 | 对角线缩放 |
| HTNOWHERE | 不在任何区域 | 忽略 |
| HTTRANSPARENT | 透明 | 传递给下层窗口 |

### 实战：无边框窗口可拖拽

一个常见的场景：你创建了一个无边框窗口（没有标题栏），但仍然希望用户能拖动它。方法就是把客户区的某个区域（比如顶部 40 像素）"伪装"成标题栏：

```cpp
LRESULT HitTestNCA(HWND hwnd, int x, int y)
{
    // 获取窗口矩形
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);

    // 获取默认的命中测试结果
    LRESULT hit = DefWindowProc(hwnd, WM_NCHITTEST, 0, MAKELPARAM(x, y));

    // 如果鼠标在客户区，检查是否在"自定义标题栏"区域
    if (hit == HTCLIENT)
    {
        int borderWidth = GetSystemMetrics(SM_CXSIZEFRAME);
        int relativeY = y - rcWindow.top;

        // 顶部 40 像素视为标题栏（可拖动）
        if (relativeY < 40)
        {
            return HTCAPTION;
        }
    }

    // 允许边框缩放
    return hit;
}

case WM_NCHITTEST:
{
    POINTS pt = MAKEPOINTS(lParam);
    return HitTestNCA(hwnd, pt.x, pt.y);
}
```

### 实战：可缩放的无边框窗口

如果你的窗口没有 `WS_THICKFRAME` 样式，系统默认不支持边框缩放。但你可以通过 WM_NCHITTEST 手动实现：

```cpp
LRESULT HitTestResizable(HWND hwnd, int x, int y)
{
    const int BORDER_WIDTH = 6;  // 缩放手柄宽度

    RECT rc;
    GetWindowRect(hwnd, &rc);

    // 先检查是否在边框区域
    bool onLeft   = x >= rc.left && x < rc.left + BORDER_WIDTH;
    bool onRight  = x < rc.right && x >= rc.right - BORDER_WIDTH;
    bool onTop    = y >= rc.top && y < rc.top + BORDER_WIDTH;
    bool onBottom = y < rc.bottom && y >= rc.bottom - BORDER_WIDTH;

    if (onTop && onLeft)    return HTTOPLEFT;
    if (onTop && onRight)   return HTTOPRIGHT;
    if (onBottom && onLeft) return HTBOTTOMLEFT;
    if (onBottom && onRight) return HTBOTTOMRIGHT;
    if (onTop)    return HTTOP;
    if (onBottom) return HTBOTTOM;
    if (onLeft)   return HTLEFT;
    if (onRight)  return HTRIGHT;

    // 检查是否在自定义标题栏区域
    if (y - rc.top < 40)
        return HTCAPTION;

    return HTCLIENT;
}
```

⚠️ 注意

**DWM 缩放帧**：在现代 Windows 上，DWM（Desktop Window Manager）可能会给你的窗口添加不可见的边框用于窗口阴影和缩放。如果你发现命中测试不准确，可能需要调用 `DwmGetWindowAttribute` 获取 DWM 的扩展帧区域并调整你的计算。

---

## 第三步——WM_GETMINMAXINFO：限制窗口大小

### 什么时候会收到这个消息

每当窗口的大小即将改变时（用户拖动边框、最大化、调用 SetWindowPos 等），系统会发送 WM_GETMINMAXINFO，让你有机会限制窗口的最大/最小尺寸以及最大化时的位置。

### 消息参数

```cpp
case WM_GETMINMAXINFO:
{
    MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
    // 修改 pmmi 的字段来限制窗口
    return 0;
}
```

### MINMAXINFO 结构

```cpp
typedef struct tagMINMAXINFO {
    POINT ptReserved;     // 保留，不要修改
    POINT ptMaxSize;      // 最大化时的尺寸
    POINT ptMaxPosition;  // 最大化时的位置
    POINT ptMinTrackSize; // 最小追踪尺寸（拖动缩放时的最小值）
    POINT ptMaxTrackSize; // 最大追踪尺寸（拖动缩放时的最大值）
} MINMAXINFO;
```

### 实战：设置窗口最小尺寸

```cpp
case WM_GETMINMAXINFO:
{
    MINMAXINFO* pmmi = (MINMAXINFO*)lParam;

    // 设置窗口最小宽度和高度
    pmmi->ptMinTrackSize.x = 640;
    pmmi->ptMinTrackSize.y = 480;

    // 可选：设置窗口最大尺寸
    // pmmi->ptMaxTrackSize.x = 1920;
    // pmmi->ptMaxTrackSize.y = 1080;

    return 0;
}
```

只需要设置你想限制的字段，不需要设置所有字段。上面的代码只限制了最小尺寸，用户仍然可以最大化窗口或拖到任意大的尺寸。

### 结合 DPI 缩放

如果你的程序需要支持 DPI 缩放（前面的文章已经讲过），最小尺寸也应该根据 DPI 调整：

```cpp
case WM_GETMINMAXINFO:
{
    MINMAXINFO* pmmi = (MINMAXINFO*)lParam;

    // 获取当前 DPI
    UINT dpi = GetDpiForWindow(hwnd);

    // 基准最小尺寸（以 96 DPI 为基准）
    int minWidth = MulDiv(640, dpi, 96);
    int minHeight = MulDiv(480, dpi, 96);

    pmmi->ptMinTrackSize.x = minWidth;
    pmmi->ptMinTrackSize.y = minHeight;

    return 0;
}
```

---

## 第四步——WM_ERASEBKGND：消除闪烁

### 为什么会闪烁

当窗口需要重绘时，系统默认会先用窗口背景色（注册窗口类时指定的 `hbrBackground`）擦除整个客户区，然后再发送 WM_PAINT 让你绘制内容。这两步操作是分开的——先擦除（白色背景闪一下），再绘制（你的内容显示出来）。如果重绘频率高（比如动画），用户就会看到明显的闪烁。

### WM_ERASEBKGND 消息

系统在擦除背景时会发送 WM_ERASEBKGND：

```cpp
case WM_ERASEBKGND:
{
    HDC hdc = (HDC)wParam;
    // 如果你自己处理了背景擦除，返回 TRUE
    // 如果返回 FALSE，系统不会帮你擦除
    return TRUE;
}
```

### 消除闪烁的标准方法

**方法一：拦截 WM_ERASEBKGND**

最简单的方法就是不让系统擦除背景，自己在 WM_PAINT 中处理一切：

```cpp
// 注册窗口类时，不设置背景刷
wc.hbrBackground = NULL;

// 在窗口过程中
case WM_ERASEBKGND:
    return TRUE;  // 告诉系统"我已经擦除了"（实际什么都没做）

case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 先用背景色填充整个客户区
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    FillRect(hdc, &rcClient, (HBRUSH)(COLOR_WINDOW + 1));

    // 然后绘制你的内容
    // ...

    EndPaint(hwnd, &ps);
    return 0;
}
```

**方法二：双缓冲**

更彻底的方法是使用双缓冲——先在一个离屏 DC 上画好所有内容，然后一次性拷贝到屏幕。前面的 GDI 篇已经详细讲过了，这里就不重复了。

### InvalidateRect 与擦除

注意 `InvalidateRect` 的第三个参数 `bErase`：

```cpp
InvalidateRect(hwnd, &rect, TRUE);   // bErase=TRUE：重绘前发送 WM_ERASEBKGND
InvalidateRect(hwnd, &rect, FALSE);  // bErase=FALSE：不发送 WM_ERASEBKGND
```

如果你已经在用双缓冲了，应该传 `FALSE` 避免多余的擦除操作。

---

## 第五步——完整示例：无边框可拖拽窗口

这个示例把今天讲的几个消息全部串起来：一个无边框窗口，可以拖动、缩放，有最小尺寸限制，支持滚轮缩放内容。

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HWND g_hWnd = NULL;
int  g_scale = 100;     // 缩放百分比

// 自定义命中测试
LRESULT CustomHitTest(HWND hwnd, int x, int y)
{
    const int BORDER = 6;
    const int CAPTION_HEIGHT = 40;

    RECT rc;
    GetWindowRect(hwnd, &rc);

    bool onLeft   = (x >= rc.left && x < rc.left + BORDER);
    bool onRight  = (x < rc.right && x >= rc.right - BORDER);
    bool onTop    = (y >= rc.top && y < rc.top + BORDER);
    bool onBottom = (y < rc.bottom && y >= rc.bottom - BORDER);

    if (onTop    && onLeft)  return HTTOPLEFT;
    if (onTop    && onRight) return HTTOPRIGHT;
    if (onBottom && onLeft)  return HTBOTTOMLEFT;
    if (onBottom && onRight) return HTBOTTOMRIGHT;
    if (onTop)    return HTTOP;
    if (onBottom) return HTBOTTOM;
    if (onLeft)   return HTLEFT;
    if (onRight)  return HTRIGHT;

    // 自定义标题栏区域
    if (y - rc.top < CAPTION_HEIGHT && !onLeft && !onRight)
        return HTCAPTION;

    return HTCLIENT;
}

void PaintContent(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    // 绘制标题栏背景
    RECT rcCaption = { 0, 0, rc.right, 40 };
    HBRUSH hBrushCaption = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rcCaption, hBrushCaption);
    DeleteObject(hBrushCaption);

    // 绘制标题文字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    wchar_t title[64];
    swprintf_s(title, L"无边框窗口示例 - 缩放 %d%%", g_scale);
    TextOut(hdc, 12, 10, title, (int)wcslen(title));

    // 绘制关闭按钮区域
    RECT rcClose = { rc.right - 46, 0, rc.right, 40 };
    HBRUSH hBrushClose = CreateSolidBrush(RGB(232, 17, 35));
    FillRect(hdc, &rcClose, hBrushClose);
    DeleteObject(hBrushClose);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, rc.right - 32, 10, L"X", 1);

    // 绘制内容区域
    RECT rcContent = { 0, 40, rc.right, rc.bottom };
    HBRUSH hBrushContent = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(hdc, &rcContent, hBrushContent);
    DeleteObject(hBrushContent);

    // 绘制示例内容（一个随缩放变化的圆）
    int radius = MulDiv(80, g_scale, 100);
    int cx = rc.right / 2;
    int cy = 40 + (rc.bottom - 40) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 212));
    HBRUSH hBrushCircle = CreateSolidBrush(RGB(0, 120, 212));
    SelectObject(hdc, hPen);
    SelectObject(hdc, hBrushCircle);

    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);

    DeleteObject(hPen);
    DeleteObject(hBrushCircle);

    // 提示文字
    SetTextColor(hdc, RGB(80, 80, 80));
    const wchar_t* hint = L"鼠标滚轮缩放 | 拖动顶部移动 | 拖动边缘缩放";
    TextOut(hdc, (rc.right - (int)wcslen(hint) * 8) / 2, rc.bottom - 30,
            hint, (int)wcslen(hint));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NCHITTEST:
        return CustomHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
        pmmi->ptMinTrackSize.x = 400;
        pmmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintContent(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int step = delta / WHEEL_DELTA;

        g_scale += step * 10;
        if (g_scale < 20)  g_scale = 20;
        if (g_scale > 500) g_scale = 500;

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 检查是否点击了关闭按钮区域
        POINTS pt = MAKEPOINTS(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (pt.y < 40 && pt.x >= rc.right - 46)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BorderlessWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"BorderlessWindow", L"",
        WS_POPUP | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWnd)
    {
        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

### 代码要点解析

1. **WS_POPUP | WS_THICKFRAME**：这个组合创建了一个没有标准标题栏但有缩放边框的窗口。系统会为 WS_THICKFRAME 保留一个不可见的边框区域用于 Aero Snap 等功能。

2. **CustomHitTest**：把窗口顶部 40 像素映射为 HTCAPTION（可拖动），四边 6 像素映射为边框缩放区域。其余部分是 HTCLIENT。

3. **WM_GETMINMAXINFO**：限制窗口最小为 400x300，防止缩到看不到内容。

4. **WM_ERASEBKGND 返回 TRUE**：阻止系统自动擦除背景，所有绘制都在 WM_PAINT 中完成。

5. **WM_MOUSEWHEEL**：滚轮控制缩放比例，修改 g_scale 后调用 InvalidateRect 触发重绘。

---

## 补充：Raw Input 简介

如果你需要处理高精度输入设备（比如游戏手柄、数位板、自定义 HID 设备），标准的 WM_KEYDOWN 和 WM_MOUSEMOVE 可能不够用。Windows 提供了 Raw Input API 来直接读取硬件数据。

### 注册 Raw Input 设备

```cpp
RAWINPUTDEVICE rid = {};
rid.usUsagePage = 0x01;  // 通用桌面控制
rid.usUsage = 0x02;      // 鼠标
rid.dwFlags = 0;          // 不设置任何标志
rid.hwndTarget = hwnd;    // 接收输入的窗口

RegisterRawInputDevices(&rid, 1, sizeof(rid));
```

### 处理 WM_INPUT

```cpp
case WM_INPUT:
{
    UINT size = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

    RAWINPUT* pRaw = (RAWINPUT*)malloc(size);
    if (pRaw)
    {
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRaw, &size, sizeof(RAWINPUTHEADER));

        if (pRaw->header.dwType == RIM_TYPEMOUSE)
        {
            // 高精度鼠标移动量（不受指针加速影响）
            LONG dx = pRaw->data.mouse.lLastX;
            LONG dy = pRaw->data.mouse.lLastY;
            // 使用 dx, dy...
        }

        free(pRaw);
    }

    DefWindowProc(hwnd, uMsg, wParam, lParam);
    return 0;
}
```

Raw Input 主要用于游戏和高精度绘图软件。对于普通桌面应用，标准的 WM_MOUSEMOVE 和 WM_KEYDOWN 就足够了。

---

## 后续可以做什么

到这里，常用的系统消息就补充完了。你现在应该能正确处理鼠标滚轮、自定义窗口命中测试（包括无边框窗口拖拽）、限制窗口大小、消除背景闪烁，以及对 Raw Input 有一个基本的了解。

接下来的几篇文章，我们会进入 **Win32 进阶专题**——子类化、Hook 机制、系统托盘、拖放、定时器。这些话题综合运用了前面所有的基础知识，是你从"会写 Win32 程序"到"能写专业 Win32 程序"的关键一步。

在此之前，建议你做一些练习巩固今天的知识：

1. **基础练习**：修改上面的无边框窗口示例，在标题栏上添加最小化和最大化按钮区域
2. **进阶练习**：实现一个窗口，鼠标滚轮控制背景颜色在色相环上旋转（提示：使用 HSV → RGB 转换）
3. **挑战练习**：用 Raw Input API 实现一个简单的鼠标轨迹记录器，记录鼠标的精确移动路径并绘制到窗口上

---

## 相关资源

- [WM_MOUSEWHEEL message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel)
- [WM_NCHITTEST message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [WM_GETMINMAXINFO message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-getminmaxinfo)
- [WM_ERASEBKGND message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-erasebkgnd)
- [Raw Input - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/raw-input)
- [MINMAXINFO structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-minmaxinfo)
