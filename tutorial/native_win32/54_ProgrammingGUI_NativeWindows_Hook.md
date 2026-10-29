# 通用GUI编程技术——Win32 原生编程实战（五十四）——Hook 机制

> 上一篇文章我们学会了子类化——修改单个控件的消息处理。但子类化有一个局限：它只能拦截发往特定窗口的消息。如果你想在消息到达任何窗口之前就拦截它呢？比如监听全局键盘输入、拦截所有鼠标点击、或者监控系统级别的窗口事件？这就是 Hook 机制的用武之地。今天我们就来聊聊 Win32 中最强大也最容易用错的机制——Windows Hook。

---

## 为什么需要 Hook

子类化的作用范围是"一个窗口"。如果你想让程序对某种输入做出全局响应——比如按 Print Screen 时自动保存截图、按某个快捷键时全局响应、或者记录用户的所有键盘输入（在授权范围内）——子类化就不够用了。

Hook 给你提供了在消息到达目标窗口**之前**拦截它的能力。你可以在消息流中插入一个"检查站"，检查每一条消息，决定是放行还是拦截。

但能力越大，责任越大。Hook 是 Win32 中最容易出问题的机制之一——处理不当会导致系统卡顿、程序崩溃、甚至安全漏洞。所以这篇文章不仅教你"怎么用"，更重要的是教你"什么时候该用"和"怎么用才安全"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道消息机制怎么工作、子类化的概念、基本的 Win32 API 使用。

⚠️ 注意

Hook 机制涉及系统级别的消息拦截。在实际项目中使用时，请注意用户隐私和系统安全。本文所有示例仅用于教学目的。

---

## 第一步——Hook 类型速查

Windows 提供了很多种 Hook 类型，每种拦截不同的消息类别：

| Hook 类型 | 常量 | 范围 | 说明 |
|-----------|------|------|------|
| 键盘低级钩子 | WH_KEYBOARD_LL | 全局 | 拦截所有键盘输入，无需 DLL |
| 鼠标低级钩子 | WH_MOUSE_LL | 全局 | 拦截所有鼠标输入，无需 DLL |
| 键盘钩子 | WH_KEYBOARD | 进程/全局 | 拦截键盘消息（全局需 DLL） |
| 鼠标钩子 | WH_MOUSE | 进程/全局 | 拦截鼠标消息（全局需 DLL） |
| 消息钩子 | WH_GETMESSAGE | 进程/全局 | 拦截 PostMessage 投递的消息 |
| 窗口过程钩子 | WH_CALLWNDPROC | 进程/全局 | 拦截 SendMessage 发送的消息 |
| CBT 钩子 | WH_CBT | 进程/全局 | 监控窗口创建/销毁/激活等 |
| Shell 钩子 | WH_SHELL | 全局 | 监控 Shell 事件（窗口激活等） |

**最重要的区分**：低级钩子（`_LL` 后缀）vs 非低级钩子。

* **低级钩子**（WH_KEYBOARD_LL、WH_MOUSE_LL）：在消息处理的最早阶段拦截，**不需要 DLL 注入**。这是大多数应用程序应该使用的类型。
* **非低级钩子**：需要在目标进程中执行代码，全局使用时需要 DLL 注入。这个教程里不涉及 DLL 注入部分，只讲低级钩子和进程内钩子。

---

## 第二步——安装和移除 Hook

### SetWindowsHookEx

```cpp
HHOOK SetWindowsHookEx(
    int       idHook,      // Hook 类型
    HOOKPROC  lpfn,        // Hook 回调函数
    HINSTANCE hMod,        // DLL 实例句柄（低级钩子用本模块）
    DWORD     dwThreadId   // 线程 ID（0 = 全局）
);
```

### UnhookWindowsHookEx

```cpp
BOOL UnhookWindowsHookEx(HHOOK hhk);
```

### 参数说明

* **idHook**：Hook 类型，比如 WH_KEYBOARD_LL。
* **lpfn**：你的 Hook 回调函数。签名取决于 Hook 类型。
* **hMod**：对于低级钩子，传 `GetModuleHandle(NULL)`（当前 exe 的模块句柄）。对于需要 DLL 注入的全局钩子，传 DLL 的句柄。
* **dwThreadId**：
  * `0`：全局 Hook（拦截所有线程的消息）。
  * 非 0 值：只拦截指定线程的消息（进程内 Hook）。

### Hook 回调的通用规则

**必须调用 CallNextHookEx**——除非你想完全拦截这条消息。Hook 是一个链表，多个程序可以同时安装 Hook。你不调用 CallNextHookEx，后续的 Hook 就收不到这条消息，可能导致其他程序异常。

```cpp
LRESULT CALLBACK HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)  // nCode < 0 时必须直接传递
    {
        // 处理消息
        // 如果想拦截，返回非零值
        // 如果想放行，调用 CallNextHookEx
    }

    // 放行：传递给下一个 Hook
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
```

⚠️ 注意

当 `nCode < 0` 时，Windows 文档要求你直接调用 `CallNextHookEx` 并返回其结果，不要做任何处理。这是为了保持向前兼容。

---

## 第三步——WH_KEYBOARD_LL：全局键盘低级钩子

这是最常用的 Hook 类型之一。它可以拦截系统级别的键盘输入，包括所有应用程序的键盘事件。

### 回调函数签名

```cpp
LRESULT CALLBACK LowLevelKeyboardProc(
    int    nCode,      // HC_ACTION (0) 表示可以处理
    WPARAM wParam,     // 消息类型：WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP
    LPARAM lParam      // 指向 KBDLLHOOKSTRUCT 结构
);
```

### KBDLLHOOKSTRUCT 结构

```cpp
typedef struct tagKBDLLHOOKSTRUCT {
    DWORD     vkCode;      // 虚拟键码
    DWORD     scanCode;    // 扫描码
    DWORD     flags;       // 标志位
    DWORD     time;        // 时间戳
    ULONG_PTR dwExtraInfo; // 额外信息
} KBDLLHOOKSTRUCT;
```

flags 的常用位：

| 位 | 含义 |
|---|------|
| LLKHF_EXTENDED | 扩展键（右侧 Alt/Ctrl 等） |
| LLKHF_INJECTED | 由程序模拟的按键（SendInput 等） |
| LLKHF_ALTDOWN | Alt 键被按下 |
| LLKHF_UP | 键被释放（没有此位表示按下） |

### 完整示例：监听 Print Screen 键

这个示例程序会在后台监听 Print Screen 键，按下时弹出一个通知而不是让系统截图。

```cpp
#include <windows.h>
#include <stdio.h>

HHOOK g_hHook = NULL;
HWND  g_hWnd = NULL;
BOOL  g_running = TRUE;

// 低级键盘钩子回调
LRESULT CALLBACK LowLevelKeyboardProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;

        // 检测 Print Screen 键按下
        if (pKbd->vkCode == VK_SNAPSHOT &&
            (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
        {
            // 通知主窗口
            PostMessage(g_hWnd, WM_USER + 1, 0, 0);

            // 拦截此按键（不让系统截图）
            return 1;
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Hook 线程（低级钩子需要消息循环）
DWORD WINAPI HookThread(LPVOID lpParam)
{
    // 安装低级键盘钩子
    g_hHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0  // 0 = 全局
    );

    if (!g_hHook)
    {
        MessageBox(NULL, L"安装 Hook 失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 低级钩子需要消息循环才能工作
    MSG msg = {};
    while (g_running && GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 移除 Hook
    UnhookWindowsHookEx(g_hHook);
    g_hHook = NULL;

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_USER + 1:
        MessageBox(hwnd,
            L"检测到 Print Screen 按键，已被拦截。\n"
            L"此示例仅用于教学目的。",
            L"Hook 通知", MB_OK | MB_ICONINFORMATION);
        return 0;

    case WM_DESTROY:
        g_running = FALSE;
        // 向 Hook 线程发送消息以退出消息循环
        PostThreadMessage(GetWindowThreadProcessId(hwnd, NULL), WM_QUIT, 0, 0);
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
    wc.lpszClassName = L"HookDemoClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"HookDemoClass",
        L"Hook 示例 - Print Screen 监听",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return 0;

    // 显示提示信息
    CreateWindowEx(0, L"STATIC",
        L"程序正在监听 Print Screen 键。\r\n\r\n"
        L"按下 Print Screen 后按键会被拦截，\r\n"
        L"同时弹出通知对话框。\r\n\r\n"
        L"关闭窗口退出程序。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 350, 140,
        g_hWnd, NULL, hInstance, NULL);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 启动 Hook 线程
    CreateThread(NULL, 0, HookThread, NULL, 0, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
```

### 代码要点解析

1. **独立的 Hook 线程**：低级钩子的回调需要在安装它的线程的消息循环中被调用。所以我们在一个独立线程中安装 Hook 并运行消息循环。

2. **GetModuleHandle(NULL)**：获取当前 exe 的模块句柄。低级钩子不需要 DLL，直接用 exe 模块即可。

3. **PostMessage 通知主窗口**：Hook 回调在工作线程中执行，不应直接操作 UI。通过 PostMessage 通知主窗口。

4. **返回 1 拦截按键**：Hook 回调返回非零值可以阻止消息继续传递。这里拦截 Print Screen，系统不会截图。

5. **WM_DESTROY 中的清理**：设置标志位 `g_running = FALSE`，然后向 Hook 线程发送 WM_QUIT 使其退出消息循环。Hook 线程在退出前调用 `UnhookWindowsHookEx`。

---

## 第四步——WH_MOUSE_LL：全局鼠标低级钩子

鼠标低级钩子的结构和键盘类似。

### 回调函数签名

```cpp
LRESULT CALLBACK LowLevelMouseProc(
    int    nCode,
    WPARAM wParam,     // WM_MOUSEMOVE, WM_LBUTTONDOWN 等
    LPARAM lParam      // 指向 MSLLHOOKSTRUCT 结构
);
```

### MSLLHOOKSTRUCT 结构

```cpp
typedef struct tagMSLLHOOKSTRUCT {
    POINT     pt;          // 鼠标屏幕坐标
    DWORD     mouseData;   // 滚轮 delta 或 X 按钮编号
    DWORD     flags;       // 标志（LLMHF_INJECTED 等）
    DWORD     time;        // 时间戳
    ULONG_PTR dwExtraInfo;
} MSLLHOOKSTRUCT;
```

### 简要示例：记录鼠标点击位置

```cpp
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;

        if (wParam == WM_LBUTTONDOWN)
        {
            wchar_t buf[128];
            swprintf_s(buf, L"鼠标左键点击：( %d, %d )",
                pMouse->pt.x, pMouse->pt.y);
            // 可以通过 PostMessage 通知主窗口显示
        }
    }

    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

// 安装
HHOOK hMouseHook = SetWindowsHookEx(
    WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
```

---

## 第五步——进程内 Hook：WH_GETMESSAGE

低级钩子拦截所有线程的消息，但有时候你只想拦截自己进程内的消息。这时候可以用 WH_GETMESSAGE 或 WH_CALLWNDPROC，指定线程 ID。

### WH_GETMESSAGE 示例

WH_GETMESSAGE 可以拦截通过 PostMessage 投递的消息（以及 WM_PAINT、WM_TIMER 等系统消息）：

```cpp
// 全局 Hook 句柄
HHOOK g_hMsgHook = NULL;

// 消息钩子回调
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        MSG* pMsg = (MSG*)lParam;

        // 拦截主窗口的 WM_ERASEBKGND
        if (pMsg->message == WM_ERASEBKGND && pMsg->hwnd == g_hWnd)
        {
            pMsg->message = WM_NULL;  // 替换为空消息，等于丢弃
        }
    }

    return CallNextHookEx(g_hMsgHook, nCode, wParam, lParam);
}

// 安装——只拦截当前线程的消息
g_hMsgHook = SetWindowsHookEx(
    WH_GETMESSAGE,
    GetMsgProc,
    NULL,               // 进程内 Hook，传 NULL
    GetCurrentThreadId() // 只拦截当前线程
);

// 移除
UnhookWindowsHookEx(g_hMsgHook);
```

注意：进程内 Hook 的 `hMod` 参数传 NULL（因为是同一个模块），`dwThreadId` 传具体的线程 ID。

---

## 第六步——性能与安全注意事项

### 时间限制

低级钩子回调在系统关键路径上执行。如果回调耗时过长，会影响整个系统的输入响应。Windows 有一条硬性规则：

**低级钩子回调执行时间不能超过约 300ms**（具体值取决于系统版本）。超过这个时间，Windows 会自动移除你的 Hook，你的程序就收不到后续消息了。

所以：
* 不要在 Hook 回调中做耗时操作
* 不要在 Hook 回调中调用 Sleep
* 不要在 Hook 回调中做网络请求或文件 I/O
* 如果需要做复杂处理，用 PostMessage 转发给工作线程

### 安全与隐私

Hook 机制可以监听所有键盘和鼠标输入，这既是强大的工具，也是安全风险：

* **不要用 Hook 做恶意监听**——未经用户同意记录键盘输入是违法的
* **注意防病毒软件**——某些安全软件会拦截或标记安装了全局 Hook 的程序
* **只 Hook 你需要的**——安装了 Hook 就要尽快卸载，不要一直挂着
* **使用 LLKHF_INJECTED 检测**——可以区分真实用户输入和程序模拟的输入

### Hook vs 子类化

| 场景 | 推荐 |
|------|------|
| 修改单个控件的行为 | 子类化 |
| 只拦截自己进程的消息 | 进程内 Hook 或子类化 |
| 需要拦截所有键盘/鼠标输入 | 低级 Hook |
| 全局快捷键 | RegisterHotKey（优先考虑） |
| 需要修改消息参数 | 子类化或进程内 Hook |

⚠️ 注意

如果你只是想实现"全局快捷键"，优先使用 `RegisterHotKey` 而不是 Hook。RegisterHotKey 是专门为这个场景设计的 API，更轻量、更安全、不需要消息循环。

```cpp
// 注册全局快捷键 Ctrl+Shift+K
RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_SHIFT, 'K');

// 在 WndProc 中处理
case WM_HOTKEY:
    if (wParam == 1)  // ID 1
    {
        // Ctrl+Shift+K 被按下
    }
    break;

// 取消注册
UnregisterHotKey(hwnd, 1);
```

---

## 常见陷阱

### 陷阱一：低级钩子回调耗时过长

```cpp
// 错误！在回调中做耗时操作
LRESULT CALLBACK BadHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Sleep(500);  // 系统 300ms 后自动移除你的 Hook！
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
```

### 陷阱二：忘记调用 CallNextHookEx

```cpp
// 错误！拦截了所有消息但不传递
LRESULT CALLBACK BadHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // 什么都不做，也不调用 CallNextHookEx
    return 0;  // 这会阻止所有后续 Hook 和目标窗口
}
```

### 陷阱三：在错误的线程安装 Hook

低级钩子必须在安装它的线程中有消息循环才能工作。如果你在主线程安装 Hook 但主线程在忙别的事情（比如在等待信号量），Hook 回调就不会被调用。

### 陷阱四：忘记 UnhookWindowsHookEx

程序退出前必须卸载 Hook。否则系统会保留一个指向已卸载模块的函数指针，下次触发 Hook 时可能导致崩溃。

---

## 后续可以做什么

到这里，Windows Hook 机制就讲完了。你现在应该理解了 Hook 的类型（低级 vs 非低级，全局 vs 进程内）、安装和移除的标准流程、低级键盘和鼠标钩子的具体用法、进程内消息钩子的使用，以及性能和安全方面的注意事项。

下一篇文章，我们会聊一个更"实用"的话题——**系统托盘**。你将学会如何让你的程序最小化到系统托盘、显示托盘图标和右键菜单、处理气球通知。

在此之前，建议你做一些练习巩固今天的知识：

1. **基础练习**：修改 Print Screen 监听示例，改为监听 Win+E 组合键（提示：检查 vkCode 和 LLKHF_ALTDOWN 等标志）
2. **进阶练习**：使用 WH_MOUSE_LL 实现一个简单的"鼠标轨迹记录器"——记录所有左键点击的坐标，在主窗口中绘制轨迹线
3. **挑战练习**：分别用 Hook 和子类化两种方案实现"按键记录器"（只记录自身窗口的输入），比较两种方案的优缺点

---

## 相关资源

- [SetWindowsHookEx function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [UnhookWindowsHookEx function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex)
- [CallNextHookEx function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-callnexthookex)
- [LowLevelKeyboardProc callback - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)
- [KBDLLHOOKSTRUCT structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct)
- [Using Hooks - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-hooks)
- [RegisterHotKey function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)
