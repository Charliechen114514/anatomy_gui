# Stage 9 — 响应式事件流：Observable 与 Rx 模式

---

## 这一阶段的目标

上一个阶段我们用 MiniUI 拼出了一个完整的 Mini 文本编辑器——MVP 架构、C++20 协程、双缓冲渲染，所有积木各司其职。在 Stage 8 的结尾我们预告过，接下来要挑战的是一个全新的抽象层。这个阶段就是兑现那个预告。

目标很具体：**给 MiniUI 引入 `Observable<T>`——一种比 `Signal<>` 更强大的事件流抽象，让你能够用声明式的方式组合和变换事件。** 做完之后，你会发现拖拽绘制、多指手势、搜索框防抖这些以前需要写大量嵌套回调和状态机的交互逻辑，都可以被几行链式调用优雅地表达出来。

核心交付物：

1. **`Observable<T>` 模板类** —— "可能产生多个值的事件源"的一等抽象
2. **四个核心操作符** —— `filter`、`map`、`take_until`、`combine_latest`
3. **Widget 集成** —— 鼠标事件源变成 Observable，与现有虚函数接口共存
4. **多手势绘图画布** —— 用 Observable 组合实现拖拽绘制的完整示例

### 为什么 Signal 不够用了

Stage 6 里我们实现了 `Signal<T>`，它解决的是"一件事发生了，通知一群人"的问题。按钮被点击、文本被修改、属性值变化——都是 `Signal` 的典型场景。它的模型是 **fire once, handle once**：信号发射一次，所有连接的槽函数各自执行一次，然后结束。

但现在考虑一个场景：**拖拽绘制**。用户在画布上按下鼠标，然后拖动，最后松开。这个交互涉及三个独立的事件：`mousePress`、`mouseMove`、`mouseRelease`，它们之间有**时序依赖**——"移动"只有在"按下"之后才有意义，"松开"意味着整个拖拽序列的终结。用 `Signal` 你会怎么写？

```cpp
// 用 Signal 写拖拽——很快就会失控
bool isDragging = false;
Point lastPoint;

canvas->mousePress().connect([&](Point p) {
    isDragging = true;
    lastPoint = p;
    beginStroke(p);
});

canvas->mouseMove().connect([&](Point p) {
    if (!isDragging) return;    // 手动过滤
    continueStroke(p);
    lastPoint = p;
});

canvas->mouseRelease().connect([&](Point p) {
    if (!isDragging) return;    // 又一次手动过滤
    endStroke(p);
    isDragging = false;
});
```

这段代码有三个问题。第一，`isDragging` 是散落在lambda外面的裸状态变量——如果有两个手指同时在画布上拖拽呢？你需要一个 `std::map<int, bool>` 来追踪每个触摸点的状态，复杂度立刻上升。第二，三个 Signal 的回调各自独立注册，它们的**组合关系**（press 启动、move 继续、release 终止）隐含在 `isDragging` 标志位里，而不是显式表达的。第三，如果你想在拖拽过程中做额外处理（比如只在鼠标移动超过 5 像素时才算拖拽、或者实时显示拖拽路径的长度），你需要在 `mouseMove` 的回调里堆砌更多条件判断。

`Signal` 是"离散事件的通知机制"，但拖拽是"连续事件的组合"。我们需要一种能够表达"事件流与事件流之间的关系"的抽象——这就是 `Observable`。

### 为什么 GUI 需要 Rx 模式

Rx（Reactive Extensions）最早由微软的 Erik Meijer 团队在 .NET 里提出，后来被移植到几乎所有主流语言——RxJava、RxJS、RxSwift、ReactiveCocoa……它的核心思想是把**一切异步事件抽象为可观察的数据流（Observable Stream）**，然后用一套声明式的操作符（operator）来组合、过滤、变换这些流。

在 GUI 编程里，这个模型尤其自然。想一想：

- 用户的鼠标移动是一个 `Observable<Point>` 流——持续产生值
- 键盘输入是一个 `Observable<Key>` 流——每次按键产生一个值
- 文本框的内容变化是一个 `Observable<string>` 流——每次编辑产生一个新字符串
- 按钮点击是一个 `Observable<void>` 流——每次点击产生一个无意义的值，本质上是"发生了"的通知

关键洞察：**当你把事件看作"流"而不是"离散通知"时，组合操作就变成了数据流变换，而不是回调和状态机的堆砌。** 这跟你用 `std::views::filter | std::views::transform` 处理集合数据是同一个思想——只不过从同步的内存集合变成了异步的时间序列。

### 需要想清楚的问题

**第一个问题：Observable 和 Signal 到底有什么区别？什么时候用哪个？**

`Signal` 是一对多的通知——一个信号源，多个监听者，每次发射通知所有监听者。它没有"结束"的概念，也没有操作符。`Observable` 是一个可能产生多个值、也可能结束的流——它有 `subscribe`（订阅）、`next`（推值）、`complete`（结束）三种操作，还有操作符来组合和变换。简单说：**Signal 是 Observable 的退化形式**——一个永远不结束、没有操作符的 Observable 就是 Signal。在 MiniUI 里，两者会共存：简单的"发生了就处理"场景继续用 Signal（保持向后兼容），需要组合和变换的场景用 Observable。

**第二个问题：Observable 的操作符返回什么？**

`filter` 返回一个新的 `Observable<T>`——只推送满足条件的值。`map` 返回一个 `Observable<U>`——值的类型可能变了。`take_until` 返回一个 `Observable<T>`——在另一个 Observable 推送值之前一直推送，之后停止。`combine_latest` 返回一个 `Observable<std::pair<T, U>>`——每当任意一个源推送值，就用两个源的最新值组合推送。**注意到了吗？每个操作符都返回一个新的 Observable**——这就是链式调用的基础。

**第三个问题：Observable 的生命周期怎么管理？**

`subscribe` 返回一个 `Unsubscribe` 函数（一个 `std::function<void()>`），调用它就取消订阅。如果你不取消订阅，观察者的 lambda 就会一直被回调——即使那个 lambda 捕获的 `this` 指针已经失效了。这跟 Signal 的 `connect` 返回 `Connection` 对象是同一个模式。想一想，你需要在 Widget 的析构函数里做什么？——取消所有跟这个 Widget 相关的订阅。

**第四个问题：怎么保证跟现有虚函数接口的兼容性？**

Stage 4 里我们在 Widget 上定义了 `virtual void onMousePress(Point)` 等虚函数，子类通过重写来处理事件。现在我们要给 Widget 加上 `Observable<Point> mousePress()` 等事件源。**两者不应该冲突**——虚函数是"内部处理"的接口（控件自己决定怎么响应），Observable 是"外部组合"的接口（外部代码把多个控件的事件流组合起来）。Widget 的默认 `onMousePress` 实现里应该调用 `mousePress_.next(p)` 把事件推送到 Observable 流上。这样两套接口同时工作，互相不干扰。

### 设计方向提示

#### Observable<T> 模板类

这是本阶段的核心。它的设计比 `Signal<T>` 复杂一些——因为它不仅要支持"推送值"，还要支持"操作符返回新的 Observable"。

```cpp
#include <functional>
#include <vector>
#include <memory>
#include <any>
#include <utility>

template <typename T>
class Observable {
public:
    using Observer   = std::function<void(T)>;
    using Unsubscribe = std::function<void()>;

    // ── 核心：订阅与推送 ──

    // 订阅这个 Observable，每次有新值时调用 observer
    // 返回一个取消订阅的函数
    Unsubscribe subscribe(Observer observer);

    // 向所有订阅者推送一个值
    void next(T value);

    // ── 操作符 ──

    // 过滤：只推送满足 predicate 的值
    // 返回一个新的 Observable<T>
    auto filter(std::function<bool(const T&)> predicate) -> Observable<T>;

    // 变换：把每个 T 类型的值转换成 U 类型
    // 返回一个新的 Observable<U>
    template <typename U>
    auto map(std::function<U(const T&)> transform) -> Observable<U>;

    // 截断：直到 until 推送第一个值之前，持续推送；
    // until 一旦推送，这个 Observable 就不再推送任何值
    auto take_until(Observable<std::any>& until) -> Observable<T>;

    // 组合：每当 this 或 other 推送值时，
    // 用两者各自的最新值组合成 pair 推送
    template <typename U>
    auto combine_latest(Observable<U>& other)
        -> Observable<std::pair<T, U>>;

private:
    // 存储所有订阅者
    std::vector<Observer> observers_;
};
```

这里的重点是理解操作符的工作方式。以 `filter` 为例：它不是在当前 Observable 上做修改，而是**创建一个新的 Observable**。新的 Observable 内部订阅了原来的 Observable，当原 Observable 推送值时，新 Observable 检查 predicate 是否满足，满足才向自己的订阅者转发。这种"创建新流"的模式是 Rx 的核心设计——操作符不改变源流，而是产生派生流。

想一想 `map` 的实现思路：它需要返回一个 `Observable<U>`——一个全新的、类型不同的 Observable。内部怎么把 `Observable<T>` 的值转换成 `Observable<U>` 的值？答案是：在 `map` 返回的新 Observable 内部，subscribe 到源 Observable，每次源推送一个 `T` 值，就用 `transform` 把它转成 `U`，然后调用新 Observable 自己的 `next` 推送出去。

`take_until` 是最有趣的一个。想一想：它需要同时监听两个 Observable——源流和终止信号。当源流推送值时，转发给订阅者；当终止信号推送值时，停止转发并取消对源流的订阅。这意味着 `take_until` 返回的新 Observable 内部持有两个订阅——一个监听源流，一个监听终止信号。两个订阅的生命周期需要协调。

#### Widget 集成：双接口共存

```cpp
class Widget {
public:
    // ── 已有：虚函数接口（Stage 4，保持不变）──
    // 子类重写这些方法来实现自己的事件处理逻辑
    virtual void onMousePress(Point p)    { /* 默认推送到 Observable */ }
    virtual void onMouseMove(Point p)     { /* 默认推送到 Observable */ }
    virtual void onMouseRelease(Point p)  { /* 默认推送到 Observable */ }

    // ── 新增：Observable 事件源 ──
    // 外部代码通过这些 Observable 把多个控件的事件流组合起来
    Observable<Point>& mousePress()   { return mousePress_; }
    Observable<Point>& mouseMove()    { return mouseMove_; }
    Observable<Point>& mouseRelease() { return mouseRelease_; }

protected:
    Observable<Point> mousePress_;
    Observable<Point> mouseMove_;
    Observable<Point> mouseRelease_;
};
```

默认的虚函数实现做什么？——把事件推送到对应的 Observable：

```cpp
// Widget.cpp 里
void Widget::onMousePress(Point p) {
    mousePress_.next(p);   // 推送到 Observable 流
}
```

如果子类重写了 `onMousePress` 但还想让 Observable 也收到事件，它需要在重写版本里调用 `Widget::onMousePress(p)`。这跟 Qt 的 `QWidget::mousePressEvent` 里需要调用 `event->accept()` 是一个道理——你不调就不传播。想一想，有没有更自动化的方式？比如在事件分发的时候先调虚函数、再推送到 Observable，这样子类就不需要手动调基类了。这会带来什么问题？——推送顺序变成固定的（先虚函数后 Observable），灵活性降低。哪种方案更好取决于你更看重方便性还是灵活性。

#### 拖拽绘制：用 Observable 组合三个事件流

这是本阶段最核心的示例。我们要实现一个画布，用户按下鼠标后拖动绘制路径，松开鼠标结束绘制。用 Observable 的组合来写：

```cpp
// DragCanvas —— 拖拽绘图画布
class DragCanvas : public Widget {
public:
    DragCanvas() {
        setupDragDrawing();
    }

    void addStroke(const std::vector<Point>& path) {
        strokes_.push_back(path);
        repaint();
    }

protected:
    void onPaint(Painter& p) override {
        p.setSourceRGB(1.0, 1.0, 1.0);   // 白色背景
        p.paint();
        p.setSourceRGB(0.0, 0.0, 0.0);   // 黑色笔触
        for (const auto& stroke : strokes_) {
            if (stroke.size() < 2) continue;
            p.moveTo(stroke[0]);
            for (size_t i = 1; i < stroke.size(); ++i) {
                p.lineTo(stroke[i]);
            }
            p.stroke();
        }
    }

private:
    std::vector<std::vector<Point>> strokes_;

    void setupDragDrawing() {
        // ── 核心：三个事件流的组合 ──
        // mousePress 启动一次拖拽
        // mouseMove 在拖拽期间收集路径点
        // mouseRelease 结束拖拽

        canvas->mousePress().subscribe(
            [&, dragStream](Point start) mutable {
                // 每次按下鼠标，启动一条新的绘制路径
                auto path = std::make_shared<std::vector<Point>>();
                path->push_back(start);

                // 订阅 mouseMove，收集拖拽过程中的点
                auto unsub = dragStream.subscribe([path](Point pos) {
                    path->push_back(pos);
                    canvas->addStroke(*path);
                });

                // 订阅 mouseRelease，松开时取消 move 的订阅
                canvas->mouseRelease().subscribe(
                    [unsub](auto) { unsub(); }
                );
            }
        );
    }
};
```

仔细读这段代码。注意发生了什么：

1. `mousePress` 的每次推送都触发一个新拖拽序列——创建一个空的路径、把起始点加进去
2. 在 press 的回调里，我们**动态订阅**了 `mouseMove`——只在拖拽期间才监听移动事件
3. 同样在 press 的回调里，我们订阅了 `mouseRelease`——松开鼠标时，通过 `unsub()` 取消对 mouseMove 的订阅

这就是 Observable 比 Signal 强大的地方：**你可以在一个流的回调里动态订阅和取消订阅另一个流**。这比维护一个 `isDragging` 标志位干净得多——状态的生命周期被订阅的生命周期自然地管理了。

但是，上面的代码有一个微妙的问题，你能看出来吗？——每次按下鼠标都会创建一个对 `mouseRelease` 的新订阅，但这些订阅永远不会被取消。如果用户画了 100 笔，`mouseRelease` 上就挂了 100 个观察者。虽然其中 99 个 lambda 捕获的 `unsub` 已经被调用过了（取消的是 move 的订阅），但这些 release 观察者本身还在列表里。想一想怎么解决这个问题？提示：`take_until` 就是干这个的。

#### 用 take_until 重写拖拽

```cpp
void setupDragDrawing() {
    mousePress_.subscribe([this](Point start) {
        auto path = std::make_shared<std::vector<Point>>();
        path->push_back(start);

        // mouseMove.take_until(mouseRelease)
        // = "监听 mouseMove，直到 mouseRelease 推送值为止"
        auto moveUntilRelease = mouseMove_
            .take_until(reinterpret_cast<Observable<std::any>&>(mouseRelease_));

        auto unsub = moveUntilRelease.subscribe([this, path](Point pos) {
            path->push_back(pos);
            addStroke(*path);
        });
    });
}
```

`take_until` 把"启动-继续-终止"三段逻辑变成了两段：press 启动，`moveUntilRelease` 自动在 release 时停止。不需要手动管理 release 的订阅了。想想看，如果你还想在每次拖拽结束后做一些收尾工作（比如保存路径到历史记录），应该怎么写？——也许你需要一个 `do_on_complete` 操作符，或者直接在 `take_until` 的实现里加一个 `on_complete` 回调。

#### combine_latest：组合两个流

`combine_latest` 的用例是什么？想象一下：你有一个颜色选择器和一个画笔粗细滑块。用户可能先选了颜色、再调粗细，或者反过来。你的画笔需要同时知道"当前颜色"和"当前粗细"——每当任意一个变化时，都用两者的最新值更新画笔设置。

```cpp
// colorPicker.colorChanged() 是 Observable<Color>
// sizeSlider.sizeChanged()   是 Observable<int>

auto brushSettings = colorPicker.colorChanged()
    .combine_latest(sizeSlider.sizeChanged());
    // -> Observable<std::pair<Color, int>>

brushSettings.subscribe([](std::pair<Color, int> setting) {
    auto [color, size] = setting;
    currentBrush.setColor(color);
    currentBrush.setSize(size);
        });
```

每当颜色或粗细发生变化，`combine_latest` 就会用两者各自的最新值组成一个 `pair` 推送出去。你不需要追踪"哪个先变、哪个后变"——`combine_latest` 帮你记住了每个流的最新值。

### 验证方法

分步验证，不要一次写完再测：

**第一步：Observable 基本功能**

先不写操作符，只验证 `subscribe` + `next` 能工作。写一个最小测试：

```cpp
Observable<int> obs;
auto unsub = obs.subscribe([](int v) {
    std::cout << "Received: " << v << std::endl;
});
obs.next(1);    // 应该输出 "Received: 1"
obs.next(2);    // 应该输出 "Received: 2"
unsub();        // 取消订阅
obs.next(3);    // 不应该有任何输出
```

如果这三步输出正确，说明订阅和取消订阅机制没问题。

**第二步：filter 和 map**

```cpp
Observable<int> source;
auto filtered = source.filter([](int v) { return v > 5; });
auto mapped   = filtered.map<std::string>([](int v) {
    return "Value: " + std::to_string(v);
});
mapped.subscribe([](const std::string& s) {
    std::cout << s << std::endl;
});
source.next(3);    // 不输出（被 filter 过滤）
source.next(7);    // 输出 "Value: 7"
source.next(10);   // 输出 "Value: 10"
```

**第三步：Widget 集成**

给一个简单的 Widget（比如 Stage 4 的 ColorWidget）加上 Observable 事件源。在 `onMousePress` 里调用 `mousePress_.next(p)`。然后在外部代码里订阅这个 Observable，验证点击控件时能收到事件。

**第四步：拖拽画布**

实现完整的 `DragCanvas`，运行程序，在画布上用鼠标画几条线。验证：按下鼠标后拖动能画线，松开后线条保留在画布上。不按鼠标直接移动不应该画任何东西。

**第五步：多手势支持**

如果前面都通过了，试试一个进阶场景：在画布上同时支持左键拖拽画线和右键拖拽擦除。你可以用 `filter` 来区分——`mousePress` 推送的值里带上按钮信息，然后 `filter` 出左键和右键各自的事件流。

### 可能踩的坑

**Observable 的拷贝语义**

`Observable<T>` 内部持有一个 `std::vector<Observer>`——观察者列表。如果你拷贝了一个 Observable，两个副本各自有自己的观察者列表，但它们不会同步。向源 Observable 推送值，副本的订阅者不会收到。这很可能不是你想要的行为。解决方案：让 Observable 不可拷贝（`delete` 拷贝构造和赋值），只允许移动。或者用 `shared_ptr` 持有内部的观察者列表，让所有副本共享同一份数据。想一想哪种方式更适合你的使用场景。

**subscribe 返回的 Unsubscribe 函数持有迭代器或索引**

如果你的 `Unsubscribe` 实现是通过在 `observers_` 向量里找到对应的观察者并删除，那你要注意——在 `next()` 遍历 `observers_` 的过程中，如果有观察者的回调调用了 `unsubscribe`（从向量中删除元素），你就面临迭代器失效的问题。解决方案：在 `next()` 开始时拷贝一份观察者列表，然后遍历拷贝。或者用 `std::list` 替代 `std::vector`（`std::list` 的删除不会使其他迭代器失效）。或者用一个"待删除"标记，在 `next()` 完成后再统一清理。想一想哪种方案最简单。

**map 操作符中模板类型的推导**

`map<U>` 返回 `Observable<U>`——一个跟源 Observable 类型不同的新对象。在 C++ 里，模板成员函数的返回类型依赖于模板参数 `U`，这意味着你需要显式指定 `U` 或者让编译器通过 lambda 的返回类型来推导。如果你的编译器支持 C++20 的 CTAD（Class Template Argument Deduction），可以简化一些。否则你可能需要写 `map<std::string>(...)` 而不是让编译器自动推导。

**take_until 的生命周期**

`take_until` 创建的新 Observable 内部同时订阅了源流和终止信号。当终止信号触发时，它需要取消对源流的订阅。但如果源流和终止信号指向同一个底层 Observable（比如两个 `filter` 派生出来的流共享同一个源的观察者列表），取消订阅可能会影响另一个流。确保你的实现中每个 Observable 的订阅是独立的。

**跟协程的交互**

如果你在 Stage 8 里实现了 `Task<T>`，你可能会想：能不能 `co_await` 一个 `Observable<T>`？——比如 `co_await mousePress()` 挂起协程，直到下次鼠标按下时恢复。这完全可以做，但它需要一个适配器——一个 Awaitable 对象，在 `await_suspend` 里 subscribe 到 Observable，在第一次推送值时恢复协程。这是一个很好的扩展练习，但不是本阶段的必须任务。把它放在脑子里，后面可以回来做。

### 如果卡住了

**如果你不确定 Observable 的操作符应该怎么实现**

用 `filter` 作为第一个练手对象。它的逻辑最简单：创建一个新的 `Observable<T>`（叫它 `result`），在 `result` 内部 subscribe 到源 Observable，每次源推送值时检查 predicate，满足就调 `result.next(value)`。其他操作符的结构都类似——创建新 Observable，内部订阅源流，按自己的逻辑决定是否转发。

**如果你在想"这不就是 Signal 加了个 map 吗"**

从某种意义上说，是的。`Observable` 和 `Signal` 的底层机制都是"观察者列表 + 遍历通知"。区别在于 `Observable` 的操作符让组合变成了一等公民。用 `Signal` 你要在回调里手写过滤和变换逻辑；用 `Observable` 你用 `filter` 和 `map` 声明你想要什么，而不是怎么做。这个区别在简单场景下不明显，但当你要组合三个以上的事件流时（比如拖拽 + 缩放 + 旋转的手势识别），声明式的方式会清晰得多。

**如果你在 take_until 的实现上卡住了**

提示：`take_until` 返回的新 Observable 需要同时持有两个订阅——一个对源流的，一个对终止信号的。你可以用一个 `shared_ptr` 包装这两个订阅，当终止信号触发时，设置一个 `completed` 标志，同时取消源流的订阅。之后源流再推送值就被忽略了。

**如果你在 Widget 双接口的兼容性上犹豫**

最简单的做法是在 Application 的事件分发代码里，先调虚函数，虚函数的默认实现推送到 Observable。子类如果重写了虚函数但不调用基类版本，Observable 就收不到事件——这是子类的主动选择，不是 bug。如果你觉得这容易出错，可以在事件分发层拆成两步：先调虚函数，再推送 Observable，不管子类怎么重写都不影响。想清楚两种方案在语义上的区别，选你觉得更合理的那个。

**如果你对本阶段的抽象层级感到困惑**

退一步想：`Observable` 解决的核心问题是什么？——**让多个事件流的组合关系成为显式的代码结构，而不是隐含在回调和状态变量里。** 如果你的交互场景很简单（一个按钮点一下做一件事），`Signal` 就够了。如果你的交互涉及多个事件的时序组合（拖拽 = press + move + release），`Observable` 就是为此而生的。你不需要在每个地方都用 `Observable` 替换 `Signal`——两者共存，各司其职。

**Win32 程序员的参照系**

如果你需要跟 Windows 的经验对照：

| MiniUI Observable 概念 | Win32 / 其他框架的等价物 |
|---|---|
| `Observable<T>` | Reactive Extensions (Rx) 的 IObservable<T> |
| `subscribe()` | `IObserver::OnNext` |
| `filter()` | 在 WndProc 里写 `if` 判断 |
| `map()` | 在回调里手动转换消息参数 |
| `take_until()` | 手动维护 `isDragging` 标志 |
| `combine_latest()` | 手动保存两个状态的最新值 |
| `Unsubscribe` | `Release()` 或 `disconnect()` |

在 Win32 里，所有这些操作都是在 `WndProc` 的 `switch-case` 里手写的。Rx 的价值是把这些模式抽取成了可组合的积木。

---

## 下一步

这个阶段我们给 MiniUI 加上了一层声明式的事件流抽象。`Observable<T>` 和它的操作符让复杂交互的实现从"回调和状态机的意大利面条"变成了"链式调用的声明式流水线"。

但有一个问题我们一直在回避：**如果事件处理的回调本身很耗时怎么办？** 比如你的 `subscribe` 回调里需要生成一张图片的缩略图、或者从网络下载一段数据——这些操作可能需要几百毫秒甚至几秒。如果回调在事件循环的线程上执行，GUI 就冻住了。

下一个阶段（Stage 10）要解决这个问题：**线程池与并发 GUI**。我们会实现一个基于 C++20 `std::jthread` 和 `std::stop_token` 的线程池，让耗时操作可以在后台线程上执行，然后通过 `postTask` 机制安全地把结果投递回 GUI 线程。同时我们还要讨论一个所有 GUI 框架都必须面对的黄金法则：**只有 GUI 线程才能操作 Widget**——以及怎么在架构层面保证这条规则不被违反。
