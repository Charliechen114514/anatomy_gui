# Stage 8 — Mini 文本编辑器：用协程缝合一切

---

## 这一阶段的目标

经过前面七个阶段的搭建，我们的 MiniUI 框架已经拥有了事件循环、Cairo 绘图、控件树、布局引擎、信号系统、双缓冲渲染管线——每一块都是独立拼好的积木。但积木拼好了不等于建筑完工了，我们还没有用这套框架从头到尾做过一个真正的应用。这个阶段就是收官之作：我们要用 MiniUI 拼出一个迷你的文本编辑器，同时在 Presenter 层引入 C++20 协程来解决上一阶段遗留的异步回调地狱问题。

做完这个阶段，你会得到三样东西：

1. **一个能用的 Mini Editor** —— 菜单栏、自绘文本画布、状态栏，支持打开/保存文件、光标移动、字符输入删除、撤销重做
2. **C++20 协程实战经验** —— 用 `Task<T>` + `co_await` 把嵌套回调拍平成线性代码
3. **MVP 架构完整实践** —— Model / View / Presenter 三层分离在真实项目里怎么落地

### 为什么要把文本编辑器作为收官项目

因为文本编辑器恰好能压到 GUI 框架的每一个痛点：需要键盘输入处理（事件系统）、需要自绘文字和光标（Cairo 绘图）、需要多控件协作（控件树 + 布局）、需要响应式更新（信号系统 + Property）、需要高效的局部刷新（双缓冲 + 脏区管理）、需要异步文件 I/O（协程）。它不是最复杂的 GUI 应用，但它是覆盖面最广的——一个文本编辑器跑通了，说明你的框架该有的能力都有了。

### 为什么要在收官阶段引入协程

Stage 7 里我们做了双缓冲和脏区管理，渲染管线已经很干净了。但你有没有注意到一个问题——如果文件操作（打开、保存）是异步的，那 Presenter 里处理这些操作的代码会变成嵌套回调的深渊。信号 A 触发回调 lambda，在 lambda 里发起异步操作，异步操作完成后再通过信号 B 通知 View 更新……两层回调还算能忍，三四层就开始人肉追踪了。C++20 协程给我们提供了 `co_await` 这个关键字，让异步代码在视觉上变成"一步一步往下写"的线性形态，但实际上执行流程会在 `co_await` 处挂起、在合适的时候恢复。这不是语法糖——这是对异步控制流的根本性抽象。

### 需要想清楚的问题

**第一个问题：Model 层到底应该知道多少 GUI 的事情？**

答案当然是零。`TextBuffer` 应该是一个纯粹的数据管理器——它管理文本行、光标位置、撤销重做栈、文件读写。它不应该知道任何关于 Cairo、Widget、信号这些东西的存在。那 Model 的变化怎么通知到 View？通过 `Signal<>`。但注意，这个 Signal 是 MiniUI 框架提供的基础设施，不是 GUI 专属的概念——你可以把它理解为"事件发生器"，它恰好也被控件用来通信，但 Model 用它来广播"我的数据变了"是完全合理的。想一想，`TextBuffer` 的 `contentChanged()` 返回一个 `Signal<>&`，这个设计有没有把 Model 绑定到 MiniUI 上？如果是你，你会选择用 MiniUI 的 Signal，还是自己写一个更轻量的观察者模式？

**第二个问题：Presenter 是 View 和 Model 之间的"调度员"，它的职责边界在哪？**

Presenter 接收来自 View 的用户操作信号（"用户按了键"、"用户点了打开"），然后翻译成对 Model 的调用（"插入字符"、"加载文件"）。Model 数据变化后发射信号，Presenter 再把信号转发给 View 刷新显示。这个三角关系中，Presenter 不应该包含任何绘制逻辑——它只负责"翻译"和"调度"。文件操作因为涉及异步 I/O，所以用协程来写 Presenter 的文件操作方法是最自然的。

**第三个问题：协程和事件循环怎么协作？**

`co_await` 挂起一个协程的时候，控制权回到事件循环；事件循环在下一次迭代中恢复协程。这意味着你需要一个机制：协程挂起时把恢复句柄注册到 Application 的任务队列里，事件循环在处理完当前事件后检查队列，取出挂起的协程恢复执行。想一想，这个机制跟 JavaScript 里的 `Promise.then()` + microtask queue 是不是同一个模式？

**第四个问题：为什么不用 `std::async` 或者线程池？**

因为我们的 MiniUI 是单线程的——所有 GUI 操作都必须在主线程（事件循环所在的线程）上执行。Cairo 绘图、Widget 操作、XCB 请求都不是线程安全的。`std::async` 会启动新线程，线程函数里操作 Widget 会出未定义行为。协程的优势在于：挂起和恢复都在同一个线程上，不需要加锁，不存在数据竞争。它解决的是"异步控制流"问题，不是"并行计算"问题。

### 设计方向提示

#### 整体架构：MVP 三角

```
                    ┌─────────────────┐
                    │   EditorView    │
                    │  ┌───────────┐  │
          signals   │  │ MenuBar   │  │  repaint
        ┌──────────│  ├───────────┤  │──────────┐
        │          │  │ TextCanvas│  │          │
        │          │  ├───────────┤  │          │
        │          │  │ StatusBar │  │          │
        │          │  └───────────┘  │          │
        │          └───────┬─────────┘          │
        │                  │                    │
        │  keyPress,       │  contentChanged    │
        │  menuAction      │  signal            │
        │                  │                    │
        │          ┌───────▼─────────┐          │
        │          │ EditorPresenter │          │
        │          │ ─ co_await ───  │          │
        │          │ onOpenFile()    │          │
        │          │ onSaveFile()    │          │
        │          └───────┬─────────┘          │
        │                  │                    │
        │          insertChar, deleteChar,      │
        │          loadFile, saveFile           │
        │                  │                    │
        │          ┌───────▼─────────┐          │
        │          │   TextBuffer    │          │
        │          │   (纯数据层)    │──────────┘
        │          └─────────────────┘   contentChanged
        │                                  → repaint
        └──────────────────────────────────┘
```

整个数据流是这样的：用户按键 → View 收集键盘事件 → 发射信号给 Presenter → Presenter 调用 Model 的方法 → Model 修改数据并发射 `contentChanged` 信号 → Presenter 转发给 View → View 请求重绘 → 双缓冲渲染管线把变化画到屏幕上。

#### Model：TextBuffer

```cpp
class TextBuffer {
public:
    // ── 编辑操作 ──
    void insertChar(char c);       // 在光标处插入字符
    void deleteChar();             // 删除光标处的字符（或选中区域）
    void newLine();                // 在光标处插入换行，拆分行

    // ── 撤销/重做 ──
    void undo();
    void redo();

    // ── 光标移动 ──
    void moveCursor(int lineDelta, int colDelta);
    void setCursor(int line, int col);

    // ── 文件 I/O ──
    void loadFile(const std::string& path);
    void saveFile(const std::string& path);

    // ── 查询接口（给 View 用）──
    const std::vector<std::string>& lines() const;
    int cursorLine() const;
    int cursorCol() const;
    bool isModified() const;

    // ── 变更通知 ──
    Signal<>& contentChanged();

private:
    std::vector<std::string> lines_  = { "" };  // 至少一行
    int cursorLine_ = 0;
    int cursorCol_  = 0;
    bool modified_  = false;

    // 撤销栈：保存每次操作前的快照（简化实现）
    struct Snapshot {
        std::vector<std::string> lines;
        int cursorLine;
        int cursorCol;
    };
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;

    Signal<> contentChanged_;
};
```

注意 `TextBuffer` 的头文件里不会 `#include` 任何 Cairo、Widget、XCB 相关的东西。它的依赖只有 `<vector>`、`<string>` 和 MiniUI 的 `Signal<>`。如果你对 Signal 的依赖感到不舒服，可以抽一个更薄的 `IChangeNotifier` 接口出来——但在教学框架里直接用 Signal 完全可以接受。

#### View：EditorView

```cpp
class EditorView : public VBox {            // 垂直排列：菜单 → 画布 → 状态栏
public:
    explicit EditorView(Widget* parent = nullptr);

    // ── 子控件访问（给 Presenter 连信号用）──
    TextCanvas*  textCanvas()  { return canvas_; }
    MenuBar*     menuBar()     { return menu_; }
    StatusBar*   statusBar()   { return status_; }

private:
    MenuBar*     menu_;
    TextCanvas*  canvas_;
    StatusBar*   status_;
};
```

EditorView 的三个子控件各有分工：

- **MenuBar**（HBoxLayout）：几个 Button 排成一行——"文件"、"编辑"、"帮助"。点击"文件"弹出一个简易的子菜单（也可以简化为直接触发动作）
- **TextCanvas**（自绘 Widget）：这是编辑器的核心画布。它没有用任何系统原生文本控件，所有文字、光标、选区都是用 Cairo 一笔一画绘制的。它捕获键盘事件并通过信号发射出去
- **StatusBar**（Label + Property）：显示当前光标位置（行号:列号）、文件是否已修改。用 Property 实现响应式更新

```cpp
class TextCanvas : public Widget {
public:
    explicit TextCanvas(Widget* parent = nullptr);

    // ── 显示接口（给 Presenter 调）──
    void setLines(const std::vector<std::string>& lines);
    void setCursor(int line, int col);

    // ── 用户输入信号（给 Presenter 连）──
    Signal<xcb_keycode_t>& keyPressed()  { return keyPressed_; }
    Signal<>&              enterPressed() { return enterPressed_; }

protected:
    void onPaint(Painter& p) override;    // 自绘：文字 + 光标 + 行号
    void onKeyEvent(xcb_keycode_t) override;

private:
    Signal<xcb_keycode_t> keyPressed_;
    Signal<>              enterPressed_;
    std::vector<std::string> displayLines_;
    int cursorLine_ = 0, cursorCol_ = 0;
};
```

#### Presenter：EditorPresenter（协程出场的地方）

```cpp
class EditorPresenter {
public:
    EditorPresenter(TextBuffer* model, EditorView* view);

    // ── 协程文件操作 ──
    Task<> onOpenFile();     // co_await 异步文件选择 → 加载 → 更新视图
    Task<> onSaveFile();     // co_await 异步保存 → 清除 modified 标记

    // ── 同步编辑操作 ──
    void onNewFile();
    void onCharInput(xcb_keycode_t keycode);
    void onUndo();
    void onRedo();

private:
    TextBuffer*  model_;
    EditorView*  view_;

    void connectSignals();   // 把 View 的信号连到自己的处理方法上
    void syncView();         // 把 Model 的当前状态同步到 View
};
```

#### C++20 协程：Task<T> 返回类型

这是本阶段的硬核部分。C++20 协程的机制是这样的：当你写一个返回 `Task<T>` 的函数并在里面用 `co_await` 时，编译器会把函数体变换成一个状态机。每次 `co_await` 都是一个潜在的挂起点——如果被等待的操作还没完成，协程就挂起，控制权回到调用者；等操作完成了，协程从挂起点恢复继续执行。

我们需要实现 `Task<T>` 这个协程返回类型。它的核心是内嵌的 `promise_type`——这是 C++20 协程的接口约定，编译器通过 `promise_type` 来控制协程的行为。

```cpp
template <typename T = void>
class Task {
public:
    // ── 编译器要求的 promise_type ──
    struct promise_type {
        // 协程函数返回值类型的对象
        auto get_return_object() {
            return Task{Handle::from_promise(*this)};
        }

        // 初始挂起：suspend_never 意味着协程函数体立即开始执行
        // （直到遇到第一个 co_await 才可能挂起）
        std::suspend_never initial_suspend() { return {}; }

        // 最终挂起：返回自定义 Awaiter，让协程在结束时恢复调用者
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }

                // 协程结束挂起前，如果有 continuation，就安排恢复它
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<> h) noexcept
                {
                    auto& promise = h.promise();
                    if (promise.continuation_) {
                        return promise.continuation_;  // 恢复等待者
                    }
                    return std::noop_coroutine();      // 没人等，啥也不做
                }

                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        // co_return value 的处理
        void return_value(T value) {
            result_ = std::move(value);
        }

        // 异常处理
        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        // ── 成员 ──
        std::optional<T>              result_;
        std::exception_ptr            exception_;
        std::coroutine_handle<>       continuation_;   // 谁在等我完成
    };

    using Handle = std::coroutine_handle<promise_type>;

    // ── Awaitable 接口（让 Task 可以被 co_await）──
    bool await_ready() {
        return handle_.done();  // 已经完成就不用挂起
    }

    // 挂起时：记录调用者（continuation），等完成后恢复它
    void await_suspend(std::coroutine_handle<> caller) {
        handle_.promise().continuation_ = caller;
        // 此时协程被挂起，控制权回到事件循环
    }

    // 恢复时：返回结果（或重新抛异常）
    T await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(*handle_.promise().result_);
    }

    // ── 显式启动（不需要 co_await 的场景）──
    void start() {
        if (!handle_.done()) {
            Application::instance().scheduleResume(handle_);
        }
    }

    ~Task() {
        if (handle_) handle_.destroy();
    }

private:
    explicit Task(Handle h) : handle_(h) {}
    Handle handle_;
};
```

对于 `Task<void>` 的特化，把 `return_value` 换成 `return_void`，`result_` 字段去掉，其余结构相同。这里不展开完整特化代码，你写的时候自然会碰到。

#### 协程调度器：集成到 Application::run()

```cpp
class Application {
public:
    // 把协程句柄加入恢复队列
    void scheduleResume(std::coroutine_handle<> handle) {
        resumeQueue_.push(handle);
        // 顺带唤醒事件循环（如果它在阻塞等待事件的话）
        wakeupEventLoop();
    }

    int run() {
        while (running_) {
            // 1. 处理 XCB 事件（非阻塞）
            processXcbEvents();

            // 2. 恢复挂起的协程（这就是协程调度！）
            while (!resumeQueue_.empty()) {
                auto handle = resumeQueue_.front();
                resumeQueue_.pop();
                handle.resume();   // 协程从挂起点继续执行
            }

            // 3. 处理待处理的信号
            processDeferredSignals();

            // 4. 执行脏区重绘
            processRepaints();

            // 5. 如果没事干，阻塞等下一个 XCB 事件
            if (resumeQueue_.empty() && !hasPendingEvents()) {
                waitForNextEvent();
            }
        }
        return exitCode_;
    }

private:
    std::queue<std::coroutine_handle<>> resumeQueue_;
};
```

注意这个调度策略的关键：**先处理事件，再恢复协程，再处理信号，最后重绘**。这个顺序保证了协程恢复后对 Model 的修改，能在同一帧内被信号传播到 View 并完成重绘。如果顺序反了（比如先重绘再恢复协程），你可能会看到一帧的闪烁——数据变了但画面还没更新。

#### 回调版本 vs 协程版本对比

这是 Stage 7 之后最直观的改进。假设我们要实现"打开文件"功能——弹出文件选择（简化为固定路径）、读取文件内容、更新 TextBuffer、刷新显示。

**回调版本（Stage 7 风格，callback hell）**：

```cpp
void EditorPresenter::onOpenFile() {
    // 第一层回调：选择文件
    fileDialog_->show([this](const std::string& path) {
        // 第二层回调：读取文件
        fileReader_->readAsync(path, [this, path](std::string content) {
            // 第三层回调：解析行
            auto lines = splitLines(content);
            model_->loadFromLines(lines);
            // 第四层：手动触发刷新
            syncView();
            status_->setText("Opened: " + path);
        });
    });
    // 注意：到这里的时候文件还没打开！
    // 所有后续逻辑都嵌在 lambda 里
}
```

三层 lambda 嵌套，变量捕获容易出错（`this`、`path` 要小心），错误处理几乎没法写（你需要在每一层 lambda 里判断有没有失败）。如果再加一个"打开前检查文件是否已修改、如果修改了就先保存"的逻辑，嵌套层数直接爆炸。

**协程版本（Stage 8 风格，线性代码）**：

```cpp
Task<> EditorPresenter::onOpenFile() {
    // 检查是否有未保存的修改
    if (model_->isModified()) {
        auto choice = co_await confirmDialog("文件已修改，是否保存？");
        if (choice == DialogResult::Yes) {
            co_await onSaveFile();
        }
    }

    // 选择文件
    auto path = co_await fileDialog_->selectFile();

    // 读取内容（异步 I/O，协程在这里挂起，不阻塞事件循环）
    auto content = co_await readFileAsync(path);

    // 解析并加载
    auto lines = splitLines(content);
    model_->loadFromLines(lines);
    model_->setFilePath(path);

    // 同步视图
    syncView();
    status_->setText("Opened: " + path);
}
```

看到了吗？同样是异步操作，但现在代码是**从上往下线性流动的**。每个 `co_await` 在视觉上就像一个同步调用——"等待结果，然后继续"。但实际上执行流在那个点挂起了，事件循环继续处理其他事件，等异步操作完成后协程才恢复。这就是协程的核心价值：**用同步的代码形态表达异步的控制流**。

### 验证方法

分步验证，不要一口气全写完再测：

**第一步：先跑通 TextBuffer**

写单元测试（或者简单的 main 函数），验证 `insertChar`、`deleteChar`、`newLine`、`undo`、`redo`、`loadFile`、`saveFile` 的行为是否正确。这一步不涉及任何 GUI 代码，纯粹是数据逻辑。

```cpp
// 快速验证思路
TextBuffer buf;
buf.insertChar('H');
buf.insertChar('i');
assert(buf.lines().size() == 1);
assert(buf.lines()[0] == "Hi");
buf.undo();
assert(buf.lines()[0] == "H");
buf.redo();
assert(buf.lines()[0] == "Hi");
```

**第二步：TextCanvas 自绘验证**

把 TextCanvas 单独放进一个窗口，硬编码几行文字，验证自绘文字、光标闪烁、行号显示是否正确。这一步不涉及 Presenter 和 Model，只测 View 的绘制能力。

**第三步：MVP 连线**

把 TextBuffer、EditorView、EditorPresenter 连起来，验证按键 → 信号 → Presenter → Model → contentChanged → syncView → repaint 的完整链条能跑通。先不做文件操作，只做字符输入和删除。

**第四步：协程文件操作**

实现 `Task<>` ，然后把 `onOpenFile` 和 `onSaveFile` 改成协程版本。验证打开文件时界面不卡死（协程挂起了，事件循环继续跑），文件加载完成后内容正确显示。

**第五步：完整端到端测试**

打开一个文件 → 编辑几行文字 → 撤销 → 重做 → 保存 → 关闭 → 重新打开 → 验证内容一致。

### 可能踩的坑

**协程对象的生命周期陷阱**

`Task<T>` 是一个 RAII 对象——析构时会 `destroy` 协程帧。这意味着你必须确保 `Task` 对象的生命周期覆盖协程的整个执行过程。如果你写 `onOpenFile()` 返回 `Task<>` 但没有 `co_await` 它也没有 `start()` 它，返回的临时 `Task` 对象在语句结束时析构，协程帧就被销毁了。这跟 `std::future` 的 `.then()` 忘记保存返回值是一个类型的 bug。

**`initial_suspend` 返回 `suspend_never` 的后果**

我们选择 `suspend_never` 作为初始挂起点，意味着调用协程函数时函数体会立即执行到第一个 `co_await`。如果你在 `Task` 的构造函数里做了一些需要在协程体执行前完成的初始化，要注意时序。另一个选择是 `suspend_always`——协程创建后立即挂起，需要手动 `resume` 才开始执行。两种方案都是合理的，选择哪一种取决于你的使用场景。

**TextCanvas 的光标闪烁**

光标闪烁需要一个定时器来周期性切换光标的可见性。但 MiniUI 的定时器是基于事件循环的——你在事件循环里检查时间戳，如果到了就触发回调。如果你的事件循环卡在 `xcb_wait_for_event` 上，定时器就不准了。解决方法是给 `xcb_wait_for_event` 设一个超时，或者在定时器到期时用 `scheduleResume` 唤醒事件循环。

**自绘文本的性能**

TextCanvas 的 `onPaint` 里你要遍历所有可见行，逐行用 Cairo 绘制文字。如果你的编辑器加载了一个几千行的文件，每次重绘都画所有行会很慢。你需要做视口裁剪——只绘制当前可见区域内的行。这需要知道每行的像素高度（固定行高的话很简单），然后根据滚动偏移算出需要绘制的起止行号。

**协程和信号的交互**

`contentChanged` 信号可能在协程挂起期间被发射。比如 `co_await readFileAsync` 挂起后，事件循环处理了其他事件，某个事件触发了 `contentChanged`。这时候 View 会被通知重绘，但数据可能还没完全加载好。解决方法是在 `loadFromLines` 之前不发信号，一次性加载完再发——这需要在 `TextBuffer` 里提供批量操作模式，或者在 `loadFromLines` 结束后手动调一次 `syncView`。

### 如果卡住了

**协程编译报错看不懂**

C++20 协程的编译错误以"模板实例化深度"著称，动辄几百行的错误栈。最常见的错误是 `promise_type` 缺少某个必要方法——编译器会告诉你 "no member named 'xxx' in your_type::promise_type"。对照上面的 `promise_type` 定义逐一检查：`get_return_object`、`initial_suspend`、`final_suspend`、`return_value`（或 `return_void`）、`unhandled_exception`，五个缺一不可。

**协程挂起后再也恢复不了**

检查 `await_suspend` 的返回值类型。如果 `await_suspend` 返回 `void`，协程挂起后控制权回到调用者，但没有任何机制来恢复它——你需要自己安排 `scheduleResume`。如果 `await_suspend` 返回 `std::coroutine_handle<>`，那返回的句柄会被立即恢复——这可以用来链式调度。如果返回 `bool`，`true` 表示挂起，`false` 表示不挂起（立即恢复）。搞清楚你的 `await_suspend` 返回的是什么类型。

**TextBuffer 的撤销重做实现复杂度爆炸**

如果每个操作都保存完整的行数组快照，那对于大文件来说内存开销很大。生产级的撤销实现用的是 Command 模式——每个操作记录一个可逆的命令对象（包含最小信息，比如 "在第 3 行第 5 列插入字符 'a'"，逆操作就是 "删除第 3 行第 5 列"）。但在教学框架里，快照方案完全够用，不要过度设计。

**自绘文字对不齐**

Cairo 的文字度量（extents）包含了 baseline、ascent、descent 等信息。如果你只看 advance width 来定位下一个字符，中英文混排的时候会歪。正确做法是用 `cairo_text_extents` 获取每个字形的精确尺寸，或者在固定宽度字体的前提下用 `cairo_font_extents` 获取全局行高。

---

## 完整按键链路：从 X11 到屏幕

当你按下键盘上的一个键，以下 14 个步骤在 MiniUI 内部依次发生：

```
 ① X Server 捕获物理按键
    │
    ▼
 ② XCB 事件队列收到 XCB_KEY_PRESS
    │
    ▼
 ③ Application::run() 的事件循环取出事件
    │
    ▼
 ④ Application 分发到 RootWindow
    │
    ▼
 ⑤ RootWindow 执行 hitTest，定位到 TextCanvas
    │
    ▼
 ⑥ 事件沿控件树传播：RootWindow → VBox → TextCanvas
    │
    ▼
 ⑦ TextCanvas::onKeyEvent(keycode) 被调用
    │
    ▼
 ⑧ TextCanvas 发射 keyPressed(keycode) 信号
    │
    ▼
 ⑨ EditorPresenter::onCharInput(keycode) 被触发（信号连接）
    │
    ▼
 ⑩ Presenter 调用 TextBuffer::insertChar(char)
    │
    ▼
 ⑪ TextBuffer 修改数据，发射 contentChanged() 信号
    │
    ▼
 ⑫ Presenter 收到 contentChanged，调用 syncView()
    │   syncView → TextCanvas::setLines() + setCursor()
    ▼
 ⑬ TextCanvas 标记自身为 dirty，请求 repaint
    │
    ▼
 ⑭ Application 渲染管线执行：
    DoubleBuffer::beginPaint → Widget::onPaint → Cairo 绘制 →
    DoubleBuffer::endPaint → XCB flush → 屏幕更新
```

整条链路涉及了我们之前七个阶段搭建的所有基础设施：Stage 1 的 XCB 连接、Stage 2 的事件循环、Stage 3 的 Cairo 绘图、Stage 4 的控件树和命中测试、Stage 5 的布局引擎（VBox 安排 TextCanvas 的位置）、Stage 6 的信号系统、Stage 7 的双缓冲渲染。一个按键事件走完全程，相当于对整个框架做了一次集成测试。

## MiniUI vs Qt vs GTK 对比

做完这个文本编辑器之后，你可能会好奇——我们手搓的这些东西，跟工业级的 GUI 框架差距在哪？下面这张表帮你建立一个参照系。

| 维度 | MiniUI（我们） | Qt | GTK 4 |
|------|---------------|-----|-------|
| **窗口系统后端** | XCB（手动封装） | QPA 插件：XCB/Wayland/Win32/macOS | GDK 后端：X11/Wayland/Broadway |
| **绘图引擎** | Cairo（直接封装） | QPainter → raster/GL/software | GSK（场景图 + GPU 渲染器） |
| **控件模型** | Widget 树 + 虚函数 onPaint | QWidget + uic + Qt Designer | GtkWidget + .ui 文件 + Blueprint |
| **布局引擎** | Measure → Arrange → Render | QLayout 系列（HBox/VBox/Grid） | GtkBoxLayout / GtkGrid / CSS |
| **信号系统** | `Signal<>` + `connect()` | QMetaObject::activate（moc 生成） | GObject 信号系统（g_signal_connect） |
| **事件传播** | hitTest + 树遍历 | QEvent + event() + installEventFilter | GtkEventController + 事件传播阶段 |
| **异步模型** | C++20 协程 Task\<T\> | QFuture + QtConcurrent + QTimer | GTask + GMainContext + GAsyncReadyCallback |
| **属性系统** | `Property<T>` | Q_PROPERTY（moc 生成） | GObject 属性系统（GParamSpec） |
| **双缓冲** | 手动实现 BackBuffer | QWidget 自动双缓冲（QWidget::repaint） | GSK 自动离屏渲染 |
| **主题/样式** | 无（硬编码颜色） | QSS（类 CSS）+ QPalette | CSS（GTK Inspector） |
| **可访问性** | 无 | QAccessible | GtkAccessible（ATK/AT-SPI） |
| **国际化和文字排版** | 无（ASCII 为主） | QTextLayout + ICU | Pango |
| **代码规模** | ~3000 行 C++ | ~1500 万行 C++ | ~800 万行 C |
| **开发团队** | 你一个人 | The Qt Company + 社区 | GNOME 社区 |

几个关键差距值得注意：

- **Qt 的 moc（Meta-Object Compiler）** 做了大量代码生成工作——信号-槽连接、属性系统、运行时类型信息——我们在 MiniUI 里用 C++20 的模板和概念手动实现了简化版本
- **GTK 4 的 GSK** 已经转向 GPU 加速的渲染——场景图（scene graph）而不是直接绘制，这跟我们 Stage 7 做的 CPU 双缓冲是不同的路线
- **两者都有完善的输入法支持**——这对 CJK 文本编辑器是必须的，MiniUI 暂时只处理 ASCII 键码
- **可访问性（Accessibility）** 是我们完全没覆盖的领域——工业级框架里这是一个完整子系统

看到差距并不意味着我们的工作白费了——恰恰相反，你现在能看懂这张表里的每一行了。你知道 QLayout 背后在做什么，你知道 QPaintDevice 的 Cairo 对等物是什么，你知道信号-槽的底层机制不需要魔法。这就是从零搭建框架的价值：**你不是学会了 MiniUI，你是学会了解读所有 GUI 框架的能力**。

---

## 下一步

这个阶段完成了 MiniUI 框架的全部核心功能和收官项目。但如果你觉得还不过瘾，还有两个方向值得深入探索。

下一个可选方向是 **Stage 9 — Observable + Rx 模式**。我们在 Stage 6 里实现的 `Signal<>` 是最基本的观察者模式——一对多，发射-接收。但在实际 GUI 编程中，事件流（event stream）的操作远不止"收到就处理"这么简单。你经常需要：过滤（只关心特定条件的按键）、防抖（搜索框输入停顿 300ms 后才触发搜索）、合并（把鼠标和键盘的事件流合并处理）、转换（把按键码翻译成字符再传递）。Rx（Reactive Extensions）模式提供了一套声明式的操作符来组合和变换事件流——`observable.filter().debounce(300ms).map().subscribe()`。这比在信号回调里写 if-else 要优雅得多，也更适合复杂交互场景的实现。

另一个可选方向是 **完整输入法集成** —— 让 MiniUI 支持 XIM（X Input Method）协议，处理中文输入法的 preedit、commit、feedback 流程。这涉及 `XCB_XIM_*` 系列事件的处理，以及与 fcitx/ibus 的交互。如果你打算让 Mini Editor 真正能用来编辑中文文本，这是一个必要的步骤。

不管你接下来往哪个方向走，到这一步你已经具备了独立设计和实现 GUI 框架核心组件的能力。事件循环、控件树、布局引擎、信号系统、渲染管线、协程调度——这些不是一个框架的"实现细节"，而是所有 GUI 框架共享的"骨架结构"。带着这些理解去读 Qt 的源码或者 GTK 的文档，你会发现它们不再是一本天书，而是一个"更完善的 MiniUI"。
