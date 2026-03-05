# 通用GUI编程技术——Win32 原生编程实战（二）——从0开始搞定用户输入

> 上一篇文章我们搭起了一个能跑的 Win32 窗口框架，但说实话，那个窗口除了会自己关闭，什么也不会干。一个不能响应用户操作的窗口就像一台没有键盘的电脑，看着挺像样，实际上毫无用处。今天我们要聊的是让窗口真正"活"过来的东西——用户输入。

## 为什么一定要搞懂用户输入

说实话，我在刚接触 Win32 的时候也被这一套消息机制搞得有点懵。现代框架把各种事件封装得舒舒服服，你只需要写个 `onClick` 就完事了。但 Win32 不是这样，它把所有的输入细节都摊开在你面前，既给你最强大的控制力，也要求你理解每一条消息是怎么流转的。

这听起来有点麻烦，但你一旦搞明白了这套机制，以后遇到什么奇怪的输入问题都不会再慌。而且，当你需要做一些非标准的输入处理——比如自定义拖拽行为、特殊的光标逻辑、或者跨窗口的鼠标捕获——这些在现代框架里可能会让你抓狂的需求，在 Win32 里都只是几行代码的事。

另一个现实原因是：键盘和鼠标消息是最基础、最频繁的消息类型。如果你的窗口过程处理不好这些消息，轻则输入响应迟钝，重则整个程序卡死。我曾经就在一个项目里因为没有正确处理键盘消息，导致程序在某些输入法下完全失控，调试了整整两天才发现问题所在。所以，花点时间把这套机制搞透，绝对是值得的。

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉上一篇文章的内容——至少知道怎么创建一个基本的窗口、消息循环是怎么跑的。如果这些概念对你来说还比较陌生，建议先去看看上一篇笔记，不然接下来的内容可能会让你有点头晕。

---

## 第一步——理解键盘输入的本质

我们先从键盘开始，因为它看起来比鼠标简单——毕竟键盘就是一堆按键，按下什么就出现什么，对吧？

但事情没那么简单。在 Win32 的世界里，键盘输入被分成了两个完全独立的概念：按键（Key Stroke）和字符（Character）。这个区别听起来很学术，但实际上非常重要。

### 按键不等于字符

当你按下键盘上的 A 键时，硬件会产生一个扫描码。这个扫描码是键盘特有的，不同键盘可能不一样。你几乎永远不会直接接触扫描码，因为键盘驱动程序会把它转换成一个设备无关的东西——虚拟键码（Virtual Key Code）。

虚拟键码的设计思路是：在任何键盘上按下同一个物理按键，都应该产生相同的虚拟键码。比如不管你用的是美式键盘、德语键盘还是日语键盘，按下 Q 键的位置都会产生同样的虚拟键码。

但这里有个坑：虚拟键码不等于字符。同一个按键可能产生不同的字符——按 A 键可能产生 'a'、'A'、甚至 'á'，取决于 Shift 键的状态和当前键盘布局。而且，有些按键根本就不对应任何字符，比如 F1、Ctrl、Alt。

所以说，按键是一个物理事件，而字符是一个逻辑结果。Win32 把这两个概念分得非常清楚，也会用不同的消息来通知你。

### 键盘消息家族

当用户按下或释放一个键时，你的窗口会收到以下几种消息之一：

```
WM_KEYDOWN      — 普通键按下
WM_KEYUP        — 普通键释放
WM_SYSKEYDOWN   — 系统键按下（通常是带 Alt 的组合）
WM_SYSKEYUP     — 系统键释放
```

系统键是什么鬼？简单来说，任何涉及 Alt 键的组合都是系统键。F10 也是系统键（因为它激活菜单栏）。系统键主要是给操作系统用的，如果你截获了这些消息但不传给 `DefWindowProc`，可能会破坏一些标准系统行为，比如 Alt+Tab 切换窗口。

如果你按住一个键不放，键盘重复功能会启动，你会收到一系列 `WM_KEYDOWN` 消息，最后在释放键时收到一个 `WM_KEYUP`。这在处理游戏输入或者长按删除时很有用。

所有这些键盘消息的 `wParam` 参数都包含虚拟键码，`lParam` 参数包含一些额外的状态信息。大多数情况下你只需要关心 `wParam`。

虚拟键码有对应的常量定义，比如 `VK_LEFT` 是左箭头键，`VK_RETURN` 是回车键。但有一个反直觉的地方：字母和数字键没有像 `VK_A` 这样的常量，它们的虚拟键码直接就是 ASCII 值——A 键是 0x41，1 键是 0x31。所以你在代码里可以直接用 `'A'` 或 `'1'` 来比较。

### 字符消息去哪了

你可能会问：如果按键消息只告诉我按了哪个键，那字符是什么？这就涉及到另一个消息转换步骤了。

还记得消息循环里的 `TranslateMessage` 函数吗？它的作用就是把按键消息转换成字符消息。每当你收到一个会产生字符的按键时，`TranslateMessage` 会把一个 `WM_CHAR` 消息放进消息队列。

```cpp
while (GetMessage(&msg, NULL, 0, 0) > 0)
{
    TranslateMessage(&msg);    // 把 WM_KEYDOWN 转换成 WM_CHAR
    DispatchMessage(&msg);
}
```

`WM_CHAR` 消息的 `wParam` 包含 UTF-16 字符值（`wchar_t` 类型）。这就是你通常理解的"用户输入的字符"。

举个例子，假设用户按下了 Shift+A。消息序列会是这样的：

```
WM_KEYDOWN: VK_SHIFT
WM_KEYDOWN: 'A'
WM_CHAR: 'A'
WM_KEYUP: 'A'
WM_KEYUP: VK_SHIFT
```

而如果是 Alt+P（系统键组合），序列会是：

```
WM_SYSKEYDOWN: VK_MENU
WM_SYSKEYDOWN: 'P'
WM_SYSCHAR: 'p'
WM_SYSKEYUP: 'P'
WM_KEYUP: VK_MENU
```

`WM_SYSCHAR` 对应系统字符，你应该把它传给 `DefWindowProc`，不要把它当成用户输入的文本来处理。否则可能会干扰一些系统命令。

### 实战：监听键盘消息

现在我们来写点代码，看看这些键盘消息到底长什么样。我们在窗口过程里加一些调试输出：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    wchar_t msg[64];

    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
        swprintf_s(msg, L"WM_SYSKEYDOWN: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_SYSCHAR:
        swprintf_s(msg, L"WM_SYSCHAR: %c\n", (wchar_t)wParam);
        OutputDebugString(msg);
        break;

    case WM_SYSKEYUP:
        swprintf_s(msg, L"WM_SYSKEYUP: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_KEYDOWN:
        swprintf_s(msg, L"WM_KEYDOWN: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_KEYUP:
        swprintf_s(msg, L"WM_KEYUP: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_CHAR:
        swprintf_s(msg, L"WM_CHAR: %c\n", (wchar_t)wParam);
        OutputDebugString(msg);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

编译运行这个程序，打开 Visual Studio 的输出窗口，然后在键盘上随便按点东西。你会看到各种消息鱼贯而出。试着按一下 Shift+A、Alt+F4、F5，看看消息序列有什么不同。相信我，亲手试一遍比看十遍文档都有用。

⚠️ 注意

一定要包含 `<wchar.h>` 头文件，否则 `swprintf_s` 会报未定义错误。如果你忘了包含，编译器会给你一个看起来很莫名其妙的错误提示，可能会让你困惑一阵子。

### 虚拟键码速查

常用虚拟键码不需要全部背下来，但有几个你应该烂熟于心：

```cpp
VK_LBUTTON    0x01    // 左鼠标键
VK_RBUTTON    0x02    // 右鼠标键
VK_CANCEL     0x03    // Control-break 处理
VK_MBUTTON    0x04    // 中鼠标键
VK_BACK       0x08    // BACKSPACE 键
VK_TAB        0x09    // TAB 键
VK_RETURN     0x0D    // 回车键
VK_SHIFT      0x10    // SHIFT 键
VK_CONTROL    0x11    // CTRL 键
VK_MENU       0x12    // ALT 键（没错，ALT 叫 VK_MENU）
VK_ESCAPE     0x1B    // ESC 键
VK_SPACE      0x20    // 空格键
VK_LEFT       0x25    // 左箭头
VK_UP         0x26    // 上箭头
VK_RIGHT      0x27    // 右箭头
VK_DOWN       0x28    // 下箭头
```

如果你需要查某个键的虚拟键码，MSDN 有一张完整的表可以参考。

---

## 第二步——掌握键盘状态的检测

键盘消息是事件驱动的，也就是说你只在"有事发生"的时候收到消息。但有些时候，你需要主动查询某个键当前是否被按下。

比如你想实现"按住 Ctrl 的同时点击鼠标"这种组合操作。你可以监听键盘消息来追踪 Ctrl 键的状态，但那样会很麻烦。Win32 提供了一个更简单的函数：`GetKeyState`。

```cpp
if (GetKeyState(VK_CONTROL) & 0x8000)
{
    // Ctrl 键当前被按下
}
```

`GetKeyState` 返回值的最高位（0x8000）表示键是否当前被按下。次高位（0x4000）表示键是否被"切换"（toggle），比如 Caps Lock 或 Num Lock。

这个函数有个有趣的特点：它报告的是"虚拟"键盘状态，基于消息队列的内容。当你的程序在处理一条消息时，`GetKeyState` 返回的是这条消息入队时的键盘状态。这听起来有点绕，但实际好处是：`GetKeyState` 会忽略发送给其他程序的键盘输入。如果用户切换到另一个程序，你的程序不会错误地检测到那个程序的键盘活动。

如果你真的需要知道键盘的即时物理状态（比如用于游戏输入），有一个叫 `GetAsyncKeyState` 的函数。但对于大多数 UI 代码来说，`GetKeyState` 才是正确的选择。

### 区分左右 Shift/Ctrl/Alt

有些时候你需要知道用户按的是左 Ctrl 还是右 Ctrl。`GetKeyState` 可以做到这一点：

```cpp
if (GetKeyState(VK_LCONTROL) & 0x8000)
{
    // 左 Ctrl 被按下
}

if (GetKeyState(VK_RMENU) & 0x8000)
{
    // 右 Alt 被按下（记住 ALT 叫 VK_MENU）
}
```

对应的常量是 `VK_L*` 和 `VK_R*` 系列，比如 `VK_LSHIFT`、`VK_RSHIFT`、`VK_LCONTROL`、`VK_RCONTROL`、`VK_LMENU`、`VK_RMENU`。

---

## 第三步——深入鼠标输入

搞定键盘之后，我们来看看鼠标。Windows 支持最多五个鼠标按键：左键、中键、右键，以及 XBUTTON1 和 XBUTTON2（通常在鼠标两侧）。

在大多数现代鼠标上，至少有左右两个键。左键是"主键"，用于选择、拖动等操作；右键是"辅助键"，通常显示上下文菜单。中键如果是滚轮的话，可能也能点击。

关于左右键有个需要注意的细节：Windows 允许用户交换左右键的功能，方便左撇子用户。所以文档里说的"主键"和"辅助键"是逻辑概念，而不是物理位置。默认设置下，左键是主键；但如果用户交换了，右键就变成主键了。

好消息是：Windows 会自动处理这种转换，你收到的消息已经根据用户的设置调整好了。所以不管用户怎么配置，`WM_LBUTTONDOWN` 永远代表主键按下，`WM_RBUTTONDOWN` 永远代表辅助键按下。

### 鼠标点击消息

当用户在窗口的客户区点击鼠标时，你会收到以下消息之一：

```
WM_LBUTTONDOWN    — 主键按下
WM_LBUTTONUP      — 主键释放
WM_MBUTTONDOWN    — 中键按下
WM_MBUTTONUP      — 中键释放
WM_RBUTTONDOWN    — 辅助键按下
WM_RBUTTONUP      — 辅助键释放
WM_XBUTTONDOWN    — XBUTTON 按下
WM_XBUTTONUP      — XBUTTON 释放
```

客户区就是窗口里不包含边框和标题栏的部分。如果鼠标在边框或标题栏上点击，你收到的是"非客户区"消息（带 NC 前缀，比如 `WM_NCLBUTTONDOWN`），这些通常不需要你自己处理。

### 获取鼠标坐标

鼠标消息的 `lParam` 参数包含了鼠标指针的坐标。但这里有个坑：坐标是打包在一个 32 位值里的。低 16 位是 x 坐标，高 16 位是 y 坐标。

你不能简单地用位移操作来提取坐标，因为在 64 位 Windows 上 `lParam` 是 64 位的，上 32 位不保证是 0。正确做法是用 Windows 提供的宏：

```cpp
#include <WindowsX.h>  // 别忘了包含这个头文件

int xPos = GET_X_LPARAM(lParam);
int yPos = GET_Y_LPARAM(lParam);
```

这两个宏在任何平台上都能正确工作。

坐标以像素为单位，相对于窗口客户区的左上角。客户区左上角是 (0, 0)，向右 x 增加，向下 y 增加。如果鼠标在客户区上方或左方，坐标会是负数——这在跟踪鼠标拖出窗口的情况时很重要。

### 检测修饰键状态

鼠标消息的 `wParam` 参数包含了一些标志位，告诉你在点击鼠标时其他键的状态：

```cpp
if (wParam & MK_CONTROL)
{
    // 点击时 Ctrl 键被按下
}

if (wParam & MK_SHIFT)
{
    // 点击时 Shift 键被按下
}

if (wParam & MK_LBUTTON)
{
    // 点击时鼠标左键也被按下（意思是多个键同时按）
}
```

可用标志包括：`MK_CONTROL`、`MK_SHIFT`、`MK_LBUTTON`、`MK_MBUTTON`、`MK_RBUTTON`、`MK_XBUTTON1`、`MK_XBUTTON2`。

### 处理 XBUTTON

XBUTTON1 和 XBUTTON2 是额外的侧键，常用于浏览器的"前进"和"后退"功能。但 `WM_XBUTTONDOWN` 消息本身不告诉你到底是 XBUTTON1 还是 XBUTTON2，你需要用另一个宏来检查：

```cpp
UINT button = GET_XBUTTON_WPARAM(wParam);
if (button == XBUTTON1)
{
    // XBUTTON1 被点击
}
else if (button == XBUTTON2)
{
    // XBUTTON2 被点击
}
```

### 双击处理

默认情况下，窗口不会收到双击消息。如果你需要处理双击，必须在注册窗口类时设置 `CS_DBLCLKS` 样式：

```cpp
WNDCLASS wc = { };
wc.style = CS_DBLCLKS;  // 启用双击消息
// ... 设置其他字段
RegisterClass(&wc);
```

启用后，当用户双击鼠标左键时，你会收到这样的消息序列：

```
WM_LBUTTONDOWN
WM_LBUTTONUP
WM_LBUTTONDBLCLK    ← 第二次按下会变成双击消息
WM_LBUTTONUP
```

一个重要的细节是：在收到 `WM_LBUTTONDBLCLK` 之前，你无法知道第一次单击是双击的开始。因此，双击操作应该延续单击操作的行为。比如在文件管理器里，单击选中文件，双击打开文件——单击不会被"浪费"。

⚠️ 注意

如果不确定是否需要双击功能，就不要启用 `CS_DBLCLKS`。因为启用后，每次双击都会少收到一条 `WM_LBUTTONDOWN` 消息，可能会影响你的逻辑。

---

## 第四步——处理鼠标移动

鼠标移动时，窗口会收到 `WM_MOUSEMOVE` 消息。这个消息的参数和点击消息一样——`lParam` 包含坐标，`wParam` 包含按钮和修饰键状态。

但你可能会发现一件事：即使鼠标看起来没动，你也可能在不断地收到 `WM_MOUSEMOVE` 消息。这是因为如果窗口的状态改变了（比如被其他窗口遮挡后又露出来），系统会重新发送鼠标移动消息，确保你的程序知道当前鼠标位置。

所以处理 `WM_MOUSEMOVE` 时，坐标可能和上次一样，你要做好这种准备。

### 跟踪窗口外的鼠标移动

默认情况下，当鼠标移出窗口客户区后，你就不会再收到 `WM_MOUSEMOVE` 消息了。这对于大多数程序没问题，但有些场景下你需要跟踪鼠标移出窗口后的移动——比如用户在绘图程序里拖拽选区，拖出窗口边缘是很常见的情况。

Win32 提供了"鼠标捕获"（Mouse Capture）机制来解决这个问题：

```cpp
SetCapture(hwnd);      // 开始捕获鼠标
// ... 现在即使鼠标移出窗口也能收到消息
ReleaseCapture();      // 释放捕获
```

调用 `SetCapture` 后，只要用户至少按住一个鼠标按钮，你的窗口就会继续接收鼠标消息，即使鼠标已经移到窗口外面。捕获窗口必须是前台窗口，而且同一时间只能有一个窗口持有捕获。

典型的用法是这样的：

```cpp
case WM_LBUTTONDOWN:
{
    SetCapture(hwnd);           // 开始拖动时捕获鼠标
    dragging = true;
    return 0;
}

case WM_MOUSEMOVE:
{
    if (dragging)
    {
        // 处理拖动逻辑
    }
    return 0;
}

case WM_LBUTTONUP:
{
    ReleaseCapture();           // 拖动结束时释放捕获
    dragging = false;
    return 0;
}
```

### DPI 感知

如果你打算做绘图相关的东西，必须了解 DPI（Dots Per Inch）的概念。现代显示器有不同的 DPI 设置——标准 DPI 是 96，但高分辨率显示器可能是 144、192 甚至更高。

Windows 的鼠标消息给你的坐标是"物理像素"，但现代图形 API（如 Direct2D）使用的是"设备无关像素"（DIP，Device-Independent Pixel）。在标准 DPI 下它们是一回事，但在高 DPI 下就不一样了。

正确的做法是在程序初始化时获取当前 DPI，然后在像素和 DIP 之间转换：

```cpp
class DPIScale
{
    static float scale;

public:
    static void Initialize(HWND hwnd)
    {
        float dpi = GetDpiForWindow(hwnd);
        scale = dpi / 96.0f;
    }

    template <typename T>
    static D2D1_POINT_2F PixelsToDips(T x, T y)
    {
        return D2D1::Point2F(static_cast<float>(x) / scale,
                            static_cast<float>(y) / scale);
    }
};

float DPIScale::scale = 1.0f;
```

在 `WM_CREATE` 时初始化：

```cpp
case WM_CREATE:
    DPIScale::Initialize(hwnd);
    return 0;
```

然后在鼠标消息里转换坐标：

```cpp
case WM_MOUSEMOVE:
{
    int pixelX = GET_X_LPARAM(lParam);
    int pixelY = GET_Y_LPARAM(lParam);
    D2D1_POINT_2F dips = DPIScale::PixelsToDips(pixelX, pixelY);
    // 使用 dips 坐标进行绘图
    return 0;
}
```

这样做的好处是你的程序在高 DPI 显示器上会自动缩放，不会出现模糊或尺寸不对的问题。

---

## 第五步——键盘加速键

聊完键盘和鼠标的基本消息后，我们来看一个更高级的主题：键盘加速键（Keyboard Accelerators），或者叫快捷键。

你肯定用过 Ctrl+C 复制、Ctrl+V 粘贴、Ctrl+S 保存这些快捷键。在 Win32 里，实现这些功能的方式不是监听 `WM_KEYDOWN` 消息然后检查 Ctrl 和 C 的组合——虽然你可以这样做，但有更好的方式。

加速键表是一种数据结构，把按键组合和命令 ID 关联起来。当用户按下对应的组合键时，系统会自动发送 `WM_COMMAND` 消息，就像用户点击了菜单项一样。

### 创建加速键表资源

最常用的方式是在资源文件里定义加速键表：

```rc
FileAccel ACCELERATORS
BEGIN
    VK_F12, IDM_OPEN, CONTROL, VIRTKEY    // Ctrl+F12
    VK_F4,  IDM_CLOSE, ALT, VIRTKEY       // Alt+F4
    VK_F12, IDM_SAVE, SHIFT, VIRTKEY      // Shift+F12
    VK_F12, IDM_SAVEAS, VIRTKEY           // F12
END
```

每一行的格式是：按键、命令 ID、修饰键、标志。按键可以是虚拟键码（带 `VIRTKEY` 标志）或 ASCII 字符。修饰键包括 `CONTROL`、`ALT`、`SHIFT`。

你也可以用 ASCII 字符定义快捷键：

```rc
"A", ID_ACCEL1         // Shift+A
65, ID_ACCEL2, ASCII   // 65 是 'A' 的 ASCII 值
```

如果要定义虚拟键码，需要加 `VIRTKEY` 标志：

```rc
"B", ID_ACCEL3, CONTROL, VIRTKEY   // Ctrl+B
```

### 加载和使用加速键表

在程序启动时加载加速键表：

```cpp
HACCEL haccel;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    // ... 创建窗口 ...

    haccel = LoadAccelerators(hInstance, L"FileAccel");
    if (haccel == NULL)
    {
        // 处理错误
        return 0;
    }

    // ... 显示窗口 ...

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (!TranslateAccelerator(hwnd, haccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

`TranslateAccelerator` 函数会检查当前消息是否匹配加速键表中的某个条目。如果匹配，它会把消息转换成 `WM_COMMAND` 或 `WM_SYSCOMMAND` 并发送到窗口过程。因为这个函数已经处理了消息，所以你只有在它返回 FALSE（表示不是加速键）时才调用 `TranslateMessage` 和 `DispatchMessage`。

### 处理加速键命令

在窗口过程里，加速键产生的 `WM_COMMAND` 消息和菜单项产生的消息是一样的：

```cpp
case WM_COMMAND:
{
    switch (LOWORD(wParam))
    {
    case IDM_OPEN:
        // 处理打开命令（可能是 Ctrl+F12 或菜单项触发的）
        return 0;

    case IDM_SAVE:
        // 处理保存命令
        return 0;
    }
    break;
}
```

如果你需要区分命令来自加速键还是菜单，可以检查 `wParam` 的高位字：

```cpp
if (HIWORD(wParam) == 1)
{
    // 来自加速键
}
else
{
    // 来自菜单
}
```

但大多数情况下你不需要关心这个区别，因为命令应该是同一个。

### 运行时创建加速键表

如果你不需要资源文件，也可以在运行时创建加速键表。这需要填充一个 `ACCEL` 结构数组，然后调用 `CreateAcceleratorTable`：

```cpp
ACCEL accelerators[] = {
    { FVIRTKEY | FCONTROL, VK_F5,  IDM_REGULAR },
    { FVIRTKEY | FCONTROL, 'B',    IDM_BOLD },
    { FVIRTKEY | FCONTROL, 'I',    IDM_ITALIC },
    { FVIRTKEY | FCONTROL, 'U',    IDM_ULINE },
};

HACCEL haccel = CreateAcceleratorTable(accelerators, 4);
```

`ACCEL` 结构的字段是：标志、按键代码、命令 ID。标志可以是 `FVIRTKEY`（表示是虚拟键码）、`FCONTROL`、`FALT`、`FSHIFT` 的组合。

运行时创建的加速键表在你调用 `DestroyAcceleratorTable` 或程序退出时会被销毁。

⚠️ 注意

用 `LoadAccelerators` 从资源加载的加速键表会在程序退出时自动销毁，不需要手动调用 `DestroyAcceleratorTable`。但用 `CreateAcceleratorTable` 创建的表如果要提前销毁，需要调用 `DestroyAcceleratorTable`。

---

## 有趣的小练习

好了，理论讲得差不多了，现在该动手了。以下是几个按难度递增的练习，每个都能帮你巩固刚才学到的知识。

### 练习 1：按键监控器 ⭐

在窗口中实时显示最近按下的键。

**具体要求**：
- 窗口中央显示最后按下的键名
- 如果是字符键，显示对应的字符
- 如果是控制键（如 F1、Ctrl），显示虚拟键码
- 用 `InvalidateRect` 触发重绘

**提示**：
- 在 `WM_KEYDOWN` 里保存按下的键到全局变量
- 在 `WM_CHAR` 里如果是可打印字符，更新显示
- 在 `WM_PAINT` 里用 `TextOutW` 或 `DrawTextW` 绘制文字
- 需要包含 `<sstream>` 来把数字转成字符串

### 练习 2：鼠标绘图板 ⭐⭐

在窗口上按住鼠标左键拖动来绘制自由曲线。

**具体要求**：
- 按下左键时开始记录点
- 移动鼠标时把当前点加入列表
- 释放左键时停止
- `WM_PAINT` 时把所有点连成线段绘制出来

**提示**：
- 用 `std::vector<POINT>` 保存点列表
- `WM_LBUTTONDOWN` 时 `SetCapture` 开始捕获
- `WM_LBUTTONUP` 时 `ReleaseCapture` 释放
- 需要多条曲线的话，可以用 `vector<vector<POINT>>`
- 绘图用 `MoveToEx` + `LineTo` 或 `Polyline`

### 练习 3：点击位置高亮 ⭐⭐

每次点击鼠标左键，在点击位置画一个永久的小标记。

**具体要求**：
- 点击位置画一个圆圈或十字
- 标记永久保留，不会因窗口重绘而消失
- 右键点击清除所有标记

**提示**：
- 用结构体保存每个标记的位置和颜色
- `std::vector<POINT>` 或 `std::vector<pair<int,int>>` 存储位置
- 右键点击时 `vector.clear()`
- 在 `WM_PAINT` 里遍历绘制所有标记

### 练习 4：简单打字游戏 ⭐⭐⭐

屏幕上随机显示字母，玩家按对应键盘按键得分。

**具体要求**：
- 窗口中间显示一个大字母
- 玩家按下对应键盘按键后，字母变化
- 显示得分和连击数
- 按错键连击清零

**提示**：
- 需要一个定时器（`SetTimer`）来自动变化字母
- `WM_TIMER` 消息里处理超时
- `WM_CHAR` 里判断输入是否正确
- 用 `GetTickCount()` 或 `<random>` 库生成随机字母
- 记得用 `KillTimer` 在程序退出时清理定时器

### 练习 5：拖拽矩形 ⭐⭐⭐⭐

实现一个可以拖拽调整大小的矩形。

**具体要求**：
- 在空白处拖拽创建新矩形
- 点击已有矩形可以选中它
- 拖拽矩形边框可以调整大小
- 拖拽矩形内部可以移动位置

**提示**：
- 定义一个 `RECT` 结构来表示矩形
- 用 `vector<RECT>` 保存多个矩形
- `WM_LBUTTONDOWN` 时检测点击位置：在矩形内还是边框上
- 需要一个状态变量表示当前操作：无、移动、调整大小
- 调整大小时要判断鼠标在哪个边或角
- 用 `InflateRect`、`OffsetRect` 等 API 辅助操作

### 练习 6：快捷键文本编辑器 ⭐⭐⭐⭐⭐

实现一个简单的文本编辑器，支持常用快捷键。

**具体要求**：
- 可以输入文字
- Ctrl+S 保存到文件
- Ctrl+O 打开文件
- Ctrl+A 全选
- 显示文件名和修改状态（是否已保存）

**提示**：
- 用 `std::wstring` 保存当前文本内容
- 文件操作用 `CreateFile` / `ReadFile` / `WriteFile`
- 或者用 C 标准库的 `fopen` 系列（更简单）
- 创建加速键表资源处理快捷键
- 需要一个布尔变量表示"是否已修改"
- 标题栏可以用 `SetWindowTextW` 更新状态
- 文件对话框可以用 `GetOpenFileName` / `GetSaveFileName`

### 练习 7：鼠标轨迹捕捉 ⭐⭐

捕捉并显示鼠标的移动轨迹，即使鼠标移出窗口。

**具体要求**：
- 按住左键时记录鼠标轨迹
- 轨迹可以延伸到窗口外
- 释放左键时绘制完整轨迹
- 显示轨迹的起点和终点坐标

**提示**：
- 使用 `SetCapture` / `ReleaseCapture`
- 在 `WM_MOUSEMOVE` 里持续添加点
- 坐标需要转换为屏幕坐标或保持客户区坐标都可以
- 用 `GetCursorPos` 获取全局鼠标位置
- 如果用屏幕坐标，可以用 `ScreenToClient` 转换

---

到这里，这篇关于用户输入的实战笔记就差不多了。我们讲了键盘消息和字符消息的区别、鼠标点击和移动的处理、键盘加速键的使用，还有一堆可以动手练习的题目。

说实话，Win32 这套输入机制看起来有点古老，但它的设计是经过时间考验的。你一旦理解了消息是怎么流转的，就能处理各种奇怪的输入需求。现代框架把这些都封装起来了，但当你需要做些不标准的东西时，还是会怀念 Win32 这种"一切尽在掌握"的感觉。

---

**相关资源**

- [键盘输入 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/keyboard-input)
- [鼠标输入 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/mouse-input)
- [鼠标移动 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/mouse-movement)
- [键盘加速键 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/keyboard-accelerators)
- [虚拟键码 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/inputdev/virtual-key-codes)
