# Stage 10 — 线程池与并发 GUI：让界面不再卡死

---

## 这一阶段的目标

到上一个阶段为止，我们的 MiniUI 已经具备了一套完整的响应式数据流——`Observable<T>` 让数据变更能自动驱动 UI 更新，信号系统让控件间通信变得优雅。但有一个问题我们一直在回避：如果某个操作本身就很慢呢？比如打开一个包含几千张图片的文件夹，为每张图片生成缩略图——这个过程可能需要好几秒。如果我们在事件循环里同步做这件事，整个窗口就会冻住：不响应鼠标、不响应键盘、甚至连关闭按钮都点不动。

这个阶段的目标就是解决这个问题。我们要给 MiniUI 引入线程池，让耗时操作可以在后台线程上执行，同时保证 UI 线程的安全。做完这一步之后，你就能写出"后台加载缩略图，界面依然流畅响应"的程序了——这在真实应用中是最基本的要求。

核心工作有三件：

1. 实现一个固定大小的线程池 `ThreadPool`，基于 C++20 的 `std::jthread` 和 `std::stop_token`
2. 让 `Application` 持有线程池，对外暴露 `runAsync()` 接口
3. 实现 `postTask()` 机制——让后台线程能够安全地把结果"投递"回 GUI 线程

### 为什么 UI 会被卡死

先回忆一下我们在 Stage 2 里建立的事件循环模型：

```
while (running) {
    event = waitForEvent();   // 阻塞等待
    dispatch(event);          // 同步处理
}
```

事件循环是单线程的。这意味着 `dispatch(event)` 这一步必须尽快返回——因为在你处理当前事件的时候，后面的事件都在排队等着。如果你在某个事件处理器里做了 3 秒的缩略图生成，那这 3 秒内所有的事件（鼠标移动、窗口重绘、键盘输入）都在队列里堆着，用户看到的就是一个冻住的窗口。

这个问题在 Win32 里完全一样——在 `WndProc` 里做了耗时操作，`GetMessage` 就取不到下一条消息，窗口同样会冻住。解决方案也是一样的思路：把耗时操作扔到另一个线程上做，做完之后把结果"通知"回 UI 线程。Win32 里用 `PostMessage` 往主线程的消息队列投递一条自定义消息，我们在 MiniUI 里要实现一个类似的机制——`postTask`。

### 需要想清楚的问题

**第一个问题：GUI 线程安全原则。**

GUI 框架里有一条几乎 universally true 的黄金法则：**只有事件循环所在的线程（也就是 GUI 线程 / 主线程）才能操作 Widget。** 这不是我们的 MiniUI 独有的规定——Win32 的 HWND 操作必须在工作线程通过 `PostMessage` 转发到创建该窗口的线程；Qt 的规则是" QObject 的亲缘关系线程"；GTK 严格到直接在非主线程调用 GTK API 就会触发 assertion failure。

为什么？因为给控件树的每一个操作都加锁的成本太高了——想象一下绘制一棵有几百个节点的控件树，每个 `paint()` 调用都要获取同一把锁，性能会惨不忍睹。更糟糕的是，锁会引入死锁风险。所以几乎所有 GUI 框架都选择了同一种策略：UI 操作单线程化，跨线程通信通过消息传递。

这个设计决策意味着我们的后台线程**绝对不能**直接调用 Widget 的方法——比如直接设置 `label->setText("完成")`。它必须通过某种机制把"设置文本"这个意图传递给 GUI 线程，让 GUI 线程来执行。这个机制就是 `postTask`。

**第二个问题：共享状态 vs 消息传递。**

多线程编程有两种基本的通信模型：共享状态（shared state）和消息传递（message passing）。共享状态就是多个线程访问同一块内存，通过 mutex 保护；消息传递就是线程之间通过队列传递数据，每个线程只操作自己的数据。

对于我们的场景，消息传递是更好的选择。原因是：后台线程计算出一个结果（比如缩略图的像素数据），然后通过 `postTask` 把这个结果"投递"给 GUI 线程。GUI 线程收到之后，在自己的线程上下文里更新 Widget。整个过程中，数据的所有权在传递时是清晰的——后台线程把结果交给 GUI 线程之后，就不再碰它了。这就是 Actor 模型的核心思想：每个"actor"（线程）有自己的私有状态，actor 之间只通过消息通信，不共享可变状态。

**第三个问题：C++20 的 `std::jthread` 和合作式取消。**

C++20 引入了 `std::jthread`（joining thread），它跟 `std::thread` 的关键区别有两个：一是析构时自动 join（不用再忘 join 了），二是内置了 `std::stop_token` 的合作式取消机制。

合作式取消的意思是：你请求一个线程停止，但线程并不会被强制终止——它只是在 `stop_token` 上标记了一个"请停止"的信号，线程内部的代码需要**主动检查**这个信号并自行退出。这比 `TerminateThread`（Win32）安全得多，因为强制终止一个线程可能导致它持有的锁永远不会释放、堆上的内存永远不会释放、甚至进程状态被损坏。

想一下：我们的线程池在工作线程的循环里应该怎么配合 `stop_token`？如果某个 task 执行时间很长，它应该怎么响应取消请求？

**第四个问题：`postTask` 的实现。**

`postTask` 的本质是：后台线程把一个 `std::function<void()>` 投递到一个队列里，GUI 线程在每次事件循环迭代时从队列里取出来执行。这跟 Win32 的 `PostMessage` 在语义上完全对等——`PostMessage` 往线程的消息队列里投递一条消息，GUI 线程的 `GetMessage` 会取到这条消息并 dispatch。

想一下怎么实现这个队列：后台线程在写入、GUI 线程在读取——这是一个经典的生产者-消费者问题。你需要 `std::mutex` + `std::condition_variable` 来保护这个队列。但还有一个设计决策：GUI 线程什么时候去检查这个队列？是在每次 `xcb_wait_for_event` 之前？还是之后？还是两者之间？考虑一下如果队列里有任务但 X11 没有事件的时候，GUI 线程会怎么样。

### 设计方向提示

整体架构如下：

```
┌──────────────────────────────────────────────────────┐
│                     Application                       │
│                                                      │
│  ┌─────────────┐    postTask()    ┌────────────────┐ │
│  │  Event Loop  │ ◄────────────── │   Task Queue   │ │
│  │  (GUI 线程)  │                 │ (线程安全队列)  │ │
│  └──────┬──────┘                  └───────▲────────┘ │
│         │                                 │          │
│         │ runAsync()                      │          │
│         ▼                                 │          │
│  ┌──────────────────────────────────────┐ │          │
│  │           ThreadPool                 │ │          │
│  │  ┌────────┐ ┌────────┐ ┌────────┐   │ │          │
│  │  │Worker 0│ │Worker 1│ │Worker 2│   │ │          │
│  │  │ jthread│ │ jthread│ │ jthread│   │ │          │
│  │  └────────┘ └────────┘ └────────┘   │ │          │
│  │        ▲                              │ │          │
│  │        │ submit()                     │ │          │
│  │  ┌─────┴──────┐                      │ │          │
│  │  │  Task Queue │ (内部任务队列)        │ │          │
│  │  └────────────┘                      │ │          │
│  └──────────────────────────────────────┘ │          │
│                                           │          │
│  ┌───────────────────────────────────────┘          │
│  │  runAsync(work, onDone):                          │
│  │    1. submit(work) 到 ThreadPool                  │
│  │    2. 工作线程执行 work，得到 result               │
│  │    3. postTask([onDone, result]) 到 GUI 线程      │
│  └──────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────┘
```

#### ThreadPool 的设计

先看线程池本身。它的核心数据成员不多：

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交一个任务，返回 future 用于获取结果
    template<typename F>
    auto submit(F&& func) -> std::future<std::invoke_result_t<F>>;

    // 优雅关闭：等所有已提交的任务完成
    void shutdown();

private:
    void workerLoop(std::stop_token stop);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};
```

构造函数里创建 `numThreads` 个 `jthread`，每个线程执行 `workerLoop`。`workerLoop` 的逻辑是：加锁 → 等待队列非空（或收到停止信号）→ 取任务 → 解锁 → 执行任务。关键点在于等待条件要用 `stop_token` 感知：

```cpp
void ThreadPool::workerLoop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::function<void()> task;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this, &stop] {
                return stop_ || !tasks_.empty() || stop.stop_requested();
            });

            if (stop_ && tasks_.empty()) return;
            if (stop.stop_requested() && tasks_.empty()) return;

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();  // 在锁外执行，允许并发
    }
}
```

`submit` 的实现需要一点模板技巧——把用户传入的可调用对象包装成一个 `std::function<void()>`，同时返回一个 `std::future` 让调用方能拿到结果：

```cpp
template<typename F>
auto ThreadPool::submit(F&& func) -> std::future<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::forward<F>(func)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool: submit on stopped pool");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return result;
}
```

这里用 `std::packaged_task` 把可调用对象和 `std::promise` 打包在一起，`get_future()` 返回的 future 就是调用者等待结果用的句柄。`std::shared_ptr` 是因为同一个 task 对象需要被 `packaged_task` 持有、被 `std::function` 捕获、被 future 引用——用共享指针管理生命周期最方便。

`shutdown` 要做的事情是：设置 `stop_` 标志，唤醒所有等待中的工作线程，然后等待它们结束（`jthread` 的析构会自动 join，但显式 join 更清晰）：

```cpp
void ThreadPool::shutdown() {
    {
        std::unique_lock lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
        worker.request_stop();  // 通知 stop_token
    }
    workers_.clear();  // jthread 析构会 join
}
```

#### Application::runAsync 的设计

`Application` 持有一个 `ThreadPool` 实例，对外暴露 `runAsync`：

```cpp
class Application {
public:
    // ... 之前的接口 ...

    // 在后台线程执行 work，完成后在 GUI 线程调用 onDone
    template<typename Work, typename Callback>
    void runAsync(Work&& work, Callback&& onDone);

    // 将一个任务投递到 GUI 线程执行
    void postTask(std::function<void()> task);

    // 获取线程池（高级用法）
    ThreadPool& threadPool();

private:
    std::unique_ptr<ThreadPool> threadPool_;
    std::queue<std::function<void()>> guiTaskQueue_;
    std::mutex guiQueueMutex_;
};
```

`runAsync` 的实现逻辑：

```cpp
template<typename Work, typename Callback>
void Application::runAsync(Work&& work, Callback&& onDone) {
    using ResultType = std::invoke_result_t<Work>;

    threadPool_->submit([this,
                          work = std::forward<Work>(work),
                          onDone = std::forward<Callback>(onDone)]() mutable {
        // 这段代码在后台工作线程上执行
        if constexpr (std::is_void_v<ResultType>) {
            work();
            postTask([onDone = std::move(onDone)]() { onDone(); });
        } else {
            auto result = work();
            postTask([onDone = std::move(onDone),
                       result = std::move(result)]() mutable {
                onDone(std::move(result));
            });
        }
    });
}
```

注意这里的关键设计：`work` 在后台线程执行，但 `onDone` 通过 `postTask` 投递到 GUI 线程执行。这保证了 `onDone` 里可以安全地操作 Widget。

#### postTask 的实现

`postTask` 需要解决两个问题：线程安全的队列写入，以及如何唤醒 GUI 线程。

写入部分比较直观：

```cpp
void Application::postTask(std::function<void()> task) {
    {
        std::unique_lock lock(guiQueueMutex_);
        guiTaskQueue_.push(std::move(task));
    }
    // 唤醒 GUI 线程 —— 具体方式见下文
    wakeGuiThread();
}
```

唤醒 GUI 线程这部分需要想一下。问题在于 GUI 线程可能阻塞在 `xcb_wait_for_event` 上。XCB 提供了 `xcb_get_file_descriptor` 来获取连接的文件描述符，但最简单也最可靠的方法是利用 X11 的客户端消息（ClientMessage）。我们可以往自己的窗口发一个自定义的 X11 事件，这样 `xcb_wait_for_event` 就会返回，事件循环就能继续，从而有机会检查 `guiTaskQueue_`。

```cpp
void Application::wakeGuiThread() {
    // 向自身窗口发送一个唤醒事件
    xcb_client_message_event_t event;
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = mainWindow_;
    event.type = wakeAtom_;  // 预先注册的原子
    event.format = 32;
    event.data.data32[0] = WAKEUP_MESSAGE;

    xcb_send_event(connection_, false, mainWindow_,
                   XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char*>(&event));
    xcb_flush(connection_);
}
```

然后在事件循环里，处理完一个 XCB 事件之后，检查并清空 GUI 任务队列：

```cpp
int Application::run() {
    while (running_) {
        // 1. 先处理所有待执行的 GUI 任务
        drainGuiTaskQueue();

        // 2. 等待下一个 XCB 事件
        auto* event = xcb_wait_for_event(connection_);
        if (!event) break;

        // 3. 分发事件
        dispatchEvent(event);
        free(event);

        // 4. 事件处理完毕后再处理一次（可能有新任务）
        drainGuiTaskQueue();
    }
    return 0;
}

void Application::drainGuiTaskQueue() {
    std::queue<std::function<void()>> tasks;
    {
        std::unique_lock lock(guiQueueMutex_);
        tasks.swap(guiTaskQueue_);
    }
    while (!tasks.empty()) {
        tasks.front()();
        tasks.pop();
    }
}
```

用 `swap` 把队列一次性换出来，然后在不持锁的情况下逐个执行任务——这样可以最小化锁的持有时间，也避免了在持锁状态下执行任务可能导致的死锁（比如任务里又调用了 `postTask`）。

### 验证方法

写一个文件浏览器 + 缩略图的示例程序来验证整个并发体系。程序结构大致是这样的：

```
┌─────────────────────────────────────────┐
│  Mini File Browser                      │
│                                         │
│  [../]  [documents/]  [images/]         │
│                                         │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │ cat  │ │ dog  │ │tree  │ │ sky  │   │
│  │  🔄  │ │  ✅  │ │  🔄  │ │  ⏳  │   │
│  │      │ │      │ │      │ │      │   │
│  └──────┘ └──────┘ └──────┘ └──────┘   │
│                                         │
│  Status: 加载中... (2/4 缩略图完成)     │
└─────────────────────────────────────────┘

缩略图加载状态说明：
  🔄 = 正在后台加载
  ✅ = 加载完成，已显示
  ⏳ = 排队等待中
```

步骤：
1. 创建 Application 和一个显示网格缩略图的窗口
2. 用户打开一个目录后，列出所有图片文件
3. 对每个文件调用 `runAsync`：
   - `work`：读取图片文件、缩放到缩略图大小、生成像素数据
   - `onDone`：将缩略图数据传给对应的 Widget，触发重绘
4. 验证：缩略图一个一个出现，但窗口始终可以响应鼠标和键盘

具体验证点：
- 缩略图加载期间窗口不冻结（可以拖动、可以点击）
- 缩略图按完成顺序出现（先完成先显示，不是按文件顺序）
- 窗口关闭时没有崩溃或内存泄漏
- 反复切换目录不会出现缩略图错位（后发起的请求覆盖前一次的结果）

### 可能踩的坑

**坑 1：`postTask` 里捕获了悬空引用。**

这是最常见的 bug。`runAsync` 的 lambda 捕获了局部变量的引用，但 lambda 是异步执行的——等它执行的时候，引用的对象可能已经析构了。比如：

```cpp
// 错误！label 可能已经析构
void loadThumbnail(Label* label, const std::string& path) {
    app.runAsync(
        [path]() { return loadImage(path); },        // path 是 const&，可能悬空
        [label](Image img) { label->setImage(img); }  // label 可能已经销毁
    );
}

// 正确：按值捕获，或者使用 shared_ptr
void loadThumbnail(std::shared_ptr<Label> label, std::string path) {
    app.runAsync(
        [path = std::move(path)]() { return loadImage(path); },
        [label](Image img) { label->setImage(img); }
    );
}
```

**坑 2：忘记在 `onDone` 回调里处理 Widget 已被销毁的情况。**

用户可能在缩略图还没加载完的时候就关闭了窗口。此时 `onDone` 回调里引用的 Widget 已经不存在了，调用它的方法会崩溃。解决方案之一是在回调里检查 Widget 是否还有效（比如用 `weak_ptr`），或者在 `shutdown` 时清空队列。这个问题在真实应用中非常常见——Qt 的 `QPointer`、WinUI 的弱引用都是专门为了解决这个问题的。

**坑 3：`xcb_wait_for_event` 的阻塞导致 `postTask` 延迟。**

如果 GUI 线程阻塞在 `xcb_wait_for_event` 上，而 X11 没有任何事件进来，那 GUI 线程就不会去检查 `guiTaskQueue_`，`postTask` 投递的任务就会一直等。这就是为什么 `wakeGuiThread()` 需要发送一个 X11 ClientMessage 事件——让 `xcb_wait_for_event` 返回，事件循环得以继续。

如果忘记实现 `wakeGuiThread`，症状是：缩略图加载完成了（后台线程的日志显示已完成），但界面上迟迟不更新，直到你动一下鼠标或者碰一下窗口（因为这会产生 X11 事件，事件循环得以继续）。

**坑 4：线程池的 `stop_` 标志不是原子变量但被多线程访问。**

在我们的设计中 `stop_` 被保护在 `mutex_` 里面，所以没问题。但如果你在某个地方不加锁就访问了 `stop_`，就会产生数据竞争——这是未定义行为。一个常见的错误是在 `shutdown` 里直接写 `stop_ = true` 然后才加锁 `notify_all`。正确做法是在锁的保护下设置 `stop_`：

```cpp
void shutdown() {
    {
        std::unique_lock lock(mutex_);  // 先加锁
        stop_ = true;                    // 在锁内设置
    }                                    // 释放锁
    cv_.notify_all();                    // 然后唤醒
}
```

**坑 5：`std::jthread` 析构时的 join vs detach。**

`jthread` 在析构时会自动 join（等待线程结束）。这意味着如果 `ThreadPool` 的析构函数不先调用 `shutdown`，程序会在线程池的循环里永远等待——因为工作线程在 `cv_.wait` 里等着任务，没有人通知它停止，`jthread` 的析构就永远不会完成。所以 `ThreadPool` 的析构函数**必须**调用 `shutdown()`：

```cpp
ThreadPool::~ThreadPool() {
    if (!stop_) {
        shutdown();
    }
}
```

### 如果卡住了

**如果不确定线程池怎么写**，先从最简单的版本开始：一个工作线程 + 一个任务队列。把 `workerLoop` 写成 `while (running) { lock; wait; get task; unlock; execute task; }`。跑通之后再扩展到多线程。核心逻辑是一样的，多线程只是多几个 `jthread` 共享同一个队列。

**如果在 `postTask` 的唤醒机制上卡住了**，有一个更简单的替代方案（但不够优雅）：把 `xcb_wait_for_event` 替换为 `xcb_poll_for_event` + `std::this_thread::sleep_for` 的轮询循环。这样 GUI 线程就不会无限阻塞，可以周期性地检查 `guiTaskQueue_`。缺点是会引入一点延迟（取决于 sleep 的时间），而且空转会浪费一点 CPU，但对于教学框架来说完全可以接受。等理解了唤醒机制之后再升级为 ClientMessage 方案。

**如果对 `std::stop_token` 的用法不熟悉**，可以先用一个简单的 `std::atomic<bool>` 替代。`stop_token` 的优势是能跟 `jthread` 的 `request_stop()` 配合，但核心逻辑跟检查一个布尔标志没有本质区别。先用简单方案跑通，再替换为 `stop_token`。

**如果 Win32 的类比能帮助你理解**，可以这样映射：

| MiniUI 概念 | Win32 对应物 | 说明 |
|---|---|---|
| `ThreadPool::submit()` | `CreateThreadpoolWork()` + `SubmitThreadpoolWork()` | 提交后台任务 |
| `Application::postTask()` | `PostMessage(hwnd, WM_USER+n, ...)` | 向 GUI 线程投递任务 |
| `Application::runAsync(work, onDone)` | 工作线程做完后 `PostMessage` 回主线程 | 完整的异步模式 |
| `std::jthread` + `std::stop_token` | `CreateThread()` + 事件对象 / `TerminateThread()` | 合作式取消 |
| `drainGuiTaskQueue()` | `GetMessage` 循环中处理 `WM_USER+n` | GUI 线程消费投递的任务 |

Win32 的 `PostMessage` 往消息队列投递一条消息，GUI 线程的 `GetMessage` 取到这条消息后 dispatch 给 `WndProc` 处理——这就是 Win32 版本的 `postTask`。我们的 `guiTaskQueue_` 就是一个自定义的"消息队列"，`drainGuiTaskQueue` 就是"处理投递消息"的步骤。

**如果 `std::packaged_task` 和 `std::future` 的组合让你头疼**，可以先不返回 future，把 `submit` 简化为 `void submit(std::function<void()> task)`。结果通过 `postTask` 回传，不需要 future。跑通之后再包装 future 版本。实际上对于 `runAsync` 这个接口来说，future 并不是必须的——因为结果已经通过 `onDone` 回调传递了。future 的价值在于更灵活的使用方式（比如等待多个任务全部完成）。

**如果对 Actor 模型感兴趣想深入了解**，可以想一想我们的 `runAsync` + `postTask` 其实就是一个简化版的 Actor 模型：GUI 线程是一个 actor，后台线程是另一个 actor，它们之间通过 `guiTaskQueue_` 传递消息。真正的 Actor 模型（比如 Erlang 或 Akka 的实现）在此基础上还加了容错（supervisor）、位置透明（actor 可能在不同机器上）等能力，但核心思想是一样的——不共享可变状态，只通过消息通信。

### Win32 横向对比：完整示例

让我们把整个异步流程在 Win32 和 MiniUI 之间做一个完整的对比，加深理解：

**Win32 版本（工作线程 → PostMessage → WndProc）：**

```cpp
// 1. 定义自定义消息
#define WM_THUMBNAIL_READY (WM_USER + 100)

// 2. 工作线程函数
DWORD WINAPI LoadThumbnailThread(LPVOID param) {
    auto* info = static_cast<ThumbnailInfo*>(param);
    HBITMAP thumbnail = GenerateThumbnail(info->filePath);
    // 通过 PostMessage 把结果投递到 GUI 线程
    PostMessage(info->hWnd, WM_THUMBNAIL_READY,
                (WPARAM)thumbnail, (LPARAM)info->itemId);
    delete info;
    return 0;
}

// 3. WndProc 里处理结果
case WM_THUMBNAIL_READY: {
    HBITMAP hBmp = (HBITMAP)wParam;
    int itemId = (int)lParam;
    UpdateThumbnail(itemId, hBmp);  // 安全：在 GUI 线程上执行
    break;
}

// 4. 发起异步加载
void RequestThumbnail(HWND hWnd, const std::string& path, int itemId) {
    auto* info = new ThumbnailInfo{hWnd, path, itemId};
    CreateThread(nullptr, 0, LoadThumbnailThread, info, 0, nullptr);
}
```

**MiniUI 版本（runAsync → postTask → event loop）：**

```cpp
// 一行搞定，不需要定义自定义消息、不需要手动管理线程
void requestThumbnail(std::shared_ptr<ThumbnailWidget> widget,
                      std::string path) {
    app().runAsync(
        [path = std::move(path)]() -> ImageData {
            return generateThumbnail(path);   // 后台线程
        },
        [widget](ImageData data) {
            widget->setImage(std::move(data)); // GUI 线程
            widget->repaint();
        }
    );
}
```

MiniUI 的版本明显更简洁，因为 `runAsync` 把"提交后台任务"和"投递结果回 GUI 线程"这两步封装成了一个原子操作。但底层做的事情是完全一样的——后台线程做计算，做完之后通过某种队列机制把结果传递给 GUI 线程。

---

## 下一步

线程池到位了，我们的 MiniUI 框架在工程层面上已经相当完整了——从底层的 XCB 连接和事件循环，到控件树、布局引擎、信号系统、响应式数据流，再到现在的并发支持，一个教学级 GUI 框架应有的骨架已经全部搭建完毕。

下一个阶段是整个系列的收官彩蛋——我们会把目光从 Linux/XCB 转回 Windows，看看如何用现代 C++ 封装 Win32 API，把我们在 Phase 1 里写的那些过程式的 Win32 代码重构成一套类型安全、RAII 管理的 C++ 封装层。这个阶段会把整个系列串起来：你在 MiniUI 里理解的控件树、布局、事件分发、信号机制，在 Win32 的封装层里都能找到对应物。从"理解 GUI 框架怎么工作"到"能用现代 C++ 写出工业级的 Win32 程序"，这就是最后一步。
