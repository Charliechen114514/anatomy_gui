# 通用GUI编程技术——Win32 原生编程实战（七）——更多高级控件

> 上一篇文章我们搞定了 TreeView 控件，那种层级结构的展示方式确实很强大。说实话，学到这里你可能已经觉得 Win32 的控件体系相当完善了——按钮、列表、树形视图，常用的 UI 元素基本都有了。但事情没那么简单，实际做项目的时候，你很快会发现还需要一些更"高级"的控件：标签页来分隔不同视图、进度条来显示任务状态、滑块来调节数值、旋转按钮来微调参数。今天我们就把这些剩下的常用控件一网打尽。

---

## 为什么还要学更多控件

说实话，我刚学 Win32 的时候也觉得控件学得差不多了。直到我开始做实际项目，才发现有些需求用基础控件实现起来特别麻烦。

举个例子，假设你要做一个设置界面，有十几类不同的设置项。用按钮切换视图？太丑了。用多个窗口？管理起来很麻烦。这时候就需要 TabControl——用户熟悉的标签页界面，一个窗口就能容纳大量内容。

再比如，你需要做一个文件复制程序，用户得知道复制进度。用一个编辑框显示百分比？不仅不好看，而且不直观。进度条控件就是为这种场景设计的——它有标准的视觉效果，用户一眼就能看出进度。

还有一个现实原因：现代操作系统和应用程序已经让用户习惯了这些控件。如果你用非标准的方式实现相同的功能，用户会觉得"这个程序很奇怪"。使用系统原生控件，不仅开发效率高，用户体验也更好。

这篇文章会带你学习 TabControl、Progress Bar、Trackbar、UpDown 这些高级控件，每个控件都有完整的示例代码。学完之后，你的 Win32 控件工具箱就更完善了。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建窗口、WM_NOTIFY 是怎么工作的、怎么处理控件通知。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一章：TabControl 标签页

### 是什么

TabControl（标签页控件）是你每天都在用的东西。打开 Chrome 或者 VS Code，顶部那一排可以点击切换的标签，就是标签页的典型应用。每个标签代表一个独立的内容区域，用户点击不同的标签就能看到不同的内容。

```
+--------------------------------------------------+
| [ General ] [ Advanced ] [ About ]               |
+--------------------------------------------------+
|                                                  |
|  (这里显示 "General" 标签页的内容)                |
|                                                  |
|                                                  |
+--------------------------------------------------+

当点击 "Advanced" 标签后：

+--------------------------------------------------+
| [ General ] [ Advanced ] [ About ]               |
+--------------------------------------------------+
|                                                  |
|  (这里显示 "Advanced" 标签页的内容)               |
|                                                  |
|                                                  |
+--------------------------------------------------+
```

### 长什么样

TabControl 本身只显示那排标签，内容区域需要你自己管理。通常的做法是：为每个标签页创建一个子窗口（或对话框），用户切换标签时显示/隐藏对应的子窗口。

### 用来做什么

标签页的主要用途是**组织大量相关但不适合同时显示的内容**：

* 设置界面的不同分类（通用设置、高级设置、关于等）
* 多标签浏览器/编辑器（Chrome、VS Code）
* 属性页对话框（文件属性、系统属性）
* 向导程序的不同步骤

### 典型场景

| 场景 | 说明 |
|------|------|
| 设置对话框 | 把设置项按类别分成多个标签页 |
| 属性窗口 | 显示对象的不同方面属性 |
| 多文档界面 | 一个窗口内打开多个文档 |
| 选项配置 | 浏览器、编辑器的设置页面 |

### WC_TABCONTROL 窗口类

创建 TabControl 使用的是 `WC_TABCONTROL` 窗口类（或者直接用 `SYNCTABCONTROL_CLASS`，不过前者更常用）：

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// 控件 ID
#define ID_TABCONTROL 1001

HWND hTab = CreateWindowEx(
    0,                          // 扩展样式
    WC_TABCONTROL,              // 窗口类名
    L"",                        // 标题（TabControl 不需要）
    WS_CHILD | WS_VISIBLE,      // 基本样式
    10, 10,                     // 位置
    400, 200,                   // 大小
    hwnd,                       // 父窗口
    (HMENU)ID_TABCONTROL,       // 控件 ID
    hInstance,                  // 实例句柄
    NULL                        // 无额外数据
);
```

⚠️ 注意

创建 TabControl 之前需要初始化通用控件库。这个我们在前面已经讲过，但这里再强调一次：

```cpp
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_TAB_CLASSES;
InitCommonControlsEx(&icex);
```

如果忘记初始化，TabControl 创建会失败，而且不会有任何错误提示。

### TCM_INSERTITEM 添加标签页

添加标签页使用 `TCM_INSERTITEM` 消息，需要一个 `TCITEM` 结构体：

```cpp
TCITEM tie = {0};
tie.mask = TCIF_TEXT;                   // 指明我们有 pszText 字段
tie.pszText = (LPWSTR)L"General";       // 标签文字
tie.cchTextMax = 0;                     // 不需要，因为 pszText 不是常量

// 在索引 0 的位置插入（或者用 TabCtrl_GetItemCount() 获取当前数量追加到末尾）
SendMessage(hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);

// 添加第二个标签
tie.pszText = (LPWSTR)L"Advanced";
SendMessage(hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);

// 添加第三个标签
tie.pszText = (LPWSTR)L"About";
SendMessage(hTab, TCM_INSERTITEM, 2, (LPARAM)&tie);
```

`TCITEM` 结构体的 `mask` 字段指定哪些字段有效：

| mask 值 | 对应字段 | 说明 |
|---------|---------|------|
| TCIF_TEXT | pszText | 标签文字 |
| TCIF_IMAGE | iImage | 图像列表索引 |
| TCIF_PARAM | lParam | 应用程序定义的数据 |
| TCIF_STATE | dwState | 标签状态 |

### TCN_SELCHANGE 通知

当用户点击切换标签时，TabControl 会发送 `TCN_SELCHANGE` 通知：

```cpp
case WM_NOTIFY:
{
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (pnmh->idFrom == ID_TABCONTROL && pnmh->code == TCN_SELCHANGE)
    {
        // 获取当前选中的标签索引
        int currentIndex = SendMessage(hTab, TCM_GETCURSEL, 0, 0);

        // 根据 currentIndex 显示/隐藏对应的内容窗口
        OnTabChanged(currentIndex);
    }
    break;
}
```

### TCITEM 结构详解

`TCITEM` 结构体定义如下：

```cpp
typedef struct tagTCITEM {
    UINT   mask;       // 指明哪些字段有效
    DWORD  dwState;    // 状态
    DWORD  dwStateMask;// 状态掩码
    LPWSTR pszText;    // 标签文字
    int    cchTextMax; // 文字缓冲区大小
    int    iImage;     // 图像索引
    LPARAM lParam;     // 应用程序定义数据
} TCITEM, *LPTCITEM;
```

最常用的字段是 `pszText`，用于设置或获取标签文字。如果需要获取标签文字，需要设置 `cchTextMax` 为缓冲区大小：

```cpp
wchar_t buffer[256];
TCITEM tie = {0};
tie.mask = TCIF_TEXT;
tie.pszText = buffer;
tie.cchTextMax = 256;

// 获取索引 0 的标签文字
SendMessage(hTab, TCM_GETITEM, 0, (LPARAM)&tie);
// 现在 buffer 里就是标签文字
```

### 完整示例：简单的标签页

下面是一个完整的 TabControl 示例：

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_TABCONTROL 1001
#define ID_TAB_CONTENT 2000  // 内容窗口 ID 基数

// 标签页内容窗口句柄
HWND g_hTabPages[3] = {NULL};

LRESULT CALLBACK TabPageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 获取标签页索引（通过窗口长整形存储）
        int index = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);

        // 根据索引创建不同的内容
        const wchar_t* texts[] = {
            L"这是 General 标签页的内容\n\n可以在这里放置通用设置选项",
            L"这是 Advanced 标签页的内容\n\n可以在这里放置高级设置选项",
            L"这是 About 标签页的内容\n\n可以在这里放置版本信息"
        };

        CreateWindowEx(0, L"STATIC", texts[index],
            WS_CHILD | WS_VISIBLE,
            20, 20, 300, 100,
            hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        return 0;
    }

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateTabPage(HWND hParent, int index, const wchar_t* title, HINSTANCE hInstance)
{
    // 注册标签页窗口类（只注册一次）
    static BOOL s_classRegistered = FALSE;
    if (!s_classRegistered)
    {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = TabPageProc;
        wc.hInstance = hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"TabPageClass";
        RegisterClass(&wc);
        s_classRegistered = TRUE;
    }

    // 创建标签页窗口（初始隐藏）
    HWND hPage = CreateWindowEx(
        0, L"TabPageClass", L"",
        WS_CHILD | WS_CLIPSIBLINGS,
        10, 40, 380, 150,
        hParent, (HMENU)(ID_TAB_CONTENT + index), hInstance, NULL
    );

    // 保存索引到窗口用户数据
    SetWindowLongPtr(hPage, GWLP_USERDATA, index);
    g_hTabPages[index] = hPage;
}

void ShowTabPage(int index)
{
    // 隐藏所有标签页
    for (int i = 0; i < 3; i++)
    {
        if (g_hTabPages[i])
            ShowWindow(g_hTabPages[i], SW_HIDE);
    }

    // 显示选中的标签页
    if (g_hTabPages[index] && index >= 0 && index < 3)
        ShowWindow(g_hTabPages[index], SW_SHOW);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TAB_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建 TabControl
        HWND hTab = CreateWindowEx(
            0, WC_TABCONTROL, L"",
            WS_CHILD | WS_VISIBLE,
            10, 10, 400, 30,
            hwnd, (HMENU)ID_TABCONTROL, hInstance, NULL
        );

        // 添加标签页
        TCITEM tie = {0};
        tie.mask = TCIF_TEXT;

        tie.pszText = (LPWSTR)L"General";
        SendMessage(hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);

        tie.pszText = (LPWSTR)L"Advanced";
        SendMessage(hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);

        tie.pszText = (LPWSTR)L"About";
        SendMessage(hTab, TCM_INSERTITEM, 2, (LPARAM)&tie);

        // 创建各个标签页的内容窗口
        CreateTabPage(hwnd, 0, L"General", hInstance);
        CreateTabPage(hwnd, 1, L"Advanced", hInstance);
        CreateTabPage(hwnd, 2, L"About", hInstance);

        // 显示第一个标签页
        ShowTabPage(0);

        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        if (pnmh->idFrom == ID_TABCONTROL && pnmh->code == TCN_SELCHANGE)
        {
            HWND hTab = GetDlgItem(hwnd, ID_TABCONTROL);
            int currentIndex = SendMessage(hTab, TCM_GETCURSEL, 0, 0);
            ShowTabPage(currentIndex);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"TabControlWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"TabControl 示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 250,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

这个示例创建了一个包含三个标签页的窗口，每个标签页显示不同的内容。当用户点击切换标签时，对应的内容窗口会显示出来。

---

## 第二章：Progress Bar 进度条

### 是什么

Progress Bar（进度条）是一种视觉反馈控件，用来显示某个操作的完成进度。你肯定见过这种控件——安装程序复制文件时、下载文件时、加载游戏时，通常都会有一个从 0% 到 100% 的条形指示器。

```

未开始时：

[████████████████████████████████████████] 100%
 完全填充

进行中：

[████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░]  40%
 部分填充
```

### 长什么样

进度条是一个矩形区域，内部会从左到右填充颜色来表示进度。填充的比例对应操作的完成百分比。

### 用来做什么

进度条的核心用途是**让用户知道操作正在进行，以及大概还需要多久**。这对用户体验非常重要——如果没有进度提示，用户可能会觉得程序卡死了。

### 典型场景

| 场景 | 说明 |
|------|------|
| 文件复制 | 显示已复制/总字节数 |
| 安装程序 | 显示安装进度 |
| 下载/上传 | 显示已传输/总字节数 |
| 数据处理 | 显示已处理/总记录数 |
| 加载资源 | 游戏或应用程序加载进度 |

### PROGRESS_CLASS 窗口类

创建进度条使用 `PROGRESS_CLASS` 窗口类：

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_PROGRESS 1002

HWND hProgress = CreateWindowEx(
    0,                      // 扩展样式
    PROGRESS_CLASS,         // 窗口类名
    L"",                    // 标题（进度条不需要）
    WS_CHILD | WS_VISIBLE,  // 基本样式
    10, 10,                 // 位置
    300, 20,                // 大小
    hwnd,                   // 父窗口
    (HMENU)ID_PROGRESS,     // 控件 ID
    hInstance,              // 实例句柄
    NULL                    // 无额外数据
);
```

⚠️ 注意

进度条也需要初始化通用控件库，使用 `ICC_PROGRESS_CLASS` 标志：

```cpp
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_PROGRESS_CLASS;
InitCommonControlsEx(&icex);
```

### PBM_SETRANGE32 设置范围

进度条有一个范围（通常是 0-100），你需要在设置进度之前先设置这个范围：

```cpp
// 设置范围从 0 到 100
SendMessage(hProgress, PBM_SETRANGE32, 0, 100);

// 或者设置为 0 到 1000（更精细的控制）
SendMessage(hProgress, PBM_SETRANGE32, 0, 1000);
```

`PBM_SETRANGE32` 消息的 `wParam` 是范围下限，`lParam` 是范围上限。都是 32 位有符号整数，所以范围可以很大。

还有一个老版本的 `PBM_SETRANGE` 消息，但它的参数是 16 位的，范围有限。新代码应该用 `PBM_SETRANGE32`。

### PBM_SETPOS 设置当前位置

设置进度条的当前位置使用 `PBM_SETPOS` 消息：

```cpp
// 设置进度为 50%
SendMessage(hProgress, PBM_SETPOS, 50, 0);

// 设置进度为 75%
SendMessage(hProgress, PBM_SETPOS, 75, 0);
```

如果当前范围是 0-100，那么 `PBM_SETPOS` 的值应该是 0-100 之间的整数。进度条会自动计算并绘制对应的填充比例。

你也可以获取当前位置：

```cpp
// 获取当前位置
int pos = SendMessage(hProgress, PBM_GETPOS, 0, 0);
```

### PBS_SMOOTH 平滑样式

默认情况下，进度条是"块状"的——它会显示一系列离散的方块。如果你想要平滑连续的进度条，可以加上 `PBS_SMOOTH` 样式：

```cpp
HWND hProgress = CreateWindowEx(
    0,
    PROGRESS_CLASS,
    L"",
    WS_CHILD | WS_VISIBLE | PBS_SMOOTH,  // 注意这里
    10, 10, 300, 20,
    hwnd, (HMENU)ID_PROGRESS, hInstance, NULL
);
```

块状进度条 vs 平滑进度条：

```
块状 (默认):
[████░░░░░░░░░░░░░░]  一块一块的

平滑 (PBS_SMOOTH):
[████████░░░░░░░░░░]  连续填充的
```

### 完整示例：模拟文件复制进度

下面是一个完整的进度条示例，模拟文件复制操作：

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_PROGRESS     1001
#define ID_START_BUTTON 1002
#define ID_STATUS_TEXT  1003

HWND g_hProgress = NULL;
HWND g_hStatusText = NULL;

VOID CALLBACK CopyProgressCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    // 获取当前进度
    int pos = SendMessage(g_hProgress, PBM_GETPOS, 0, 0);

    // 如果已经到 100%，停止定时器
    if (pos >= 100)
    {
        KillTimer(hwnd, idEvent);
        SetWindowText(g_hStatusText, L"复制完成！");
        EnableWindow(GetDlgItem(hwnd, ID_START_BUTTON), TRUE);
        return;
    }

    // 增加进度
    pos += 2;  // 每次增加 2%
    if (pos > 100) pos = 100;

    SendMessage(g_hProgress, PBM_SETPOS, pos, 0);

    // 更新状态文字
    wchar_t status[64];
    swprintf_s(status, 64, L"正在复制... %d%%", pos);
    SetWindowText(g_hStatusText, status);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icex);

        // 创建进度条
        g_hProgress = CreateWindowEx(
            0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 20, 300, 25,
            hwnd, (HMENU)ID_PROGRESS, hInstance, NULL
        );

        // 设置进度条范围
        SendMessage(g_hProgress, PBM_SETRANGE32, 0, 100);
        SendMessage(g_hProgress, PBM_SETPOS, 0, 0);

        // 创建状态文字
        g_hStatusText = CreateWindowEx(
            0, L"STATIC", L"点击开始按钮开始复制",
            WS_CHILD | WS_VISIBLE,
            20, 55, 300, 20,
            hwnd, (HMENU)ID_STATUS_TEXT, hInstance, NULL
        );

        // 创建开始按钮
        CreateWindowEx(
            0, L"BUTTON", L"开始复制",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 85, 100, 30,
            hwnd, (HMENU)ID_START_BUTTON, hInstance, NULL
        );

        return 0;
    }

    case WM_COMMAND:
    {
        WORD controlId = LOWORD(wParam);
        WORD notificationCode = HIWORD(wParam);

        if (controlId == ID_START_BUTTON && notificationCode == BN_CLICKED)
        {
            // 禁用按钮，防止重复点击
            EnableWindow((HWND)lParam, FALSE);

            // 重置进度
            SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
            SetWindowText(g_hStatusText, L"正在复制... 0%");

            // 启动定时器模拟进度
            SetTimer(hwnd, 1, 100, CopyProgressCallback);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ProgressWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"进度条示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 200,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

这个示例创建了一个进度条和一个开始按钮，点击按钮后进度条会自动增加，模拟文件复制操作。

---

## 第三章：Trackbar 滑块

### 是什么

Trackbar（也叫 Slider，滑块控件）是一个可以让用户拖动滑块来选择数值的控件。你调节系统音量、屏幕亮度时用的就是这种控件。

```

水平滑块：

0 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100
              ▲
             滑块

带刻度的水平滑块：

0 ━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━ 100
    ▲   ▲   ▲   ▲   ▲   ▲   ▲   ▲
   刻度
```

### 长什么样

Trackbar 由一条轨道和一个可移动的滑块组成。用户可以用鼠标拖动滑块，或者点击轨道来快速跳转。轨道两侧可以显示刻度标记，帮助用户估计数值。

### 用来做什么

Trackbar 的主要用途是**让用户在一个连续范围内选择数值**：

* 音量控制（0-100）
* 亮度调节
* 进度跳转（视频播放器）
* 数值微调（各种设置项）

### 典型场景

| 场景 | 说明 |
|------|------|
| 音量控制 | 系统音量、应用程序音量 |
| 视频播放 | 跳转到指定时间位置 |
| 显示设置 | 亮度、对比度、色彩调节 |
| 游戏设置 | 难度、灵敏度等数值参数 |
| 缩放控制 | 地图、图片的缩放级别 |

### TRACKBAR_CLASS 窗口类

创建 Trackbar 使用 `TRACKBAR_CLASS` 窗口类：

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_TRACKBAR 1003

HWND hTrackbar = CreateWindowEx(
    0,                      // 扩展样式
    TRACKBAR_CLASS,         // 窗口类名
    L"",                    // 标题（Trackbar 不需要）
    WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,  // 样式
    10, 10,                 // 位置
    300, 30,                // 大小
    hwnd,                   // 父窗口
    (HMENU)ID_TRACKBAR,     // 控件 ID
    hInstance,              // 实例句柄
    NULL                    // 无额外数据
);
```

⚠️ 注意

Trackbar 也需要初始化通用控件库，使用 `ICC_BAR_CLASSES` 标志：

```cpp
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_BAR_CLASSES;
InitCommonControlsEx(&icex);
```

### TBM_SETRANGE 设置范围

设置 Trackbar 的范围使用 `TBM_SETRANGE` 消息：

```cpp
// 设置范围从 0 到 100
SendMessage(hTrackbar, TBM_SETRANGE,
    (WPARAM)TRUE,           // 重绘控件
    (LPARAM)MAKELONG(0, 100) // LOWORD=最小值, HIWORD=最大值
);

// 或者分别设置最小值和最大值
SendMessage(hTrackbar, TBM_SETRANGEMIN, TRUE, 0);
SendMessage(hTrackbar, TBM_SETRANGEMAX, TRUE, 100);
```

`TBM_SETRANGE` 的 `wParam` 是 `TRUE` 表示控件需要重绘，`lParam` 是一个 `LONG` 类型，用 `MAKELONG` 宏把最小值和最大值打包进去。

### TBM_SETPOS 设置位置

设置滑块位置使用 `TBM_SETPOS` 消息：

```cpp
// 设置滑块位置到 50
SendMessage(hTrackbar, TBM_SETPOS,
    (WPARAM)TRUE,   // 重绘控件
    (LPARAM)50      // 新位置
);
```

### TBM_GETPOS 获取位置

获取滑块当前位置使用 `TBM_GETPOS` 消息：

```cpp
// 获取当前位置
int pos = SendMessage(hTrackbar, TBM_GETPOS, 0, 0);
```

### TBS_AUTOTICKS 自动刻度

`TBS_AUTOTICKS` 样式会让 Trackbar 自动显示刻度。你也可以手动设置刻度间隔：

```cpp
// 每 10 个单位显示一个刻度
SendMessage(hTrackbar, TBM_SETTICFREQ, 10, 0);
```

其他常用样式：

| 样式 | 说明 |
|------|------|
| TBS_HORZ | 水平方向（默认） |
| TBS_VERT | 垂直方向 |
| TBS_AUTOTICKS | 自动显示刻度 |
| TBS_NOTICKS | 不显示刻度 |
| TBS_BOTH | 在两侧都显示刻度 |
| TBS_TOOLTIPS | 支持工具提示 |
| TBS_ENABLESELRANGE | 支持选择范围 |

### WM_HSCROLL/WM_VSCROLL 通知

Trackbar 通过 `WM_HSCROLL`（水平）或 `WM_VSCROLL`（垂直）消息通知父窗口滑块位置的变化：

```cpp
case WM_HSCROLL:
case WM_VSCROLL:
{
    HWND hTrackbar = (HWND)lParam;

    // 检查是否是我们的 Trackbar
    if (hTrackbar && GetWindowLongPtr(hTrackbar, GWLP_ID) == ID_TRACKBAR)
    {
        // 获取当前位置
        int pos = SendMessage(hTrackbar, TBM_GETPOS, 0, 0);

        // 根据通知代码处理
        switch (LOWORD(wParam))
        {
        case TB_THUMBPOSITION:
        case TB_THUMBTRACK:
            // 用户正在拖动滑块
            break;

        case TB_LINEUP:
        case TB_LINEDOWN:
        case TB_PAGEUP:
        case TB_PAGEDOWN:
        case TB_TOP:
        case TB_BOTTOM:
        case TB_ENDTRACK:
            // 其他通知
            break;
        }

        // 更新显示
        OnTrackbarChanged(pos);
    }
    break;
}
```

常用的通知代码：

| 通知代码 | 说明 |
|---------|------|
| TB_BOTTOM | 滑块移到最小位置 |
| TB_ENDTRACK | 用户释放鼠标 |
| TB_LINEDOWN | 按右箭头键 |
| TB_LINEUP | 按左箭头键 |
| TB_PAGEDOWN | 按 PgDn 或点击滑块右侧 |
| TB_PAGEUP | 按 PgUp 或点击滑块左侧 |
| TB_THUMBPOSITION | 拖动结束后滑块位置（只在 `WM_HSCROLL` 中） |
| TB_THUMBTRACK | 拖动过程中滑块位置（只在 `WM_HSCROLL` 中） |
| TB_TOP | 滑块移到最大位置 |

⚠️ 注意

`TB_THUMBTRACK` 和 `TB_THUMBPOSITION` 只在 `WM_HSCROLL` 消息中发送，不在 `WM_VSCROLL` 中。这是 Windows 的一个历史遗留问题，处理时需要注意。

### 完整示例：音量调节器

下面是一个完整的 Trackbar 示例，实现一个简单的音量调节器：

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_TRACKBAR     1001
#define ID_VOLUME_TEXT  1002

HWND g_hTrackbar = NULL;
HWND g_hVolumeText = NULL;

void UpdateVolumeText(int volume)
{
    wchar_t text[64];
    swprintf_s(text, 64, L"音量: %d%%", volume);
    SetWindowText(g_hVolumeText, text);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建音量显示文字
        g_hVolumeText = CreateWindowEx(
            0, L"STATIC", L"音量: 50%",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            20, 20, 260, 25,
            hwnd, (HMENU)ID_VOLUME_TEXT, hInstance, NULL
        );

        // 创建 Trackbar
        g_hTrackbar = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_BOTH,
            20, 55, 260, 40,
            hwnd, (HMENU)ID_TRACKBAR, hInstance, NULL
        );

        // 设置范围 0-100
        SendMessage(g_hTrackbar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));

        // 设置刻度间隔为 10
        SendMessage(g_hTrackbar, TBM_SETTICFREQ, 10, 0);

        // 设置初始位置为 50
        SendMessage(g_hTrackbar, TBM_SETPOS, TRUE, 50);

        return 0;
    }

    case WM_HSCROLL:
    {
        HWND hTrackbar = (HWND)lParam;

        if (hTrackbar == g_hTrackbar)
        {
            // 获取当前位置
            int pos = SendMessage(hTrackbar, TBM_GETPOS, 0, 0);

            // 更新显示
            UpdateVolumeText(pos);

            // 这里可以调用实际设置音量的 API
            // 例如：waveOutSetVolume() 等
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"TrackbarWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Trackbar 示例 - 音量调节",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 180,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

这个示例创建了一个音量调节器，拖动滑块会实时显示当前音量值。

---

## 第四章：UpDown 旋转按钮

### 是什么

UpDown 控件（也叫 Spin Button、旋转按钮控件）由两个小箭头按钮组成，一个向上、一个向下。用户点击箭头可以增加或减少数值。

```

┌─────────────┐
│     50      │ ┥  <- 伙伴控件（通常是编辑框）
│             │ ┝
└─────────────┘ ┃
                ┟  <- UpDown 控件
                ┃

┌─────────────────┐
│     255     0 0 │ ┥  <- 编辑框显示颜色值
│                 │ ┝
└─────────────────┘ ┃  <- UpDown 控件
                    ┟
                    ┃
```

### 长什么样

UpDown 控件通常紧挨着一个编辑框，两个箭头垂直排列。上箭头增加数值，下箭头减少数值。

### 用来做什么

UpDown 控件的主要用途是**微调数值**，特别是那些不需要大范围调整、只需要精确微调的场合：

* 颜色选择器中的 RGB 值
* 年月日选择
* 坐标精确调整
* 小范围数值微调

### 典型场景

| 场景 | 说明 |
|------|------|
| 颜色选择 | RGB 值的精确调整 |
| 日期时间 | 年、月、日、时、分、秒选择 |
| 坐标调整 | 精确调整 X/Y 坐标 |
| 数值微调 | 小范围内的数值调整 |

### UPDOWN_CLASS 窗口类

创建 UpDown 控件使用 `UPDOWN_CLASS` 窗口类：

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_UPDOWN 1004

HWND hUpDown = CreateWindowEx(
    0,                      // 扩展样式
    UPDOWN_CLASS,           // 窗口类名
    L"",                    // 标题（UpDown 不需要）
    WS_CHILD | WS_VISIBLE |
    UDS_ALIGNRIGHT |        // 右对齐伙伴控件
    UDS_SETBUDDYINT |       // 自动更新伙伴控件的文字
    UDS_ARROWKEYS,          // 允许用键盘上下键
    10, 10,                 // 位置（通常会自动调整）
    20, 30,                 // 大小（通常会自动调整）
    hwnd,                   // 父窗口
    (HMENU)ID_UPDOWN,       // 控件 ID
    hInstance,              // 实例句柄
    NULL                    // 无额外数据
);
```

⚠️ 注意

UpDown 也需要初始化通用控件库，使用 `ICC_UPDOWN_CLASS` 标志：

```cpp
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_UPDOWN_CLASS;
InitCommonControlsEx(&icex);
```

### UDM_SETBUDDY 设置伙伴控件

UpDown 控件通常需要和一个"伙伴控件"（通常是编辑框）配合使用。设置伙伴控件使用 `UDM_SETBUDDY` 消息：

```cpp
// 先创建编辑框
HWND hEdit = CreateWindowEx(
    WS_EX_CLIENTEDGE, L"EDIT", L"50",
    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
    10, 10, 80, 20,
    hwnd, NULL, hInstance, NULL
);

// 设置伙伴控件
SendMessage(hUpDown, UDM_SETBUDDY, (WPARAM)hEdit, 0);
```

设置了伙伴控件后，UpDown 控件会自动调整自己的位置和大小，紧挨着伙伴控件。

### UDM_SETRANGE 设置范围

设置 UpDown 的数值范围使用 `UDM_SETRANGE` 消息：

```cpp
// 设置范围 0-100
SendMessage(hUpDown, UDM_SETRANGE,
    0,              // 保留，必须为 0
    MAKELONG(100, 0) // HIWORD=最大值, LOWORD=最小值
);
```

注意这里的参数顺序：`HIWORD` 是最大值，`LOWORD` 是最小值，和 Trackbar 的 `TBM_SETRANGE` 相反。

### UDM_SETPOS 设置位置

设置当前位置使用 `UDM_SETPOS` 消息：

```cpp
// 设置位置为 50
SendMessage(hUpDown, UDM_SETPOS,
    0,      // 保留，必须为 0
    50      // 新位置（short 类型）
);
```

获取当前位置使用 `UDM_GETPOS` 消息：

```cpp
// 获取当前位置
// 返回值：LOWORD=位置, HIWORD=错误码（0 表示成功）
LRESULT result = SendMessage(hUpDown, UDM_GETPOS, 0, 0);
short pos = LOWORD(result);
```

### 常用样式

| 样式 | 说明 |
|------|------|
| UDS_HORZ | 水平排列箭头（左右而不是上下） |
| UDS_ALIGNLEFT | 左对齐伙伴控件 |
| UDS_ALIGNRIGHT | 右对齐伙伴控件 |
| UDS_SETBUDDYINT | 自动更新伙伴控件的文字 |
| UDS_ARROWKEYS | 允许用键盘上下键调整 |
| UDS_WRAP | 超过最大值时回到最小值 |
| UDS_NOTHOUSANDS | 不显示千位分隔符 |

### UDN_DELTAPOS 通知

当用户点击箭头改变数值时，UpDown 控件会发送 `UDN_DELTAPOS` 通知（通过 `WM_NOTIFY`）：

```cpp
case WM_NOTIFY:
{
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (pnmh->code == UDN_DELTAPOS)
    {
        LPNMUPDOWN pnmud = (LPNMUPDOWN)lParam;

        // pnmud->iPos 是当前位置
        // pnmud->iDelta 是变化量（+1 或 -1）

        // 可以在这里修改变化量
        if (pnmud->iPos + pnmud->iDelta > 100)
        {
            // 阻止超过 100
            return TRUE;  // 返回 TRUE 表示阻止变化
        }

        // 或者修改变化量
        // pnmud->iDelta *= 10;  // 每次变化 10 而不是 1
    }
    break;
}
```

`NMUPDOWN` 结构体定义：

```cpp
typedef struct tagNMUPDOWN {
    NMHDR hdr;       // 标准通知头
    int iPos;        // 当前位置
    int iDelta;      // 变化量
} NMUPDOWN, *LPNMUPDOWN;
```

### 完整示例：颜色值微调器

下面是一个完整的 UpDown 示例，实现一个简单的 RGB 颜色值微调器：

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_EDIT_R       1001
#define ID_UPDOWN_R     2001
#define ID_EDIT_G       1002
#define ID_UPDOWN_G     2002
#define ID_EDIT_B       1003
#define ID_UPDOWN_B     2003
#define ID_COLOR_PREVIEW 1004

HWND g_hEditR = NULL;
HWND g_hEditG = NULL;
HWND g_hEditB = NULL;
HWND g_hColorPreview = NULL;

void UpdateColorPreview()
{
    // 获取 RGB 值
    int r = GetDlgItemInt(GetParent(g_hEditR), ID_EDIT_R, NULL, FALSE);
    int g = GetDlgItemInt(GetParent(g_hEditG), ID_EDIT_G, NULL, FALSE);
    int b = GetDlgItemInt(GetParent(g_hEditB), ID_EDIT_B, NULL, FALSE);

    // 创建画刷并重绘预览窗口
    if (g_hColorPreview)
    {
        InvalidateRect(g_hColorPreview, NULL, TRUE);
    }
}

LRESULT CALLBACK ColorPreviewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 获取 RGB 值
        int r = GetDlgItemInt(GetParent(hwnd), ID_EDIT_R, NULL, FALSE);
        int g = GetDlgItemInt(GetParent(hwnd), ID_EDIT_G, NULL, FALSE);
        int b = GetDlgItemInt(GetParent(hwnd), ID_EDIT_B, NULL, FALSE);

        // 创建纯色画刷
        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        FillRect(hdc, &ps.rcPaint, hBrush);
        DeleteObject(hBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&icex);

        // 注册颜色预览窗口类
        WNDCLASS wcPreview = {0};
        wcPreview.lpfnWndProc = ColorPreviewProc;
        wcPreview.hInstance = hInstance;
        wcPreview.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wcPreview.lpszClassName = L"ColorPreviewClass";
        RegisterClass(&wcPreview);

        // 创建标签
        CreateWindowEx(0, L"STATIC", L"R:",
            WS_CHILD | WS_VISIBLE,
            20, 20, 20, 20,
            hwnd, NULL, hInstance, NULL);

        CreateWindowEx(0, L"STATIC", L"G:",
            WS_CHILD | WS_VISIBLE,
            20, 50, 20, 20,
            hwnd, NULL, hInstance, NULL);

        CreateWindowEx(0, L"STATIC", L"B:",
            WS_CHILD | WS_VISIBLE,
            20, 80, 20, 20,
            hwnd, NULL, hInstance, NULL);

        // 创建编辑框
        g_hEditR = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"128",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
            50, 20, 50, 20,
            hwnd, (HMENU)ID_EDIT_R, hInstance, NULL);

        g_hEditG = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"128",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
            50, 50, 50, 20,
            hwnd, (HMENU)ID_EDIT_G, hInstance, NULL);

        g_hEditB = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"128",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
            50, 80, 50, 20,
            hwnd, (HMENU)ID_EDIT_B, hInstance, NULL);

        // 创建 UpDown 控件
        HWND hUpDownR = CreateWindowEx(0, UPDOWN_CLASS, L"",
            WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_UPDOWN_R, hInstance, NULL);
        SendMessage(hUpDownR, UDM_SETBUDDY, (WPARAM)g_hEditR, 0);
        SendMessage(hUpDownR, UDM_SETRANGE, 0, MAKELONG(0, 255));
        SendMessage(hUpDownR, UDM_SETPOS, 0, 128);

        HWND hUpDownG = CreateWindowEx(0, UPDOWN_CLASS, L"",
            WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_UPDOWN_G, hInstance, NULL);
        SendMessage(hUpDownG, UDM_SETBUDDY, (WPARAM)g_hEditG, 0);
        SendMessage(hUpDownG, UDM_SETRANGE, 0, MAKELONG(0, 255));
        SendMessage(hUpDownG, UDM_SETPOS, 0, 128);

        HWND hUpDownB = CreateWindowEx(0, UPDOWN_CLASS, L"",
            WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_UPDOWN_B, hInstance, NULL);
        SendMessage(hUpDownB, UDM_SETBUDDY, (WPARAM)g_hEditB, 0);
        SendMessage(hUpDownB, UDM_SETRANGE, 0, MAKELONG(0, 255));
        SendMessage(hUpDownB, UDM_SETPOS, 0, 128);

        // 创建颜色预览窗口
        g_hColorPreview = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"ColorPreviewClass", L"",
            WS_CHILD | WS_VISIBLE,
            120, 20, 100, 80,
            hwnd, (HMENU)ID_COLOR_PREVIEW, hInstance, NULL
        );

        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        if (pnmh->code == UDN_DELTAPOS)
        {
            // 更新颜色预览
            UpdateColorPreview();
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"UpDownWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"UpDown 示例 - RGB 颜色选择器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 280, 180,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

这个示例创建了一个 RGB 颜色选择器，用户可以通过点击上下箭头微调 RGB 值，右侧会实时显示对应的颜色。

---

## 第五章：RichEdit 简介

### 与 Edit Control 的区别

我们前面学过的 Edit Control（编辑框控件）功能很基础，只能显示纯文本。而 RichEdit（富编辑框）是一个功能更强大的编辑控件，支持：

* **格式化文本**：不同文字可以有不同字体、颜色、大小
* **段落格式**：对齐方式、缩进、行距等
* **嵌入对象**：图片、OLE 对象等
* **撤销/重做**：内置的多级撤销功能
* **搜索替换**：强大的文本搜索功能
* **URL 检测**：自动识别并高亮 URL
* **更多**：拼写检查、自动更正等（取决于版本）

```
普通 Edit Control:

┌─────────────────────────────────────┐
│ 这是纯文本，只有一种字体和颜色      │
└─────────────────────────────────────┘

RichEdit:

┌─────────────────────────────────────┐
│ 这是 粗体 文字，这是 斜体           │
│ 这是红色文字 这是蓝色文字            │
│             ┌───┐                    │
│             │图片│                   │
│             └───┘                    │
└─────────────────────────────────────┘
```

### RICHEDIT_CLASS 窗口类

创建 RichEdit 使用 `RICHEDIT_CLASS` 窗口类（注意：不同版本的 RichEdit 有不同的类名）：

| 类名 | 版本 | 说明 |
|------|------|------|
| `RICHEDIT_CLASS` | 1.0 | 最老的版本，功能有限 |
| `RICHEDIT_CLASS10` | 1.0 | 同上，别名 |
| `RICHEDIT_CLASS20` | 2.0 | 增强版，支持更多功能 |
| `RICHEDIT_CLASS30` | 3.0 | 再增强，支持表格等 |
| `MSFTEDIT_CLASS` | 4.1+ | 最新版本（需要 msftedit.dll） |

现代应用程序应该使用 `MSFTEDIT_CLASS`，它是功能最完整的版本。

### LoadLibrary 加载库

RichEdit 控件不像其他通用控件那样在 comctl32.dll 里，它有自己的 DLL。使用前需要先加载对应的 DLL：

```cpp
// 对于 RichEdit 2.0/3.0
LoadLibrary(L"riched20.dll");

// 对于 RichEdit 4.1（MSFTEDIT_CLASS）
LoadLibrary(L"msftedit.dll");
```

⚠️ 注意

`LoadLibrary` 调用后应该记得在程序退出时调用 `FreeLibrary`。而且由于 DLL 是按引用计数加载的，多次调用 `LoadLibrary` 只会增加引用计数，不会重复加载。

### 基本用法示例

下面是一个使用 RichEdit 4.1 的简单示例：

```cpp
#include <windows.h>
#include <richedit.h>
#pragma comment(lib, "comctl32.lib")

#define ID_RICHEDIT 1001

// 在 WM_CREATE 或 WinMain 开头加载 DLL
HMODULE hRichEditDll = LoadLibrary(L"msftedit.dll");

// 创建 RichEdit 控件
HWND hRichEdit = CreateWindowEx(
    WS_EX_CLIENTEDGE,
    MSFTEDIT_CLASS,           // 使用 4.1 版本
    L"",                      // 初始文字
    WS_CHILD | WS_VISIBLE |
    WS_VSCROLL | WS_HSCROLL | // 显示滚动条
    ES_MULTILINE |            // 多行
    ES_AUTOVSCROLL | ES_AUTOHSCROLL |
    ES_WANTRETURN,            // 回车换行
    10, 10, 500, 300,
    hwnd, (HMENU)ID_RICHEDIT, hInstance, NULL
);

// 设置文字
SendMessage(hRichEdit, WM_SETTEXT, 0, (LPARAM)L"这是富文本编辑框");

// 程序退出时
if (hRichEditDll)
    FreeLibrary(hRichEditDll);
```

### 功能简介

RichEdit 的功能非常丰富，这里简单介绍一些常用的：

#### 设置文字格式

```cpp
// 设置选中的文字为粗体
CHARFORMAT2 cf = {0};
cf.cbSize = sizeof(CHARFORMAT2);
cf.dwMask = CFM_BOLD;
cf.dwEffects = CFE_BOLD;

SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
```

#### 插入图片

```cpp
// 使用 EM_GETOLEINTERFACE 获取 IRichEditOle 接口
// 然后通过 OLE 接口插入图片（这个比较复杂，需要了解 COM）
```

#### 获取 RTF 文本

```cpp
// 获取 RTF 格式的文本
int len = SendMessage(hRichEdit, WM_GETTEXTLENGTH, 0, 0);
char* buffer = new char[len * 2 + 1];  // RTF 可能比纯文本长

SendMessage(hRichEdit, EM_GETTEXTEX, GETTEXTEX参数, (LPARAM)buffer);

// 使用 buffer...
delete[] buffer;
```

说实话，RichEdit 是一个非常复杂的控件，完整讲解需要单独一篇甚至几篇文章。如果你需要做复杂的文本编辑功能，建议查阅 MSDN 文档，或者考虑使用现成的富文本编辑库。

---

## 第六章：综合实战——系统设置对话框

现在我们把今天学的几个控件整合起来，做一个完整的"系统设置"对话框。这个示例会使用 TabControl、Progress Bar、Trackbar 和 UpDown 控件。

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// 控件 ID
#define ID_TABCONTROL       1001
#define ID_TAB_CONTENT_BASE 2000

// 显示设置页
#define ID_BRIGHTNESS_TRACKBAR 3001
#define ID_BRIGHTNESS_TEXT     3002
#define ID_VOLUME_TRACKBAR     3003
#define ID_VOLUME_TEXT         3004

// 性能设置页
#define ID_PROGRESS        4001
#define ID_START_BUTTON    4002
#define ID_CPU_SPIN_EDIT   4003
#define ID_CPU_UPDOWN      4004

// 标签页内容窗口句柄
HWND g_hTabPageDisplay = NULL;
HWND g_hTabPagePerformance = NULL;

// 显示设置页的控件
HWND g_hBrightnessTrackbar = NULL;
HWND g_hBrightnessText = NULL;
HWND g_hVolumeTrackbar = NULL;
HWND g_hVolumeText = NULL;

// 性能设置页的控件
HWND g_hProgress = NULL;
HWND g_hCpuSpinEdit = NULL;

// 更新亮度显示
void UpdateBrightnessText()
{
    if (!g_hBrightnessTrackbar || !g_hBrightnessText)
        return;

    int brightness = SendMessage(g_hBrightnessTrackbar, TBM_GETPOS, 0, 0);
    wchar_t text[64];
    swprintf_s(text, 64, L"亮度: %d%%", brightness);
    SetWindowText(g_hBrightnessText, text);
}

// 更新音量显示
void UpdateVolumeText()
{
    if (!g_hVolumeTrackbar || !g_hVolumeText)
        return;

    int volume = SendMessage(g_hVolumeTrackbar, TBM_GETPOS, 0, 0);
    wchar_t text[64];
    swprintf_s(text, 64, L"音量: %d%%", volume);
    SetWindowText(g_hVolumeText, text);
}

// 显示设置页窗口过程
LRESULT CALLBACK DisplayPageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建亮度显示
        g_hBrightnessText = CreateWindowEx(
            0, L"STATIC", L"亮度: 50%",
            WS_CHILD | WS_VISIBLE,
            20, 20, 200, 20,
            hwnd, (HMENU)ID_BRIGHTNESS_TEXT, hInstance, NULL
        );

        // 创建亮度滑块
        g_hBrightnessTrackbar = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            20, 45, 300, 30,
            hwnd, (HMENU)ID_BRIGHTNESS_TRACKBAR, hInstance, NULL
        );
        SendMessage(g_hBrightnessTrackbar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hBrightnessTrackbar, TBM_SETTICFREQ, 10, 0);
        SendMessage(g_hBrightnessTrackbar, TBM_SETPOS, TRUE, 50);

        // 创建音量显示
        g_hVolumeText = CreateWindowEx(
            0, L"STATIC", L"音量: 75%",
            WS_CHILD | WS_VISIBLE,
            20, 90, 200, 20,
            hwnd, (HMENU)ID_VOLUME_TEXT, hInstance, NULL
        );

        // 创建音量滑块
        g_hVolumeTrackbar = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            20, 115, 300, 30,
            hwnd, (HMENU)ID_VOLUME_TRACKBAR, hInstance, NULL
        );
        SendMessage(g_hVolumeTrackbar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hVolumeTrackbar, TBM_SETTICFREQ, 10, 0);
        SendMessage(g_hVolumeTrackbar, TBM_SETPOS, TRUE, 75);
        UpdateVolumeText();

        return 0;
    }

    case WM_HSCROLL:
    {
        HWND hTrackbar = (HWND)lParam;

        if (hTrackbar == g_hBrightnessTrackbar)
            UpdateBrightnessText();
        else if (hTrackbar == g_hVolumeTrackbar)
            UpdateVolumeText();

        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// 性能测试定时器回调
VOID CALLBACK PerformanceTestCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    int pos = SendMessage(g_hProgress, PBM_GETPOS, 0, 0);

    if (pos >= 100)
    {
        KillTimer(hwnd, idEvent);
        MessageBox(hwnd, L"性能测试完成！", L"提示", MB_OK);
        EnableWindow(GetDlgItem(hwnd, ID_START_BUTTON), TRUE);
        return;
    }

    pos += 5;
    if (pos > 100) pos = 100;
    SendMessage(g_hProgress, PBM_SETPOS, pos, 0);
}

// 性能设置页窗口过程
LRESULT CALLBACK PerformancePageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_UPDOWN_CLASS;
        InitCommonControlsEx(&icex);

        // CPU 限制标签
        CreateWindowEx(
            0, L"STATIC", L"CPU 使用限制:",
            WS_CHILD | WS_VISIBLE,
            20, 20, 100, 20,
            hwnd, NULL, hInstance, NULL
        );

        // CPU 限制编辑框
        g_hCpuSpinEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"50",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
            120, 20, 50, 20,
            hwnd, (HMENU)ID_CPU_SPIN_EDIT, hInstance, NULL
        );

        // CPU 限制 UpDown
        HWND hUpDown = CreateWindowEx(
            0, UPDOWN_CLASS, L"",
            WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_CPU_UPDOWN, hInstance, NULL
        );
        SendMessage(hUpDown, UDM_SETBUDDY, (WPARAM)g_hCpuSpinEdit, 0);
        SendMessage(hUpDown, UDM_SETRANGE, 0, MAKELONG(1, 100));
        SendMessage(hUpDown, UDM_SETPOS, 0, 50);

        // 百分号标签
        CreateWindowEx(
            0, L"STATIC", L"%",
            WS_CHILD | WS_VISIBLE,
            175, 20, 20, 20,
            hwnd, NULL, hInstance, NULL
        );

        // 性能测试按钮
        CreateWindowEx(
            0, L"BUTTON", L"运行性能测试",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 60, 120, 30,
            hwnd, (HMENU)ID_START_BUTTON, hInstance, NULL
        );

        // 进度条
        g_hProgress = CreateWindowEx(
            0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 100, 300, 25,
            hwnd, (HMENU)ID_PROGRESS, hInstance, NULL
        );
        SendMessage(g_hProgress, PBM_SETRANGE32, 0, 100);
        SendMessage(g_hProgress, PBM_SETPOS, 0, 0);

        return 0;
    }

    case WM_COMMAND:
    {
        WORD controlId = LOWORD(wParam);
        WORD notificationCode = HIWORD(wParam);

        if (controlId == ID_START_BUTTON && notificationCode == BN_CLICKED)
        {
            EnableWindow(GetDlgItem(hwnd, ID_START_BUTTON), FALSE);
            SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
            SetTimer(hwnd, 1, 200, PerformanceTestCallback);
        }
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// 创建标签页窗口
void CreateTabPage(HWND hParent, int index, WNDPROC wndProc, const wchar_t* title, HINSTANCE hInstance)
{
    static BOOL s_classRegistered = FALSE;
    if (!s_classRegistered)
    {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"TabPageClass";
        RegisterClass(&wc);
        s_classRegistered = TRUE;
    }

    // 创建带自定义窗口过程的标签页
    HWND hPage = CreateWindowEx(
        0, L"TabPageClass", L"",
        WS_CHILD | WS_CLIPSIBLINGS,
        10, 40, 340, 150,
        hParent, NULL, hInstance, NULL
    );

    // 子类化窗口过程
    SetWindowLongPtr(hPage, GWLP_WNDPROC, (LONG_PTR)wndProc);

    // 发送 WM_CREATE 消息
    CREATESTRUCT cs = {0};
    cs.hInstance = hInstance;
    SendMessage(hPage, WM_CREATE, 0, (LPARAM)&cs);

    // 保存到全局变量
    if (index == 0)
        g_hTabPageDisplay = hPage;
    else if (index == 1)
        g_hTabPagePerformance = hPage;
}

// 显示指定的标签页
void ShowTabPage(int index)
{
    HWND pages[] = { g_hTabPageDisplay, g_hTabPagePerformance };

    for (int i = 0; i < 2; i++)
    {
        if (pages[i])
            ShowWindow(pages[i], i == index ? SW_SHOW : SW_HIDE);
    }
}

// 主窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TAB_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建 TabControl
        HWND hTab = CreateWindowEx(
            0, WC_TABCONTROL, L"",
            WS_CHILD | WS_VISIBLE,
            10, 10, 360, 30,
            hwnd, (HMENU)ID_TABCONTROL, hInstance, NULL
        );

        // 添加标签页
        TCITEM tie = {0};
        tie.mask = TCIF_TEXT;

        tie.pszText = (LPWSTR)L"显示";
        SendMessage(hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);

        tie.pszText = (LPWSTR)L"性能";
        SendMessage(hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);

        // 创建标签页内容
        CreateTabPage(hwnd, 0, DisplayPageProc, L"显示", hInstance);
        CreateTabPage(hwnd, 1, PerformancePageProc, L"性能", hInstance);

        // 显示第一个标签页
        ShowTabPage(0);

        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        if (pnmh->idFrom == ID_TABCONTROL && pnmh->code == TCN_SELCHANGE)
        {
            HWND hTab = GetDlgItem(hwnd, ID_TABCONTROL);
            int currentIndex = SendMessage(hTab, TCM_GETCURSEL, 0, 0);
            ShowTabPage(currentIndex);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"SettingsDialog";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"系统设置",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 260,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

这个综合示例创建了一个带有两个标签页的设置对话框：

1. **显示标签页**：包含亮度和音量调节滑块
2. **性能标签页**：包含 CPU 限制微调和性能测试进度条

---

## 后续可以做什么

到这里，Win32 常用的高级控件就讲完了。你现在应该能够使用 TabControl 组织界面、用 Progress Bar 显示进度、用 Trackbar 和 UpDown 调节数值。加上之前学过的按钮、编辑框、列表框、ListView、TreeView，你的 Win32 控件工具箱已经相当完善了。

但说实话，我们今天只是学习了这些控件的基本用法。每个控件都有更多高级功能和细节，特别是 RichEdit，那是一个可以单独写一本书的复杂控件。如果你需要在实际项目中使用这些控件，建议深入阅读 MSDN 文档。

---

## 系列总结

到目前为下，我们的 Win32 原生编程系列已经涵盖了：

1. **基础概念**：开发环境、类型命名、字符串处理、窗口概念、入口点
2. **消息循环**：窗口创建、消息循环、窗口过程
3. **键盘鼠标**：键盘消息、鼠标消息、定时器
4. **基础控件**：按钮、编辑框、列表框、组合框、WM_COMMAND 机制
5. **WM_NOTIFY**：高级控件的消息通知机制
6. **ListView**：列表视图控件
7. **TreeView**：树形视图控件
8. **更多高级控件**：TabControl、Progress Bar、Trackbar、UpDown、RichEdit（本篇）

掌握了这些内容，你已经具备了开发完整 Win32 GUI 应用程序的能力。

好了，今天的文章就到这里。我们下一篇再见！

---

**相关资源**

- [TabControl 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/tab-controls)
- [Progress Bar 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/progress-bar-controls)
- [Trackbar 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/trackbar-controls)
- [UpDown 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/up-down-controls)
- [RichEdit 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/about-rich-edit-controls)
