# Stage 5 — 布局引擎：让控件自己找到位置

---

## 这一阶段的目标

上一个阶段我们搭建了控件树——每个 Widget 有自己的 `paint()`、能响应事件、能通过父子关系组成一棵树。但有一个问题一直被我们回避了：控件的位置和尺寸到底由谁决定？在 Stage 4 里我们用硬编码的坐标把每个控件"钉"在固定位置上，这在演示阶段够用，但一旦窗口大小改变、或者控件数量动态变化，这种硬编码就彻底崩盘了。

这个阶段的目标是给 MiniUI 加上一个布局引擎——它能根据可用空间和布局策略，自动计算每个控件应该出现的精确位置和尺寸。我们会引入 GUI 编程中最经典的 Measure → Arrange → Render 三遍渲染管线，同时用 C++20 的 Concepts 来约束布局策略的接口，做到零运行时开销的类型安全抽象。

做完这一步之后，你写出来的代码大概是这个感觉：

```cpp
auto panel = std::make_shared<Container<HBoxLayout>>();
panel->add_child(button1);
panel->add_child(button2);
panel->add_child(button3);
// 不需要手动设置任何 x, y, width, height
// 布局引擎会自动把三个按钮水平排列，均匀分配空间
```

### 为什么硬编码布局不行

如果你写过 Phase 1 的 Win32 程序，一定对这个模式不陌生：

```cpp
// Win32 经典的"手动布局"
MoveWindow(hButton1, 10, 10, 200, 30, TRUE);
MoveWindow(hButton2, 220, 10, 200, 30, TRUE);
MoveWindow(hEdit, 10, 50, 410, 200, TRUE);
```

这种写法有三个致命问题。第一，它不响应窗口大小变化——用户拖动窗口边缘，控件纹丝不动，大片空白或者被截断。第二，它不可组合——你不能把"三个按钮水平排列"这件事封装成一个可复用的布局单元。第三，它不支持动态内容——如果按钮数量在运行时改变了，你得手动重新算一遍所有坐标。

所有成熟的 GUI 框架都解决了同一个问题，只是方式不同。Win32 后来有了 `WM_SIZE` + 手动重算的模式（还是很原始），GTK 用了 CSS 风格的 box model，Qt 用了 `QLayout` 的约束系统，SwiftUI 用了声明式的修饰符链。它们的核心思想都指向同一个东西：**把布局决策从"手动指定坐标"变成"声明布局策略，由引擎自动计算"**。

### 为什么现在才引入布局而不是更早

因为在 Stage 4 之前我们连控件树都没有——没有父子关系，就没有"容器安排子控件"这个概念的落脚点。布局引擎必须建立在控件树之上：它是容器（Container）对子控件施加的排列策略。控件树是布局引擎的前提条件，正如事件循环是控件树的前提条件。每一层只解决一个问题。

---

## Measure → Arrange → Render：GUI 渲染的通用管线

几乎所有主流 GUI 框架都采用了某种形式的"两遍布局 + 一遍绘制"模型。在我们的 MiniUI 里，它看起来是这样：

```
┌──────────────────────────────────────────────────────┐
│                  MiniUI 渲染管线                      │
│                                                      │
│   ┌────────────┐    ┌────────────┐    ┌───────────┐  │
│   │  Measure   │───>│  Arrange   │───>│  Render   │  │
│   │  自底向上   │    │  自顶向下   │    │  自顶向下  │  │
│   │            │    │            │    │           │  │
│   │ "你需要多大"│    │ "你被分配了 │    │ "画出自己" │  │
│   │            │    │  这块区域"  │    │           │  │
│   └────────────┘    └────────────┘    └───────────┘  │
│                                                      │
│   Measure: 叶子节点返回自然尺寸，容器汇总子节点需求      │
│   Arrange: 根节点分配屏幕区域，递归分派给子节点          │
│   Render:  按照分配好的 Rect 依次绘制                   │
└──────────────────────────────────────────────────────┘
```

### 三遍各自的职责

**Measure（测量）**——自底向上。从控件树的叶子节点开始，每个控件回答一个问题："如果给你这么大的可用空间，你实际需要多大？"叶子节点（比如一个 Label）可以根据文字内容和字体大小计算出自己的"自然尺寸"。容器节点收集所有子节点的尺寸需求，然后根据自己的布局策略汇总出一个整体需求。这一步的关键是它不修改任何状态，只是"问"——返回一个期望的 Size。

**Arrange（排列）**——自顶向下。根节点拿到了屏幕上的实际区域（一个 `Rect`，包含位置和尺寸），然后按照布局策略把这个区域分配给每个子节点。分配的结果是每个子控件都拿到了一个确定的 `Rect`——"你就待在这个位置，就这么大"。这一步是实际修改控件几何状态的地方。

**Render（绘制）**——自顶向下。每个控件按照 Arrange 分配给自己的 `Rect` 来绘制自己。这就是我们在 Stage 3 里做的 `paint()`，只不过现在控件有了明确的绘制区域。

### 为什么不是两遍或者一遍

能不能把 Measure 和 Arrange 合并成一遍？有些简单的布局策略确实可以——比如绝对定位根本不需要 Measure，直接分配就行。但一旦涉及"子控件的尺寸需求影响容器的尺寸"这种双向依赖，一遍就搞不定了。比如一个竖向排列的容器，它的高度取决于所有子控件高度之和，而每个子控件的高度又取决于被给予的宽度（文字换行就是典型例子）。Measure 负责把信息从叶子汇总到根，Arrange 负责把决策从根分派到叶子——方向相反，职责清晰，这是几十年来 GUI 框架设计收敛到的最优解。

---

## 三种布局模型：从原始到现代

在动手写代码之前，我们先横向对比三种主流的布局思路。理解了"为什么别人这么做"，你才能做出有意识的设计选择。

### 模型一：绝对定位

每个控件的坐标和尺寸由程序员硬编码。Win32 的 `MoveWindow` / `SetWindowPos` 就是这个模型。

```
优点：简单直接，零计算开销
缺点：不响应尺寸变化，不可复用，不可组合
代表：Win32 原始 API，早期的 Visual Studio 拖拽式设计器
```

我们在 Stage 4 里用的就是绝对定位——它是合理的起点，但不是一个合理的终点。

### 模型二：Box Model

容器沿主轴（水平或垂直）依次排列子控件，支持间距、对齐、弹性伸缩。这是 CSS Flexbox / GTK Box 的核心思路。

```
优点：可组合，自动响应尺寸变化，覆盖大多数常见布局
缺点：嵌套层级深时性能开销累积，复杂布局需要大量嵌套
代表：CSS Flexbox, GTK Box, Flutter Flex
```

这是我们 MiniUI 要实现的模型。它足够通用、足够好理解，同时能让你完整地经历 Measure → Arrange 管线。

### 模型三：约束布局

程序员声明控件之间的约束关系（"A 的右边紧贴 B 的左边"、"C 的宽度是 D 的一半"），求解器自动计算出满足所有约束的布局。

```
优点：表达力最强，能描述任意复杂的布局关系
缺点：实现复杂度高，调试困难，求解可能有多个解或无解
代表：Qt Layout (QGridLayout 等), macOS Auto Layout, SwiftUI
```

约束布局是一个很好的"下一步挑战"，但对教学级框架来说太重了。了解它的存在就够了。

---

## C++20 Concepts：零开销的类型约束

在实现布局引擎之前，我们需要引入一个 C++20 的语言特性——Concepts。如果你之前没接触过，别紧张，它的核心思想非常直觉：**给模板参数加约束，让编译器在实例化之前就能检查类型是否满足要求**。

### 为什么用 Concepts 而不是虚函数

布局策略最自然的实现方式是策略模式（Strategy Pattern）。传统 C++ 做策略模式有两种路径：

**路径 A：运行时多态（虚函数）**

```cpp
class LayoutStrategy {
public:
    virtual ~LayoutStrategy() = default;
    virtual Size measure(const std::vector<Widget*>& children, Size available) = 0;
    virtual void arrange(const std::vector<Widget*>& children, Rect allocated) = 0;
};

class Container : public Widget {
    std::unique_ptr<LayoutStrategy> strategy_;  // 运行时可替换
};
```

优点是灵活——运行时可以动态切换布局策略。缺点是每次 `measure` / `arrange` 调用都经过虚函数表，有一层间接跳转。对于一个控件可能嵌套几十层的 GUI 来说，这些间接开销会累积。

**路径 B：编译时多态（模板 + Concepts）**

```cpp
template<LayoutStrategy S>
class Container : public Widget {
    S strategy_;  // 编译时确定，零开销
};
```

布局策略在编译时就确定了——一个容器要么是 HBoxLayout，要么是 VBoxLayout，几乎不会在运行时动态切换。那虚函数的灵活性就是浪费的，我们完全可以用模板在编译时把策略"烘焙"进去，消除所有的虚函数调用开销。

C++20 Concepts 的作用就是给模板参数加约束——不是什么类型都能当 LayoutStrategy，它必须提供 `measure` 和 `arrange` 这两个方法。Concepts 让你在模板定义时就声明这个约束，编译器会在实例化时检查，而不是在报一大堆模板展开错误后才让你意识到类型不对。

### `LayoutStrategy` Concept 的定义

我们的 Concept 应该长这样：

```cpp
#include <concepts>
#include <vector>

// 前向声明
class Widget;
struct Size;
struct Rect;

template<typename T>
concept LayoutStrategy = requires(T t,
                                   const std::vector<Widget*>& children,
                                   Size available,
                                   Rect allocated) {
    { t.measure(children, available) } -> std::same_as<Size>;
    { t.arrange(children, allocated) } -> std::same_as<void>;
};
```

这个 Concept 说的是：一个类型 T 要满足 `LayoutStrategy`，它的实例 t 必须能接受 `(const std::vector<Widget*>&, Size)` 参数调用 `measure` 并返回 `Size`，同时能接受 `(const std::vector<Widget*>&, Rect)` 参数调用 `arrange`。

如果你传入的类型没有这些方法，或者签名不匹配，编译器会直接告诉你"`YourType` 不满足 `LayoutStrategy` 约束"，而不是给你抛出一屏的模板实例化错误。

---

## Container 模板类：带布局策略的容器

有了 Concept 约束，我们就可以定义 `Container` 模板类了。它的核心思想是：**Container 是一种特殊的 Widget，它拥有子控件列表，并且用编译时确定的布局策略来安排这些子控件的位置和尺寸**。

### 类的骨架

```cpp
template<LayoutStrategy LayoutPolicy>
class Container : public Widget {
public:
    void add_child(std::shared_ptr<Widget> child);

    // 重写 Widget 的虚函数
    Size measure(Size available) override;
    void arrange(Rect allocated) override;
    void paint(Painter& p) override;

private:
    LayoutPolicy layout_;                          // 布局策略实例
    std::vector<std::shared_ptr<Widget>> children_; // 子控件列表
};
```

注意几个设计要点：

第一，`Container` 继承自 `Widget`——它本身也是一个 Widget，可以被添加到其他 Container 里。这是组合模式（Composite Pattern）的标准实现。

第二，`LayoutPolicy` 是模板参数，在编译时确定。这意味着 `Container<HBoxLayout>` 和 `Container<VBoxLayout>` 是两个不同的类型。如果你需要运行时切换布局，这种设计不支持——但正如前面分析的，GUI 布局策略几乎从不需要运行时切换。

第三，`measure` 和 `arrange` 仍然是虚函数（`override`）。这是因为父 Widget 通过 `Widget*` 指针调用子控件的 `measure`/`arrange`——容器和非容器控件需要通过虚函数区分行为。但容器之间的布局策略分发是编译时的，不经过虚函数。所以开销只存在于"是否是容器"这一层，不存在于"用什么布局策略"这一层。

### 需要想清楚的问题

`Container::measure` 的实现流程是什么？它应该先让每个子控件自己 measure（自底向上），然后把子控件的尺寸需求汇总交给 `layout_.measure()`，由布局策略计算出容器整体的需求尺寸并返回。想一想，如果某个子控件在 measure 阶段返回了一个比可用空间还大的尺寸，布局策略应该怎么处理？裁剪？允许溢出？还是压缩其他子控件？

`Container::arrange` 的实现流程呢？它先把自己拿到的 `allocated` 区域交给 `layout_.arrange()`，由布局策略计算每个子控件应该分到的 `Rect`，然后调用每个子控件的 `arrange` 把分配结果传递下去。

`Container::paint` 呢？它不需要自己画什么（除非容器有背景色之类的装饰），主要职责是依次调用每个子控件的 `paint()`。但这里有一个优化空间：如果子控件的 `Rect` 跟脏区没有交集，就不需要绘制它。这个优化留到 Stage 7（渲染性能）再做，现在先全部绘制。

---

## HBoxLayout 和 VBoxLayout：两种具体的布局策略

现在我们来定义两种最基础的布局策略。它们都满足 `LayoutStrategy` Concept。

### HBoxLayout：水平排列

```cpp
class HBoxLayout {
public:
    Size measure(const std::vector<Widget*>& children, Size available) {
        // 实现思路：
        // 1. 遍历所有子控件，调用 child->measure(available)
        // 2. 宽度 = 所有子控件宽度之和 + 间距 * (n-1)
        // 3. 高度 = 所有子控件中的最大高度
        // 4. 返回汇总后的 Size
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) {
        // 实现思路：
        // 1. 计算每个子控件应该分到的宽度（均分 or 弹性分配）
        // 2. 从左到右依次设置每个子控件的 x 坐标和宽度
        // 3. y 坐标根据对齐方式（top / center / bottom）计算
        // 4. 调用 child->arrange(rect) 把结果传递给子控件
    }
};

// 编译时验证：HBoxLayout 确实满足 LayoutStrategy
static_assert(LayoutStrategy<HBoxLayout>);
```

### VBoxLayout：竖直排列

```cpp
class VBoxLayout {
public:
    Size measure(const std::vector<Widget*>& children, Size available) {
        // 跟 HBoxLayout 对称：
        // 宽度 = 所有子控件中的最大宽度
        // 高度 = 所有子控件高度之和 + 间距 * (n-1)
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) {
        // 跟 HBoxLayout 对称：
        // 从上到下依次分配，y 累加
    }
};

static_assert(LayoutStrategy<VBoxLayout>);
```

### 设计方向提示

关于间距（spacing）和边距（margin）：间距是子控件之间的间隔，边距是容器内边距到第一个子控件之间的间隔。你可以把它们作为 HBoxLayout / VBoxLayout 的构造参数。想一想，间距应该存储在哪里——布局策略对象里还是 Container 里？存储在布局策略里更合理，因为不同的布局策略对间距有不同的处理方式。

关于弹性分配：最简单的实现是所有子控件平分可用空间。但实际场景中，你可能需要某些子控件固定宽度、某些子控件按比例伸缩。想一想怎么在 measure 阶段让控件表达自己的"弹性需求"——也许 `Widget::measure` 的返回值除了 Size 还可以包含一个权重信息？先用最简的"均分"实现跑通，后面再扩展。

关于空容器的边界情况：如果 `children` 是空的，`measure` 应该返回什么？`Size{0, 0}` 是一个合理的选择——容器在没有子控件时不占空间。`arrange` 对空列表应该直接 return。

### Widget 基类的接口变化

为了支持布局引擎，Widget 基类需要新增两个虚函数：

```cpp
class Widget {
public:
    virtual ~Widget() = default;

    // 新增：布局引擎需要的两个阶段
    virtual Size measure(Size available);
    virtual void arrange(Rect allocated);

    // 已有：绘制（Stage 3）
    virtual void paint(Painter& p) = 0;

    // 已有：控件树（Stage 4）
    Widget* parent() const;
    void set_parent(Widget* p);

    // 新增：获取 arrange 分配给自己的区域
    Rect bounds() const { return bounds_; }

protected:
    Rect bounds_;  // arrange 阶段写入，paint 阶段读取
};
```

`measure` 的默认实现可以返回一个固定的"自然尺寸"（比如 `Size{0, 0}`），叶子控件重写它来返回自己的真实需求。`arrange` 的默认实现就是简单地把传入的 `Rect` 存到 `bounds_` 里。

注意 `measure` 不是 `const` 的——因为有些控件在 measure 阶段可能会缓存计算结果。但 `measure` 不应该修改控件的可见状态（位置和尺寸），那是 `arrange` 的活。

---

## 验证方法

写一个测试程序来验证布局引擎：

1. 创建一个 `Container<HBoxLayout>`，往里面添加三个相同大小的 ColorWidget（Stage 4 里做的纯色矩形控件）
2. 把这个 Container 设为窗口的根控件
3. 在 `XCB_CONFIGURE_NOTIFY` 事件（窗口大小改变）的处理中，触发重新布局：用新的窗口尺寸调用根控件的 `measure` → `arrange` → `paint`
4. 运行程序，你应该看到三个色块水平排列，均匀填满窗口宽度
5. 拖动窗口边缘改变大小——三个色块应该自动等比缩放

然后再测 VBoxLayout：换成 `Container<VBoxLayout>`，三个色块应该竖直排列。

如果色块叠在一起或者看不见，在 `measure` 和 `arrange` 里打印日志，检查每个控件拿到的 `Rect` 是否合理。如果窗口大小改变后布局没更新，检查 `XCB_CONFIGURE_NOTIFY` 事件是否正确触发了重新布局。

关于 `static_assert` 的验证：在编译时确认 `LayoutStrategy<HBoxLayout>` 和 `LayoutStrategy<VBoxLayout>` 通过。如果你新写了一个布局策略但忘了实现某个方法，`static_assert` 会在编译阶段直接报错，错误信息会比模板实例化错误清晰得多。

---

## 可能踩的坑

**Concept 约束太严或太松。** 如果你定义的 `LayoutStrategy` Concept 要求的函数签名跟实际使用场景不完全匹配（比如你要求 `measure` 接受 `const std::vector<Widget*>&` 但你的 Container 内部存的是 `std::vector<std::shared_ptr<Widget>>`），那你需要在调用 `layout_.measure()` 的时候做一次转换——把 `shared_ptr` 转成裸指针的 vector。这个转换虽然有点丑，但在布局引擎里是可以接受的（布局只在窗口大小改变时触发，不在热路径上）。或者你可以调整 Concept 的签名直接接受 `shared_ptr` 的 vector——想清楚哪种方式对你的代码更自然。

**measure 和 arrange 的调用顺序不能反。** 必须先 measure 再 arrange。如果先调用了 arrange，子控件的 `bounds_` 可能还是上一次的旧值，布局结果会不正确。这是一个不变量——你需要在代码结构上保证这个顺序，比如用一个 `layout()` 公共方法把 measure + arrange 打包在一起。

**布局计算的坐标系。** `measure` 返回的 Size 是相对于父控件的"需求尺寸"，不包含位置信息。`arrange` 传入的 Rect 的 x, y 是相对于父控件内容区域的偏移——不是屏幕绝对坐标。每个控件只关心"我在父容器里的相对位置"，最终的屏幕坐标在 paint 阶段由绘图上下文的变换矩阵处理。如果你在 arrange 里用了绝对坐标，嵌套容器就会错位。

**C++20 编译器支持。** Concepts 需要 GCC 10+ 或 Clang 10+，并且需要在编译选项里加 `-std=c++20`（或 `-std=c++2a` 对于较早的编译器版本）。如果你的 CMakeLists.txt 还在用 `C++17`，记得升级。同时 `<concepts>` 头文件在有些编译器版本上需要显式 include。

⚠️ **模板代码的编译错误。** 模板类的实现必须放在头文件里（或者使用显式实例化），因为编译器需要在实例化时看到完整定义。如果你把 `Container<LayoutPolicy>` 的实现放在 `.cpp` 文件里，链接阶段会报 undefined reference。这是 C++ 模板的经典问题，不是你代码写错了。

---

## 如果卡住了

如果你不确定 `measure` 的自底向上递归应该怎么写，想象一下这个过程：最外层的调用是 `root->measure(screen_size)`。如果 root 是一个 `Container<HBoxLayout>`，它的 `measure` 会先对每个子控件调用 `child->measure(available)`。如果某个子控件也是 Container，它就递归进去。最终递归到叶子节点——比如一个 Label，它根据文字长度和字号计算出自己的 Size 然后返回。所有叶子节点返回后，容器收集结果、汇总、返回给上层。这跟 DFS（深度优先搜索）的遍历顺序是一样的。

如果你在 Concept 定义上卡住了——不确定 `requires` 表达式怎么写——去看一下 cppreference 上 `std::same_as` 和 `requires` 表达式的文档。一个常见的错误是在 Concept 里忘记写 `-> std::same_as<ReturnType>` 这个返回类型约束，导致 Concept 检查不到返回值类型不匹配的问题。

如果你在想"为什么不用 `std::variant<HBoxLayout, VBoxLayout>` 做运行时切换"——可以，但这违反了我们"编译时确定布局策略"的设计原则。`variant` 本质上还是运行时分发（通过内部的 tag 判断当前持有哪种类型），跟虚函数的开销在同一个量级。如果你确实需要运行时切换，虚函数反而是更自然的选择——但先问自己：你的使用场景真的需要运行时切换布局吗？

另一个有帮助的思考方式：把 Measure → Arrange 跟我们在 Win32 里做的 `WM_SIZE` 处理做类比。在 Win32 里，`WM_SIZE` 的处理函数会拿到新的窗口尺寸，然后手动调用 `MoveWindow` 重新摆放所有子控件。我们的布局引擎做的是同一件事——只不过"怎么摆放"的逻辑从手写代码变成了布局策略对象，"什么时候重新摆放"从手动处理 `WM_SIZE` 变成了自动触发的 Measure → Arrange 管线。

如果你对 C++20 Concepts 的语法还不熟悉，推荐先写一个最简的 Concept 验证程序练手：

```cpp
#include <concepts>
#include <iostream>

template<typename T>
concept HasSize = requires(T t) {
    { t.size() } -> std::same_as<size_t>;
};

struct Good { size_t size() const { return 42; } };
struct Bad  { int size() const { return 42; } };

static_assert(HasSize<Good>);   // OK
// static_assert(HasSize<Bad>);  // 编译错误：返回类型不匹配

int main() {
    std::cout << "Concepts work!\n";
}
```

先把这个小程序跑通，确认你的编译器和工具链都正确支持了 Concepts，然后再回来布局引擎的实现。

---

## 下一步

布局引擎搭好了，我们的控件已经能自动排列、响应窗口大小变化了。但还有一个关键的能力缺失：**控件之间的通信**。一个按钮被点击了，怎么通知父容器？一个文本框的内容变了，谁应该知道？目前的控件树只能处理"父 → 子"的渲染和事件分发方向，缺少"子 → 父"或者"任意两个控件之间"的通信机制。下一阶段我们会实现一个信号系统（Signal System），用观察者模式解耦控件间的通信，并引入 Button 等真正的交互控件。有了信号系统，MiniUI 就具备了构建真实应用的能力——它将是 MVx（Model-View-x）架构的基础设施。
