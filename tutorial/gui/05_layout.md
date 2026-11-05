---
title: 抽象 4/5 · 让代码算位置
---

# 抽象 4/5 · 让代码算位置

您把窗口从 720×480 拖到 1400×1000，编辑框跟着变宽了，按钮的宽度一点没变。宽窄的差别，源头是 `layout_controls` 里的算术。

上一层的 `layout_controls` 很难一眼读出结果，您得自己在纸上把 `kButtonTop`、`kLabelTop`、`kDrawnTop` 一路算下来，算完了才知道按钮落在哪。

您改了一行算术，后面的几行都得跟着动，编译器一句都不会提醒您。

## 改一个间距，要动四处

布局函数当初是这样的，笔者把七个常量和五行算术抄在了下面。

```cpp
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
```

顶上的七个常量管尺寸，中间夹着的五行算术管位置，底下四次 `set_bounds` 把算出来的中间量拼回去，拼完了您才看见控件落在哪。

- **顺序写死在算术里。** 要在按钮上面插一行静态文本，底下的 `Top` 连同 `set_bounds` 都得重算。
- **数字之间的关系没人看着。** 把 `kGap` 拨成 12，另外三处漏没漏，只能一个地方一个地方查。
- **四个控件重复了同样的活。** 每个都取上一块的底边，再加一个间距。

真正让笔者坐不住的是另一件事：控件自己报的尺寸，和容器给它的位置，本来是分开的两件事，算术里却串成了一条链。

## 尺寸归节点，位置归容器

拍板时笔者只留下一句话：**基本的布局一开始就立好**，别凑一个写死的排布，日后再补就贵了。

所以笔者把排布抽成了一份策略，而容器才是持有它的那一方：

```cpp
class Layout {
public:
    virtual void arrange(Widget& container, const Rect& area) = 0;
};
```

`Layout` 里只留了一个 `arrange`，收下容器和容器手里的区域。控件不是从它派生的，它只认交上来的矩形。您想换一种排法的时候，换掉的是容器手里的策略，控件的代码一行不用动。

框架里管尺寸和位置的两个词是 measure 和 arrange，您写布局的时候都会碰到。代码里只出现了 `arrange`，尺寸的那一半由 `preferred_size` 管。

`StackLayout` 是笔者写的第一个实现，摞的方向从上往下，容器边距和邻居间距各要了一个参数：

```cpp
void StackLayout::arrange(Widget& container, const Rect& area) {
    int top = area.top + padding_;

    for (const auto& child : container.children()) {
        const Size& wanted = child->preferred_size();
        const int kWidth   = wanted.width > 0 ? wanted.width : area.width() - (padding_ * 2);

        child->set_bounds(Rect{.left = area.left + padding_,
                               .top = top,
                               .right = area.left + padding_ + kWidth,
                               .bottom = top + wanted.height});
        top += wanted.height + gap_;
    }
}
```

`arrange` 里循环写的是 `for (const auto& child : container.children())`，树在这里第二次派上了用场，您第一次见到它的地方是上一层的冒泡。布局要容器做到的事只有一样，要的就是一串子节点。

到了节点这边，您只剩下一句声明要写。

```cpp
edit_->set_preferred_size(Size{.width = 0, .height = 28});
button_->set_preferred_size(Size{.width = 120, .height = 36});
```

宽度填 0 的交给容器定，填了 120 的，宽度就固定下来了。编辑框填的是 0，您把窗口拉宽的时候它跟着变宽。按钮填的是 120，窗口拖到 1400 的时候它还是 120。

**节点报的是自己想要的尺寸，位置由布局算好了交给它，写控件的时候您不必知道它落在哪**。

笔者让重排自动发生，只靠 `Widget::set_bounds` 里的三行：

```cpp
void Widget::set_bounds(const Rect& bounds) {
    bounds_ = bounds;
    on_bounds_changed();

    // A container lays its children out again after every move, so nobody has
    // to remember to call the layout by hand.
    layout_children();
}
```

容器挪了位置，子节点也跟着重排了一遍，靠的就是 `layout_children()`。挂了布局的节点，`set_bounds` 之后的重排是自动的，您什么都不用做。

## 三十行算术，剩六行

原来的 `layout_controls` 一共三十行，笔者把算术全压在了里头。

```cpp
void DemoWindow::layout_controls() {
    RECT client{};
    GetClientRect(handle(), &client);

    root_.set_bounds(Rect{.left = 0,
                          .top = 0,
                          .right = client.right - client.left,
                          .bottom = client.bottom - client.top});
}
```

您翻到函数体，只剩了六行。

窗口报上来的只有客户区的大小。您把宽高交给根节点，排布的活归布局。

`layout_controls` 开头挂着的 `if (edit_ == nullptr) return;` 是第 1 步留下的，防的是第一次 `WM_SIZE` 来得比子控件还早。现在它被删掉了，窗口里的控件照样落在原来的位置。布局算的只是节点的矩形，节点还没有窗口的时候，跳过去是 `NativeWidget::on_bounds_changed` 自己的事。咱们能删掉一项防御性检查，因为职责回到了该在的地方。

## 量了两遍，数没变

换布局的前后，笔者用同一个脚本各量了一版窗口。窗口停在 720×480 的时候，读到的是 `Edit:259 Button:48 Static:259`，把窗口拖到 1400×1000 的时候，读数变成了 `Edit:1371 Button:48 Static:1371`。两版的读数逐项相同，布局换掉的是写法，排出来的几何没动。

`preferred_size` 是笔者添的新概念，不是给旧东西改的名字。以前的控件不开口要尺寸，尺寸由父容器按常量算好了塞进去。添它是有代价的，每个用到的地方都得自己填一份。它同时是布局能跑起来的前提，缺了它，剩下的只有父容器硬给尺寸，第 1 步的样子正是这样。

笔者只写了一种布局。`StackLayout` 够 M1 用了，布局抽象的价值要等第二种实现写出来了才看得清。横向、网格、绝对定位的实现，笔者一个都没写。接口立住了，用不上的东西就不造了。

## 状态改了，界面没动

前面几篇记下的五条痛点，到这里已经解决了四条：状态有了归属，加控件改成了一行，消息出了 `switch`，位置由布局算了出来。最后剩下的是状态改了界面不动，笔者把它留给属性与重画。

其实它跟结构没什么关系，问的是您手里的哪些活交给代码、哪些活留给自己惦记。

## 附录

### `framework/layout.h`

```cpp
/**
 * @file layout.h
 * @brief The strategy that arranges a container's children.
 */

#pragma once

#include "framework/geometry.h"

class Widget;

/**
 * @brief Assigns rectangles to the children of a container.
 *
 * A layout is a strategy, not a base class of the widgets: the container owns
 * one and asks it to place the children. Swapping the strategy swaps the
 * arrangement without touching a single widget.
 */
class Layout {
public:
    Layout()                         = default;
    virtual ~Layout()                = default;
    Layout(const Layout&)            = delete;
    Layout& operator=(const Layout&) = delete;

    /**
     * @brief Give every child of the container a rectangle.
     *
     * @param[in] container The node whose children get arranged.
     * @param[in] area The rectangle the container has to work with.
     */
    virtual void arrange(Widget& container, const Rect& area) = 0;
};
```

### `framework/stack_layout.h`

```cpp
/**
 * @file stack_layout.h
 * @brief Stacks a container's children vertically.
 */

#pragma once

#include "framework/layout.h"

/**
 * @brief Places children one below another, top to bottom.
 *
 * A child that asks for a width of zero stretches to the container's width; a
 * child that asks for a width keeps it. Heights always come from the child,
 * because this layout has no opinion about how tall a thing should be.
 */
class StackLayout : public Layout {
public:
    /**
     * @brief Build a vertical stack.
     *
     * @param[in] padding Space between the container's edge and its children.
     * @param[in] gap Space between two neighbouring children.
     */
    StackLayout(int padding, int gap);

    /**
     * @brief Stack the children top to bottom.
     *
     * @param[in] container The node whose children get arranged.
     * @param[in] area The rectangle the container has to work with.
     */
    void arrange(Widget& container, const Rect& area) override;

private:
    int padding_;
    int gap_;
};
```

### `framework/stack_layout.cpp`

```cpp
#include "framework/stack_layout.h"

#include "framework/widget.h"

StackLayout::StackLayout(int padding, int gap) : padding_(padding), gap_(gap) {}

void StackLayout::arrange(Widget& container, const Rect& area) {
    int top = area.top + padding_;

    for (const auto& child : container.children()) {
        const Size& wanted = child->preferred_size();
        const int   kWidth = wanted.width > 0 ? wanted.width : area.width() - (padding_ * 2);

        child->set_bounds(Rect{.left   = area.left + padding_,
                               .top    = top,
                               .right  = area.left + padding_ + kWidth,
                               .bottom = top + wanted.height});
        top += wanted.height + gap_;
    }
}
```

### `framework/widget.h`

```cpp
/**
 * @file widget.h
 * @brief A node in the widget tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "framework/event.h"
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
class Layout;

class Widget {
public:
    /**
     * @brief The signature a node uses to install its event handler.
     */
    using EventHandler = std::function<bool(Event&)>;

    Widget();
    virtual ~Widget();
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

    /**
     * @brief Install the handler this node runs for events it accepts.
     *
     * @param[in] handler Called with every accepted event; returns true when
     * the handler dealt with it.
     */
    void set_handler(EventHandler handler);

    /**
     * @brief Answer whether this node wants the event.
     *
     * @param[in] event The event on its way through the tree.
     * @return true to let the event reach on_event.
     */
    [[nodiscard]] virtual bool accepts(const Event& event) const;

    /**
     * @brief Handle an accepted event.
     *
     * @param[in] event The event to handle.
     * @return true when this node handled it and bubbling stops.
     */
    virtual bool on_event(Event& event);

    /**
     * @brief Install the strategy that arranges this node's children.
     *
     * @param[in] layout The strategy, or nullptr to remove the current one.
     */
    void set_layout(std::unique_ptr<Layout> layout);

    /**
     * @brief Arrange the children.
     *
     * Runs on its own after every bounds change, so a container never has
     * to remember to lay out again.
     */
    void layout_children();

    /**
     * @brief Read the size this node asks for.
     *
     * @return The hint; a zero component means "whatever the layout decides".
     */
    [[nodiscard]] const Size& preferred_size() const { return preferred_size_; }

    /**
     * @brief Ask for a size.
     *
     * @param[in] size The hint; a zero component means "whatever the layout
     * decides".
     */
    void set_preferred_size(const Size& size) { preferred_size_ = size; }

    /**
     * @brief Ask for a repaint.
     *
     * Walks to the root and calls its repaint hook, so a node that owns a
     * property can refresh itself without knowing the platform exists.
     */
    void invalidate();

    /**
     * @brief Install the hook that turns a repaint request into a real redraw.
     *
     * Set on the root; the window decides what repainting means in its terms.
     *
     * @param[in] handler Called when some node below asks for a repaint.
     */
    void set_repaint_handler(std::function<void()> handler) { repaint_ = std::move(handler); }

protected:
    /**
     * @brief Runs after the bounds change.
     *
     * A subclass overrides this to move whatever native object it owns.
     */
    virtual void on_bounds_changed() {}

private:
    static int              next_id_;
    EventHandler            handler_;
    std::unique_ptr<Layout> layout_;
    Size                    preferred_size_{};
    std::function<void()>   repaint_;

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

#include "framework/layout.h"

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

bool Widget::accepts(const Event& /*unused*/) const {
    return true;
}

bool Widget::on_event(Event& event) {
    return handler_ != nullptr && handler_(event);
}

void Widget::set_handler(EventHandler handler) {
    handler_ = std::move(handler);
}

Widget::~Widget() = default;

void Widget::set_bounds(const Rect& bounds) {
    bounds_ = bounds;
    on_bounds_changed();

    // A container lays its children out again after every move, so nobody has
    // to remember to call the layout by hand.
    layout_children();
}

void Widget::set_layout(std::unique_ptr<Layout> layout) {
    layout_ = std::move(layout);
    layout_children();
}

void Widget::layout_children() {
    if (layout_ != nullptr) {
        layout_->arrange(*this, bounds_);
    }
}

void Widget::invalidate() {
    // A rectangle is relative to the parent, so a node cannot compute its own
    // screen area yet. The whole client area is repainted instead: the M1
    // simplification, with damage regions arriving in M2.
    const Widget* root = this;
    while (root->parent_ != nullptr) {
        root = root->parent_;
    }
    if (root->repaint_ != nullptr) {
        root->repaint_();
    }
}
```
