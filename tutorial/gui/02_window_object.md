---
title: 抽象 1/5 · 给窗口找个家
---

# 抽象 1/5 · 给窗口找个家

> 第 1 步跑出来的窗口能显示、能响应点击，全部的记性却记在文件头上五个 `static` 变量里。
> 您在这一篇里会看到这层记性怎么搬进一个对象，代价是代码从 277 行涨到了 375 行。

## 五个 static 变量，没有一行代码说得清它们属于谁

第 1 步收工的时候，您把五样状态写成了五个文件作用域的变量：

```cpp
static HWND g_edit_hwnd   = nullptr;
static HWND g_button_hwnd = nullptr;
static HWND g_label_hwnd  = nullptr;
static wchar_t g_drawn_text[256] = L"（这行字是本程序自己画的，还没被更新过）";
static RECT g_drawn_rect{};
```

您数一遍就知道：三个是控件的 `HWND`，第四行装那行字的文本，第五行存它占的矩形。窗口自己的句柄不在里面，它一直只是 `wWinMain` 里的一个局部变量，窗口过程靠参数把它接了回去。

五行变量的主人都是同一个窗口，可语言层面找不到一处写着归属的地方。您把 demo 复制一份、改个类名当第二个窗口用，两个窗口就抢了同一组变量。您在新窗口里敲一个字，旧窗口的 `STATIC` 也跟着变。拖动新窗口的边缘，旧窗口那边的编辑框也跟着挪。

您改错一个名字，系统的任何一处机制都不会拦您。窗口销毁了以后，这几个变量还活着并指着一个已经死掉的 `HWND`。更麻烦的一处落在消息处理上：系统调 `WndProc` 的时候手里只有一个 `hwnd`，签名里没有留给对象的位置，而函数本身也认不出自己是谁，想碰 `g_edit_hwnd` 就只能去翻文件头上的全局变量。

窗口和它的状态之间需要一条语言级别的线，您不用再靠命名约定去维持它。线接上了以后，消息处理才找得回自己的位置。

## 把 this 挂到窗口自己身上

您改不了窗口过程的签名，系统的要求就写在 C 回调的类型里：

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
```

系统调它的时候只知道 `hwnd`，别的都在它的视野之外。消息处理却想写成的是成员函数，让您伸手就能摸到自己的 `edit_` 和 `button_`。接上两边的，是系统给每个窗口留的指针大小槽位 `GWLP_USERDATA`，它专门给应用挂自己的数据。

放指针的时机只有一个，就是 `WM_NCCREATE` 到来的时候，您在窗口过程里能看到它比 `WM_CREATE` 更早。您顺着窗口的创建顺序走一遍，理由自然就出来了：

- 它**在 `CreateWindowExW` 返回之前就发出来了**，那时候窗口对象已经存在，而 `create()` 还没返回，您正好有机会在里面做初始化
- 它是第一条带着创建参数的消息，那些参数就是 `CreateWindowExW` 的最后一个实参

于是创建窗口的时候，您把 `this` 从最后一个实参递了进去：

```cpp
HWND hwnd = CreateWindowExW(0, kWindowClassName, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                            instance, this);
```

接住它的地方在窗口过程里，您手边常用的写法是这样：

```cpp
if (msg == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self   = static_cast<Window*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
    return self->handle_message(msg, wparam, lparam);
}

auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
```

`lpCreateParams` 是把 `this` 递进创建流程的入口，`WM_NCCREATE` 是唯一赶在窗口成型之前接住它的时机，之后的消息全靠 `GWLP_USERDATA` 把它取回来。您在调试器里断在 `window_proc` 的入口处，会看到第一次进来时 `GetWindowLongPtrW` 拿到的是空指针，`self->hwnd_ = hwnd` 那一行执行完了，值就变成了 `this`。

> **`window_proc` 为什么必须是 `static`。** 普通成员函数身上藏着的 `this` 参数，签名跟系统要的函数指针对不上，您在 `lpfnWndProc` 那一行就会看到编译失败。而 `static` 成员函数身上没有隐藏参数，签名才是干净的。代价是它得自己想办法找回自己的 `this`，上面那段代码干的就是这件事。
>
> **`SetWindowLongPtrW` 里的 `Ptr` 是认真的。** 您要是换成 `SetWindowLongW`，64 位下指针的高 32 位会被削掉，编译这一关还是过得去的，运行到第一次点击就是一条 0xC0000005 的访问冲突。

有了 `self`，您手里的消息处理就变成了一次虚函数调用：

```cpp
LRESULT Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }
}
```

子类覆写了 `handle_message`，就插进了消息流。框架二字在这一层没有更深的含义，无非是把系统定死的 C 回调，变成一件您能继承的东西。

## demo 变成子类，行数从 277 涨到 375

您会在应用侧看到两处改动。`demo` 原来靠的是五个全局变量加一堆自由函数，现在缩成了一段声明，五个名字都收了进来：

```cpp
class DemoWindow : public Window {
private:
    HWND edit_   = nullptr;
    HWND button_ = nullptr;
    HWND label_  = nullptr;
    wchar_t drawn_text_[kTextCapacity] = L"（这行字是本程序自己画的，还没被更新过）";
    RECT    drawn_rect_{};
};
```

另一处改动落在 `wWinMain` 的入口段，您一眼就能看到。原来那串注册窗口类、创建窗口、建控件、显示的动作，收成了三行：

```cpp
DemoWindow window;
if (!window.create(hInstance, L"Anatomy GUI · 抽象 1/5", 720, 480)) {
    return 1;
}
window.create_controls(hInstance);
window.show(nShowCmd);
```

您跑起来看到的东西和原来一样，还是三个控件、同样的布局、同样那行不会更新的字。状态有了主，而主是语言层面给的，而不是一个约定。

行数摆出来是这样的，您把四个数字放在一起看：

| | 行数 |
|:---|--:|
| 裸版（`app/m1a_naked.cpp`） | 277 |
| 框架版 demo（`app/demo.cpp`） | 222 |
| 框架本身（`platform/window.h` + `.cpp`） | 153 |
| **框架版合计** | **375** |

您能看到代码从 277 行涨到了 375 行，多了 98 行，涨了三分之一。

您会看到应用代码确实短了 55 行，从 277 行降到了 222 行，注册窗口类、创建窗口那些一次性动作搬进了框架。框架那 153 行是新增的，其中约 45 行是 doxygen 注释（本仓库的写法要求把每个公开函数的参数和返回值都写全），约 15 行花在 `this` 的搬运上，其余的都留给接口和默认实现。

变长的行数，咱们不打算省掉。往后四层（控件树、事件路由、布局、属性）的东西都要往这套框架里放。您每过一层都会看到，框架替您腾出来的地方派上了用场，而少了 `Window` 当宿主，那些东西就只好继续散成全局函数和全局变量了。

状态收进了对象，可 `create_controls` 里还是一样的写法，三个 `CreateWindowExW` 的调用配上三个 ID 常量，跟原来的一模一样。`handle_message` 里的分支也没动过。加第四个控件的时候，您要改的地方仍然是三处，控件之间的关系一处都没写进代码。

`handle_message` 那边还是一样的排法，五个消息分支全挤进了同一个 `switch`，位置还得靠手算的魔数摆，文字改了，界面上的字也不会跟着更新。下一层接走的是控件树，控件之间的关系到时候才写进代码，您改起控件来就不必再动三处。

## 附录：这一层的全部代码

### `platform/window.h`

```cpp
/**
 * @file window.h
 * @brief A window together with the object that owns it.
 */

#pragma once

#include <windows.h>

/**
 * @brief A window plus the object that owns it, tied so the window procedure
 * can find its way back to the instance.
 *
 * The window procedure is a C callback that knows nothing about `this`, so the
 * object travels through the creation parameters and gets stashed on the window
 * itself. This is the answer to pain point 1 of the concrete pass: state
 * finally has somewhere to live.
 *
 * Parameter names keep the SDK's spelling (msg / wparam / lparam) so this code
 * still reads against the official documentation.
 */
class Window {
public:
    Window()                         = default;
    virtual ~Window()                = default;
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Register the window class and create the window.
     *
     * @param[in] instance Module instance handle from wWinMain.
     * @param[in] title Window title, shown in the caption.
     * @param[in] width Initial width in pixels.
     * @param[in] height Initial height in pixels.
     * @return true when the system accepts both steps.
     */
    bool create(HINSTANCE instance, const wchar_t* title, int width, int height);

    /**
     * @brief Show the window and force the first paint.
     *
     * @param[in] show_command One of the SW_* values, usually straight from
     * wWinMain's nShowCmd.
     */
    void show(int show_command) const;

    /**
     * @brief Read the native window handle.
     *
     * @return The handle, or nullptr before create() succeeds.
     */
    [[nodiscard]] HWND handle() const { return hwnd_; }

protected:
    /**
     * @brief Handle one message.
     *
     * A subclass handles the messages it knows and delegates everything else to
     * Window::handle_message.
     *
     * @param[in] msg Message identifier.
     * @param[in] wparam Message-specific word parameter.
     * @param[in] lparam Message-specific long parameter.
     * @return The value the system expects for this message.
     */
    virtual LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

private:
    // The C callback the system calls. It cannot be an ordinary member
    // function: the system knows nothing about `this`.
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND hwnd_ = nullptr;
};
```

### `platform/window.cpp`

```cpp
#include "platform/window.h"

namespace {

// The class name must be unique inside the process. One class serves every
// Window instance.
constexpr wchar_t kWindowClassName[] = L"AnatomyGuiWindow";

}  // namespace

bool Window::create(HINSTANCE instance, const wchar_t* title, int width, int height) {
    WNDCLASSEXW window_class{};
    window_class.cbSize        = sizeof(window_class);
    window_class.lpfnWndProc   = &Window::window_proc;
    window_class.hInstance     = instance;
    window_class.lpszClassName = kWindowClassName;
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    // The class brush stays null: the paint handler owns the client area, and
    // letting the system erase first is what makes a resize flash.
    window_class.hbrBackground = nullptr;

    // Registering the same class twice fails. A second window is a supported
    // case, so treat "already there" as success.
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // The last argument is the only channel that reaches the window procedure
    // before CreateWindowExW returns, so the object travels through it.
    HWND hwnd = CreateWindowExW(0, kWindowClassName, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                                instance, this);
    if (hwnd == nullptr) {
        return false;
    }

    hwnd_ = hwnd;
    return true;
}

void Window::show(int show_command) const {
    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
}

// NOLINTBEGIN(performance-no-int-to-ptr)
LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // WM_NCCREATE is the only message that carries the creation parameters, and
    // it arrives before CreateWindowExW returns. Stash the object pointer here;
    // every later message reads it back.
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self   = static_cast<Window*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
        return self->handle_message(msg, wparam, lparam);
    }

    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->handle_message(msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
// NOLINTEND(performance-no-int-to-ptr)

LRESULT Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        // A destroyed window is not an exiting process: drop WM_QUIT in the
        // queue and let the message loop end on its own terms.
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }
}
```

### `app/demo.cpp`

```cpp
#include "platform/window.h"

namespace {

// Control IDs. Pain point 2 is untouched: each control still costs an ID
// constant, a CreateWindowExW call and a branch in the message handler.
constexpr int kIdEdit   = 1001;
constexpr int kIdButton = 1002;
constexpr int kIdLabel  = 1003;

constexpr int kTextCapacity = 256;

// The same demo as the concrete pass, now a Window subclass. Every global
// becomes a member and the message switch becomes a method: pain point 1 is
// gone, the other four are untouched on purpose.
class DemoWindow : public Window {
public:
    void create_controls(HINSTANCE instance);

protected:
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    void layout_controls();
    void on_button_clicked();
    void paint();

    HWND edit_   = nullptr;
    HWND button_ = nullptr;
    HWND label_  = nullptr;

    // The line the program paints itself, plus the rectangle it lives in. Both
    // used to be file-scope globals.
    wchar_t drawn_text_[kTextCapacity] = L"（这行字是本程序自己画的，还没被更新过）";
    RECT    drawn_rect_{};
};

}  // namespace

void DemoWindow::create_controls(HINSTANCE instance) {
    // NOLINTBEGIN(performance-no-int-to-ptr)
    edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0,
        handle(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEdit)), instance, nullptr);

    button_ = CreateWindowExW(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, handle(),
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdButton)), instance,
                              nullptr);

    label_ = CreateWindowExW(
        0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        handle(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLabel)), instance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)

    // Controls default to the antique SYSTEM_FONT, which renders Chinese small
    // and cramped. Stock objects are shared: never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(button_, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(label_, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

    layout_controls();
}

void DemoWindow::layout_controls() {
    // The first WM_SIZE arrives from CreateWindowExW, before the children
    // exist. The guard is load-bearing.
    if (edit_ == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(handle(), &client);

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

    SetWindowPos(edit_, nullptr, kMargin, kEditTop, kContentWidth, kEditHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(button_, nullptr, kMargin, kButtonTop, kButtonWidth, kButtonHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(label_, nullptr, kMargin, kLabelTop, kContentWidth, kLabelHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    drawn_rect_.left   = kMargin;
    drawn_rect_.top    = kDrawnTop;
    drawn_rect_.right  = kMargin + kContentWidth;
    drawn_rect_.bottom = kDrawnTop + kDrawnHeight;
}

void DemoWindow::on_button_clicked() {
    wchar_t text[kTextCapacity];
    GetWindowTextW(edit_, text, kTextCapacity);

    // Output 1: a system-drawn STATIC. SetWindowTextW invalidates the control
    // itself, so the new text appears with no help from here.
    SetWindowTextW(label_, text);

    // Output 2: the line painted by paint(). Copy the text and stop: still no
    // InvalidateRect. Pain point 5 stays in place on purpose.
    lstrcpynW(drawn_text_, text, kTextCapacity);
}

void DemoWindow::paint() {
    PAINTSTRUCT paint_state{};
    HDC         window_dc = BeginPaint(handle(), &paint_state);

    // Compose the frame off screen and blit it in one go; painting straight
    // into the window DC leaves a visible gap between the two steps.
    RECT client{};
    GetClientRect(handle(), &client);
    HDC     buffer_dc = CreateCompatibleDC(window_dc);
    HBITMAP buffer    = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
    HGDIOBJ previous  = SelectObject(buffer_dc, buffer);

    // WM_ERASEBKGND refuses to erase, so the frame starts here.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    FillRect(buffer_dc, &client, reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)));
    // NOLINTEND(performance-no-int-to-ptr)

    SetBkMode(buffer_dc, TRANSPARENT);
    DrawTextW(buffer_dc, drawn_text_, -1, &drawn_rect_,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);

    SelectObject(buffer_dc, previous);
    DeleteObject(buffer);
    DeleteDC(buffer_dc);

    EndPaint(handle(), &paint_state);
}

LRESULT DemoWindow::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_COMMAND:
        // LOWORD(wparam) is the control ID, HIWORD(wparam) the notification
        // code, lparam the control's own HWND.
        if (LOWORD(wparam) == kIdButton && HIWORD(wparam) == BN_CLICKED) {
            on_button_clicked();
            return 0;
        }
        break;

    case WM_SIZE:
        // Nothing else moves the controls. layout_controls reads the fresh
        // client size straight from the window.
        if (wparam != SIZE_MINIMIZED) {
            layout_controls();
        }
        return 0;

    case WM_ERASEBKGND:
        // Claim the erase and do nothing: paint() owns the whole client area.
        return 1;

    case WM_PAINT:
        paint();
        return 0;

    case WM_DPICHANGED: {
        // Windows suggests a new rectangle and expects SetWindowPos back. The
        // WM_SIZE that follows re-runs layout_controls.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        // NOLINTEND(performance-no-int-to-ptr)
        SetWindowPos(handle(), nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    default:
        break;
    }

    // Everything else, WM_DESTROY included, belongs to the base class.
    return Window::handle_message(msg, wparam, lparam);
}

// Naming exemption: the parameter names follow the winbase.h declaration and
// MSDN.
// NOLINTBEGIN(readability-identifier-naming)
int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] PWSTR pCmdLine, int nShowCmd) {
    // NOLINTEND(readability-identifier-naming)
    DemoWindow window;
    if (!window.create(hInstance, L"Anatomy GUI · 抽象 1/5", 720, 480)) {
        return 1;
    }
    window.create_controls(hInstance);
    window.show(nShowCmd);

    // The message loop stays in the application: it is fifteen lines and it is
    // the clearest thing in the file.
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

    return static_cast<int>(msg.wParam);
}
```
