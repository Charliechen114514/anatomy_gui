# 通用GUI编程技术——Win32 原生编程实战（十三）——图标、光标、位图资源

> 之前我们聊过对话框的基本使用，但你可能已经发现了一个问题：我们的程序窗口看起来还是有点"素"。任务栏上的图标还是默认的，鼠标指针也一直是系统默认样式，想加个背景图片或者 logo 更是无从下手。这些问题都指向同一个主题——Win32 资源。今天我们要深入的就是其中最直观的部分：图标、光标和位图资源。

---

## 为什么要专门聊这个"面子工程"

说实话，我在刚开始学 Win32 的时候，对资源这件事其实有点不以为然。那时候觉得功能最重要，界面好看难看有什么关系？结果当你真正把自己写的小程序分享给别人用的时候，才发现这个问题有多尴尬。用户看到任务栏上一堆默认图标的应用，第一反应往往是"这是什么来路不明的东西"？更别提你想给公司做产品的时候，一个没有图标的程序基本上就是"业余"的代名词。

但资源的意义远不止"好看"这么简单。图标是用户识别程序的第一视觉元素，一个好的图标设计能让用户在众多程序中一眼就找到你。自定义光标可以给用户明确的反馈——比如加载的时候显示沙漏，调整大小时显示双向箭头。位图资源则是实现各种自定义视觉效果的基础，从启动画面到按钮图片，都离不开它。

另一个现实原因是：现代框架把这些东西封装得太好了，你可能只需要在项目属性里设置一下图标路径就行了。但一旦你需要用纯 Win32 写程序，或者需要做一些底层优化（比如运行时动态切换图标、创建自定义光标），这些知识就会成为你的短板。而且，理解了 Win32 的资源加载机制之后，你会对所有框架的底层实现有更清晰的认识。

这篇文章会带着你从零开始，把 Win32 的图形资源彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows Vista+ 都支持）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的窗口、资源文件是怎么回事、怎么编译一个 Win32 程序。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——理解图标资源的用途

图标是 Windows 程序最直观的标识。但你可能不知道的是，一个程序实际上可以关联多个图标，它们在不同场景下使用。

### 图标的尺寸规格

Windows 使用不同尺寸的图标来适应不同的显示场景：

| 尺寸 | 用途 | 何时显示 |
|------|------|----------|
| 16×16 | 小图标 | 任务栏、窗口标题栏、资源管理器列表视图 |
| 32×32 | 标准图标 | 桌面快捷方式、资源管理器图标视图 |
| 48×48 | 中图标 | 控制面板、一些对话框 |
| 256×256 | 大图标 | Windows Vista+ 的资源管理器超大图标视图 |

一个专业的图标文件（.ico）实际上是一个容器，里面包含了同一图标的多个尺寸版本。Windows 会根据当前的显示 DPI 和视图模式自动选择合适的尺寸。

⚠️ 注意

千万别以为用一个尺寸的图标就行。如果你的图标文件只包含 32×32 的图像，Windows 在显示小图标时会把它缩放到 16×16，结果往往会模糊难看。专业的做法是准备多个尺寸的源图，然后用工具生成包含所有尺寸的 .ico 文件。

### 程序图标 vs 窗口图标

这里有一个容易混淆的概念：程序图标和窗口图标的区别。

**程序图标**是存储在可执行文件资源中的图标，当资源管理器显示你的文件时，或者在开始菜单里看到你的程序时，显示的就是这个图标。程序图标在编译时就确定了，是文件的一部分。

**窗口图标**是运行时关联到特定窗口的图标，显示在窗口标题栏左上角和任务栏上。一个程序可以有多个窗口，每个窗口可以有不同的图标。比如主窗口用公司 logo，子窗口用不同的图标。

程序图标通过资源文件定义，窗口图标通过 `WM_SETICON` 消息或窗口类的 `hIcon` 字段设置。

---

## 第二步——在资源文件中定义图标

定义图标资源非常简单，在 .rc 文件中添加一行就行：

```rc
#include <windows.h>
#include "resource.h"

// 主程序图标
IDI_APPICON          ICON    "res\\app.ico"

// 其他图标资源
IDI_DOCUMENT         ICON    "res\\document.ico"
IDI_FOLDER           ICON    "res\\folder.ico"
IDI_WARNING          ICON    "res\\warning.ico"
```

`IDI_APPICON` 是图标资源的 ID，通常在 resource.h 中定义：

```cpp
#define IDI_APPICON          100
#define IDI_DOCUMENT         101
#define IDI_FOLDER           102
#define IDI_WARNING          103
```

图标文件路径可以是相对路径，也可以是绝对路径。Visual Studio 的资源编辑器会帮你管理这些文件，把它们编译到最终的可执行文件中。

### 使用 LoadIcon 加载图标

传统的 `LoadIcon` 函数只能加载标准尺寸的图标：

```cpp
// 从应用程序资源加载图标
HICON hIcon = LoadIcon(
    hInstance,              // 应用程序实例句柄
    MAKEINTRESOURCE(IDI_APPICON)  // 图标资源 ID
);

// 加载系统预定义图标
HICON hHandCursor = LoadIcon(NULL, IDI_HAND);
HICON hQuestionIcon = LoadIcon(NULL, IDI_QUESTION);
HICON hExclamationIcon = LoadIcon(NULL, IDI_EXCLAMATION);
```

`LoadIcon` 的第一个参数如果是 `NULL`，表示加载系统预定义图标，比如 `IDI_HAND`（手形警告图标）、`IDI_QUESTION`（问号图标）等。

但 `LoadIcon` 有个限制：它只能加载标准尺寸的资源图标。如果你想加载特定尺寸的图标，或者从文件加载，就需要用更强大的 `LoadImage` 函数。

---

## 第三步——使用 LoadImage 统一加载资源

`LoadImage` 是一个更通用的函数，它可以加载图标、光标和位图，而且支持指定尺寸：

```cpp
HANDLE LoadImage(
    HINSTANCE hInst,   // 应用程序实例句柄
    LPCSTR lpszName,   // 图像名称
    UINT uType,        // 图像类型
    int cxDesired,     // 期望宽度
    int cyDesired,     // 期望高度
    UINT fuLoad        // 加载选项
);
```

### 加载不同尺寸的图标

```cpp
// 加载 32×32 的大图标
HICON hIconLarge = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_APPICON),
    IMAGE_ICON,
    32, 32,           // 期望尺寸
    LR_SHARED         // 共享资源，不需要手动销毁
);

// 加载 16×16 的小图标
HICON hIconSmall = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_APPICON),
    IMAGE_ICON,
    16, 16,
    LR_SHARED
);
```

`LR_SHARED` 是一个重要的标志。如果设置了这个标志，系统会缓存加载的资源。多次加载同一个资源时，会返回同一个句柄，而且不需要手动调用 `DestroyIcon` 释放。对于标准图标和从资源加载的图像，通常都应该设置 `LR_SHARED`。

⚠️ 注意

千万别在没有设置 `LR_SHARED` 的情况下忘记释放资源。如果你用 `LoadImage` 加载了一个非共享的图标，用完之后必须调用 `DestroyIcon`，否则会内存泄漏。但对于设置了 `LR_SHARED` 的资源，绝对不要手动释放，系统会统一管理。

### 从文件加载图像

`LoadImage` 还支持从文件直接加载图像：

```cpp
// 从文件加载位图
HANDLE hBitmap = LoadImage(
    NULL,                   // 不使用资源
    L"res\\background.bmp", // 文件路径
    IMAGE_BITMAP,           // 类型是位图
    0, 0,                   // 使用实际尺寸
    LR_LOADFROMFILE | LR_CREATEDIBSECTION  // 从文件加载，创建 DIB Section
);

// 从文件加载图标
HICON hIconFromFile = (HICON)LoadImage(
    NULL,
    L"res\\custom.ico",
    IMAGE_ICON,
    64, 64,                // 可以指定加载后缩放到特定尺寸
    LR_LOADFROMFILE
);
```

`LR_LOADFROMFILE` 标志告诉 `LoadImage` 从文件系统加载，而不是从应用程序资源。这对于动态更换皮肤或者加载用户自定义图片的场景非常有用。

---

## 第四步——设置窗口图标

设置窗口图标有两个时机：创建窗口时通过窗口类设置，或者运行时通过 `WM_SETICON` 消息设置。

### 在窗口类中设置默认图标

注册窗口类时设置 `hIcon` 和 `hIconSm` 字段：

```cpp
WNDCLASSEX wcex = {};
wcex.cbSize = sizeof(WNDCLASSEX);
wcex.style = CS_HREDRAW | CS_VREDRAW;
wcex.lpfnWndProc = WndProc;
wcex.hInstance = hInstance;

// 加载大图标和小图标
wcex.hIcon = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_APPICON),
    IMAGE_ICON,
    32, 32,
    LR_SHARED
);

wcex.hIconSm = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_APPICON),
    IMAGE_ICON,
    16, 16,
    LR_SHARED
);

wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
wcex.lpszClassName = L"MYWNDCLASS";

if (!RegisterClassEx(&wcex))
{
    return FALSE;
}
```

`hIcon` 是大图标，`hIconSm` 是小图标。如果只设置了 `hIcon` 而不设置 `hIconSm`，系统会尝试把大图标缩放到小尺寸。但缩放效果往往不如专门设计的小图标，所以最好两个都设置。

### 运行时动态更改图标

有时候你需要在运行时更改窗口图标，比如根据程序状态显示不同的图标：

```cpp
case WM_SETICON:
{
    // wParam 是图标类型：ICON_BIG 或 ICON_SMALL
    // lParam 是新图标的句柄

    // 让系统处理默认行为
    return DefWindowProc(hwnd, WM_SETICON, wParam, lParam);
}

// 在其他地方主动设置图标
void SetWindowIconState(HWND hwnd, BOOL isActive)
{
    HICON hIcon = (HICON)LoadImage(
        GetModuleHandle(NULL),
        isActive ? MAKEINTRESOURCE(IDI_APPICON_ACTIVE) : MAKEINTRESOURCE(IDI_APPICON_INACTIVE),
        IMAGE_ICON,
        32, 32,
        LR_SHARED
    );

    // 设置大图标（窗口标题栏、Alt+Tab）
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // 设置小图标（任务栏、窗口标题栏小图）
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}
```

`WM_SETICON` 消息的 `wParam` 可以是 `ICON_BIG` 或 `ICON_SMALL`，分别表示设置大图标还是小图标。`ICON_BIG` 影响的是 Alt+Tab 对话框和窗口标题栏的大图标，`ICON_SMALL` 影响的是任务栏图标和标题栏小图。

---

## 第五步——创建和使用光标资源

光标（鼠标指针）是用户交互的重要反馈机制。Win32 提供了丰富的预定义光标，也支持自定义光标。

### 在资源文件中定义光标

```rc
// 自定义光标资源
IDC_CURSOR_CROSS    CURSOR   "res\\cross.cur"
IDC_CURSOR_HAND     CURSOR   "res\\hand.cur"
IDC_CURSOR_WAIT     CURSOR   "res\\wait.cur"
```

光标文件（.cur）和图标文件类似，但有一些特殊要求：
- 光标必须指定"热点"（Hot Spot）位置——这是光标中实际点击生效的点
- 标准箭头光标的热点在箭头尖端
- 十字光标的热点在中心
- 手指光标的热点在食指尖端

创建自定义光标通常需要专门的工具，比如 Visual Studio 的资源编辑器或者专门的图标/光标编辑器。

### 加载和设置光标

用 `LoadCursor` 或 `LoadImage` 加载光标：

```cpp
// 使用 LoadCursor（传统方式）
HCURSOR hCursor = LoadCursor(hInstance, MAKEINTRESOURCE(IDC_CURSOR_CROSS));

// 使用 LoadImage（更灵活）
HCURSOR hCursor = (HCURSOR)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDC_CURSOR_CROSS),
    IMAGE_CURSOR,
    0, 0,           // 光标使用系统默认尺寸
    LR_SHARED
);

// 加载系统预定义光标
HCURSOR hArrow = LoadCursor(NULL, IDC_ARROW);       // 标准箭头
HCURSOR hIBeam = LoadCursor(NULL, IDC_IBEAM);       // I 形文本光标
HCURSOR hWait = LoadCursor(NULL, IDC_WAIT);         // 沙漏等待光标
HCURSOR hCross = LoadCursor(NULL, IDC_CROSS);       // 十字光标
HCURSOR hHand = LoadCursor(NULL, IDC_HAND);         // 手形光标
```

### 窗口类中的默认光标

注册窗口类时设置 `hCursor` 字段：

```cpp
WNDCLASSEX wcex = {};
wcex.hCursor = LoadCursor(NULL, IDC_ARROW);  // 使用标准箭头
// 或者
wcex.hCursor = LoadCursor(hInstance, MAKEINTRESOURCE(IDC_CURSOR_CROSS));
```

当鼠标移到窗口上时，系统会自动显示窗口类中指定的光标。这是设置窗口默认光标的标准方法。

### WM_SETCURSOR 消息——动态更改光标

如果你想在不同的窗口区域显示不同的光标，就需要处理 `WM_SETCURSOR` 消息：

```cpp
case WM_SETCURSOR:
{
    // lParam 的低字是鼠标命中测试代码
    // lParam 的高字是鼠标消息标识符

    if (LOWORD(lParam) == HTCLIENT)  // 鼠标在客户区
    {
        // 检查鼠标是否在某个特定区域
        DWORD dwPos = GetMessagePos();
        POINT pt = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };
        ScreenToClient(hwnd, &pt);

        RECT rcResizeZone;
        SetRect(&rcResizeZone, 200, 0, 220, 100);  // 右侧调整大小区域

        if (PtInRect(&rcResizeZone, pt))
        {
            // 显示调整大小光标
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;  // 我们已经处理了，不让系统使用默认光标
        }

        // 否则使用默认光标
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }

    // 对于非客户区（边框、标题栏等），使用默认处理
    return DefWindowProc(hwnd, WM_SETCURSOR, wParam, lParam);
}
```

`WM_SETCURSOR` 在鼠标移动时频繁发送，所以处理这个消息时要尽量高效。如果返回 `TRUE`，表示你已经设置了光标；如果返回 `FALSE` 或调用 `DefWindowProc`，系统会使用窗口类中注册的默认光标。

### SetCursor 函数

`SetCursor` 函数设置当前光标，但只在当前线程拥有鼠标输入焦点时有效：

```cpp
HCURSOR SetCursor(HCURSOR hCursor);
```

这个函数只影响当前光标显示，不会改变窗口类的默认光标。一旦鼠标移动或者 `WM_SETCURSOR` 再次被处理，光标可能会被重新设置。

---

## 第六步——加载和使用位图资源

位图是 Win32 中最常用的图像格式之一。虽然现代开发中更多使用 PNG、JPEG 等格式，但在 Win32 资源系统中，传统的 BMP 格式仍然有一席之地。

### 在资源文件中定义位图

```rc
// 位图资源
IDB_BACKGROUND      BITMAP   "res\\background.bmp"
IDB_TOOLBAR         BITMAP   "res\\toolbar.bmp"
IDB_BUTTON_NORMAL   BITMAP   "res\\btn_normal.bmp"
IDB_BUTTON_PRESSED  BITMAP   "res\\btn_pressed.bmp"
IDB_BUTTON_HOVER    BITMAP   "res\\btn_hover.bmp"
```

### 加载位图

```cpp
// 使用 LoadBitmap（传统方式，已过时但仍然可用）
HBITMAP hBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BACKGROUND));

// 使用 LoadImage（推荐方式）
HBITMAP hBitmap = (HBITMAP)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDB_BACKGROUND),
    IMAGE_BITMAP,
    0, 0,               // 使用实际尺寸
    LR_CREATEDIBSECTION // 创建 DIB Section，便于直接访问像素数据
);

// 从文件加载位图
HBITMAP hBitmap = (HBITMAP)LoadImage(
    NULL,
    L"res\\custom_background.bmp",
    IMAGE_BITMAP,
    0, 0,
    LR_LOADFROMFILE | LR_CREATEDIBSECTION
);
```

`LR_CREATEDIBSECTION` 标志创建一个 DIB Section（设备无关位区块），这种位图允许你直接访问和修改像素数据。如果你只是要显示位图，不需要这个标志；如果你需要处理位图数据，这个标志会很有用。

### 在窗口中显示位图

显示位图需要用到 GDI 的位图传输函数：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 加载位图
    HBITMAP hBitmap = (HBITMAP)LoadImage(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDB_BACKGROUND),
        IMAGE_BITMAP,
        0, 0,
        LR_CREATEDIBSECTION
    );

    if (hBitmap)
    {
        // 获取位图尺寸
        BITMAP bm;
        GetObject(hBitmap, sizeof(bm), &bm);

        // 创建兼容 DC 并选入位图
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

        // 获取客户区尺寸
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 计算居中位置
        int x = (rcClient.right - bm.bmWidth) / 2;
        int y = (rcClient.top - bm.bmHeight) / 2;

        // 显示位图（拉伸填充）
        StretchBlt(
            hdc,
            0, 0, rcClient.right, rcClient.bottom,  // 目标矩形
            hdcMem,
            0, 0, bm.bmWidth, bm.bmHeight,         // 源矩形
            SRCCOPY
        );

        // 清理
        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);
    }

    EndPaint(hwnd, &ps);
    return 0;
}
```

`BitBlt` 和 `StretchBlt` 是 GDI 的位图传输函数。`BitBlt` 用于 1:1 复制，`StretchBlt` 可以缩放。`SRCCOPY` 是最常用的光栅操作码，表示直接复制源像素到目标。

⚠️ 注意

千万别忘记释放 GDI 对象！`CreateCompatibleDC` 创建的 DC 必须用 `DeleteDC` 释放，`LoadImage` 加载的非共享位图必须用 `DeleteObject` 释放。GDI 资源泄漏不像内存泄漏那么明显，但累积多了会导致系统绘图性能下降甚至崩溃。

---

## 第七步——完整示例程序

让我们把以上知识整合成一个完整的示例程序。这个程序会设置自定义图标，根据鼠标位置显示不同的光标，并在窗口中央显示一张位图。

### 资源文件（resource.rc）

```rc
#include <windows.h>
#include "resource.h"

// 图标资源
IDI_APPICON          ICON    "res\\app.ico"

// 自定义光标
IDC_CURSOR_CROSS    CURSOR   "res\\cross.cur"

// 位图资源
IDB_BACKGROUND      BITMAP   "res\\background.bmp"
```

### 资源头文件（resource.h）

```cpp
#define IDI_APPICON          100
#define IDC_CURSOR_CROSS     200
#define IDB_BACKGROUND       300
```

### 主程序

```cpp
#include <windows.h>
#include "resource.h"

// 窗口类名
static const wchar_t g_szClassName[] = L"IconCursorBitmapDemo";

// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;

    // 加载并设置图标
    wcex.hIcon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_APPICON),
        IMAGE_ICON,
        32, 32,
        LR_SHARED
    );

    wcex.hIconSm = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(IDI_APPICON),
        IMAGE_ICON,
        16, 16,
        LR_SHARED
    );

    // 设置默认光标
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        L"图标、光标、位图资源示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, L"窗口创建失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HBITMAP g_hBitmap = NULL;

    switch (message)
    {
    case WM_CREATE:
    {
        // 加载位图资源（只加载一次）
        g_hBitmap = (HBITMAP)LoadImage(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDB_BACKGROUND),
            IMAGE_BITMAP,
            0, 0,
            LR_CREATEDIBSECTION
        );
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT)
        {
            // 获取鼠标位置
            DWORD dwPos = GetMessagePos();
            POINT pt = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };
            ScreenToClient(hwnd, &pt);

            // 获取客户区
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            // 在右边缘 50 像素区域显示调整大小光标
            if (pt.x > rcClient.right - 50)
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }

            // 在下边缘 50 像素区域显示调整大小光标
            if (pt.y > rcClient.bottom - 50)
            {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            }

            // 右下角显示对角线调整大小光标
            if (pt.x > rcClient.right - 50 && pt.y > rcClient.bottom - 50)
            {
                SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                return TRUE;
            }

            // 默认箭头光标
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
        return DefWindowProc(hwnd, WM_SETCURSOR, wParam, lParam);
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 填充背景
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        FillRect(hdc, &rcClient, (HBRUSH)(COLOR_WINDOW + 1));

        // 如果有位图，绘制位图
        if (g_hBitmap)
        {
            BITMAP bm;
            GetObject(g_hBitmap, sizeof(bm), &bm);

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, g_hBitmap);

            // 计算居中位置
            int x = (rcClient.right - bm.bmWidth) / 2;
            int y = (rcClient.bottom - bm.bmHeight) / 2;

            // 显示位图
            BitBlt(hdc, x, y, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

            // 清理
            SelectObject(hdcMem, hbmOld);
            DeleteDC(hdcMem);
        }
        else
        {
            // 没有位图时显示提示文字
            DrawText(hdc, L"请添加位图资源 IDB_BACKGROUND",
                -1, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        // 双击时切换图标（演示动态更改图标）
        static BOOL s_bActiveIcon = FALSE;
        s_bActiveIcon = !s_bActiveIcon;

        HICON hIcon;
        if (s_bActiveIcon)
        {
            hIcon = LoadIcon(NULL, IDI_EXCLAMATION);
        }
        else
        {
            hIcon = (HICON)LoadImage(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_APPICON),
                IMAGE_ICON,
                32, 32,
                LR_SHARED
            );
        }

        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_DESTROY:
    {
        // 释放位图资源
        if (g_hBitmap)
        {
            DeleteObject(g_hBitmap);
            g_hBitmap = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
```

### 运行效果

编译运行这个程序，你会看到：
- 窗口标题栏和任务栏显示自定义图标
- 鼠标移到窗口右边缘显示左右箭头光标
- 鼠标移到窗口下边缘显示上下箭头光标
- 鼠标移到右下角显示对角线箭头光标
- 窗口中央显示位图（如果资源存在）
- 双击窗口会切换图标（使用系统感叹号图标）

---

## 第八步——常见问题和调试技巧

在实际开发中，你可能会遇到各种资源加载的问题。这里分享一些常见的坑和调试方法。

### 资源加载失败怎么办

当 `LoadImage` 或 `LoadIcon` 返回 `NULL` 时，表示资源加载失败：

```cpp
HICON hIcon = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_APPICON),
    IMAGE_ICON,
    32, 32,
    LR_SHARED
);

if (hIcon == NULL)
{
    DWORD dwError = GetLastError();
    wchar_t szError[256];
    swprintf_s(szError, 256, L"加载图标失败，错误码：%lu", dwError);
    MessageBox(NULL, szError, L"错误", MB_OK | MB_ICONERROR);
}
```

常见的失败原因包括：
- 资源 ID 没有在 .rc 文件中定义
- 图标文件路径不正确
- 资源文件没有被正确编译到可执行文件中
- 图标文件格式损坏

### 验证资源是否被编译

用 Visual Studio 的资源查看器或者第三方工具（如 Resource Hacker）打开编译后的 .exe 文件，检查资源是否正确嵌入。

### 确定图标的实际尺寸

有时候你不确定图标文件里包含哪些尺寸，可以用代码枚举：

```cpp
void EnumerateIconSizes(HINSTANCE hInstance, int nIconId)
{
    // 这个功能比较复杂，需要直接操作资源数据
    // 简单的做法是用工具（如 Greenfish Icon Editor Pro）查看图标文件

    // 或者尝试加载不同尺寸看哪个成功
    int sizes[] = { 16, 32, 48, 64, 128, 256 };
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        HICON hIcon = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(nIconId),
            IMAGE_ICON,
            sizes[i], sizes[i],
            LR_SHARED
        );

        if (hIcon != NULL)
        {
            wchar_t szMsg[256];
            swprintf_s(szMsg, 256, L"找到 %d×%d 图标", sizes[i], sizes[i]);
            OutputDebugString(szMsg);
        }
    }
}
```

---

## 后续可以做什么

到这里，Win32 的图标、光标、位图资源基础知识就讲完了。你现在应该能够：
- 在资源文件中定义图形资源
- 用 `LoadImage` 统一加载不同类型的资源
- 设置窗口图标和程序图标
- 处理 `WM_SETCURSOR` 消息动态更改光标
- 在窗口中显示位图

但这些只是基础，Win32 资源系统还有更多内容值得探索：

1. **动画光标**：.ani 格式的动画光标资源
2. **图标组资源**：RT_GROUP_ICON 资源类型，理解 .ico 文件的内部结构
3. **DIB Section**：直接访问和修改位图像素数据
4. **Alpha 混合**：使用带有透明通道的位图
5. **PNG 支持**：通过 WIC（Windows Imaging Component）加载现代图像格式

建议你先做一些练习巩固一下：
1. 创建一个完整的图标集，包含 16×16、32×32、48×48 和 256×256 四个尺寸
2. 设计并实现一个自定义光标（比如一个带热点的小圆点）
3. 做一个简单的图片查看器，能够加载和显示 BMP 文件
4. 实现一个皮肤切换功能，通过加载不同的位图资源改变程序外观

下一步，我们可以探讨 Win32 资源系统的其他重要主题：字符串表与国际化、对话框模板深入。这些都是让你的程序更加专业和易用的关键技能。

---

## 相关资源

- [使用光标 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/using-cursors)
- [LoadCursor 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-loadcursora)
- [LoadImage 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-loadimagea)
- [WM_SETICON 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/winmsg/wm-seticon)
- [WM_GETICON 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/winmsg/wm-geticon)
- [WM_SETCURSOR 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/wm-setcursor)
- [资源定义语句 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/resource-definition-statements)
- [C++: Properly using the LoadImage and SetCursor functions - Stack Overflow](https://stackoverflow.com/questions/38166076/c-properly-using-the-loadimage-and-setcursor-functions)
- [Setting the Cursor Image - Win32 apps - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/learnwin32/setting-the-cursor-image)
