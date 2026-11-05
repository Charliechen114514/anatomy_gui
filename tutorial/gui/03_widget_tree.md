---
title: 抽象 2/5 · 让控件成为一棵树
---

# 抽象 2/5 · 让控件成为一棵树

> 第 1 步的 demo 有 277 行。您加第四个控件那天，在文件头上添了个常量，在创建函数里又抄了一遍 ID，再到消息处理里补了个分支。三处都改对了，您点一下按钮才看见标签换了字。

## 三处改动隔着半个文件

第 1 步的 demo 里，您多挂一个控件要改三处。

```cpp
// 第一处：一个必须手工保证唯一的常量
constexpr int kIdEdit   = 1001;
constexpr int kIdButton = 1002;
constexpr int kIdLabel  = 1003;
```

您往文件头看，常量区的几个常量排成了一列，而谁跟谁配对全靠名字。

```cpp
// 第二处：一次创建调用
g_button_hwnd = CreateWindowExW(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdButton)), hInstance,
                                nullptr);
```

您写创建控件的那次调用，ID 又抄了一遍，转换成 `HMENU` 的活也得自己干。

```cpp
// 第三处：消息处理里的一个分支
if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) { ... }
```

您在消息处理里也添了一个分支，判断条件写成了 `LOWORD(wParam) == kIdButton`。

您把这三段并排放在屏幕上，唯一的联系就是同一个数字，而它出现了三次。

### 三处彼此不认识

多写几行并不是麻烦的地方，麻烦的地方在于这三段彼此不认识，您改对一处也保证不了另外两处。

- 常量抄错、两个 ID 写成同一个值，编译器不会替您拦下来。
- 创建调用漏了，`WM_COMMAND` 里的分支永远走不到，您得到的是一个点了没反应的按钮，而不是一条报错。
- 分支漏了，控件照常显示出来，点下去什么都没发生，也没有一行报错可看。

**父子关系只活在创建时的次序里。** 谁是谁的子节点，全看创建的时候有没有把父窗口句柄递进去。程序自己其实不知道这棵树是什么样子，所以您既没法遍历所有控件、也没法按名字挑出一个。

**生命周期靠的是约定。** 裸版的 277 行里没有一行销毁代码，子控件会在父窗口销毁的时候由系统一并回收，这是 Win32 的规则，所以 demo 不漏。您要在运行的时候摘掉一个控件，就得自己调 `DestroyWindow`、还得把 HWND 表里记着的那一项清干净。漏掉了一半，手上剩下的是一个悬垂句柄。

## 一个节点，一棵树

这一层要的东西不多，您让每个控件成为树里的一个节点，让这棵树成为语言层面的事实。

```cpp
class Widget {
public:
    Widget& add_child(std::unique_ptr<Widget> child);

    [[nodiscard]] Widget* parent() const;
    [[nodiscard]] const std::vector<std::unique_ptr<Widget>>& children() const;
    [[nodiscard]] int id() const;
    [[nodiscard]] Widget* find_by_id(int node_id);

    [[nodiscard]] const Rect& bounds() const;
    void set_bounds(const Rect& bounds);

protected:
    virtual void on_bounds_changed() {}
};
```

摆到明面上的成员只有四个。表里右边一列写着每个成员买到的东西，您对着读一遍就知道为什么要留它们。

| 成员 | 它买到什么 |
|:---|:---|
| `children_` 是 `unique_ptr` 的容器 | **父节点析构 = 整棵子树析构**。销毁不再是一份要记得维护的清单 |
| `parent_` | 往上走的能力，遍历/查找都需要 |
| `id()` | 构造时自动分配，**没有任何地方需要手工维护常量** |
| `find_by_id` | 从任意节点往下找 —— 深度优先递归，树很小，不值得优化 |

四行里最要紧的那一行管的是销毁。父节点析构的时候，`children_` 里那些 `unique_ptr` 就跟着析构了，整棵子树跟着没了，您在任何地方都不必维护一张销毁清单。

### id 从节点走到消息

这一层省事的地方在 id 上，您从头到尾没写过一句赋值。创建控件窗口的时候，节点把自己的 id 交了出去，它就成了控件的 ID。

```cpp
hwnd_ = CreateWindowExW(extended_style_, class_name_, text_.c_str(), style_, 0, 0, 0, 0,
                        parent_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id())),
                        instance, nullptr);
```

所以您在 `WM_COMMAND` 里收到的整数，天生就是某个节点的 id。

```cpp
case WM_COMMAND:
    if (HIWORD(wparam) == BN_CLICKED && root_.find_by_id(LOWORD(wparam)) == button_) {
        on_button_clicked();
        return 0;
    }
    break;
```

**系统消息里的一个整数，和树里的一个节点，靠一次类型转换接上了。** 您不用再维护一张 ID 到对象的表，因为要找的节点本来就在树里。

> id 的起点是 1，0 被特意留空了。`WM_COMMAND` 里的 `LOWORD(wparam)` 在某些情况下会是 0（菜单就是一例），这样 0 就匹配不上任何真实节点了，您能省掉一类特判。

### 有窗口的节点和没窗口的节点

树里现在挂上了两种叶子。带真窗口的一类交给系统画，`WS_EX_CLIENTEDGE` 的边框和命中测试都是系统给的。没有窗口的一类只占一个矩形，字得您自己画。

```cpp
// 有真窗口：系统画它、系统命中测试、系统给它无障碍
edit_ = &add_native(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL);

// 没有窗口：就是那行自己画的字
auto drawn = std::make_unique<Widget>();
drawn_     = &root_.add_child(std::move(drawn));
```

您把那行自己画的字也挂进了树里，它没有自己的 HWND，而 id、父节点、`bounds()` 一样不缺。布局算出来的矩形交给它，绘制读完的也是同一个矩形，跟别的节点一视同仁。

> **`NativeWidget` 为什么住在 `platform/` 而不在 `framework/`？**
> 仓库里定下了一条线，您在 `framework/` 里找不到 `windows.h`。`NativeWidget` 手里攥着的 `HWND` 天生就是平台绑定的东西，所以它只能住在 platform 层。它**继承**的是框架层的 `Widget`，依赖方向是单向的，platform 方向的头文件能看见 framework 的声明，framework 那边就看不见 platform 了。

`NativeWidget` 替咱们把两件杂活收进了框架。

- **销毁**：析构函数里调 `DestroyWindow`，树拆到哪儿就销到哪儿
- **字体**：每个原生控件默认补一条 `WM_SETFONT`。裸版里这行要复制三遍，如今它是框架的默认行为

## 一行写一个控件

第一篇留的作业是加第四个控件、数一数您改了几处。

```cpp
void DemoWindow::create_controls(HINSTANCE instance) {
    instance_ = instance;

    edit_   = &add_native(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL);
    button_ = &add_native(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE);
    label_  = &add_native(0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE);
    ...
}
```

现在一个控件只占一行的位置，ID、创建调用、销毁都不用管了。点了之后要干什么仍然得您自己写，那是业务的事。

您跑一遍，窗口是 720×480 的，编辑框、按钮、静态文本三样都挂好了，底下那行字是本程序自己画的。

### 总行数短了，代码长了

您把两份代码摊开数一遍，总数是短的，代码反而长了。

| | 总行 | 纯代码 |
|:---|--:|--:|
| 裸版 | 277 | 140 |
| 框架版 demo（`app/demo.cpp`） | 242 | 159 |
| 框架层（`framework/` + `platform/`） | 485 | 210 |
| **合计** | **727** | **369** |

`framework/widget.h` 的 96 行里，您只找得到 26 行代码，多出来的都给了 doxygen（本仓库对头文件的要求）。所以 727 行里真正在跑的是 369 行。

应用侧的纯代码比裸版多了 19 行，总行数从 277 掉到了 242。您翻开 `layout_controls` 就明白多出来的行花在哪儿，原因在 `set_bounds` 的写法上。

```cpp
edit_->set_bounds(Rect{.left = kMargin, .top = kEditTop, .right = kMargin + kContentWidth, .bottom = kEditTop + kEditHeight});
```

这比裸版里那行 `SetWindowPos` 啰嗦了不少。clang-tidy 要求聚合类型写指定初始化器（`.left = ...` 的样子），clang-format 又把结果折成了好几行。您写的时候光标要在括号里跳四五次，读的人还得把五行重新拼回一行。这是一处**还没磨的 API 毛刺**，您往后每次写 `set_bounds` 都会碰到它。

`if (... == button_)` 那句分支您还得写。ID 常量、创建调用、销毁代码都没了，而某次点击要办什么事，那是应用自己的业务逻辑。业务分支怎么写得优雅，是事件路由那一篇的事。

### 树已经在了，布局还没用上

第 1 步留下的五个麻烦，现在划掉了两个（状态有了归属、加控件改成一行）。您手上还剩三个，消息处理挤在一个 `switch` 里、位置靠手算的魔数、界面跟不上状态。事件路由那一篇处理的是 `switch`，另外两个就留给布局和属性重画那两篇了。

布局的处境有点意思，树已经能遍历了，布局已经能顺着这棵树往下排了。您翻到 `layout_controls` 里，看到的还是一行一行手算的魔数，`kEditTop` 就是 `kMargin` 加出来的。到了布局那一篇，这些常数就从树上算出来了。

## 附录：这一层的全部代码

### `framework/geometry.h`

```cpp
/**
 * @file geometry.h
 * @brief Layout geometry, in logical units.
 */

#pragma once

/**
 * @brief A point in a parent's coordinate space.
 */
struct Point {
    int x = 0;
    int y = 0;
};

/**
 * @brief A width and a height.
 */
struct Size {
    int width  = 0;
    int height = 0;
};

/**
 * @brief An axis-aligned rectangle.
 *
 * The framework keeps its own rectangle type on purpose: nothing under
 * framework/ includes windows.h, so this layer never learns what a RECT is.
 */
struct Rect {
    int left   = 0;
    int top    = 0;
    int right  = 0;
    int bottom = 0;

    /**
     * @brief Read the horizontal extent.
     *
     * @return right minus left.
     */
    [[nodiscard]] int width() const { return right - left; }

    /**
     * @brief Read the vertical extent.
     *
     * @return bottom minus top.
     */
    [[nodiscard]] int height() const { return bottom - top; }
};
```

### `framework/widget.h`

```cpp
/**
 * @file widget.h
 * @brief A node in the widget tree.
 */

#pragma once

#include <memory>
#include <vector>

#include "framework/geometry.h"

/**
 * @brief A node in the widget tree.
 *
 * This layer answers pain point 2. A control used to cost three scattered
 * edits: an id constant, a CreateWindowExW call and a branch in the message
 * handler. Now it costs one node.
 *
 * Layer rule: nothing under framework/ includes windows.h. Anything that owns
 * a native handle lives in the platform layer and derives from this class.
 */
class Widget {
public:
    Widget();
    virtual ~Widget()                = default;
    Widget(const Widget&)            = delete;
    Widget& operator=(const Widget&) = delete;

    /**
     * @brief Take ownership of a child node.
     *
     * @param[in] child The node to attach.
     * @return A reference to the attached child, for chaining.
     */
    Widget& add_child(std::unique_ptr<Widget> child);

    /**
     * @brief Read the node that owns this one.
     *
     * @return The parent, or nullptr for the root.
     */
    [[nodiscard]] Widget* parent() const { return parent_; }

    /**
     * @brief Read the children.
     *
     * @return The owned children, in insertion order.
     */
    [[nodiscard]] const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

    /**
     * @brief Read the node's identity.
     *
     * @return An id handed out at construction, unique inside the process.
     */
    [[nodiscard]] int id() const { return id_; }

    /**
     * @brief Find a node by id.
     *
     * @param[in] node_id The id to look for.
     * @return The matching node, or nullptr. Searches this subtree depth first.
     */
    [[nodiscard]] Widget* find_by_id(int node_id);

    /**
     * @brief Read the node's rectangle.
     *
     * @return The rectangle, in the parent's coordinate space.
     */
    [[nodiscard]] const Rect& bounds() const { return bounds_; }

    /**
     * @brief Move and resize the node.
     *
     * @param[in] bounds The new rectangle, in the parent's coordinate space.
     */
    void set_bounds(const Rect& bounds);

protected:
    /**
     * @brief Runs after the bounds change.
     *
     * A subclass overrides this to move whatever native object it owns.
     */
    virtual void on_bounds_changed() {}

private:
    static int next_id_;

    int                                  id_     = 0;
    Widget*                              parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
    Rect                                 bounds_{};
};
```

### `framework/widget.cpp`

```cpp
#include "framework/widget.h"

#include <utility>

namespace {

// Ids start at 1: zero is reserved so a zeroed Win32 control id (or a stray
// default-constructed message parameter) never matches a real node.
constexpr int kFirstId = 1;

}  // namespace

int Widget::next_id_ = kFirstId;

Widget::Widget() : id_(next_id_++) {}

Widget& Widget::add_child(std::unique_ptr<Widget> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));

    // The tree owns the child from here: destroying the parent destroys the
    // whole subtree, with no hand-written teardown anywhere.
    return *children_.back();
}

Widget* Widget::find_by_id(int node_id) {
    if (id_ == node_id) {
        return this;
    }
    for (auto& child : children_) {
        if (Widget* found = child->find_by_id(node_id); found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

void Widget::set_bounds(const Rect& bounds) {
    bounds_ = bounds;
    on_bounds_changed();
}
```

### `platform/native_widget.h`

```cpp
/**
 * @file native_widget.h
 * @brief A widget node backed by a real child window.
 */

#pragma once

#include <windows.h>

#include <string>

#include "framework/widget.h"

/**
 * @brief A widget that owns a child window.
 *
 * The system paints it, hit-tests it and gives it an accessibility story for
 * free. The price is that its pixels are not ours, and that it only exists
 * where Windows exists: this class lives in the platform layer for that reason.
 *
 * The node's id doubles as the Win32 control id, which is what lets a
 * WM_COMMAND turn back into a node with one lookup.
 */
class NativeWidget : public Widget {
public:
    /**
     * @brief Build a node for a system control class.
     *
     * @param[in] extended_style WS_EX_* flags for the child window.
     * @param[in] class_name A pre-registered class name, such as L"EDIT".
     * @param[in] text The initial text.
     * @param[in] style WS_* and class-specific flags for the child window.
     */
    NativeWidget(DWORD extended_style, const wchar_t* class_name, std::wstring text, DWORD style);

    ~NativeWidget() override;

    NativeWidget(const NativeWidget&)            = delete;
    NativeWidget& operator=(const NativeWidget&) = delete;

    /**
     * @brief Create the child window under a parent window.
     *
     * @param[in] parent_window The window that hosts the control.
     * @param[in] instance Module instance handle.
     * @return true when the system creates the control.
     */
    bool create(HWND parent_window, HINSTANCE instance);

    /**
     * @brief Replace the text the control shows.
     *
     * @param[in] text The new text.
     */
    void set_text(const std::wstring& text);

    /**
     * @brief Read the text the control shows.
     *
     * @return The current text.
     */
    [[nodiscard]] std::wstring text() const;

    /**
     * @brief Read the native handle.
     *
     * @return The handle, or nullptr before create() succeeds.
     */
    [[nodiscard]] HWND handle() const { return hwnd_; }

protected:
    void on_bounds_changed() override;

private:
    DWORD          extended_style_ = 0;
    const wchar_t* class_name_     = nullptr;
    std::wstring   text_;
    DWORD          style_ = 0;
    HWND           hwnd_  = nullptr;
};
```

### `platform/native_widget.cpp`

```cpp
#include "platform/native_widget.h"

#include <utility>

NativeWidget::NativeWidget(DWORD extended_style, const wchar_t* class_name, std::wstring text,
                           DWORD style)
    : extended_style_(extended_style),
      class_name_(class_name),
      text_(std::move(text)),
      style_(style) {}

NativeWidget::~NativeWidget() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

bool NativeWidget::create(HWND parent_window, HINSTANCE instance) {
    // NOLINTBEGIN(performance-no-int-to-ptr)
    hwnd_ = CreateWindowExW(extended_style_, class_name_, text_.c_str(), style_, 0, 0, 0, 0,
                            parent_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id())),
                            instance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)
    if (hwnd_ == nullptr) {
        return false;
    }

    // Controls default to the antique SYSTEM_FONT, which renders Chinese small
    // and cramped. Every native control gets a sane default here, once.
    // Stock objects are shared: never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

    // Apply whatever bounds the parent already assigned.
    on_bounds_changed();
    return true;
}

void NativeWidget::set_text(const std::wstring& text) {
    text_ = text;
    if (hwnd_ != nullptr) {
        SetWindowTextW(hwnd_, text_.c_str());
    }
}

std::wstring NativeWidget::text() const {
    if (hwnd_ == nullptr) {
        return text_;
    }

    const int    kLength = GetWindowTextLengthW(hwnd_);
    std::wstring result(static_cast<size_t>(kLength) + 1, L'\0');
    const int    kCopied = GetWindowTextW(hwnd_, result.data(), kLength + 1);
    result.resize(static_cast<size_t>(kCopied));
    return result;
}

void NativeWidget::on_bounds_changed() {
    if (hwnd_ == nullptr) {
        return;
    }

    const Rect& box = bounds();
    SetWindowPos(hwnd_, nullptr, box.left, box.top, box.width(), box.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}
```

### `app/demo.cpp`

```cpp
#include <memory>
#include <utility>

#include "platform/native_widget.h"
#include "platform/window.h"

namespace {

constexpr int kTextCapacity = 256;

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

    HINSTANCE instance_ = nullptr;
    Widget    root_;

    NativeWidget* edit_   = nullptr;
    NativeWidget* button_ = nullptr;
    NativeWidget* label_  = nullptr;

    // A leaf that owns no window: the tree holds it, the layout gives it a
    // rectangle, and paint() draws it. Two kinds of node in one tree.
    Widget* drawn_ = nullptr;

    wchar_t drawn_text_[kTextCapacity] = L"（这行字是本程序自己画的，还没被更新过）";
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

    auto drawn = std::make_unique<Widget>();
    drawn_     = &root_.add_child(std::move(drawn));

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

    // The arithmetic is still hand-written magic (pain point 4). What changed
    // is that a node now carries its own rectangle, so nobody re-derives a
    // window handle just to move something.
    edit_->set_bounds(Rect{.left   = kMargin,
                           .top    = kEditTop,
                           .right  = kMargin + kContentWidth,
                           .bottom = kEditTop + kEditHeight});
    button_->set_bounds(Rect{.left   = kMargin,
                             .top    = kButtonTop,
                             .right  = kMargin + kButtonWidth,
                             .bottom = kButtonTop + kButtonHeight});
    label_->set_bounds(Rect{.left   = kMargin,
                            .top    = kLabelTop,
                            .right  = kMargin + kContentWidth,
                            .bottom = kLabelTop + kLabelHeight});
    drawn_->set_bounds(Rect{.left   = kMargin,
                            .top    = kDrawnTop,
                            .right  = kMargin + kContentWidth,
                            .bottom = kDrawnTop + kDrawnHeight});
}

void DemoWindow::on_button_clicked() {
    const std::wstring kText = edit_->text();

    // Output 1: a system-drawn STATIC. SetWindowTextW invalidates the control
    // itself, so the new text appears with no help from here.
    label_->set_text(kText);

    // Output 2: the line painted by paint(). Copy the text and stop: still no
    // InvalidateRect. Pain point 5 stays in place on purpose.
    lstrcpynW(drawn_text_, kText.c_str(), kTextCapacity);
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
    DrawTextW(buffer_dc, drawn_text_, -1, &native_bounds,
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
        // The control id is a node id, so the handler finds its node with one
        // lookup instead of comparing against memorized constants.
        if (HIWORD(wparam) == BN_CLICKED && root_.find_by_id(LOWORD(wparam)) == button_) {
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
