# 通用GUI编程技术——Win32 原生编程实战（五十七）——定时器

> 上一篇文章我们聊了拖放——让你的窗口可以接收来自资源管理器的文件和其他数据。今天我们要讲的是一个看起来简单但暗藏玄机的功能：定时器。你可能觉得 SetTimer 谁不会用？但 WM_TIMER 的精度到底有多少？为什么有时候定时器消息会被"吞掉"？怎么做高精度动画？QueryPerformanceCounter 和 SetTimer 是什么关系？今天我们把定时器彻底讲透。

---

## 为什么需要定时器

定时器在 GUI 程序中有两大用途：

1. **定时任务**：每隔一段时间执行某个操作。比如自动保存（每 30 秒）、UI 状态轮询（每秒检查一次网络连接）、倒计时显示。
2. **动画**：以固定帧率更新画面。比如时钟指针每秒转动、进度条平滑增长、粒子效果更新。

Win32 提供了多种定时方案，适用于不同的精度需求：

| 方案 | 精度 | 复杂度 | 适用场景 |
|------|------|--------|---------|
| SetTimer / WM_TIMER | ~15ms | 极简 | 普通定时任务、简单动画 |
| timeSetEvent（多媒体定时器） | ~1ms | 中等 | 音频、精确计时 |
| CreateWaitableTimer | ~1ms | 中等 | 后台线程定时 |
| 独立线程 + Sleep | ~1ms | 简单 | 后台轮询 |
| QueryPerformanceCounter | <1μs | 简单 | 时间测量（不是定时触发） |

这篇文章会从最简单的 SetTimer 开始，逐步讲到高精度定时和动画循环。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

---

## 第一步——SetTimer 与 WM_TIMER

### 创建定时器

```cpp
// 方法一：基于窗口的定时器
UINT_PTR timerId = SetTimer(
    hwnd,           // 窗口句柄
    1,              // 定时器 ID（非零）
    1000,           // 间隔（毫秒），1000 = 1 秒
    NULL            // 定时器过程（NULL = 发送 WM_TIMER）
);

// 方法二：基于回调的定时器（不需要窗口）
UINT_PTR timerId = SetTimer(
    NULL,           // 无窗口
    0,              // ID（会被忽略）
    1000,           // 间隔
    TimerProc       // 回调函数
);
```

### 销毁定时器

```cpp
KillTimer(hwnd, timerId);
```

### 处理 WM_TIMER

```cpp
case WM_TIMER:
{
    UINT_PTR timerId = wParam;
    switch (timerId)
    {
    case 1:
        // 定时器 1 触发
        break;
    case 2:
        // 定时器 2 触发
        break;
    }
    return 0;
}
```

### 回调方式

```cpp
void CALLBACK TimerProc(
    HWND   hwnd,       // 调用 SetTimer 时传的窗口（可能为 NULL）
    UINT   uMsg,       // WM_TIMER
    UINT_PTR idEvent,  // 定时器 ID
    DWORD  dwTime      // 系统启动后的毫秒数（GetTickCount）
)
{
    // 定时器回调
}
```

⚠️ 注意

回调方式和 WM_TIMER 方式的选择：如果你的程序已经有窗口和消息循环，用 WM_TIMER 更简单。如果定时器需要在没有窗口的模块中使用（比如 DLL），用回调方式。

---

## 第二步——WM_TIMER 的精度问题

### 默认精度：约 15.6ms

Windows 系统定时器的默认分辨率是约 15.6ms（64Hz）。这意味着：

* `SetTimer(hwnd, 1, 1, NULL)` 实际间隔约 15ms，不是 1ms
* `SetTimer(hwnd, 1, 10, NULL)` 实际间隔约 15ms，不是 10ms
* `SetTimer(hwnd, 1, 16, NULL)` 实际间隔约 15-31ms

对于大部分 GUI 场景（每秒更新一次状态、每 100ms 做一次检查），15ms 的精度完全够用。但如果你需要做流畅的 60fps 动画（每帧 16.67ms），WM_TIMER 就不太可靠了。

### 消息合并

如果消息队列中已经有一条 WM_TIMER 等待处理，系统不会添加第二条相同 ID 的 WM_TIMER。这意味着如果你的消息处理耗时超过了定时器间隔，你会"丢失"一些定时器消息。

```
时间轴：
0ms:   WM_TIMER 入队
15ms:  WM_TIMER 入队（上一条还没处理，被合并）
20ms:  开始处理第一条 WM_TIMER（耗时 50ms）
70ms:  处理完毕
70ms:  WM_TIMER 入队（新的）
...
```

这不是 bug——这是一种优化策略，防止定时器消息堆积。但对于精确计时来说，你需要意识到这一点。

### 提高系统定时器精度

可以通过 `timeBeginPeriod` 临时提高系统定时器精度：

```cpp
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

// 提高精度到 1ms
timeBeginPeriod(1);

// ... 你的定时器操作 ...

// 恢复默认精度
timeEndPeriod(1);
```

⚠️ 注意

`timeBeginPeriod` 是全局设置，会影响整个系统的定时器分辨率（包括所有其他进程）。这会增加系统功耗。使用完后必须调用 `timeEndPeriod` 恢复。现代 Windows（Windows 10 2004+）对这个 API 的行为做了一些限制——如果调用进程没有前台窗口，可能不生效。

---

## 第三步——动画循环的标准模式

用 WM_TIMER 做动画时，不要依赖定时器间隔作为"帧时间"。正确做法是测量实际经过的时间（delta time），根据 delta time 更新状态。

### 错误做法：假设帧率恒定

```cpp
// 错误！实际帧时间可能远大于 16ms
static int x = 0;
x += 2;  // 每帧移动 2 像素
InvalidateRect(hwnd, NULL, FALSE);
```

### 正确做法：基于 delta time

```cpp
case WM_TIMER:
{
    if (wParam == ANIM_TIMER_ID)
    {
        // 获取当前时间
        static DWORD lastTime = 0;
        DWORD now = GetTickCount64();
        float deltaTime = (now - lastTime) / 1000.0f;  // 秒
        lastTime = now;

        // 根据 delta time 更新位置
        static float x = 0;
        x += 200.0f * deltaTime;  // 每秒移动 200 像素

        if (x > clientWidth)
            x = 0;

        InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
}
```

---

## 第四步——QueryPerformanceCounter：高精度时间测量

`QueryPerformanceCounter` 不是定时器——它不能"每隔 X 触发一次"。但它是做精确时间测量的首选工具。

```cpp
LARGE_INTEGER freq, start, end;

// 获取频率（每秒计数次数）
QueryPerformanceFrequency(&freq);

// 开始计时
QueryPerformanceCounter(&start);

// ... 要测量的代码 ...

// 结束计时
QueryPerformanceCounter(&end);

// 计算经过的秒数
double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
```

### 为什么不用 GetTickCount

`GetTickCount` 返回的是系统启动后的毫秒数，精度约 15ms。对于 GUI 程序的粗略计时够用，但对于动画和性能测量不够。

| 函数 | 精度 | 返回值 | 溢出 |
|------|------|--------|------|
| GetTickCount | ~15ms | DWORD（毫秒） | 约 49 天 |
| GetTickCount64 | ~15ms | ULONGLONG（毫秒） | 约 5.8 亿年 |
| QueryPerformanceCounter | <1μs | LARGE_INTEGER | 不会溢出 |

---

## 第五步——高精度定时器线程

当你需要比 WM_TIMER 更精确的定时触发（比如 1ms 级别），可以使用独立线程 + 等待机制。

### 方案一：独立线程 + Sleep

```cpp
#include <windows.h>

volatile LONG g_timerRunning = 1;

DWORD WINAPI PrecisionTimerThread(LPVOID lpParam)
{
    HWND hwnd = (HWND)lpParam;

    while (InterlockedCompareExchange(&g_timerRunning, 1, 1))
    {
        // 做精确计时的工作
        Sleep(1);  // 约 1ms（受系统定时器精度影响）

        // 通知主线程
        PostMessage(hwnd, WM_USER + 1, 0, 0);
    }

    return 0;
}
```

⚠️ 注意

`Sleep(1)` 的实际精度取决于系统定时器分辨率。默认约 15ms。如果需要真正的 1ms 精度，需要先调用 `timeBeginPeriod(1)`。

### 方案二：Waitable Timer

```cpp
DWORD WINAPI WaitableTimerThread(LPVOID lpParam)
{
    HWND hwnd = (HWND)lpParam;

    // 创建可等待定时器
    HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

    // 设置定时器：立即开始，每 10ms 触发一次
    LARGE_INTEGER dueTime = {};
    dueTime.QuadPart = 0;  // 立即开始
    SetWaitableTimer(hTimer, &dueTime, 10, NULL, NULL, FALSE);

    while (InterlockedCompareExchange(&g_timerRunning, 1, 1))
    {
        if (WaitForSingleObject(hTimer, 100) == WAIT_OBJECT_0)
        {
            PostMessage(hwnd, WM_USER + 1, 0, 0);
        }
    }

    CancelWaitableTimer(hTimer);
    CloseHandle(hTimer);

    return 0;
}
```

### 方案三：多媒体定时器 timeSetEvent

```cpp
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

void CALLBACK TimeCallback(UINT uTimerID, UINT uMsg,
    DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    HWND hwnd = (HWND)dwUser;
    PostMessage(hwnd, WM_USER + 1, 0, 0);
}

// 启动
MMRESULT timerId = timeSetEvent(
    10,                 // 间隔 10ms
    1,                  // 分辨率 1ms
    TimeCallback,       // 回调
    (DWORD_PTR)hwnd,    // 用户数据
    TIME_PERIODIC       // 周期性
);

// 停止
timeKillEvent(timerId);
```

⚠️ 注意

`timeSetEvent` 在 Windows 10 中已被标记为废弃（虽然仍可使用）。微软推荐使用 `CreateWaitableTimerEx` 配合 `SetWaitableTimerEx` 替代。但在实践中，timeSetEvent 仍然是最简单的高精度定时方案。

---

## 第六步——完整示例：模拟时钟

这个示例综合运用 WM_TIMER 和 GDI 绘制，实现一个带有时针、分针、秒针的模拟时钟。

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <math.h>

#define ID_CLOCK_TIMER 1
#define TIMER_INTERVAL 1000  // 1 秒

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 全局变量
HWND  g_hWnd = NULL;
SIZE  g_clientSize = { 400, 400 };

// 将角度（以 12 点为 0，顺时针）转换为弧度
double ClockToRadian(int hours, int minutes, int seconds)
{
    double totalSeconds = hours * 3600.0 + minutes * 60.0 + seconds;
    double fraction = totalSeconds / 43200.0;  // 12 小时 = 43200 秒
    return fraction * 2.0 * M_PI - M_PI / 2.0;
}

void DrawClock(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    int cx = rc.right / 2;
    int cy = rc.bottom / 2;
    int radius = min(cx, cy) - 20;

    // 背景
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // 表盘外圈
    HPEN hPenBorder = CreatePen(PS_SOLID, 3, RGB(60, 60, 60));
    SelectObject(hdc, hPenBorder);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    DeleteObject(hPenBorder);

    // 刻度
    for (int i = 0; i < 60; i++)
    {
        double angle = (i / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int innerR = (i % 5 == 0) ? radius - 20 : radius - 10;
        int outerR = radius - 4;

        int x1 = cx + (int)(innerR * cos(angle));
        int y1 = cy + (int)(innerR * sin(angle));
        int x2 = cx + (int)(outerR * cos(angle));
        int y2 = cy + (int)(outerR * sin(angle));

        if (i % 5 == 0)
        {
            HPEN hPenTick = CreatePen(PS_SOLID, 2, RGB(40, 40, 40));
            SelectObject(hdc, hPenTick);
            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y2);
            DeleteObject(hPenTick);
        }
        else
        {
            HPEN hPenTick = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
            SelectObject(hdc, hPenTick);
            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y2);
            DeleteObject(hPenTick);
        }
    }

    // 获取当前时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    // 时针
    {
        double angle = ClockToRadian(st.wHour, st.wMinute, st.wMinute * 60);
        int len = radius * 55 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 6, RGB(30, 30, 30));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 分针
    {
        double angle = ClockToRadian(st.wMinute, st.wSecond / 60.0, 0);
        // 分针角度 = 分钟 + 秒/60
        double totalMinutes = st.wMinute + st.wSecond / 60.0;
        angle = (totalMinutes / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int len = radius * 70 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 4, RGB(50, 50, 50));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 秒针
    {
        double angle = (st.wSecond / 60.0) * 2.0 * M_PI - M_PI / 2.0;
        int len = radius * 80 / 100;
        int x2 = cx + (int)(len * cos(angle));
        int y2 = cy + (int)(len * sin(angle));

        // 秒针尾部
        int tailLen = radius * 15 / 100;
        int xTail = cx - (int)(tailLen * cos(angle));
        int yTail = cy - (int)(tailLen * sin(angle));

        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 30, 30));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, xTail, yTail, NULL);
        LineTo(hdc, x2, y2);
        DeleteObject(hPen);
    }

    // 中心点
    HBRUSH hBrush = CreateSolidBrush(RGB(200, 30, 30));
    SelectObject(hdc, hBrush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - 5, cy - 5, cx + 5, cy + 5);
    DeleteObject(hBrush);

    // 数字
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(40, 40, 40));
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFont);

    for (int i = 1; i <= 12; i++)
    {
        double angle = (i / 12.0) * 2.0 * M_PI - M_PI / 2.0;
        int numR = radius - 35;
        int nx = cx + (int)(numR * cos(angle));
        int ny = cy + (int)(numR * sin(angle));

        wchar_t buf[8];
        swprintf_s(buf, L"%d", i);

        SIZE sz;
        GetTextExtentPoint32(hdc, buf, (int)wcslen(buf), &sz);
        TextOut(hdc, nx - sz.cx / 2, ny - sz.cy / 2, buf, (int)wcslen(buf));
    }

    DeleteObject(hFont);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 启动定时器，每秒触发一次
        SetTimer(hwnd, ID_CLOCK_TIMER, TIMER_INTERVAL, NULL);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_CLOCK_TIMER)
            InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawClock(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
        pmmi->ptMinTrackSize.x = 300;
        pmmi->ptMinTrackSize.y = 300;
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_CLOCK_TIMER);
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
    wc.lpszClassName = L"AnalogClock";
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"AnalogClock", L"模拟时钟 - WM_TIMER 示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 440,
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

1. **SetTimer + WM_TIMER**：每秒触发一次，调用 `InvalidateRect` 触发重绘。

2. **GetLocalTime**：获取当前系统时间，用于计算指针角度。

3. **角度计算**：12 点方向为起始（-π/2），顺时针旋转。每个指针的角度根据时/分/秒计算。

4. **WM_ERASEBKGND 返回 TRUE**：阻止系统擦除背景，避免闪烁。所有绘制在 WM_PAINT 中完成。

5. **WM_GETMINMAXINFO**：限制窗口最小 300x300，防止时钟缩到看不清。

6. **KillTimer**：在 WM_DESTROY 中销毁定时器，避免资源泄漏。

---

## 常见陷阱

### 陷阱一：KillTimer 后收到残留 WM_TIMER

```cpp
// KillTimer 之后，消息队列中可能还有一条 WM_TIMER
KillTimer(hwnd, timerId);

// 安全做法：用标志位防御
case WM_TIMER:
    if (!g_timerActive) return 0;  // 已停止，忽略
    // ...
```

### 陷阱二：WM_TIMER 处理中做耗时操作

```cpp
// 错误！WM_TIMER 和其他消息共享同一个线程
case WM_TIMER:
    DoExpensiveCalculation();  // 阻塞 5 秒
    // 这 5 秒内窗口完全无响应
    return 0;

// 正确：把耗时操作放到工作线程
case WM_TIMER:
    CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    return 0;
```

### 陷阱三：混淆定时精度和测量精度

`SetTimer(hwnd, 1, 1, NULL)` 不会给你 1ms 的定时精度。但 `QueryPerformanceCounter` 可以给你亚微秒级的**测量**精度。精度不是准确度——定时器可能会晚触发，但你可以用 QPC 测量到底晚了多久。

---

## 后续可以做什么

到这里，定时器的知识就讲完了。你现在应该理解了 SetTimer/WM_TIMER 的用法和精度限制、基于 delta time 的动画循环模式、QueryPerformanceCounter 的时间测量用法，以及高精度定时器线程的几种实现方案。

到这里，Win32 进阶的五个专题——子类化、Hook、系统托盘、拖放、定时器——就全部讲完了。加上消息机制补充和系统消息补充，你的 Win32 知识体系已经相当完整了。

如果想继续深入，可以尝试综合运用这些知识做一个阶段项目：

1. **基础练习**：修改模拟时钟示例，将定时器间隔从 1000ms 改为 50ms，让秒针平滑移动而不是逐秒跳动
2. **进阶练习**：实现一个简单的"番茄钟"程序——结合定时器、系统托盘、气泡通知，实现 25 分钟倒计时并显示在托盘图标上
3. **挑战练习**：实现一个多线程动画框架——工作线程用高精度定时器计算动画帧数据，通过 PostMessage 通知主线程绘制，实现流畅的 60fps 动画

---

## 相关资源

- [SetTimer function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer)
- [KillTimer function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-killtimer)
- [WM_TIMER message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer)
- [QueryPerformanceCounter function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)
- [CreateWaitableTimer function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerw)
- [timeSetEvent function - Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/dd757634(v=vs.85))
- [GetTickCount64 function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount64)
