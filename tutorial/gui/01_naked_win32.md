---
title: 具象 · 脱掉封装，把窗口立起来
---

# 具象 · 脱掉封装，把窗口立起来

277 行的 `app/m1a_naked.cpp` 里，`class` 出现的次数是 0，构建目标的名字是 `gui_naked`。双击编出来的 exe，屏幕上是一个 720×480 的窗口，标题上写的是 `Anatomy GUI · M1a`，咱们从上往下看，客户区里摆着的是四样东西。

输入框的位置是左边 20 像素、高 28 像素、右边顶到客户区宽度减 20，按钮的位置在它下面 8 像素、120 像素宽、36 像素高，STATIC 标签的位置再往下 8 像素，最后一行字来自 `WM_PAINT` 里的 `DrawTextW`，它不在控件的范围里。您把窗口横向拉大，四样东西的行距不变，跟着变的只有宽度。

## 七条验收

“能跑”是个很松的词，您手上那一版得同时满足七条才算能用。

1. 窗口里有一个输入框、一个按钮、一行文本，能点、能打字
2. 缩放窗口时内容跟着重排，且**不闪**
3. 中文能正常显示
4. 高 DPI 显示器上不糊、不错位
5. 改一个状态，界面跟着更新
6. 关掉时干净退出，没有泄漏
7. 一条命令就能构建

七条里漏掉任何一条的版本都只能算能演示。演示和能用的距离，往往就落在了第五条上，您改一次状态就看得见。

## 让窗口站住

Windows 程序的入口不是 `main`，这一点在子系统设成 WINDOWS 的时候就已经定死了。您要是在 CMake 里写的是 `add_executable(gui WIN32 ...)`，链接器要找的是 `WinMain`，而按习惯写下的 `main` 会被晾在一边，输出窗口里留下的是一行 `error LNK2019: 无法解析的外部符号 WinMain`。

```cpp
int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] PWSTR pCmdLine, int nShowCmd) {
```

`hInstance` 是本模块在内存里的实例句柄，注册窗口类和创建窗口都缺不了它，您在哪一处都得把它带上。

窗口类要回答的是两个问题，窗口的样子和消息的去处。您填进去的字段是窗口过程、实例句柄、类名、光标和背景刷，五项都缺不了。类名在进程内必须是唯一的，重名注册的结果是失败，`GetLastError()` 报回来的是 1410。

```cpp
WNDCLASSEXW window_class{};
window_class.cbSize        = sizeof(window_class);
window_class.lpfnWndProc   = WndProc;     // where messages land
window_class.hInstance     = hInstance;
window_class.lpszClassName = kClassName;
window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
if (RegisterClassExW(&window_class) == 0) {
    return 1;
}
```

`reinterpret_cast` 那一行是 SDK 的官方写法而不是咱们偷懒，背景刷的来源是系统颜色索引，它的值是 `COLOR_* + 1`，而 `HBRUSH` 是个句柄类型，`reinterpret_cast` 就是这么来的。

注册窗口类这一步成功了，您要做的就只剩给创建窗口填参数。

```cpp
HWND hwnd = CreateWindowExW(0, kClassName, L"Anatomy GUI · M1a",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                            720, 480, nullptr, nullptr, hInstance, nullptr);
```

`WS_CLIPCHILDREN` 的作用是让父窗口重画时避开子控件占的那块区域。少了它，您拖窗口右边框的时候，子控件的位置会留下一层拖影。

窗口开了出来，您手上还缺一个让消息动起来的循环。

```cpp
MSG msg{};
while (true) {
    const BOOL kResult = GetMessageW(&msg, nullptr, 0, 0);
    if (kResult == 0) {
        break;  // WM_QUIT
    }
    if (kResult == -1) {
        return 1;  // failure
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
}
```

`GetMessageW` 给回来的 0 和 -1 是两件事，0 表示收到了 `WM_QUIT`，该退出了，-1 表示的是出错。把判断写成 `while (GetMessage(...))` 的版本，-1 会被当成继续的信号，错误就这么没了，而循环接着空转，您在任务管理器里能看到一个挂着的进程。

消息最后的去处是 `WndProc`，您在 `switch` 上一条条看下去，每个分支回答的都是一条消息。

```cpp
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    // ...
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
```

`DefWindowProcW` 是省不掉的一步，没处理的消息必须交回系统，否则您连标题栏和边框都看不见，那些东西同样是消息画出来的。

窗口销毁的时候我们只做一件事，往消息队列里投的是一条 `WM_QUIT`。`PostQuitMessage` 之后进程还要走完 `wWinMain` 剩下的几行，所以窗口销毁和进程退出是分开的两件事。

## 控件也是窗口

控件用的还是同一个 `CreateWindowExW`，您要改的只是父窗口和样式位。

```cpp
g_edit_hwnd = CreateWindowExW(
    WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEdit)), hInstance, nullptr);
```

`EDIT`、`BUTTON`、`STATIC` 的窗口类由系统早就注册好了，您不用再管 `RegisterClassExW` 了。`hMenu` 在子窗口上装的是控件 ID，参数名和类型都是给顶层窗口准备的，到了子窗口里塞进去的却是一个整数。看着别扭的 `reinterpret_cast` 就出自这里，而它是官方约定。

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) {
        wchar_t text[256];
        GetWindowTextW(g_edit_hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
        SetWindowTextW(g_label_hwnd, text);
        lstrcpynW(g_drawn_text, text, 256);
        return 0;
    }
    break;
```

`WM_COMMAND` 送来的是个信封，里面装着的三样东西分别管一件事。`LOWORD(wParam)` 给的是控件 ID，`HIWORD(wParam)` 给的是通知码，按钮点下去那一下的码是 `BN_CLICKED`，`lParam` 递过来的是控件自己的 HWND。三样凑齐了，您才能确定是谁在报什么。分支末尾用的是 `break`，命令不是自己的就往下走，最后交给了 `DefWindowProcW`。

```cpp
const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
SendMessageW(g_edit_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
```

控件默认带的是古董字体，不下这一行补丁的话，输入框里的中文又小又挤。`GetStockObject` 领到的是共享对象，删了会出事，因为系统手里握着的也是同一份，您根本删不动它。

加一个控件要动的地方是三处，您一处都不能漏。

1. 加一个 ID 常量，`kIdEdit`、`kIdButton`、`kIdLabel` 那排要多出一行
2. 加一次 `CreateWindowExW`
3. 在 `WM_COMMAND` 里加一个判断分支

改的地方隔着几十行，漏掉一处的后果是编译照样通过，只是您手上会多出一个点了没反应的按钮。

## 位置是算出来的

窗口一缩的瞬间，三个控件挤到了左上角叠成一团。位置是要您自己算的，`layout_controls` 干的就是这件事。

```cpp
    RECT client{};
    GetClientRect(parent, &client);

    const int kMargin       = 20;
    const int kGap          = 8;
    const int kEditHeight   = 28;
    const int kButtonHeight = 36;
    const int kButtonWidth  = 120;
    const int kLabelHeight  = 32;
    const int kDrawnHeight  = 32;

    const int kContentWidth = client.right - client.left - (kMargin * 2);
    const int kEditTop      = kMargin;
    const int kButtonTop    = kEditTop + kEditHeight + kGap;
    const int kLabelTop     = kButtonTop + kButtonHeight + kGap;
    const int kDrawnTop     = kLabelTop + kLabelHeight + kGap;

    SetWindowPos(g_edit_hwnd, nullptr, kMargin, kEditTop, kContentWidth, kEditHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    // ... 另外两个控件同理
```

布局要用到的尺寸来自 `GetClientRect`，子窗口的坐标相对的是客户区，而不是屏幕。`WM_SIZE` 把新尺寸塞在 `lParam` 的两个 16 位里，低 16 位放的是宽，高 16 位放的是高。窗口最小化的时候两个值都是 0，`SIZE_MINIMIZED` 您得单独判掉，不然布局算出来的宽度就是 0。

`WM_SIZE` 的第一次到达发生在 `CreateWindowExW` 内部，而子窗口那时候还没建出来。函数开头那句 `if (g_edit_hwnd == nullptr)` 是这段代码真的会走到的分支而不是防御性编程，您把断点停上去，监视窗口里三个 HWND 的值全是 0。

副作用是绕不开的：按钮被咱们逼成了全局变量。它本来是不需要全局的，`WM_COMMAND` 里递过来的就是它的 HWND，不过布局要给三个控件定位，于是它也变成了全局变量。

## 糊和丑

您把窗口拖到 150% 缩放的显示器上，能看到的症状是两个，窗口整体是发虚的，按钮的样式还停在 Windows 95 的年代。看着无关的两个症状出自同一处，程序的 exe 里少了 manifest。

```xml
<dependency>
  <dependentAssembly>
    <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
                      version="6.0.0.0" processorArchitecture="*"
                      publicKeyToken="6595b64144ccf1df" language="*"/>
  </dependentAssembly>
</dependency>
<application xmlns="urn:schemas-microsoft-com:asm.v3">
  <windowsSettings>
    <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
    <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
  </windowsSettings>
</application>
```

- **下面那段**声明进程是 per-monitor DPI 感知的。不声明的话，系统会拿您的窗口当位图拉伸，于是糊。声明之后，系统不再拉伸，改为发 `WM_DPICHANGED` 让您自己处理。
- **上面那段**声明依赖 Common Controls v6。不声明的话，`BUTTON` / `EDIT` 走的是 XP 之前那套画法，于是丑。

您把它列进 CMake 的源文件列表就行，合进 exe 这一步是链接器做的。

```cmake
add_executable(gui WIN32
    app/m1a_naked.cpp
    app/gui.manifest)
```

构建目标的名字是 `gui_naked`，您编出来的 exe 跟它同名。

### 声明之后的另一半

manifest 里声明了 DPI 感知，另一半的活您得自己接住。

```cpp
case WM_DPICHANGED: {
    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
}
```

您把窗口拖到缩放比例不同的另一块显示器上，系统算出来的新尺寸通过 `lParam` 递过来，等的就是一次 `SetWindowPos`。不办这件事的话，窗口突然大出了一圈，位置也跟着错开了。后面跟来的 `WM_SIZE` 会重跑 `layout_controls`，而布局那边的代码里没有 DPI 的位置。

## 自己画的那一行

屏幕上已有的像素全是系统替咱们画的，按钮和输入框画在 `comctl32` 的窗口过程里，而合成上屏那一步归 DWM。

```cpp
case WM_PAINT: {
    PAINTSTRUCT paint{};
    HDC window_dc = BeginPaint(hwnd, &paint);

    HDC buffer_dc    = CreateCompatibleDC(window_dc);
    HBITMAP buffer   = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
    HGDIOBJ previous = SelectObject(buffer_dc, buffer);

    FillRect(buffer_dc, &client, /* 背景色 */);
    SetBkMode(buffer_dc, TRANSPARENT);
    DrawTextW(buffer_dc, g_drawn_text, -1, &g_drawn_rect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);
    // ... 收尾释放
}
```

直接往窗口 DC 上画的版本，在背景擦掉和文字画上之间会露出一个肉眼可见的缝隙，您缩放窗口的时候能看见它闪一下。离屏位图的用法是把整帧组好再一次性贴过去，缝隙就没了。配套的还有一条，`WM_ERASEBKGND` 里返回的是 1，等于告诉了系统背景由自己擦。

按钮点下去的结果是两个输出对不上，您在屏幕上就能看见：

- 系统画的 `STATIC`，文字跟着变了
- 程序自己画的那一行，纹丝不动

点击处理的三行里写着 bug，您一眼就能认出来：

```cpp
SetWindowTextW(g_label_hwnd, text);   // 系统控件：它自己会失效、自己会重画
lstrcpynW(g_drawn_text, text, 256);   // 我们自己的：改完就停手了
```

`SetWindowTextW` 在内部把控件标成了脏，剩下的重画交给系统。程序自己只是把字符串拷进了全局变量，而把 `g_drawn_rect` 标成脏的动作谁也没做。想让文字更新的办法，是您自己补一句 `InvalidateRect(hwnd, &g_drawn_rect, TRUE)`。

这句故意没写的代码，就是咱们在第 2 步要解决的第五处别扭。

原生控件的像素不是您画的，一行绘制代码都没写的版本里按钮照样出现了，答案全在 `comctl32` 的窗口过程上。

## 五处别扭

这一版到这里就完整了，七条验收挨着过了一遍，都过了，身上却带着五处刻意的别扭。五处都是故意留的，改起来的难度并不高，难的是您得看见它们。

| # | 痛点 | 在哪儿 | 第 2 步由谁解决 |
|:--:|:---|:---|:---|
| 1 | 状态无处安放，全是全局变量 | `g_edit_hwnd` / `g_button_hwnd` / `g_label_hwnd` / `g_drawn_text` / `g_drawn_rect` | 窗口对象、控件对象 |
| 2 | 加一个控件要改三处 | ID 常量 + `CreateWindowExW` + `WM_COMMAND` 分支 | 控件树 + 自动 ID |
| 3 | 消息处理挤在一个 `switch` 里 | `WndProc` 的六个 `case` | 类型化事件 + 事件路由 |
| 4 | 位置全靠手算魔数 | `layout_controls` | 布局抽象 |
| 5 | 状态改了界面不动 | `g_drawn_text` 改了却没人失效它 | 属性 + 自动重画 |

## 三个练习

下面三个练习的指向都很具体，您做的时候能把别扭摸到手。

1. 加 ESC 退出，会碰到键盘消息该发给谁、焦点在谁身上。
2. 加第四个控件（清空按钮），亲手数一数改了几处。
3. 把控件间距从 8 改成 12，改完去确认另外两处没受影响。

## 附录：完整代码

```cpp
#include <windows.h>

// Control IDs. Defined here, consumed far away: once in CreateWindowExW
// below, once in the WM_COMMAND branch of WndProc above. Adding a fourth
// control means touching all three places (fuel for pain point 2).
constexpr int kIdEdit   = 1001;
constexpr int kIdButton = 1002;
constexpr int kIdLabel  = 1003;

// Window class name. Used once when registering the class and once more when
// creating the window, hence hoisted to a constant.
constexpr wchar_t kClassName[] = L"AnatomyGuiM1a";

// The HWNDs of the controls. The click handler reaches the edit and the label
// through these. The button once managed without a global, because WM_COMMAND
// hands its HWND over in lParam - then hand layout arrived and asked for all
// three. Globals it is (fuel for pain point 1); static keeps them file-local.
static HWND g_edit_hwnd   = nullptr;
static HWND g_button_hwnd = nullptr;
static HWND g_label_hwnd  = nullptr;

// The line this program paints itself, plus the rectangle layout_controls
// hands it. More file-scope state, same debt as the HWNDs above (fuel for
// pain point 1).
static wchar_t g_drawn_text[256] = L"（这行字是本程序自己画的，还没被更新过）";
static RECT    g_drawn_rect{};

// Place the controls by hand. Every resize runs this, and every new control
// costs one more SetWindowPos plus more arithmetic (fuel for pain point 4).
// The margin lives here as a local constant and nothing else can read it.
//
// Ordering trap: the first WM_SIZE arrives from CreateWindowExW before the
// child windows exist, so the guard below is load-bearing.
static void layout_controls(HWND parent) {
    if (g_edit_hwnd == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(parent, &client);

    const int kMargin       = 20;
    const int kGap          = 8;
    const int kEditHeight   = 28;
    const int kButtonHeight = 36;
    const int kButtonWidth  = 120;
    const int kLabelHeight  = 32;
    const int kDrawnHeight  = 32;

    const int kContentWidth = client.right - client.left - (kMargin * 2);
    const int kEditTop      = kMargin;
    const int kButtonTop    = kEditTop + kEditHeight + kGap;
    const int kLabelTop     = kButtonTop + kButtonHeight + kGap;
    const int kDrawnTop     = kLabelTop + kLabelHeight + kGap;

    SetWindowPos(g_edit_hwnd, nullptr, kMargin, kEditTop, kContentWidth, kEditHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_button_hwnd, nullptr, kMargin, kButtonTop, kButtonWidth, kButtonHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_label_hwnd, nullptr, kMargin, kLabelTop, kContentWidth, kLabelHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // The painted line is not a window, so it gets no SetWindowPos: its
    // rectangle is arithmetic handed to WM_PAINT through a global.
    g_drawn_rect.left   = kMargin;
    g_drawn_rect.top    = kDrawnTop;
    g_drawn_rect.right  = kMargin + kContentWidth;
    g_drawn_rect.bottom = kDrawnTop + kDrawnHeight;
}

// The window procedure: where every message this program receives lands.
// static: only the system calls it back, and only this file needs it.
// Naming exemption: WndProc / wParam / lParam are the SDK's own words. The
// platform boundary skips this repo's naming rules - readers hold this code
// against the official docs.
// NOLINTBEGIN(readability-identifier-naming)
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // NOLINTEND(readability-identifier-naming)
    // Six messages known now - the switch keeps growing. Every control added
    // from here lengthens this branch (fuel for pain point 3).
    switch (msg) {
    case WM_COMMAND:
        // A control notified its parent. Unpack the envelope:
        //   LOWORD(wParam) = control ID, HIWORD(wParam) = notification code,
        //   lParam = the control's own HWND.
        if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) {
            // The click handler: read the edit box, feed both outputs.
            wchar_t text[256];
            GetWindowTextW(g_edit_hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));

            // Output 1 - a system-drawn STATIC. SetWindowTextW invalidates the
            // control itself, so the new text appears with no help from here.
            SetWindowTextW(g_label_hwnd, text);

            // Output 2 - the line painted in WM_PAINT below. Copy the text and
            // stop: no InvalidateRect, on purpose. Nothing marks that
            // rectangle dirty, so the line keeps showing its previous value.
            // Pain point 5, left in place deliberately; the abstract pass
            // makes this call unnecessary instead of mandatory.
            lstrcpynW(g_drawn_text, text, 256);
            return 0;
        }
        break;  // someone else's command - hand it to DefWindowProc below

    case WM_SIZE:
        // Nothing else moves the controls: they sit where they were put until
        // this branch runs again (fuel for pain point 4).
        if (wParam != SIZE_MINIMIZED) {
            layout_controls(hwnd);
        }
        return 0;

    case WM_ERASEBKGND:
        // Claim the erase and do nothing. The class brush set in wWinMain
        // never runs now: WM_PAINT below owns the whole client area. Letting
        // the system erase first is what makes a resize flash.
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC         window_dc = BeginPaint(hwnd, &paint);

        // Double buffer: compose the frame in an off-screen bitmap, then blit
        // it in one go. Painting straight into the window DC leaves a visible
        // gap between filling the background and drawing the text.
        RECT client{};
        GetClientRect(hwnd, &client);
        HDC     buffer_dc = CreateCompatibleDC(window_dc);
        HBITMAP buffer    = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
        HGDIOBJ previous  = SelectObject(buffer_dc, buffer);

        // WM_ERASEBKGND above refused to erase, so the frame starts here.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        FillRect(buffer_dc, &client,
                 reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)));
        // NOLINTEND(performance-no-int-to-ptr)

        // The line this program paints itself. The STATIC above refreshes on
        // its own; this one changes only when something asks for an
        // invalidate (fuel for pain point 5).
        SetBkMode(buffer_dc, TRANSPARENT);
        DrawTextW(buffer_dc, g_drawn_text, -1, &g_drawn_rect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);

        SelectObject(buffer_dc, previous);
        DeleteObject(buffer);
        DeleteDC(buffer_dc);

        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_DPICHANGED: {
        // The other half of the DPI bill. Declaring per-monitor awareness in
        // the manifest means the window must resize itself when the system
        // moves it to a monitor with a different scale. Windows hands over a
        // suggested rectangle in lParam and expects SetWindowPos back; the
        // WM_SIZE that follows re-runs layout_controls.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        // NOLINTEND(performance-no-int-to-ptr)
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DESTROY:
        // Window destroyed != process exit. PostQuitMessage drops WM_QUIT
        // into the queue, GetMessage then returns 0 and the loop ends -
        // two separate things.
        PostQuitMessage(0);
        return 0;

    default:
        // Unhandled messages must go back to the system, or the window
        // cannot even paint its title bar and frame.
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // A case that broke out instead of returning lands here.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Naming exemption as above: parameter names follow the winbase.h declaration
// and MSDN (nShowCmd matches the header's own word).
// NOLINTBEGIN(readability-identifier-naming)
int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] PWSTR pCmdLine, int nShowCmd) {
    // NOLINTEND(readability-identifier-naming)
    // 1) Register the window class: tell the system what this kind of window
    //    looks like and who receives its messages.
    WNDCLASSEXW window_class{};
    window_class.cbSize        = sizeof(window_class);
    window_class.lpfnWndProc   = WndProc;     // where messages land
    window_class.hInstance     = hInstance;   // this module's instance handle
    window_class.lpszClassName = kClassName;  // class name, unique in the process
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    // The SDK's official idiom: the background brush comes from a system
    // color index (COLOR_* + 1); the integer-to-handle cast is unavoidable.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    // NOLINTEND(performance-no-int-to-ptr)
    if (RegisterClassExW(&window_class) == 0) {
        return 1;
    }

    // 2) Create the window. WS_CLIPCHILDREN stops the parent's repaints
    //    from smearing over the controls - without it, resizing leaves
    //    ghosts. ShowWindow has not run yet, so it stays invisible.
    HWND hwnd = CreateWindowExW(0, kClassName, L"Anatomy GUI · M1a",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                720, 480, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        return 1;
    }

    // 3) Create the controls. They are windows too - same CreateWindowExW,
    //    just with a parent and the system's pre-registered class names
    //    (EDIT / BUTTON / STATIC). Child coordinates are relative to the
    //    parent's client area, in pixels, and they never move on their own:
    //    layout_controls has to move them (fuel for pain point 4).
    // The zeros below are placeholders. The real geometry arrives from
    // layout_controls, which runs before ShowWindow, so nothing shows at 0,0.
    // The SDK pun: for a child window, hMenu carries the control ID - an
    // integer smuggled through a handle parameter.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    g_edit_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEdit)), hInstance, nullptr);

    g_button_hwnd = CreateWindowExW(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdButton)),
                                    hInstance, nullptr);

    g_label_hwnd = CreateWindowExW(
        0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLabel)), hInstance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)

    // Controls default to the antique SYSTEM_FONT - small and ugly for
    // Chinese text. One WM_SETFONT each with a stock font fixes it. Stock
    // objects are shared; never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(g_edit_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(g_button_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(g_label_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

    // Place the controls once before the window appears. The WM_SIZE that
    // arrived during creation fired while the children did not exist yet.
    layout_controls(hwnd);

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    // 4) The message loop: the heartbeat of the program.
    //    GetMessage returns 0 on WM_QUIT and -1 on failure.
    //    Writing while (GetMessage(...)) treats -1 as "keep going" - a
    //    classic trap.
    MSG msg{};
    while (true) {
        const BOOL kResult = GetMessageW(&msg, nullptr, 0, 0);
        if (kResult == 0) {
            break;  // WM_QUIT
        }
        if (kResult == -1) {
            return 1;  // failure
        }

        TranslateMessage(&msg);  // turns key messages into character messages
        DispatchMessageW(&msg);  // delivers the message to the window procedure
    }

    return static_cast<int>(msg.wParam);
}
```
