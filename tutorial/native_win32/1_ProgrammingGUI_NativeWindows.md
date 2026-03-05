# Win32 原生编程实战——从 Hello World 到完整窗口程序

> 这是我们的第一篇实战笔记，之前在 《通用GUI编程指南——从0开始 Win32 编程实战指南》 里聊了各种概念，现在我们要真正动手写代码了。

## 写一个真正的 Windows 程序

我们先来看完整的代码——大概 50 行左右，不计算空行和注释：

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // 注册窗口类
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,                              // 可选窗口样式
        CLASS_NAME,                     // 窗口类名
        L"Learn to Program Windows",    // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式

        // 位置和大小
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // 父窗口句柄
        NULL,       // 菜单句柄
        hInstance,  // 实例句柄
        NULL        // 额外应用数据
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // 消息循环
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
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
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 所有绘制操作都在 BeginPaint 和 EndPaint 之间进行

            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这段代码虽然不长，但包含了 Win32 程序的核心骨架。我们把它拆开来看。

## 第一步：注册窗口类

在创建窗口之前，我们得先告诉 Windows：「我想要一种什么样的窗口」。这就像是在餐馆点菜前先看看菜单——你需要描述你想要的东西。

**等等，为什么叫「窗口类」？**

这可不是 C++ 的 `class`。在 Win32 里，「窗口类」指的是操作系统内部的一组数据结构，用来定义一类窗口的**共同行为**。比如所有按钮都差不多——点击会响应、有文字标签、能聚焦。这些共同行为就是一个「窗口类」。

```
窗口类 = 菜谱
窗口实例 = 按菜谱做出来的一道菜

一道菜（一个窗口实例）可以有自己的特点（不同的文字、位置），
但基本做法（行为）来自同一个菜谱（窗口类）。
```

注册窗口类用 `WNDCLASS` 结构：

```cpp
WNDCLASS wc = { };

wc.lpfnWndProc   = WindowProc;      // 窗口过程函数指针
wc.hInstance     = hInstance;       // 应用程序实例句柄
wc.lpszClassName = CLASS_NAME;      // 类名

RegisterClass(&wc);
```

`WNDCLASS` 还有很多其他字段，这里设为零表示使用默认值。三个必填字段：

| 字段         | 作用                             |
| ------------ | -------------------------------- |
| `lpfnWndProc` | 指向窗口过程函数的指针           |
| `hInstance`   | 应用程序实例句柄（从 `wWinMain` 来） |
| `lpszClassName` | 类名字符串（当前进程内唯一即可） |

**关于类名的一点提醒**

标准控件的类名已经被占用了，比如按钮的类名就是 `"BUTTON"`。所以选类名的时候避开这些保留字，不然你会创建出一个按钮而不是你想要的窗口。

## 第二步：创建窗口

注册完类之后，就可以创建窗口实例了：

```cpp
HWND hwnd = CreateWindowEx(
    0,                              // 扩展窗口样式
    CLASS_NAME,                     // 窗口类名
    L"Learn to Program Windows",    // 窗口文字（标题栏显示）
    WS_OVERLAPPEDWINDOW,            // 窗口样式

    // 位置和大小
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

    NULL,       // 父窗口句柄
    NULL,       // 菜单句柄
    hInstance,  // 实例句柄
    NULL        // 额外应用数据
);
```

`CreateWindowEx` 的参数确实有点多，我们按重要性来过一遍：

### 窗口样式

`WS_OVERLAPPEDWINDOW` 是几个标志的按位或组合：

```cpp
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED     | \
                             WS_CAPTION        | \
                             WS_SYSMENU        | \
                             WS_THICKFRAME     | \
                             WS_MINIMIZEBOX    | \
                             WS_MAXIMIZEBOX)
```

翻译成人话就是：一个标准的带标题栏、系统菜单、可调整大小、有最小化/最大化按钮的应用程序窗口。这是最常用的组合。

### 位置和大小

`CW_USEDEFAULT` 意思是「你看着办」。Windows 会自动选个位置和大小，挺好用的。

### 父窗口和菜单

顶层窗口的父窗口是 `NULL`。如果不使用菜单，菜单句柄也是 `NULL`。

### 那个神秘的最后一个参数

最后一个参数是 `void*` 类型的指针，可以传递任意数据。窗口创建时会收到 `WM_NCCREATE` 和 `WM_CREATE` 消息，你可以在那时候从 `lParam` 里把这个指针掏出来。这是「将数据关联到窗口」的官方推荐方式，后面「管理应用程序状态」章节会细讲。

### 显示窗口

`CreateWindowEx` 创建的窗口默认是隐藏的，需要调用 `ShowWindow` 显示出来：

```cpp
ShowWindow(hwnd, nCmdShow);
```

`nCmdShow` 参数来自 `wWinMain`，操作系统会告诉你窗口应该怎么显示——正常、最小化还是最大化。

## 第三步：消息循环

窗口创建好了，但程序还不会动。我们需要一个循环来持续处理消息：

```cpp
MSG msg = { };
while (GetMessage(&msg, NULL, 0, 0) > 0)
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}
```

这就是传说中的**消息循环**。

### 什么是消息？

消息就是一个数值代码，代表某个事件：

```cpp
#define WM_LBUTTONDOWN    0x0201   // 鼠标左键按下
#define WM_KEYDOWN        0x0100   // 键盘按下
#define WM_PAINT          0x000F   // 需要重绘
```

用户移动鼠标、点击键盘、另一个窗口遮挡了你的窗口……这些事件都会变成消息，送到你的程序的消息队列里。

### 消息队列是什么鬼？

每个创建窗口的线程都有一个消息队列。你可以把它想象成一个信箱：

```
操作系统 → [消息队列] → GetMessage → 你的程序

操作系统把事件扔进信箱，GetMessage 从信箱里取信。
如果信箱是空的，GetMessage 会阻塞等待（程序不会无响应）。
```

### 消息循环的三驾马车

```cpp
while (GetMessage(&msg, NULL, 0, 0) > 0)  // 1. 取消息
{
    TranslateMessage(&msg);                // 2. 翻译键盘消息
    DispatchMessage(&msg);                 // 3. 派发消息
}
```

1. **`GetMessage`**：从队列取消息。如果收到 `WM_QUIT`，返回 0，循环结束。
2. **`TranslateMessage`**：把键盘按键消息转换成字符消息。比如你按下 A 键，它会生成一个 `WM_CHAR` 消息。不用管细节，调用就是了。
3. **`DispatchMessage`**：告诉操作系统调用目标窗口的窗口过程。

### 消息是怎么流转的？

以用户点击鼠标左键为例：

```
1. 用户点击鼠标左键
   ↓
2. 操作系统把 WM_LBUTTONDOWN 消息放入程序的消息队列
   ↓
3. GetMessage 从队列取出这条消息
   ↓
4. TranslateMessage 处理（鼠标消息不会被转换）
   ↓
5. DispatchMessage 被调用
   ↓
6. 操作系统找到目标窗口的 WindowProc 函数并调用它
   ↓
7. WindowProc 处理完返回
   ↓
8. 回到消息循环，等待下一条消息
```

### 队列消息与非队列消息

上面说的是**队列消息**（Posted Messages），它们会先进入消息队列。

还有一种叫**非队列消息**（Sent Messages），操作系统直接调用窗口过程，绕过队列。作为程序员，你通常不需要关心这个区别——窗口过程都会被调用。但如果你想自己发送消息给其他窗口，这个区别就重要了。

## 第四步：编写窗口过程

窗口过程（Window Procedure）是一个函数，定义了窗口的行为：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

### 函数签名解析

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
```

| 参数     | 含义                     |
| -------- | ------------------------ |
| `hwnd`   | 窗口句柄                 |
| `uMsg`   | 消息代码（如 `WM_PAINT`） |
| `wParam` | 消息相关数据（含义依赖于消息类型） |
| `lParam` | 消息相关数据（含义依赖于消息类型） |

`wParam` 和 `lParam` 是指针宽度的整数（32 位程序是 32 位，64 位程序是 64 位）。具体含义要查每个消息的文档。

### 消息处理的模式

典型的窗口过程就是一个大 `switch` 语句：

```cpp
switch (uMsg)
{
case WM_SIZE:
    // 处理窗口大小改变
    break;

case WM_PAINT:
    // 处理绘制
    break;

// ... 更多消息
}
```

### 不要忘记默认处理

对于你不处理的消息，必须调用 `DefWindowProc`：

```cpp
return DefWindowProc(hwnd, uMsg, wParam, lParam);
```

这个函数会执行默认操作——否则你的窗口会表现得非常奇怪（比如拖不动、点不了）。

### 让代码更模块化

窗口过程可以变得很长。一个好办法是把每个消息的处理逻辑抽成单独的函数：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
        {
            int width = LOWORD(lParam);   // 宽度在 lParam 的低位
            int height = HIWORD(lParam);  // 高度在 lParam 的高位
            OnSize(hwnd, (UINT)wParam, width, height);
        }
        break;
    }
}

void OnSize(HWND hwnd, UINT flag, int width, int height)
{
    // 处理大小改变逻辑
}
```

`LOWORD` 和 `HIWORD` 是宏，用来从 32 位/64 位值中提取低 16 位和高 16 位。

### 避免阻塞窗口过程

窗口过程执行期间，同线程创建的其他窗口都收不到消息。所以**不要在窗口过程里做耗时操作**。

```cpp
case WM_SOMETHING:
{
    // 错误示范：网络请求会卡死 UI
    WaitForServerResponse();  // 不要这样做！

    // 正确做法：放到另一个线程
    CreateThread(..., BackgroundTask, ...);
}
```

替代方案包括：创建新线程、使用线程池、异步 I/O、异步过程调用（APC）。

## 第五步：绘制窗口

窗口需要画点什么的时候，会收到 `WM_PAINT` 消息：

```cpp
case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 所有绘制操作都在这里

        FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

        EndPaint(hwnd, &ps);
    }
    return 0;
```

### 什么时候会收到 WM_PAINT？

- 窗口第一次显示时
- 另一个窗口遮挡了你的窗口，然后移开
- 用户拉伸窗口大小
- 你的代码调用了 `InvalidateRect` 函数

### 什么是更新区域？

需要重绘的部分叫「更新区域」。Windows 只会把这部分标记为无效，然后发送 `WM_PAINT`。

```
正常窗口：
┌─────────────────┐
│                 │
│                 │
│                 │
└─────────────────┘

被另一个窗口遮挡后移开：
┌─────────────────┐
│ ✓✓✓✓✓✓✓✓✓✓✓✓ │  ← 这部分需要重绘（更新区域）
│ ✓✓✓✓✓✓✓       │
│ ✓✓✓✓✓✓✓       │
└─────────────────┘
```

### BeginPaint 和 EndPaint

```cpp
HDC hdc = BeginPaint(hwnd, &ps);
// ... 绘制代码 ...
EndPaint(hwnd, &ps);
```

- `BeginPaint` 返回一个 `HDC`（设备上下文句柄），这是用来画画的「画笔」
- `PAINTSTRUCT` 结构包含重绘相关的信息，其中 `rcPaint` 是更新区域的矩形
- `EndPaint` 清除更新区域，告诉 Windows 绘制完成

**重要**：所有绘制操作必须在 `BeginPaint` 和 `EndPaint` 之间进行。

### 绘制整个客户区还是只绘制更新区域？

你有两个选择：

1. **简单粗暴**：每次都重绘整个客户区，Windows 会自动裁剪掉更新区域外的部分
2. **精细优化**：只重绘 `ps.rcPaint` 标记的区域

对于简单程序，第一种足够了。如果绘制逻辑很复杂，第二种更高效。

### GDI 和 Direct2D

`FillRect` 是 GDI（Graphics Device Interface）函数。GDI 是 Windows 的传统图形库，稳定但性能一般。

Windows 7 之后，微软引入了 Direct2D，支持硬件加速的高性能图形。新项目可以考虑用 Direct2D，但 GDI 依然完全支持，处理简单绘图够用了。

## 第六步：关闭窗口

用户关闭窗口时，会触发一系列消息：

```
用户点击 X 按钮 → WM_CLOSE → [可选: DestroyWindow] → WM_DESTROY → PostQuitMessage → WM_QUIT → 消息循环结束
```

### WM_CLOSE：给你个机会反悔

```cpp
case WM_CLOSE:
    if (MessageBox(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
    {
        DestroyWindow(hwnd);
    }
    // Else: 用户点了取消，什么都不做
    return 0;
```

`WM_CLOSE` 让你有机会弹出确认对话框。如果用户确认，调用 `DestroyWindow`；如果直接返回 0，窗口会保持打开。

实际上，如果你不处理 `WM_CLOSE`，`DefWindowProc` 会自动调用 `DestroyWindow`。所以如果你不需要确认对话框，可以忽略这个消息。

### WM_DESTROY：真的要销毁了

```cpp
case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
```

`WM_DESTROY` 在窗口已经被从屏幕移除后发送。对于主窗口，你通常在这里调用 `PostQuitMessage(0)`。

`PostQuitMessage` 会在消息队列里放一条 `WM_QUIT` 消息，这会导致 `GetMessage` 返回 0，消息循环结束。

有趣的是，`WM_QUIT` 消息永远不会被传递到窗口过程，所以你不需要在 `WindowProc` 里处理它。

## 管理应用程序状态

窗口过程是无状态的——它只是一个被反复调用的函数。那怎么保存应用程序的状态呢？

### 方案一：全局变量

简单粗暴，有效但不够优雅：

```cpp
int g_clickCount = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        g_clickCount++;  // 直接访问全局变量
        return 0;
    }
}
```

对于单窗口小程序，这个方法足够了。但多窗口程序会很难管理。

### 方案二：使用窗口的实例数据

`CreateWindowEx` 的最后一个参数可以传递任意数据：

```cpp
struct StateInfo {
    int clickCount;
    std::wstring lastMessage;
};

StateInfo *pState = new (std::nothrow) StateInfo;

HWND hwnd = CreateWindowEx(
    0, CLASS_NAME, L"My Window", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
    NULL, NULL, hInstance,
    pState  // ← 把状态指针传进去
);
```

然后在 `WM_NCCREATE` 或 `WM_CREATE` 中取出并保存：

```cpp
case WM_NCCREATE:
{
    CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
    StateInfo *pState = reinterpret_cast<StateInfo*>(pCreate->lpCreateParams);

    // 保存到窗口的实例数据中
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pState);
    return TRUE;
}
```

以后任何时候都可以取出来：

```cpp
StateInfo* GetAppState(HWND hwnd)
{
    LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    return reinterpret_cast<StateInfo*>(ptr);
}
```

### 方案三：面向对象封装

更进一步，把窗口过程和数据封装成类：

```cpp
template <class DERIVED_TYPE>
class BaseWindow
{
public:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        DERIVED_TYPE *pThis = NULL;

        if (uMsg == WM_NCCREATE)
        {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (DERIVED_TYPE*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->m_hwnd = hwnd;
        }
        else
        {
            pThis = (DERIVED_TYPE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (pThis)
        {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        else
        {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    HWND Window() const { return m_hwnd; }

protected:
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
    HWND m_hwnd;
};
```

使用：

```cpp
class MainWindow : public BaseWindow<MainWindow>
{
public:
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        // ... 其他消息
        }
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    MainWindow win;
    win.Create(L"Hello", WS_OVERLAPPEDWINDOW);
    ShowWindow(win.Window(), nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
```

这样你就用上了 C++ 的威力。MFC 和 ATL 这些框架本质上也是这么干的。

---

## 有趣的小练习

好了，理论讲完了，动手试试吧！以下练习按难度递增：

### 练习 1：点击计数器 ⭐

在窗口中间显示点击次数。每次点击左键，数字加 1 并刷新显示。

**提示**：你需要：
- 保存点击计数（全局变量或窗口实例数据）
- 处理 `WM_LBUTTONDOWN` 消息
- 调用 `InvalidateRect` 触发重绘
- 在 `WM_PAINT` 中用 `TextOut` 或 `DrawText` 显示文字

### 练习 2：随机方块画板 ⭐⭐

每次点击，在窗口随机位置画一个随机颜色的矩形。

**提示**：
- 用 `srand(GetTickCount())` 初始化随机数种子
- 用 `rand() % width` 生成随机坐标
- 用 `CreateSolidBrush` 创建画刷，用 `Rectangle` 画矩形
- 记得用 `DeleteObject` 释放 GDI 对象

### 练习 3：鼠标追踪器 ⭐⭐

在窗口标题栏实时显示鼠标坐标。

**提示**：
- 处理 `WM_MOUSEMOVE` 消息
- `lParam` 的低位是 X 坐标，高位是 Y 坐标
- 用 `SetWindowTextW` 更新窗口标题

### 练习 4：简单的记事本 ⭐⭐⭐

实现一个最简单的文本编辑器：
- 可以输入文字
- 支持退格键
- 显示光标位置

**提示**：这比想象中复杂，你需要处理：
- `WM_CHAR` 消息来接收字符输入
- 用 `std::wstring` 保存文本内容
- 在 `WM_PAINT` 中用 `DrawTextW` 或 `TextOutW` 显示
- `WM_KEYDOWN` 处理特殊键（如退格）

### 练习 5：可拖动的小球 ⭐⭐⭐⭐

在窗口里画一个圆球，可以用鼠标拖动它。

**提示**：
- 记录小球的中心坐标和半径
- `WM_LBUTTONDOWN` 检测是否点在小球内
- `WM_MOUSEMOVE` 时如果按住左键，更新小球位置
- `WM_LBUTTONUP` 结束拖动

### 练习 6：双缓冲消除闪烁 ⭐⭐⭐⭐⭐

如果你在练习 2 或 5 中频繁重绘，可能会发现窗口在闪烁。实现双缓冲来解决这个问题。

**提示**：
- 创建一个内存 DC（`CreateCompatibleDC`）
- 创建一个内存位图（`CreateCompatibleBitmap`）
- 先在内存 DC 上画完，然后一次性 `BitBlt` 到窗口
- 别忘了释放所有 GDI 对象！

---

**相关资源**

- [Microsoft Learn - Win32 编程入门](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/)
- [你的第一个 Windows 程序](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/your-first-windows-program)
- [Windows Hello World 示例代码](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/begin/LearnWin32/HelloWorld)
