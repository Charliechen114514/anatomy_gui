# Stage 6 — 信号系统与响应式属性：让控件学会对话

---

## 这一阶段的目标

上个阶段我们搞定了布局引擎——控件们现在能自动找到自己的位置，不用再手动搬砖了。但一个 GUI 光有"长得好看"是不够的，控件之间得能互相说话。用户点了一下按钮，显示区怎么知道要更新文字？滑块被拖动了，旁边的标签怎么知道要刷新数值？如果每个控件都直接持有其他控件的指针然后直接调用方法，那代码很快就会变成一团意大利面。

这个阶段的目标是给 MiniUI 引入一套**信号-槽（Signal-Slot）系统**和**响应式属性（Property）**。信号系统让控件之间能"松耦合"地通信——按钮不需要知道 Label 是谁，它只需要发出一个 `clicked()` 信号，谁想听谁就接上。响应式属性则更进一步——当你给 `text` 赋一个新值的时候，它会自动通知所有监听者"我变了"，不需要手动触发信号。做完这一步，我们就有了构建 MVx 架构（MVC / MVP / MVVM）的基础设施。

做完之后，你的代码大概会是这种感觉：

```cpp
// 按钮不需要知道谁在听——它只管发信号
auto btn7 = std::make_shared<Button>();
btn7->text() = "7";

// 显示区订阅按钮的点击信号
btn7->clicked().connect([&]() {
    display->text() = display->text().get() + "7";
});

// Property 赋值自动触发通知——不需要手动 emit
display->text().changed().connect([&](const std::string& newText) {
    std::cout << "display changed to: " << newText << std::endl;
});
```

### 为什么不直接用回调函数

最直觉的做法是在 Button 里面存一个 `std::function<void()>`，点一下就调一次。这在只有一两个控件的时候够用。但想象一下：一个按钮被点击后，可能需要通知显示区更新、通知计算引擎重新运算、通知日志系统记录操作、通知状态栏刷新——你需要维护一个回调列表。而且如果某个回调的订阅者不想听了，你还得从列表里移除它。如果回调持有已经销毁的对象的引用……恭喜你，undefined behavior 正在向你招手。

这些问题——一对多通知、动态订阅/取消订阅、生命周期安全——的通用解法有一个名字：**观察者模式（Observer Pattern）**。信号-槽就是观察者模式在 GUI 领域的一种工程化实现。

### 为什么 Win32 的 WM_COMMAND 不是好方案

如果你回想一下 Phase 1 里写 Win32 程序的经历，控件之间的通信走的是 `WM_COMMAND` 或者 `WM_NOTIFY` 消息。按钮被点击，父窗口的窗口过程收到一条 `WM_COMMAND`，`wParam` 里塞的是控件 ID，`lParam` 里塞的是控件句柄。然后你在一个巨大的 `switch-case` 里面根据 ID 分发：

```cpp
// Win32 经典的消息分发
switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
            case IDC_BUTTON_7:  onButton7();  break;
            case IDC_BUTTON_8:  onButton8();  break;
            case IDC_BUTTON_PLUS: onPlus();   break;
            // ... 十几个 case ...
        }
        break;
}
```

这个方案有三个严重问题。**第一，强耦合**——按钮和父窗口之间通过一个整数 ID 绑定，你必须在资源头文件里定义这些 ID，必须在 `switch-case` 里手动匹配。如果你动态创建控件，ID 管理就成了噩梦。**第二，不可组合**——`WM_COMMAND` 只能发给父窗口，如果想让按钮通知一个跟父窗口平级的组件，你得自己想办法转发。**第三，不类型安全**——`wParam` 和 `lParam` 是整数，所有信息都被塞进两个整数里，没有任何编译期检查。

Qt 的 Signal-Slot 机制就是为了解决这些问题而生的。它的核心思想是：**信号发出者不需要知道接收者是谁，接收者也不需要知道信号从哪来——中间的连接由一个独立的机制管理**。这就是"松耦合"的本质。

### 需要想清楚的问题

**第一个问题：信号和回调函数有什么本质区别？** 回调是一对一的关系——你把一个函数指针交给对方，对方调你。信号是一对多的关系——一个信号可以被任意多个槽函数订阅。想一想，这意味着 `Signal` 内部需要维护一个什么数据结构？当信号被 `emit` 的时候，这个数据结构里的元素要怎么被遍历和调用？

**第二个问题：槽函数的生命周期怎么管理？** 如果一个 Label 订阅了 Button 的 `clicked()` 信号，然后这个 Label 被销毁了，但 Button 还在——下次点击按钮的时候，Signal 会试图调用一个已经不存在的对象上的方法。这是经典的"悬挂引用"问题。想一想，订阅者能不能在析构的时候自动取消订阅？需要什么机制来实现这个"自动"？

**第三个问题：连接（Connection）应该是什么语义？** `signal.connect(callback)` 返回给你一个什么东西？如果返回 `void`，那你就永远无法取消这个连接——这在短期订阅的场景下是不可接受的。如果返回一个对象，那这个对象的析构是否应该自动断开连接？这就是 RAII 管理连接的核心思路。

**第四个问题：Property 的赋值操作符应该返回什么？** `prop = newValue` 触发 change 通知，但 `prop = prop.get()` 呢——值没变，应不应该触发通知？如果连续两次赋相同的值，监听者会收到两次通知吗？想一想这个"相同值不触发"的优化在什么场景下是有意义的，在什么场景下反而会坏事。

**第五个问题：MVx 架构选哪个？** MVC、MVP、MVVM 这三个模式在概念上很相似，但在实际工程中的侧重点不同。想一想，对于一个计算器应用来说，"Model"应该封装什么逻辑？View 和 Presenter/ViewModel 之间的边界在哪里？Signal 和 Property 分别在哪个层次发挥作用？

### 设计方向提示

我们的信号系统分为三层，从底层到上层依次是：**Signal 模板**（核心通信机制）→ **ScopedConnection**（RAII 连接管理）→ **Property 模板**（响应式属性）。每一层都只做一件事，合在一起就构成了一套完整的响应式编程基础设施。

#### 第一层：SlotCallable Concept

在定义 `Signal::connect` 之前，我们需要一个 Concept 来约束"什么可以被当成槽函数"。一个合法的槽函数就是：能用 `Signal` 的参数类型来调用的东西——lambda、函数指针、函数对象、`std::function`，统统可以，只要签名对得上。

```cpp
#include <concepts>
#include <functional>
#include <vector>

template <typename F, typename... Args>
concept SlotCallable = std::invocable<F, Args...>;
```

这个 Concept 用了 C++20 的 `std::invocable`，它检查的是：给定参数类型 `Args...`，`F` 的实例能不能被调用。如果你在 `connect` 里传了一个签名不匹配的 lambda，编译器会直接告诉你"不满足 `SlotCallable` 约束"——比传统模板的一屏错误信息友好太多了。

回想一下 Stage 5 里我们定义 `LayoutStrategy` Concept 的做法——这里用的是完全相同的思路：**用 Concept 给模板参数加约束，让编译器在实例化之前就能给出清晰的错误信息**。

#### 第二层：ScopedConnection — RAII 连接管理

`connect` 返回的不是 `void`，而是一个 `ScopedConnection` 对象。它的核心思想是：**连接的生命周期由这个对象管理——对象析构时自动断开连接**。这和我们在 Stage 4 里用 `std::unique_ptr` 管理控件 ownership 是同一套 RAII 思路。

```cpp
class ScopedConnection {
public:
    ScopedConnection() = default;
    ScopedConnection(std::function<void()> disconnector, int id);
    ~ScopedConnection(); // 自动调用 disconnect_

    // 支持移动——连接可以被转移，但不能被复制
    ScopedConnection(ScopedConnection&& other) noexcept;
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection& operator=(ScopedConnection&& other) noexcept;

    // 手动断开（析构时会自动调用，但你也可以提前调）
    void disconnect();

    // 检查连接是否仍然有效
    bool connected() const;

private:
    std::function<void()> disconnect_;
    int id_ = -1;
};
```

想一想 `disconnect_` 这个 `std::function<void()>` 应该怎么构造。在 `Signal::connect` 里面，当你把一个回调加入 `slots_` 列表的时候，你需要同时创建一个"移除函数"——这个函数捕获 `this`（Signal 指针）和 slot 的 ID，执行时从 `slots_` 里移除对应的条目。这个"移除函数"就是 `disconnect_`。当 `ScopedConnection` 析构的时候，它调用 `disconnect_()`，slot 就从 Signal 的列表里消失了。

注意 `ScopedConnection` 是不可复制的——复制一个连接意味着什么？两个对象析构时都试图断开同一个连接？这没有意义。但它可以移动——你可能会把连接存到某个对象的成员变量里，让连接的生命周期跟对象一样长。

#### 第三层：Signal<Args...> 模板

这是整个信号系统的核心。`Signal<Args...>` 是一个模板类，`Args...` 定义了信号携带的参数类型。`Signal<>` 表示没有参数（比如"按钮被点击"），`Signal<const std::string&>` 表示携带一个字符串引用（比如"文本变化了"）。

```cpp
template <typename... Args>
class Signal {
public:
    // 连接一个槽函数，返回 RAII 连接管理对象
    template <SlotCallable<Args...> F>
    ScopedConnection connect(F&& callback);

    // 发射信号——调用所有已连接的槽函数
    void emit(Args... args);

    // 临时断开所有连接（用于对象析构前的安全清理）
    void disconnect_all();

private:
    struct Slot {
        int id;                              // 唯一标识，用于断开连接
        std::function<void(Args...)> callback; // 类型擦除后的回调
    };
    std::vector<Slot> slots_;
    int nextId_ = 0;
};
```

`connect` 的实现思路：给新来的回调分配一个递增的 ID，然后把它包装成 `std::function` 存入 `slots_`。同时构造一个"断开函数"，这个函数从 `slots_` 里找对应的 ID 并移除。把这个断开函数和 ID 一起打包成一个 `ScopedConnection` 返回给调用者。

`emit` 的实现思路：遍历 `slots_`，依次调用每个回调。但这里有一个微妙的问题要思考——如果在 `emit` 的过程中，某个回调里又触发了同一个信号的 `emit`（递归发射），或者某个回调里断开了另一个槽的连接（迭代器失效），会发生什么？一个简单但安全的做法是：`emit` 之前先拷贝一份 `slots_`，对拷贝做遍历。这样即使在回调中修改了原始的 `slots_`，也不会影响当前这轮通知。

#### 第四层：Property<T> — 响应式属性

`Property<T>` 在 `Signal` 的基础上更进了一步：它把"值"和"变化通知"绑定在一起。当你给它赋一个新值的时候，它会自动比较新旧值，如果不同就触发 `changed_` 信号。这让"数据驱动的 UI 更新"变得非常自然。

```cpp
template <typename T>
class Property {
public:
    Property() = default;
    Property(T initial);

    // 赋值操作符——如果新值跟旧值不同，触发 changed 信号
    Property& operator=(const T& newValue);

    // 读取值
    const T& get() const;
    operator const T&() const;  // 隐式转换，方便读取

    // 访问变化信号——外部可以订阅
    Signal<const T&>& changed();

private:
    T value_{};
    Signal<const T&> changed_;
};
```

想一想 `operator=` 的实现：先比较 `value_` 和 `newValue`，如果相同就直接返回 `*this`，不做任何通知。如果不同，先更新 `value_`，再 `changed_.emit(value_)`。注意这里是 emit 新值——这样订阅者就能在回调里拿到新值，不需要再回头去查 Property。

`changed()` 方法返回的是 `Signal<const T&>` 的引用——外部代码可以通过这个引用来 `connect` 槽函数。但你不应该让外部代码直接调用 `changed().emit()`——谁来触发通知应该是 Property 自己说了算。想一想怎么做到这一点？一个简单的做法是把 `emit` 设为 `private` 或 `protected`，然后让 `Property` 成为 `Signal` 的 friend。

#### Button 控件

有了 Signal 和 Property，我们终于可以给 Button 一个完整的定义了：

```cpp
class Button : public Widget {
public:
    Signal<>& clicked();            // 点击信号
    Property<std::string>& text();  // 按钮文字（响应式）

    Size measure(Size available) override;
    void paint(Painter& painter) override;
    void onMousePress(Point pos, int button) override;
    void onMouseRelease(Point pos, int button) override;

private:
    Property<std::string> text_;
    bool pressed_ = false;
    Signal<> clicked_;
};
```

`onMousePress` 把 `pressed_` 设为 `true` 并请求重绘（按钮变成"按下"的样子）。`onMouseRelease` 检查鼠标释放的位置是否还在按钮范围内——如果是，说明这是一次完整的"点击"，就 `clicked_.emit()`。这个"按下-释放都在控件范围内才算点击"的逻辑在几乎所有 GUI 框架里都是这样实现的，想一想为什么？

`text()` 返回 `Property<std::string>` 的引用，这意味着你可以写 `btn->text() = "Hello"` 来赋值，也可以写 `btn->text().changed().connect(...)` 来监听变化。这两种操作通过 `Property` 的运算符重载自然地统一在了一起。

---

## MVx 架构：Model、View 和它中间的那个人

在开始写计算器之前，我们需要聊一个更宏观的话题——GUI 应用的代码怎么组织。从 Smalltalk 时代开始，人们就在思考一个问题：**业务逻辑和界面展示到底要不要分开？怎么分？** 三个最经典的答案分别是 MVC、MVP 和 MVVM。

### 三种模式的对比

| | MVC | MVP | MVVM |
|---|---|---|---|
| **全称** | Model-View-Controller | Model-View-Presenter | Model-View-ViewModel |
| **核心理念** | Controller 接收输入，操控 Model，Model 通知 View | Presenter 是 View 和 Model 之间的中间人 | ViewModel 是 View 的"可绑定抽象" |
| **View 知道 Model 吗** | 是（View 直接观察 Model） | 否（View 只跟 Presenter 说话） | 否（View 通过数据绑定观察 ViewModel） |
| **谁处理输入** | Controller | View 转发给 Presenter | View 直接绑定到 ViewModel 命令 |
| **更新机制** | Model → View（观察者） | Presenter → View（接口调用） | ViewModel → View（数据绑定） |
| **典型框架** | Rails、Django、早期的 Smalltalk | WinForms、Android MVP | WPF、MAUI、Knockout.js |
| **复杂度** | 中等 | 中等偏高 | 前期投入大，后期省心 |
| **可测试性** | 中等 | 好（Presenter 可脱离 View 测试） | 最好（ViewModel 完全不知道 View） |

### 我们选 MVP——为什么

对于 MiniUI 这个教学级框架来说，MVVM 需要一套完整的数据绑定引擎（WPF 用的是依赖属性 + XAML 绑定表达式），对我们来说太重了。MVC 的"View 直接观察 Model"在小型应用里会让 View 和 Model 耦合在一起，不够干净。MVP 是一个很好的平衡点：**View 只管展示和转发用户输入，Presenter 持有业务逻辑和 Model，通过接口操作 View**。

Signal 和 Property 在 MVP 里扮演的角色很清晰：

- **Signal** 实现 View → Presenter 的通信（"用户点了一下按钮"）
- **Property** 实现 Presenter → View 的数据流（"显示区应该显示这个值"）
- **Model** 是纯业务逻辑，不依赖任何 GUI 类

### Mini 计算器：MVP 实战

我们要用 MVP + Signal/Property 写一个迷你计算器。它的界面长这样：

```
┌─────────────────────┐
│         42          │  ← Label (display)
├─────────────────────┤
│ [7] [8] [9] [+]    │
│ [4] [5] [6] [-]    │
│ [1] [2] [3] [*]    │
│ [C] [0] [=] [/]    │
└─────────────────────┘
```

**Model** 负责计算：接收一个表达式字符串，返回计算结果。它不知道任何 GUI 的存在。

**View** 负责展示：创建所有按钮和显示区，把按钮的 `clicked()` 信号暴露出去，把显示区的 `text` Property 暴露出去。它不包含任何业务逻辑。

**Presenter** 负责协调：订阅所有按钮的 `clicked()` 信号，根据按钮的角色（数字键、运算符、等号、清除）调用 Model 进行计算，然后把结果写入 View 的显示区 Property。

信号连接的代码大致是这样：

```cpp
// 数字键
button7->clicked().connect([&]() {
    presenter.onDigit("7");
});

// 运算符
buttonPlus->clicked().connect([&]() {
    presenter.onOperator("+");
});

// 等号
buttonEq->clicked().connect([&]() {
    presenter.onEquals();
});

// 清除
buttonC->clicked().connect([&]() {
    presenter.onClear();
});

// Presenter 内部
void CalculatorPresenter::onDigit(const std::string& digit) {
    expression_ += digit;
    view_->displayText() = expression_;  // Property 赋值，自动通知 View
}

void CalculatorPresenter::onEquals() {
    auto result = model_->evaluate(expression_);
    view_->displayText() = std::to_string(result);
    expression_ = std::to_string(result);
}
```

想一想：如果不用 MVP，而是把所有逻辑塞进 View 里（按钮的 `clicked` 回调里直接做计算），代码会变成什么样？能不能单独测试计算逻辑？如果以后想把界面从 MiniUI 换成 Qt，需要改多少代码？这就是分层架构的价值——**每一层只做自己的事，替换某一层不影响其他层**。

### 验证方法

分步验证，不要一口气写完再测试：

**第一步：Signal 基础功能。** 创建一个 `Signal<int>`，连接两个 lambda（一个打印接收到的值，一个把值累加），然后 `emit(42)`。确认控制台输出了 `42`，累加结果正确。然后通过 `ScopedConnection` 断开其中一个，再次 `emit(100)`，确认只有未断开的那个 lambda 收到了通知。

**第二步：ScopedConnection 的 RAII 行为。** 创建一个 Signal，连接一个 lambda，把返回的 `ScopedConnection` 存在一个作用域内的局部变量里。走出作用域后 emit 信号，确认 lambda 没有被调用（连接已自动断开）。

**第三步：Property 赋值触发通知。** 创建一个 `Property<std::string>`，给 `changed()` 连接一个打印 lambda。赋一个新值 `prop = "hello"`，确认打印了 `"hello"`。再赋相同的值 `prop = "hello"`，确认没有打印（相同值不触发）。

**第四步：Button 控件。** 把 Button 放进控件树，连接 `clicked()` 信号，点击按钮确认信号被发射。连接 `text().changed()` 信号，修改按钮文字确认通知被触发。验证按钮在 `pressed_` 为 true 和 false 时的绘制外观不同。

**第五步：Mini 计算器。** 组装完整的计算器界面，测试以下场景：输入 `7 + 3 =`，显示区应该显示 `10`；输入 `C` 清除后重新输入；连续输入多个数字；按运算符后按另一个运算符（覆盖还是忽略？这取决于你的设计）。尝试给 Presenter 写单元测试——只需要 mock 一个 View 接口就行。

### 可能踩的坑

**Signal 的递归发射。** 如果槽函数 A 在被调用的时候又 `emit` 了同一个信号，而信号又调用了 A……无限递归，栈溢出。这在 Property 里特别容易发生：你订阅了 `prop.changed()`，然后在回调里又给 `prop` 赋值。解决方法是加一个"正在发射"的标志位，或者干脆用前面提到的"先拷贝 slots 再遍历"策略——拷贝的那个列表不会包含中途加入的新 slot，至少不会无限增长。

**ScopedConnection 的移动语义。** 如果你用 `auto conn = signal.connect(...)` 拿到一个连接，然后把 `conn` 存进了某个对象的成员变量里——确保你用的是移动而不是复制。`ScopedConnection` 是 deleted copy 的，如果你不小心写了 `auto conn2 = conn;`，编译器会直接拒绝。但如果你用 `std::move` 把连接转移了，原对象就变成"空连接"——再析构不会触发断开。想清楚这个语义是不是你想要的。

**Property 的比较语义。** `operator=` 里比较新旧值用的是 `==`。对于 `std::string` 和 `int` 这没问题，但如果 `T` 是一个自定义类型，它的 `operator==` 可能没有定义，或者定义的行为不是你期望的。C++20 的 `std::equality_comparable_with<T>` Concept 可以帮你加约束。另外，对于浮点数，`==` 的精度问题可能导致"应该触发但没触发"或"不该触发但触发了"的 bug——这是浮点比较的老问题了。

**Lambda 捕获的悬挂引用。** 如果你这样写：

```cpp
void setup(Button& btn, Label& label) {
    btn.clicked().connect([&]() {
        label.text() = "clicked";  // label 是引用捕获
    });
}
// setup 返回后，label 可能被销毁
// 但连接还在，下次点击按钮就是 undefined behavior
```

`[&]` 捕获的是局部变量的引用，函数返回后引用就悬挂了。解决方案：确保 `ScopedConnection` 的生命周期跟捕获的对象保持同步——比如把连接存为对象的成员变量，或者用 `std::shared_ptr` + 值捕获来延长对象寿命。

**emit 时的迭代器失效。** 如果在 `emit` 的遍历过程中，某个槽函数调用了 `disconnect`（通过某个还活着的 `ScopedConnection`），`slots_` 列表就被修改了，你正在用的迭代器就失效了。这就是为什么前面建议"先拷贝再遍历"——多一次拷贝的开销远比调试一个偶发的 use-after-free 要便宜。

### 如果卡住了

**如果你不确定 Signal 的 connect 应该怎么构造 ScopedConnection**，想一想这样的思路：`connect` 内部知道 `this` 指针（Signal 自己）和分配给新 slot 的 `id`。你可以构造一个 lambda，捕获 `this` 和 `id`，lambda 的函数体是在 `slots_` 里用 `std::erase_if` 或 `std::remove_if` 把对应 ID 的 slot 移除。把这个 lambda 作为 `disconnect_` 传给 `ScopedConnection` 的构造函数。

**如果你在 Property 的隐式转换上遇到编译错误**——`operator const T&() const` 有时候会让编译器在你不期望的地方尝试隐式转换。如果出现歧义，可以用 `.get()` 方法显式取值，暂时去掉隐式转换运算符。

**如果你在计算器的 MVP 分层上拿不准边界**，问自己一个问题："这段代码如果放在没有 GUI 的命令行程序里还有意义吗？"如果有，它属于 Model 或 Presenter；如果没有，它属于 View。比如"解析表达式并计算结果"显然跟 GUI 无关——Model。"把结果显示到界面上"——View。"用户按了等号按钮，通知 Presenter 调用 Model 计算"——Presenter 转发。

**如果你对观察者模式的本质还不太清楚**，想一想这个类比：报纸订阅。报社（Signal）不认识任何一个读者（Slot），它只知道"有人订了报"。每天印好报纸（emit），投递员按名单送一份给每个订户。订户可以随时退订（disconnect），退订之后就收不到了。报社不关心谁在看报、看了之后做什么——它只管印和送。这就是"松耦合"的核心。

**关于 C++20 Concepts 语法**，如果你看到编译器报"不满足约束"的错误但不确定怎么修，回想一下 Stage 5 里 `LayoutStrategy` Concept 的定义方式。`SlotCallable` 用的是 `std::invocable<F, Args...>`，它等价于 `requires(F f, Args... args) { f(args...); }`——确保 F 可以用 Args... 参数调用。如果你的 lambda 少了一个参数或者多了一个参数，约束就会失败。

---

## 下一步

信号系统和响应式属性到位了，我们的 MiniUI 已经具备了"响应用户操作"的能力。但如果你在当前阶段放上一百个控件然后疯狂点击，你会发现两件事：画面在闪烁，而且窗口偶尔会"卡住"。

下一个阶段——**Stage 7：渲染管线与异步 IO**——要解决的就是这两个问题。闪烁的根源是直接对屏幕做绘制，需要引入双缓冲；卡住的根源是事件循环被阻塞了，需要引入异步 IO 和基于 `poll()` 的统一事件循环。渲染管线优化（双缓冲 + 脏区管理）和异步 IO 看起来不相关，但它们最终会在同一个地方汇合——`Application::run()` 会被重写成一个同时监听 XCB 连接 fd 和自定义 IO fd 的统一循环，在约 60 FPS 的帧率下驱动整个框架。
