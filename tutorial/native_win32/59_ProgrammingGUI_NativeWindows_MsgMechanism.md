# 通用GUI编程技术——Win32 原生编程实战（五十九）——消息机制补充：SendMessage、PostMessage 与跨线程通信

> 上一篇文章我们聊了工具栏和状态栏，你的 Win32 程序现在已经有了"正经桌面应用"的模样——菜单、工具栏、编辑框、状态栏一应俱全。但一直以来，我们都是在一个线程里处理所有事情：创建窗口、处理消息、更新 UI。这在简单程序里没问题，但一旦你需要在后台做耗时操作（比如加载大文件、网络请求、复杂计算），主线程就会被阻塞，窗口冻结、拖不动、点不了——用户体验灾难。今天我们就来把 Win32 消息机制真正吃透：SendMessage 和 PostMessage 到底有什么区别？跨线程通信怎么才能不死锁？工作线程怎么安全地更新 UI？

---

## 为什么需要深入理解消息机制

说实话，前面那篇讲消息机制的文章（第一篇）帮你搭起了基本框架——消息循环、窗口过程、常用消息。但那篇文章有意回避了一些比较底层的问题：消息是怎么从"投递"变成"处理"的？SendMessage 为什么有时候会卡死？PostMessage 传的指针为什么有时候会变成野指针？

这些问题在写简单程序的时候不太会遇到，但当你开始写多线程程序、做插件系统、或者调试一些莫名其妙的卡死问题的时候，它们就会变成拦路虎。

更重要的是，现代 GUI 程序几乎都是多线程的。后台线程负责计算和网络通信，主线程负责 UI 更新。这两者之间的桥梁就是消息机制。不理解 SendMessage 和 PostMessage 的区别，你就无法正确地实现线程间通信。

这篇文章会带你从消息队列的内部结构开始，一步步搞懂 Win32 消息机制的每一个细节。学完之后，你会对"消息是怎么流动的"有一个清晰的心智模型，遇到卡死、消息丢失、野指针等问题时也能快速定位原因。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道消息循环怎么写、窗口过程怎么处理消息、基本的 Win32 API 使用。如果这些概念对你来说还比较陌生，建议先去看看第一篇文章。

---

## 第一步——回顾：消息循环的本质

在深入之前，我们先来快速回顾一下消息循环的基本结构。你可能已经写过无数遍了：

```cpp
MSG msg = {};
while (GetMessage(&msg, NULL, 0, 0))
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

这三行代码做了什么？

1. **GetMessage**：从消息队列里取出一条消息。如果队列是空的，线程会阻塞（休眠），直到有新消息到来。
2. **TranslateMessage**：把虚拟键消息翻译成字符消息（比如把 WM_KEYDOWN 翻译成 WM_CHAR）。这一步对键盘输入很关键，但不是所有消息都需要。
3. **DispatchMessage**：根据消息的目标窗口，调用对应的窗口过程（WndProc）来处理。

关键洞察：**每个线程都有自己独立的消息队列**。这意味着，如果你在工作线程里创建了一个窗口，那个线程也需要有自己的消息循环。这也是为什么很多新手在后台线程创建窗口后，发现窗口"没反应"——因为那个线程根本没有在跑消息循环。

---

## 第二步——SendMessage 与 PostMessage：同步 vs 异步

这是今天最重要的一个对比。很多人混淆了这两个函数，导致各种难以排查的 bug。

### 函数签名对比

```cpp
// 同步发送：等待对方处理完毕才返回
LRESULT SendMessage(
    HWND   hWnd,    // 目标窗口
    UINT   Msg,     // 消息 ID
    WPARAM wParam,  // 参数 1
    LPARAM lParam   // 参数 2
);

// 异步投递：把消息放进队列就返回，不等处理
BOOL PostMessage(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam
);
```

看起来参数一模一样，但行为完全不同。

### 核心区别：调用方式

**SendMessage** 是"直接调用"——它绕过消息队列，直接调用目标窗口的窗口过程。调用者会**阻塞**，直到窗口过程返回，然后 SendMessage 带着返回值回来。

```
线程 A                          窗口过程
  |                               |
  |---SendMessage(WM_X)--------->|
  |       （阻塞等待）             |  处理消息
  |<--------return result---------|
  |                               |
```

**PostMessage** 是"投递"——它把消息放进目标线程的消息队列，然后**立即返回**。消息会在目标线程的 GetMessage 循环中被取出来处理。

```
线程 A                          消息队列          窗口过程
  |                               |                |
  |---PostMessage(WM_X)--------->|                |
  |  （立即返回 TRUE）             |                |
  |                               |----GetMessage->|
  |                               |                | 处理消息
```

### 详细对比表

| 特性 | SendMessage | PostMessage |
|------|-------------|-------------|
| 执行方式 | 直接调用窗口过程 | 投递到消息队列 |
| 返回时机 | 窗口过程返回后 | 消息入队后立即 |
| 返回值 | 窗口过程的返回值 | BOOL（是否投递成功） |
| 是否阻塞 | 是 | 否 |
| 处理顺序 | 立即处理 | 排队等待 |
| 跨线程 | 复杂（有死锁风险） | 简单安全 |
| 适用场景 | 需要立即得到结果 | 异步通知、状态更新 |

### 同一线程内的 SendMessage

当 SendMessage 的发送者和接收者在**同一线程**时，事情很简单——就是一次直接的函数调用：

```cpp
// 同一线程内，SendMessage = 直接函数调用
LRESULT result = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);
// result 就是编辑框文字的长度，到这里时已经拿到了
```

这时候不存在死锁风险，效率也很高。你在处理 WM_COMMAND 时调用 SendMessage 获取控件状态，就是这种情况。

### 跨线程的 SendMessage

当发送者和接收者在**不同线程**时，事情就复杂了。发送线程会阻塞，等待接收线程在它的消息循环里处理这条消息。注意：接收线程不会在处理其他消息的中途被 SendMessage 打断——它必须回到 GetMessage/DispatchMessage 循环，系统才会在那里处理这条跨线程 SendMessage。

这就是死锁的温床：

```
线程 A（主线程）                  线程 B（工作线程）
  |                                |
  |---SendMessage(B的窗口)-------->|
  |    （阻塞，等 B 处理）          |
  |                                |---SendMessage(A的窗口)--->
  |                                |    （阻塞，等 A 处理）
  |                                |
  | <----------互相等待============>|
             死锁！
```

两个线程互相 SendMessage 给对方的窗口，双方都在等对方处理，谁也动不了。这是 Win32 多线程编程中最经典的死锁模式。

---

## 第三步——其他消息函数

除了 SendMessage 和 PostMessage，Windows 还提供了几个"中间态"的消息函数，它们各有适用场景。

### SendNotifyMessage

```cpp
BOOL SendNotifyMessage(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam
);
```

如果目标窗口在**同一线程**，行为和 SendMessage 一样（同步调用）。

如果目标窗口在**不同线程**，行为类似 PostMessage（不等待处理完毕），但消息的优先级高于普通的 PostMessage 消息——它会被放在发送消息队列（sent-message queue）中，比投递消息队列（posted-message queue）优先处理。

适用场景：你想发一个通知给另一个线程，不需要知道处理结果，但希望对方尽快处理。

### SendMessageCallback

```cpp
BOOL SendMessageCallback(
    HWND   hWnd,
    UINT   Msg,
    WPARAM wParam,
    LPARAM lParam,
    SENDASYNCPROC lpCallBack,  // 回调函数
    ULONG_PTR dwData           // 传给回调的用户数据
);
```

跨线程版本的"异步 SendMessage"——投递消息后立即返回，当接收线程处理完消息后，系统会在**发送线程**的上下文中调用你提供的回调函数。

回调函数的原型：

```cpp
typedef void (CALLBACK* SENDASYNCPROC)(
    HWND   hWnd,       // 目标窗口
    UINT   uMsg,       // 消息 ID
    ULONG_PTR dwData,  // 你传的用户数据
    LRESULT lResult    // 窗口过程的返回值
);
```

适用场景：你需要跨线程发送消息并获取返回值，但又不想阻塞发送线程。

### PostThreadMessage

```cpp
BOOL PostThreadMessage(
    DWORD  dwThreadId,  // 目标线程 ID
    UINT   Msg,         // 消息 ID
    WPARAM wParam,
    LPARAM lParam
);
```

直接给一个线程（而不是窗口）发消息。目标线程会在 GetMessage 中收到这条消息，但 `msg.hwnd` 是 NULL——因为它不关联任何窗口。

```cpp
// 目标线程的消息循环
MSG msg = {};
while (GetMessage(&msg, NULL, 0, 0))
{
    if (msg.hwnd == NULL)
    {
        // 这是一条线程消息（由 PostThreadMessage 投递）
        switch (msg.message)
        {
        case WM_USER + 1:
            // 处理自定义线程消息
            break;
        }
        continue;  // 不要 DispatchMessage，没有窗口过程
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

⚠️ 注意

PostThreadMessage 投递的消息 `msg.hwnd` 为 NULL。如果你不小心把它传给了 DispatchMessage，什么都不会发生（因为没有目标窗口），但也不会报错。所以最好在消息循环里检查 `msg.hwnd` 是否为 NULL，提前处理线程消息。

### 消息函数选择指南

| 场景 | 推荐函数 |
|------|---------|
| 同线程获取控件状态 | SendMessage |
| 同线程触发控件操作 | SendMessage |
| 跨线程通知 UI 更新 | PostMessage |
| 跨线程传结果，不急 | PostMessage |
| 跨线程需要返回值，不阻塞 | SendMessageCallback |
| 跨线程通知，优先级高 | SendNotifyMessage |
| 给没有窗口的线程发消息 | PostThreadMessage |

---

## 第四步——消息队列的两层结构

这里要揭开一个很多教程都不讲的知识点：每个线程的消息队列其实有多个子队列，它们的优先级不同。

### 发送消息队列（Sent-message queue）

存放跨线程 SendMessage、SendNotifyMessage 发来的消息。这些消息的优先级**最高**——在 GetMessage 返回任何投递消息之前，系统会先处理完所有发送消息队列里的消息。

### 投递消息队列（Posted-message queue）

存放 PostMessage 投递的消息，以及 WM_PAINT、WM_TIMER 等系统生成的消息。

### 优先级排序

当线程调用 GetMessage 时，消息的处理优先级是：

1. **发送消息队列**（最高）—— 跨线程 SendMessage
2. **QS_SENDMESSAGE 标志** —— 系统内部标记
3. **投递消息队列** —— PostMessage、用户输入等
4. **QS_PAINT 标志** —— WM_PAINT
5. **QS_TIMER 标志** —— WM_TIMER（最低）

这意味着：如果有人不停地给你 SendMessage，你的 PostMessage 消息可能永远得不到处理。这也是为什么跨线程 SendMessage 要格外小心。

---

## 第五步——InSendMessage 与 ReplyMessage

当你的窗口过程在处理一条跨线程 SendMessage 时，你可能想知道"我现在是不是在帮另一个线程干活"。

### InSendMessage

```cpp
BOOL InSendMessage();
```

返回 TRUE 表示当前窗口过程正在处理一条来自其他线程的 SendMessage。这个函数在你需要判断是否要避免某些耗时操作时很有用。

### ReplyMessage

```cpp
BOOL ReplyMessage(LRESULT lResult);
```

在处理跨线程 SendMessage 时，提前回复发送方。调用后，发送方的 SendMessage 就会立即返回（带着你提供的 lResult），但你的窗口过程可以继续执行后续代码。

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_USER + 100)
    {
        if (InSendMessage())
        {
            // 提前回复发送方，避免对方阻塞太久
            ReplyMessage(42);  // 返回值 42

            // 现在可以安全地做耗时操作，不会阻塞发送方了
            Sleep(5000);  // 模拟耗时操作
        }
        return 42;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

⚠️ 注意

ReplyMessage 只能在处理跨线程 SendMessage 时调用。如果是同线程 SendMessage，调用它没有效果（也不报错，但无意义）。

---

## 第六步——跨线程安全通知模式

现在我们把前面的知识组合起来，实现一个标准的多线程 UI 更新模式。

### 核心原则

1. **工作线程绝不直接操作 UI 控件**（不调用 SendMessage 获取/设置控件状态）
2. **工作线程通过 PostMessage 通知主线程**
3. **主线程在 WndProc 中处理通知，更新 UI**

### 消息定义

使用 `WM_APP` 作为自定义消息的起点（避免与系统消息和控件消息冲突）：

```cpp
// 自定义消息定义
#define WM_WORK_PROGRESS   (WM_APP + 1)  // 工作进度通知
#define WM_WORK_COMPLETED  (WM_APP + 2)  // 工作完成通知
#define WM_WORK_ERROR      (WM_APP + 3)  // 工作出错通知
```

为什么用 `WM_APP` 而不是 `WM_USER`？`WM_USER` 的范围（0x0400-0x7FFF）被很多控件类内部使用了。`WM_APP`（0x8000-0xBFFF）是微软推荐的应用程序自定义消息范围，更安全。

### 数据传递方式

PostMessage 的 wParam 和 lParam 只能传整数/指针。如果需要传复杂数据，有两种安全做法：

**方法一：用 wParam 和 lParam 编码简单数据**

```cpp
// 发送方（工作线程）
PostMessage(hMainWnd, WM_WORK_PROGRESS, currentStep, totalSteps);

// 接收方（主线程 WndProc）
case WM_WORK_PROGRESS:
{
    int currentStep = (int)wParam;
    int totalSteps = (int)lParam;
    // 更新进度条
    break;
}
```

**方法二：用堆分配传递复杂数据**

```cpp
// 发送方（工作线程）
struct WorkResult {
    int code;
    wchar_t message[256];
};

WorkResult* result = new WorkResult{};
result->code = 0;
wcscpy_s(result->message, L"处理完成");
PostMessage(hMainWnd, WM_WORK_COMPLETED, 0, (LPARAM)result);

// 接收方（主线程 WndProc）
case WM_WORK_COMPLETED:
{
    WorkResult* result = (WorkResult*)lParam;
    // 使用 result...
    MessageBox(hwnd, result->message, L"完成", MB_OK);
    delete result;  // 必须释放！
    break;
}
```

⚠️ 注意

**lParam 传指针的陷阱**：工作线程里如果传了栈上变量的指针（局部变量），PostMessage 返回后变量就被销毁了——主线程收到消息时，那个指针已经是野指针。必须用 `new` 分配堆内存，由接收方负责 `delete`。

### 完整示例：后台计算进度条

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define IDC_PROGRESS  1001
#define IDC_STATUS    1002
#define IDC_START_BTN 1003

// 自定义消息
#define WM_WORK_PROGRESS   (WM_APP + 1)
#define WM_WORK_COMPLETED  (WM_APP + 2)

// 全局变量
HWND g_hWnd = NULL;
HWND g_hProgress = NULL;
HWND g_hStatus = NULL;
HWND g_hBtnStart = NULL;
volatile LONG g_isRunning = 0;

// 工作线程函数
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    const int totalSteps = 100;

    for (int i = 1; i <= totalSteps; i++)
    {
        // 模拟耗时操作
        Sleep(50);

        // 通知主线程更新进度
        PostMessage(g_hWnd, WM_WORK_PROGRESS, i, totalSteps);
    }

    // 通知主线程完成
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, 0);

    InterlockedExchange(&g_isRunning, 0);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建进度条
        g_hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 20, 400, 25,
            hwnd, (HMENU)IDC_PROGRESS,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 创建状态文本
        g_hStatus = CreateWindowEx(0, L"STATIC", L"点击\"开始\"按钮启动计算",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 55, 400, 20,
            hwnd, (HMENU)IDC_STATUS,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 创建开始按钮
        g_hBtnStart = CreateWindowEx(0, L"BUTTON", L"开始计算",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 85, 120, 30,
            hwnd, (HMENU)IDC_START_BTN,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // 设置进度条范围
        SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_START_BTN)
        {
            if (InterlockedCompareExchange(&g_isRunning, 1, 0) == 0)
            {
                // 禁用按钮，防止重复点击
                EnableWindow(g_hBtnStart, FALSE);
                SetWindowText(g_hStatus, L"计算中...");

                // 启动工作线程
                CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
            }
        }
        return 0;
    }

    case WM_WORK_PROGRESS:
    {
        int current = (int)wParam;
        int total = (int)lParam;

        // 更新进度条
        SendMessage(g_hProgress, PBM_SETPOS, current, 0);

        // 更新状态文本
        wchar_t buf[64];
        swprintf_s(buf, L"进度：%d / %d (%.0f%%)",
            current, total, (double)current / total * 100.0);
        SetWindowText(g_hStatus, buf);

        return 0;
    }

    case WM_WORK_COMPLETED:
    {
        SetWindowText(g_hStatus, L"计算完成！");
        EnableWindow(g_hBtnStart, TRUE);
        MessageBox(hwnd, L"后台计算已完成", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
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
    // 初始化通用控件
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CrossThreadDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"CrossThreadDemo", L"跨线程通信示例",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 170,
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

1. **工作线程只做两件事**：执行计算 + PostMessage 通知主线程。绝不直接操作 UI 控件。

2. **InterlockedCompareExchange**：用原子操作防止用户在计算进行中重复点击"开始"按钮。比用 critical section 更轻量。

3. **PostMessage 传简单数据**：进度和总步数直接用 wParam 和 lParam 传递，不需要堆分配。

4. **主线程的 WM_WORK_PROGRESS 处理**：在这里调用 SendMessage 操作进度条是安全的，因为已经在主线程。

---

## 常见陷阱

### 陷阱一：工作线程中调用 SendMessage

```cpp
// 错误！工作线程直接操作 UI 控件
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    // 如果主线程正在等待这个线程...死锁！
    SendMessage(g_hProgress, PBM_SETPOS, 50, 0);
    return 0;
}
```

**正确做法**：工作线程用 PostMessage 通知主线程，主线程来更新控件。

### 陷阱二：PostMessage 传递栈指针

```cpp
// 错误！buf 是栈上局部变量
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    wchar_t buf[256];
    wcscpy_s(buf, L"处理完成");
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, (LPARAM)buf);
    // 函数返回后 buf 被销毁，主线程收到消息时 lParam 是野指针
    return 0;
}
```

**正确做法**：用 `new` 分配，接收方 `delete`。

### 陷阱三：窗口销毁后仍 PostMessage

```cpp
// 错误！窗口已经关闭了
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    // 如果用户已经关闭了窗口，g_hWnd 可能无效
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, 0);
    return 0;
}
```

**正确做法**：在工作线程中检查窗口是否仍然有效（用 `IsWindow` 函数），或者用一个标志位通知线程退出。

```cpp
if (IsWindow(g_hWnd))
{
    PostMessage(g_hWnd, WM_WORK_COMPLETED, 0, 0);
}
```

更健壮的做法是：在 WM_DESTROY 中设置一个事件对象（Event），工作线程在每次循环中检查这个事件，如果窗口已关闭就提前退出。

---

## 后续可以做什么

到这里，Win32 消息机制的深层知识就补充完了。你现在应该理解了 SendMessage 和 PostMessage 的本质区别、消息队列的优先级结构、以及跨线程安全通信的标准模式。这些知识在后面的文章中会反复用到——系统托盘、定时器、自定义控件等场景都需要线程间通信。

下一篇文章，我们会补充一些**常用的系统消息**——WM_MOUSEWHEEL、WM_NCHITTEST、WM_GETMINMAXINFO、WM_ERASEBKGND 等。这些消息在你做实际项目的时候几乎一定会遇到，但前面的文章没有详细展开。

在此之前，建议你先做一些练习巩固今天的知识：

1. **基础练习**：修改上面的进度条示例，添加一个"取消"按钮，点击后通知工作线程提前退出（提示：用 Event 或 volatile 标志位）
2. **进阶练习**：实现一个"双线程协作"程序——工作线程每隔 100ms 生成一个随机数，通过 PostMessage 发给主线程显示，主线程可以暂停/恢复工作线程
3. **挑战练习**：用 SendMessageCallback 实现跨线程请求-响应模式——主线程向工作线程发送一个"查询"消息，工作线程异步处理后通过回调返回结果

---

## 相关资源

- [SendMessage function (winuser.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessage)
- [PostMessage function (winuser.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew)
- [SendNotifyMessage function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendnotifymessagew)
- [SendMessageCallback function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagecallbackw)
- [PostThreadMessage function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagew)
- [About Messages and Message Queues - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
- [InSendMessage function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-insendmessage)
