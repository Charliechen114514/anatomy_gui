---
title: 抽象 3/5 · 消息不再自己处理
---

# 抽象 3/5 · 消息不再自己处理

## 一个 `case` 开始替所有人做决定

您翻回上一层收尾的地方，一次点击是这么接住的：

```cpp
case WM_COMMAND:
    if (HIWORD(wparam) == BN_CLICKED && root_.find_by_id(LOWORD(wparam)) == button_) {
        on_button_clicked();
        return 0;
    }
    break;
```

`if` 里塞了两问。头一问认的是 `BN_CLICKED`，另一问靠 `find_by_id` 认出 `LOWORD(wparam)` 给的 id。两问都过了，剩下的才是您熟悉的 `on_button_clicked()` 的事。

这两问干的是不同的事，您看头一问把 Win32 的通知翻成“某个控件被点了”，第二问负责把消息交到该收的控件手上。`on_button_clicked()` 管的是第三件事，而这件事只有应用自己清楚。

麻烦的地方在于它们住在一起。三样东西挤进了同一个 `case`，您改一件，另外两件也跟着变了。

- 加一个会响应的控件，您得跑到文件另一头去找 `switch`，再塞一个分支进去
- 往上冒泡这类规则没有固定的住处，每多出一种事件，都要在 `switch` 里重新想一遍
- 控件收不收、处理没处理，答案散落在窗口过程的各个分支里，不在控件身上

窗口过程本来是给系统消息留的接收处，您在这一层会看到它变成整个程序的调度中心。谁都能往里头加一个分支了。

## 控件只回答两个问题

路由搬走了以后，您手里能问控件的只剩两句话。第一句问的是“我收不收”，第二句问的是“我处理了没有”。

这两句话落进代码了，控件手里就得有一个能装它的东西，而声明本身不长，您看下面这几行。

```cpp
enum class EventKind { Click, TextChanged };

class Event {
public:
    Event(EventKind kind, Widget* target);
    [[nodiscard]] EventKind kind() const;
    [[nodiscard]] Widget* target() const;
    [[nodiscard]] bool handled() const;
    void mark_handled();
};
```

笔者只给 `EventKind` 留了 `Click` 和 `TextChanged` 两个值，够这一层的示范用。真正决定要不要接着往下走的位置，留给了 `handled_`。写它的是 `mark_handled()`，读它的是 `handled()`。

两个虚函数的签名不长，您看这两行：

```cpp
// 我收不收这个事件
[[nodiscard]] virtual bool accepts(const Event& event) const;

// 我处理了没有
virtual bool on_event(Event& event);
```

`accepts` 回答的是第一句，`on_event` 回答的是第二句。两条答案凑到了一起，您就能看出路由器凭什么停下来了。

## 分派有了固定的住处

路由规则归了 `EventRouter`，控件再也碰不到它了。您要守的只有一条：从目标节点出发，沿着父链一级级地往上走，谁认领了就停在谁那里。

```cpp
bool EventRouter::dispatch(Event& event) const {
    for (Widget* node = event.target(); node != nullptr; node = node->parent()) {
        if (!node->accepts(event)) {
            continue;
        }
        if (node->on_event(event)) {
            event.mark_handled();
            return true;
        }
    }
    return false;
}
```

您留意 `for` 的第三段，它写的是 `node = node->parent()`。那段写成了自增的样子，做的却是整条父链的遍历。上一层搭好的父子关系，到这里才真的被用上。不认领的节点会被跳过，而冒泡不会因为这一跳断了。

您把断点打在 `dispatch` 里，会看到循环体只走了一次。`node` 指的就是那颗按钮，`on_event` 返回的是 true，`mark_handled()` 的调用紧跟在后，父链上什么都没动。冒泡这一段修好了，而 M1 的按钮还用不上它。

## 窗口过程退成翻译层

`WM_COMMAND` 进了窗口过程以后，您手上只剩翻译这一件事。系统发来的通知在这里被翻成“某个控件的单击”，分派那一步交给了路由器。`WM_NOTIFY` 走的是同一套翻法，M1 只用了前面那一路。

```cpp
case WM_COMMAND:
    if (HIWORD(wparam) == BN_CLICKED) {
        if (Widget* target = root_.find_by_id(LOWORD(wparam)); target != nullptr) {
            Event event(EventKind::Click, target);
            if (router_.dispatch(event)) {
                return 0;
            }
        }
    }
    break;
```

它不再知道哪个节点是按钮了，也不再管点下去该做什么了。您留给窗口过程的只有一句话：把系统的话翻成框架的话。

## 干活的那一行和按钮挨着了

```cpp
label_ = &add_native(0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE);

// What the button does lives next to where the button is created, not in
// a switch far below.
button_->set_handler([this](Event& event) {
    if (event.kind() != EventKind::Click) {
        return false;
    }
    on_button_clicked();
    return true;
});
```

您往下读两行，`add_native` 刚给 STATIC 挂上了树，下一句就给按钮挂上了处理器。

创建控件的那一行，离挂处理器的那一句只有两行的距离。要改按钮的行为，您改的就是这一段，不必再跳到文件别处去找 `switch` 了。

上一层把加控件从三处压到了一处，而会响应的控件还留在文件另一头的 `switch` 里。这一层把那一半也搬回来了，您改按钮的行为时，动手的地方只剩一段。

程序跑起来的时候，您在编辑框里敲了几个字，按下那颗写着 `显示` 的按钮，上面 STATIC 那行立刻换了字，下面自己画的那行也跟着换了。窗口开出来的时候是 720×480，两行字分给了两条不同的路，而按钮只有 `on_button_clicked()` 一个入口。

您把那几句 `set_handler` 注释掉了，编译这一关还是过得去的，点下去什么反应都没有。编译器什么也不说，运行时什么也不说，少掉的那一行只有您自己清楚。

## 这一层没做干净的地方

`EventKind` 是一张要人维护的表。您每加进一个值，某个处理器里就多了一次判断，而枚举更像一张框架能力清单。M1 里它只留了两个值，笔者没有为了对称去凑数。

路由现在的起点落在目标节点上，命中测试这一环还没轮到您做。M1 用的是原生控件，落点由系统算好了，消息也由系统直接投给该收的控件。路由器省掉了最难的一步。等 M2 把输入接管过来了，命中测试就该搬进 `EventRouter` 了，也就是控件行为清单里的 D1 和 D4 两格。

`set_handler` 定下的是一对一，一个节点身上只挂了它一个处理器。您真把它做全了，`set_handler` 会演化成 `Signal<T>` 那样的东西，同一个事件上挂了好几位订阅者，而每一位都有自己的回调。M1 的按钮只需要一件事，笔者就没有做订阅者列表。真要用上的时候再加，`connect` 交回来的 `Connection` 还得有人管断开的时机。

上一层的痛点清单上，前三行到这一步您都能划掉了，剩下的位置和重画归后面两层。

## 附录

这一层新写的代码都在下面，咱们在文件头的注释里写了取舍。`event.h` 定下事件的样子，`event_router` 管的是分派，`demo.cpp` 把它们拼到了一起。

### `framework/event.h`

```cpp
/**
 * @file event.h
 * @brief Typed events handed to widgets.
 */

#pragma once

#include "framework/geometry.h"

class Widget;

/**
 * @brief What happened.
 *
 * The list stays tiny on purpose: one kind per thing the demo actually reacts
 * to. Every kind added here costs a branch somewhere.
 */
enum class EventKind {
    Click,
    TextChanged,
};

/**
 * @brief One typed occurrence, carrying where it happened.
 *
 * A widget answers two separate questions about an event: whether it wants the
 * event at all, and whether it handled it. This object carries the answer to
 * the second one, so a router can decide whether to keep bubbling.
 */
class Event {
public:
    /**
     * @brief Build an event aimed at one node.
     *
     * @param[in] kind What happened.
     * @param[in] target The node the event is delivered to first.
     */
    Event(EventKind kind, Widget* target) : kind_(kind), target_(target) {}

    /**
     * @brief Read what happened.
     *
     * @return The event kind.
     */
    [[nodiscard]] EventKind kind() const { return kind_; }

    /**
     * @brief Read the node the event is aimed at.
     *
     * @return The target node, never null.
     */
    [[nodiscard]] Widget* target() const { return target_; }

    /**
     * @brief Ask whether somebody already claimed the event.
     *
     * @return true once a node handled it.
     */
    [[nodiscard]] bool handled() const { return handled_; }

    /**
     * @brief Mark the event as handled.
     *
     * Stops the router from bubbling any further.
     */
    void mark_handled() { handled_ = true; }

private:
    EventKind kind_;
    Widget*   target_;
    bool      handled_ = false;
};
```

### `framework/event_router.h`

```cpp
/**
 * @file event_router.h
 * @brief Moves an event through the widget tree.
 */

#pragma once

#include "framework/event.h"

/**
 * @brief Delivers events and owns the routing rules.
 *
 * The rules live here rather than in the widgets. A widget answers two
 * questions and nothing else: whether it wants the event at all, and whether it
 * handled the event.
 *
 * Routing today starts at the event's target, because Windows already did the
 * hit test for native controls. When M2 takes input routing over, the hit test
 * moves in here.
 */
class EventRouter {
public:
    /**
     * @brief Deliver an event to its target, then bubble up the parent chain.
     *
     * Stops at the first node that reports the event as handled.
     *
     * @param[in,out] event The event to deliver.
     * @return true when some node handled the event.
     */
    [[nodiscard]] bool dispatch(Event& event) const;
};
```

### `framework/event_router.cpp`

```cpp
#include "framework/event_router.h"

#include "framework/widget.h"

bool EventRouter::dispatch(Event& event) const {
    // Walk from the target towards the root. A node that does not accept the
    // event is skipped without stopping the walk.
    for (Widget* node = event.target(); node != nullptr; node = node->parent()) {
        if (!node->accepts(event)) {
            continue;
        }
        if (node->on_event(event)) {
            event.mark_handled();
            return true;
        }
    }
    return false;
}
```

### `app/demo.cpp`

```cpp
#include <memory>
#include <utility>

#include "framework/event_router.h"
#include "framework/property.h"
#include "framework/stack_layout.h"
#include "platform/native_widget.h"
#include "platform/window.h"

namespace {

// A leaf that owns the text it paints. Wiring the invalidation inside the
// node is what removes pain point 5: forgetting the repaint stops being
// something a caller can do.
class DrawnLine : public Widget {
public:
    DrawnLine() {
        text.on_changed([this] { invalidate(); });
    }

    Property<std::wstring> text;
};

// The same demo as the concrete pass, with the controls moved into a widget
// tree. Pain point 2 is gone: a control is one node now, and its id, its
// native window and its teardown all follow from that node.
class DemoWindow : public Window {
public:
    void create_controls(HINSTANCE instance);

protected:
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    // One line per control: no id constant, no CreateWindowExW, no teardown.
    NativeWidget& add_native(DWORD extended_style, const wchar_t* class_name, std::wstring text,
                             DWORD style);

    void layout_controls();
    void on_button_clicked();
    void paint();

    HINSTANCE   instance_ = nullptr;
    Widget      root_;
    EventRouter router_;

    NativeWidget* edit_   = nullptr;
    NativeWidget* button_ = nullptr;
    NativeWidget* label_  = nullptr;

    // A leaf that owns no window: the tree holds it, the layout gives it a
    // rectangle, and paint() draws it. Two kinds of node in one tree.
    DrawnLine* drawn_ = nullptr;
};

}  // namespace

NativeWidget& DemoWindow::add_native(DWORD extended_style, const wchar_t* class_name,
                                     std::wstring text, DWORD style) {
    auto widget =
        std::make_unique<NativeWidget>(extended_style, class_name, std::move(text), style);

    NativeWidget& node = *widget;
    root_.add_child(std::move(widget));
    node.create(handle(), instance_);
    return node;
}

void DemoWindow::create_controls(HINSTANCE instance) {
    instance_ = instance;

    edit_   = &add_native(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL);
    button_ = &add_native(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE);
    label_  = &add_native(0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE);

    // What the button does lives next to where the button is created, not in
    // a switch far below. Adding a control and teaching it to react are now
    // the same edit.
    button_->set_handler([this](Event& event) {
        if (event.kind() != EventKind::Click) {
            return false;
        }
        on_button_clicked();
        return true;
    });
    auto drawn = std::make_unique<DrawnLine>();
    drawn_     = drawn.get();
    root_.add_child(std::move(drawn));

    // Each node declares how big it wants to be; the layout decides where.
    edit_->set_preferred_size(Size{.width = 0, .height = 28});
    button_->set_preferred_size(Size{.width = 120, .height = 36});
    label_->set_preferred_size(Size{.width = 0, .height = 32});
    drawn_->set_preferred_size(Size{.width = 0, .height = 32});

    // One line describes the whole arrangement: twenty pixels of padding,
    // eight between neighbours.
    root_.set_layout(std::make_unique<StackLayout>(20, 8));

    drawn_->text.set(L"（这行字是本程序自己画的，还没被更新过）");

    // The root owns the hook that turns "somebody wants a repaint" into a
    // real invalidate on the window.
    root_.set_repaint_handler([this] { invalidate(); });

    layout_controls();
}

void DemoWindow::layout_controls() {
    // No arithmetic is left: hand the root the client area and let the layout
    // walk the tree. The ordering trap is gone with it - a node without a
    // native window simply has nothing to move yet.
    RECT client{};
    GetClientRect(handle(), &client);

    root_.set_bounds(Rect{.left   = 0,
                          .top    = 0,
                          .right  = client.right - client.left,
                          .bottom = client.bottom - client.top});
}

void DemoWindow::on_button_clicked() {
    const std::wstring kText = edit_->text();

    // Output 1: a system-drawn STATIC. SetWindowTextW invalidates the control
    // itself, so the new text appears with no help from here.
    label_->set_text(kText);

    // Output 2: the line painted by paint(). The node owns its text, so
    // writing it also schedules the repaint. Forgetting was the whole bug.
    drawn_->text.set(kText);
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

    // The framework rectangle becomes a Win32 RECT here and only here: this is
    // the seam between the two layers.
    const Rect& node_bounds = drawn_->bounds();
    RECT        native_bounds{.left   = node_bounds.left,
                              .top    = node_bounds.top,
                              .right  = node_bounds.right,
                              .bottom = node_bounds.bottom};

    SetBkMode(buffer_dc, TRANSPARENT);
    DrawTextW(buffer_dc, drawn_->text.value().c_str(), -1, &native_bounds,
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
        // The window procedure is a translation layer now, not a decision
        // maker: turn the Win32 notification into a framework event and let
        // the router find somebody willing to handle it.
        if (HIWORD(wparam) == BN_CLICKED) {
            if (Widget* target = root_.find_by_id(LOWORD(wparam)); target != nullptr) {
                Event event(EventKind::Click, target);
                if (router_.dispatch(event)) {
                    return 0;
                }
            }
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
    if (!window.create(hInstance, L"Anatomy GUI · 抽象 2/5", 720, 480)) {
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
