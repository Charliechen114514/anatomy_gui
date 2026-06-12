# Stage 4 — 从白板到控件树：GUI 框架的灵魂

---

## 这一阶段的目标

前三个阶段我们把三件事搞定了：用 XCB 打开了窗口，把事件循环封装成了 Application，用 Cairo 让窗口学会了画东西。但这三根支柱到目前为止是互相独立的——Application 管事件，Painter 管画图，两者之间没有任何结构上的关联。这个阶段的目标是把它们统一起来：设计 Widget 基类，建立控件树，让每个控件自己负责自己的绘制和事件处理。做完这一步，我们的程序就从"一个窗口上画一堆东西"升级为"一棵控件树在驱动整个界面"——这是所有 GUI 框架最核心的架构决策。

与此同时，我们还要顺手解决一个从 Stage 1 就积累下来的技术债：那些裸的 C 风格资源句柄——`xcb_connection_t*`、`cairo_surface_t*`、`cairo_t*`——一直靠手动管理生命周期，该用现代 C++ 的 RAII 机制好好封装一下了。

### 为什么控件树是所有 GUI 框架的共同选择

如果你回想一下我们在 Phase 1 里用 Win32 写程序的经历，每一个控件——按钮、编辑框、列表——都是通过 `CreateWindowEx` 创建的一个独立的系统窗口，它们之间通过 `HWND` 的父子关系构成一棵窗口树。你可以用 `GetParent` 找到父窗口，用 `EnumChildWindows` 遍历子窗口，`WM_PARENTNOTIFY` 把子窗口的事件通知给父窗口。Qt 里面是同一套概念：`QObject` 的父子关系构成对象树，父对象析构的时候自动销毁所有子对象。Web 前端的 DOM 树更不用说了——`<div>` 嵌套 `<span>` 嵌套 `<p>`，事件冒泡从子节点一路传到 `document`。

为什么所有人都不约而同选择了"树"这种结构？因为 GUI 界面本身就是嵌套的——窗口里有面板，面板里有按钮，按钮里有文字。树结构天然地表达了这种包含关系，而且它给了我们三样至关重要的东西：坐标变换（子控件的坐标相对于父控件）、命中测试（从树的根节点一路往下找到被点击的那个叶子）、以及事件传播（子控件处理不了的事件可以向上传递给父控件）。

### 关键区别：Win32 的窗口树 vs 我们的控件树

这里有一个非常重要的架构差异需要想清楚。Win32 里每个 `HWND` 对应操作系统里的一个真实窗口对象——它有自己的消息队列、自己的绘制表面、自己的 Z-Order。Windows 窗口管理器为每个 `HWND` 维护了一整套系统状态，这就是为什么一个包含几百个子窗口的对话框在 Win32 上性能很差——系统资源开销太大了。我们在 Phase 1 里也见过，想要自定义按钮的外观，你得处理 `WM_DRAWITEM`，走 Owner-Draw 路线，因为标准控件的绘制逻辑是 Windows 内部管的。

我们的 MiniUI 走的是完全不同的路线：整个程序只有一个 XCB 窗口（一个 `xcb_window_t`），所有控件都是我们自己画出来的"虚拟控件"。这跟 Qt 的 QWidget 在 X11 上的做法是一样的——Qt 默认也是在一个顶层窗口上自己管理控件树、自己绘制、自己做事件分发。这种方案的好处是控件数量不会受系统资源限制，坏处是所有的事情都得我们自己来——命中测试、裁剪、焦点管理、Z-Order，一个都跑不掉。

### 需要想清楚的问题

第一个问题：Widget 基类应该持有状态还是只定义接口？它至少需要知道自己的边界矩形（位置和尺寸）、父控件是谁、子控件有哪些、自己是否可见。除此之外，想一想每个控件还需要哪些共同状态——比如"是否启用"（enabled）、"是否接受焦点"（focusable）、"是否透明"（transparent for hit-test）。这些状态哪些应该在基类里，哪些应该留给子类？

第二个问题：坐标系统。子控件的坐标应该是相对于父控件的，还是相对于整个窗口的？如果你选择相对坐标（推荐），那命中测试的时候就需要从根节点开始，把每个层级的偏移量累加起来。想一想：当用户在屏幕坐标 (300, 200) 点击了一下，这棵树要怎么遍历才能找到被点击的那个控件？如果子控件的位置是相对于父控件的，那坐标转换的过程是什么样的？

第三个问题：控件的 ownership 语义。父控件应该"拥有"子控件——父控件析构的时候，所有子控件应该一起被销毁。这个用 C++ 怎么实现最自然？想一想 `std::unique_ptr<Widget>` 和容器（比如 `std::vector<std::unique_ptr<Widget>>`）的组合怎么表达这个 ownership 关系。`addChild` 的接口签名应该长什么样？如果有人试图把一个已经有父控件的 Widget 再加到另一个父控件下，该怎么处理？

第四个问题：绘制顺序。父控件先画还是子控件先画？如果子控件画在父控件之后，那子控件自然就覆盖在父控件上面——这是最常见的做法。但如果一个控件有背景色，它应该在画自己的内容之前还是之后填充背景？Painter 的 `save` / `restore` 机制在这里怎么用——每个控件绘制之前保存状态，绘制之后恢复状态，是不是就能避免状态污染？

第五个问题：事件传播的方向。鼠标按下事件应该从树的根节点向下传播（capture 阶段）还是从叶子节点向上传播（bubble 阶段）？还是两者都要？想一想 Web DOM 的事件模型——它同时支持 capture 和 bubble 两个阶段，但这对我们教学级框架来说是不是过度设计了？一个更简单的方案是：先通过命中测试找到目标控件，然后把事件直接发给它；如果它不处理（返回 `false`），就沿着父链向上传递。这个方案够不够用？

### 设计方向提示

Widget 基类的核心职责是定义控件树的节点协议。它不是抽象接口（不是所有方法都是纯虚函数），而是一个有默认实现的基类——大部分控件不需要重写所有方法，只需要重写自己关心的那几个。具体来说，你需要为 Widget 设计以下几个关键方法：

控件的测量与布局协议。每个控件需要知道自己"想要多大"（measure）和"被放在哪里"（arrange）。measure 接收父控件给出的可用空间，返回控件期望的尺寸；arrange 接收父控件决定的最终位置和尺寸。这个 Measure → Arrange 的两遍协议在下一阶段（布局引擎）会被完整使用，但现在先把接口定义好。`measure` 和 `arrange` 的签名大致是：

```cpp
struct Size {
    int width{};
    int height{};
};

struct Rect {
    int x{}, y{};
    int width{}, height{};
};

struct Point {
    int x{}, y{};
};

class Widget {
public:
    // 测量：给定父控件提供的可用空间，返回期望尺寸
    virtual Size measure(Size available);
    // 布局：父控件告知你的最终位置和尺寸
    virtual void arrange(Rect finalRect);

    // 绘制
    virtual void paint(Painter& painter) {}

    // 命中测试：给定相对于此控件的坐标，返回命中的控件（可能不是 this）
    virtual Widget* hitTest(Point localPos);

    // 输入事件，返回 true 表示已处理，false 表示不处理（向上传播）
    virtual bool onMousePress(Point localPos) { return false; }
    virtual bool onMouseRelease(Point localPos) { return false; }
    virtual bool onMouseMove(Point localPos) { return false; }
    virtual bool onKeyPress(int keyCode) { return false; }

    // 树操作
    void addChild(std::unique_ptr<Widget> child);
    Widget* parent() const;
    const std::vector<std::unique_ptr<Widget>>& children() const;

    // 几何
    Rect bounds() const;       // 相对于父控件的位置和尺寸
    void setBounds(Rect bounds);
    bool visible() const;
    void setVisible(bool v);

    virtual ~Widget() = default;

protected:
    Widget* parent_{nullptr};
    std::vector<std::unique_ptr<Widget>> children_;
    Rect bounds_;
    bool visible_{true};
};
```

注意 `paint` 的参数是 `Painter&`——就是我们 Stage 3 里封装的那个绘图接口。但这里有个关键问题：当父控件调用子控件的 `paint` 时，需要先把 Painter 的坐标原点平移到子控件的位置，还要设置裁剪区域防止子控件画到自己的边界外面。想一想这个坐标变换应该由谁来做——是父控件在调用子控件的 `paint` 之前设置好，还是子控件在自己的 `paint` 里自己处理？推荐前者，因为这样每个控件的 `paint` 实现都可以假设自己的坐标系原点就是自己的左上角，不需要关心自己在父控件里的位置。

`hitTest` 的逻辑也值得仔细想。默认实现应该是：遍历子控件（从后往前，因为后加入的子控件在视觉上覆盖在前面的上面），对每个子控件把坐标转换到它的局部坐标系，递归调用它的 `hitTest`。如果所有子控件都没命中，再看这个点是否落在 `bounds_` 里面。想一想为什么从后往前遍历而不是从前往后——因为后加入的子控件 Z-Order 更高，应该优先被命中。

RootWindow 是整棵控件树的宿主，它既是 XCB 窗口的拥有者，也是 Widget 树的根节点。它的职责是把 XCB 事件翻译成 Widget 树上的方法调用：

```cpp
class RootWindow {
public:
    RootWindow(xcb_connection_t* conn, int screenNum,
               int width, int height);
    ~RootWindow();

    // XCB 窗口 ID
    xcb_window_t windowId() const;

    // 控件树操作
    void setRoot(std::unique_ptr<Widget> root);
    Widget* root() const;

    // 处理一个 XCB 事件
    void handleXcbEvent(xcb_generic_event_t* event);

    // 重绘整个窗口
    void repaintAll();

    // 窗口尺寸
    void resize(int width, int height);
    Size clientSize() const;

private:
    xcb_connection_t* conn_;
    xcb_window_t window_;
    std::unique_ptr<Widget> root_;
    int width_, height_;
};
```

`handleXcbEvent` 是事件翻译的核心。当收到 `XCB_BUTTON_PRESS` 的时候，它需要把屏幕坐标转换成窗口坐标（减去窗口在屏幕上的偏移，不过对于顶层窗口来说通常就是屏幕坐标本身），然后调用根控件的 `hitTest` 找到目标控件，最后调用目标控件的 `onMousePress`。如果目标控件返回 `false`，就沿着 `parent_` 链向上传递。`XCB_EXPOSE` 事件则触发 `repaintAll`，后者创建 Cairo surface 和 Painter，从根控件开始递归调用整棵树的 `paint`。

作为第一个具体的控件实现，你可以先写一个最简单的 Label——它只负责显示一段文字：

```cpp
class Label : public Widget {
public:
    explicit Label(std::string text);

    void setText(std::string text);
    void setColor(Color color);
    void setFontSize(double size);

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    std::string text_;
    Color color_{0.0, 0.0, 0.0};  // 黑色
    double fontSize_{14.0};
};
```

Label 的 `measure` 需要知道文字渲染后占多少像素——这需要用 Cairo 的文字度量 API（`cairo_text_extents`）来计算。想一想，`measure` 里怎么获取一个 Cairo 上下文来做文字度量？一个简单的方案是创建一个临时的 image surface 专门用于度量，不需要真正绘制任何东西。

### RAII 资源管理：把 C 句柄关进 C++ 的笼子

从 Stage 1 开始我们就一直在跟 C 风格的资源句柄打交道——`xcb_connection_t*`、`cairo_surface_t*`、`cairo_t*`。之前为了聚焦核心逻辑，我们一直用裸指针 + 手动释放的方式管理它们，技术债越积越多。现在是时候用 RAII 把它们收拾干净了。

先说思路。每个 C 库的资源都有一个"创建函数"和一个"销毁函数"：

- `xcb_connect` / `xcb_disconnect`
- `cairo_xcb_surface_create` / `cairo_surface_destroy`
- `cairo_create` / `cairo_destroy`

RAII 封装的经典做法是用 `std::unique_ptr` 配合自定义 deleter。自定义 deleter 是一个可调用对象，在 `unique_ptr` 析构的时候自动调用对应的销毁函数：

```cpp
struct XcbConnectionDeleter {
    void operator()(xcb_connection_t* conn) const {
        if (conn) xcb_disconnect(conn);
    }
};

using XcbConnectionPtr = std::unique_ptr<xcb_connection_t, XcbConnectionDeleter>;

struct CairoSurfaceDeleter {
    void operator()(cairo_surface_t* surface) const {
        if (surface) cairo_surface_destroy(surface);
    }
};

using CairoSurfacePtr = std::unique_ptr<cairo_surface_t, CairoSurfaceDeleter>;

struct CairoContextDeleter {
    void operator()(cairo_t* cr) const {
        if (cr) cairo_destroy(cr);
    }
};

using CairoContextPtr = std::unique_ptr<cairo_t, CairoContextDeleter>;
```

使用的时候就像普通的 `unique_ptr` 一样——构造时传入资源指针，作用域结束自动销毁。`get()` 方法可以拿到裸指针传给 C API。想一想，这些 RAII 类型应该放在哪里——是每个类各自 typedef 自己的，还是统一放在一个公共头文件里？

如果你想要更完整的封装（不只是 RAII 包装，还提供类型安全的操作接口），可以考虑写一个 `XcbConnection` 类，在构造函数里调用 `xcb_connect`，在析构函数里调用 `xcb_disconnect`，同时暴露一些常用操作的成员方法。这比裸的 `unique_ptr` 多一层抽象，但使用起来更舒服：

```cpp
class XcbConnection {
public:
    explicit XcbConnection(const char* displayname = nullptr);

    xcb_connection_t* get() const;
    int screenCount() const;
    xcb_screen_t* screen(int index) const;
    void flush();

    // 禁止拷贝，只允许移动
    XcbConnection(const XcbConnection&) = delete;
    XcbConnection& operator=(const XcbConnection&) = delete;
    XcbConnection(XcbConnection&&) noexcept = default;
    XcbConnection& operator=(XcbConnection&&) noexcept = default;

private:
    XcbConnectionPtr conn_;
};
```

这里有几个 C++ 资源管理的概念需要理清。**Rule of Zero**：如果一个类的所有成员都已经是 RAII 管理的（`unique_ptr`、`vector`、`string`），那你不需要手写析构函数、拷贝/移动操作——编译器默认生成的就够用了。**Rule of Five**：如果一个类直接管理了某种资源（比如裸指针），你就需要自己定义析构函数、拷贝构造、拷贝赋值、移动构造、移动赋值这五个操作。我们的 Widget 基类持有 `vector<unique_ptr<Widget>>`，它的所有成员都是 RAII 管理的，所以 Widget 遵循的是 Rule of Zero——不需要手写析构函数。但 Widget 本身是多态基类，所以虚析构函数是必须的。

另一个值得用上的工具是 `std::scope_exit`（C++23 引入，或者你自己实现一个简化版）。它能在作用域退出时自动执行一段代码，非常适合"保存状态 → 做事情 → 恢复状态"这种模式。在控件树的绘制流程里，每个控件绘制之前都要 `painter.save()`，绘制之后都要 `painter.restore()`——用 `scope_exit` 可以确保即使控件中途 `return` 或者抛异常，状态也不会泄漏：

```cpp
void SomeWidget::paint(Painter& painter) {
    painter.save();
    auto guard = std::scope_exit([&painter] { painter.restore(); });
    // ... 绘制逻辑 ...
    // 无论怎么退出，restore 都会被调用
}
```

如果你的编译器还不支持 C++23 的 `std::scope_exit`，自己写一个极简版也就几行代码——构造时捕获一个可调用对象，析构时调用它。这个工具在整个框架里都非常有用，值得早点准备。

### 验证方法

写一个测试程序：创建一个 RootWindow，给它设置一个根 Widget（可以是自定义的彩色矩形），根 Widget 下面添加两个 Label 子控件——一个显示 "Hello" 在左上角，一个显示 "MiniUI" 在右下角。运行后窗口应该同时显示背景矩形和两段文字。点击窗口上某个位置，终端应该打印出命中测试的结果——是哪个控件被命中了。点击文字应该命中 Label，点击空白处应该命中背景矩形。

调整窗口大小后，如果你实现了 `XCB_CONFIGURE_NOTIFY` 的处理，窗口内容应该根据新尺寸重绘。如果你还没处理 resize，那至少重绘的时候不能崩溃。

如果想验证 RAII 封装是否正确，可以故意让程序在创建资源之后提前 `return`，然后用 Valgrind 检查是否有内存泄漏：

```bash
valgrind --leak-check=full ./your_test_program
```

正确的 RAII 封装不应该报告任何资源泄漏。如果 `xcb_disconnect` 或者 `cairo_destroy` 没有被调用，说明某个 RAII 包装的生命周期管理有问题。

### 可能踩的坑

⚠️ `unique_ptr` 的自定义 deleter 会影响类型。`unique_ptr<xcb_connection_t>` 和 `unique_ptr<xcb_connection_t, XcbConnectionDeleter>` 是两个完全不同的类型——你不能把前者赋值给后者。如果你在某个地方用裸的 `unique_ptr<T>` 创建了资源，后面又想用带 deleter 的版本，需要通过 release + reset 的方式转移 ownership，不能直接赋值。

另一个容易翻车的地方是 `hitTest` 的坐标转换。每个子控件的位置是相对于父控件的，所以从父控件的局部坐标转换到子控件的局部坐标时，需要减去子控件在父控件中的偏移。如果你忘了这一步，命中测试的坐标就会偏移——点击明明在子控件上面，却报告没有命中。这个 bug 很隐蔽，建议在命中测试的实现里加上调试打印，把每一步的坐标转换都输出来看看。

还有一个关于 `repaintAll` 的细节：每次重绘都创建一个新的 Cairo surface 和 context 是最简单的方案，但如果有大量控件，频繁创建和销毁 surface 会有性能开销。现阶段不用管这个——先跑通再说，性能优化是 Stage 7 的事。但你心里要有数：这种"每次重建"的方案不是最终方案。

控件树的生命周期也需要注意。RootWindow 拥有根控件（通过 `unique_ptr`），根控件拥有子控件（通过 `vector<unique_ptr<Widget>>`）。只要 RootWindow 析构了，整棵树都会被正确销毁。但要避免一个陷阱：不要在控件的 `paint` 或者事件处理方法里修改控件树（比如添加或删除子控件），因为这些操作发生在树的遍历过程中，修改正在遍历的容器会导致未定义行为。如果确实需要延迟修改，可以用一个"待处理操作队列"，在当前事件处理完毕后再执行。

### 如果卡住了

如果你在"Widget 基类应该有哪些方法"这个点上纠结，去翻一翻 Qt 的 `QWidget` 文档——不需要看完，只要扫一眼它的 public 方法列表，就能感受到一个成熟的 Widget 基类需要处理多少事情。我们只需要它的一个最小子集：几何、树、绘制、事件。其他的（样式表、动画、拖放……）都是后面可以加的。

如果你在命中测试的实现上卡住了，试一个最简单的情况：只有两层——根控件加一个子控件。手动追踪一下坐标转换的过程：用户点击屏幕坐标 (px, py)，减去窗口偏移得到窗口坐标，传给根控件的 `hitTest`，减去子控件的偏移得到子控件的局部坐标，看是否落在子控件的 `bounds_` 里。把这个过程用纸笔画出来，比盯着代码想更有效。

如果你在 RAII 封装上遇到编译错误，大概率是 `unique_ptr` 的 deleter 类型问题。记住 `std::unique_ptr<T, Deleter>` 是一个完整的类型，你需要让类型声明和实际使用完全一致。如果 `XcbConnectionPtr` 在头文件里被使用了，确保它的 typedef 所在的头文件已经被 include 了。

关于 `std::scope_exit`，如果你的编译器报"scope_exit is not a member of std"，说明你的编译器标准版本设得不够高。你可以把 `-std=c++23` 加到 CMake 里，或者自己实现一个极简版——它本质上就是一个只移动的 RAII 包装，构造时保存一个 lambda，析构时调用它。

---

## 下一步

控件树搭好了，Widget 基类有了，但你会发现一个让人不爽的事情：每个子控件的位置和尺寸都是在 `addChild` 之后手动调用 `setBounds` 硬编码的。改一下窗口大小，布局全乱了。下一个阶段我们要解决这个问题——设计布局引擎，实现 Measure → Arrange → Render 的三遍渲染管线，同时用 C++20 Concepts 约束布局协议的类型安全。到时候你只需要告诉布局容器"这些子控件要水平排列"或者"这个子控件占据剩余空间"，位置和尺寸就自动算好了。
