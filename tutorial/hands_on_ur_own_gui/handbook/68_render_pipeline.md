# Stage 7 — 渲染管线与异步 IO：双缓冲、脏区管理与不冻结的 GUI

---

## 这一阶段的目标

前六个阶段我们把 MiniUI 的骨架搭完了——事件循环、Cairo 绘图、控件树、布局引擎、信号系统，该有的都有了。但如果你在 Stage 5 的布局引擎里放上一百个控件然后疯狂调整窗口大小，你会发现两件事：画面在闪烁，而且窗口偶尔会"卡住"——鼠标点下去没反应，过一会突然又恢复了。

这个阶段我们要解决这两个问题。闪烁的根源是我们一直在直接对屏幕 DC 做绘制，需要引入双缓冲；卡住的根源是事件循环被阻塞了——如果你在某个事件回调里同步读了一个 10MB 的 PNG 文件，整个 GUI 就冻住了，因为事件循环在等 IO 完成，没办法处理用户的鼠标和键盘事件。

所以这个阶段有两块内容：**渲染管线优化**（双缓冲 + 脏区管理）和**异步 IO**（非阻塞文件读取 + poll 多路复用）。两块看起来不相关，但它们最终会在同一个地方汇合——Application::run() 会被重写成一个基于 `poll()` 的事件循环，同时监听 XCB 连接的 fd 和自定义的 IO 完成 fd，在 ~60 FPS 的帧率下驱动整个框架。

### 为什么全窗口重绘是浪费

在前面几个阶段里，每次收到 `XCB_EXPOSE` 事件，我们就对整个窗口从头到尾重新画一遍。如果你的窗口是 1920x1080，那每一帧都要处理两百多万个像素。但实际情况是，大部分时候窗口里只有一个很小的区域发生了变化——比如鼠标悬停时一个按钮的背景色变了，或者一个 Label 的文本更新了。为了改几个像素而重绘整个窗口，就像为了修一个水龙头而把整栋楼的水管全换了一样。

在 Win32 里，这个问题的标准解决方案是 `InvalidateRect` + `WM_PAINT` 机制：你不需要每次都重绘整个窗口，而是告诉系统"这个矩形区域变脏了"，系统在合适的时机给你发一个 `WM_PAINT`，通过 `PAINTSTRUCT::rcPaint` 告诉你哪些区域需要重绘，你只画那些区域就行了。这个机制在 Phase 1 的 GDI 双缓冲章节里我们已经见过（交叉引用第 23 章）。

### 为什么双缓冲能消除闪烁

如果你在 XCB 里直接对窗口 surface 做 Cairo 绘制，用户会看到绘制过程的中间状态——先看到背景被清空（变白），然后看到控件一个一个冒出来，最后才是完整的画面。如果绘制速度不够快，或者帧率不够稳定，这个"撕裂"就会表现为闪烁。

双缓冲的思路我们在 Win32 GDI 里已经实践过了（交叉引用第 23 章）：先在一个离屏的内存 surface 上画完所有内容，然后一次性把结果拷贝到屏幕上。用户永远只看到最终结果，看不到中间过程。在 Win32 里我们用 `CreateCompatibleDC` + `CreateCompatibleBitmap` + `BitBlt` 三件套来实现；在 XCB + Cairo 里，对应的是创建一个 `cairo_image_surface_t` 作为后端缓冲区，在上面完成所有绘制，最后用 `cairo_set_source_surface` + `cairo_paint` 一次性提交。

### 为什么 GUI 会冻结——事件循环被阻塞

GUI 程序的心脏是事件循环。我们的 `Application::run()` 本质上是 `while(running) { event = wait(); dispatch(event); }`。这意味着所有用户交互（鼠标移动、键盘输入、窗口尺寸变化）都是在 `dispatch` 这一步被处理的。如果某个事件回调里做了一件耗时的事情——比如同步读取一个大文件、做一个复杂的计算——那事件循环就卡在那个回调里出不来，后面所有的事件都排队等着。

想象一下：用户点了一个"打开文件"按钮，你的 `onClick` 回调里调用 `fread` 去读一个 10MB 的 PNG。磁盘 IO 可能需要几十毫秒甚至几百毫秒，这段时间里你的事件循环完全停滞。用户会看到窗口标题栏变成"未响应"，鼠标变成转圈圈。在 Win32 里你一定见过这个现象——"程序未响应"的本质就是消息循环卡住了。

解决方案是异步 IO：把耗时操作扔到另一个线程里去执行，等它完成了再把结果通过某种机制投递回事件循环。在 Win32 里这个模式是 `ReadFileEx` + 完成回调，或者 `OVERLAPPED` IO + `GetQueuedCompletionStatus`；在我们的 MiniUI 里，我们会实现一个简单的 `AsyncFile` 工具类加上 `Application::postTask()` 方法。

### 需要想清楚的问题

**关于脏区管理**：当一个控件调用 `repaint()` 的时候，你需要知道这个控件在屏幕上的哪个区域。想一想，控件的绝对位置信息在布局阶段（Stage 5）已经算好了，那脏区应该用什么数据结构来记录？如果同一帧内多个控件都请求重绘，脏区应该合并还是分开？合并的好处是什么？不合并的好处又是什么？如果脏区之间有大量重叠，合并能节省多少绘制工作？

**关于双缓冲的生命周期**：后端缓冲区是一个跟窗口同等大小的 `cairo_image_surface_t`。想一想，它应该在哪里创建？在 `Application` 里还是在一个独立的 `DoubleBuffer` 类里？窗口大小变化的时候它需要重新创建，这个重新创建的时机是什么——收到 `XCB_CONFIGURE_NOTIFY` 事件时？还是在下一次绘制时检查尺寸？

**关于 poll 和事件循环**：XCB 的事件是通过文件描述符传递的——`xcb_get_file_descriptor(connection)` 返回的是一个 fd，当这个 fd 可读时说明有 XCB 事件到达。想一想，如果我们用 `poll()` 同时监听这个 fd 和其他自定义 fd（比如管道的读端），是不是就能在一个循环里同时处理 XCB 事件和异步 IO 完成通知？这跟 Win32 的 `MsgWaitForMultipleObjectsEx` 是不是同一个思路？

**关于线程安全**：`postTask()` 是从工作线程调用的，它往事件循环的任务队列里塞一个 `std::function`。事件循环在主线程里从这个队列取任务来执行。想一想，这个队列需要什么同步机制？`std::mutex` + `std::condition_variable` 能不能搞定？有没有更轻量的方案（比如 `eventfd` + `poll`）？

**关于帧率控制**：如果我们用 `poll()` 并设一个 16ms 的超时（约 60 FPS），那在没有事件也没有任务的时候，循环会每 16ms 醒来一次。想一想，这个"空闲唤醒"应该做什么？是直接跳过还是执行一些低优先级的任务？如果连续多帧都没有脏区，这个空转的开销能不能接受？

### 设计方向提示

#### DoubleBuffer 类

这是一个独立的类，不继承 Widget，不参与控件树。它唯一的职责是管理后端缓冲区和前后端之间的拷贝。

```cpp
class DoubleBuffer {
public:
    // 构造时创建与窗口等大的后端缓冲区
    DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                 uint16_t width, uint16_t height);

    // 开始绘制：传入脏区列表，返回一个绑定到后端缓冲区的 Painter
    // Painter 的裁剪区域会被设置为脏区的并集
    Painter beginPaint(const std::vector<Rect>& dirtyRegions);

    // 结束绘制：把后端缓冲区的脏区部分拷贝到前端（屏幕）
    void endPaint();

    // 窗口大小变化时调用，重建后端缓冲区
    void resize(uint16_t width, uint16_t height);

private:
    xcb_connection_t* conn_;
    xcb_window_t window_;
    uint16_t width_, height_;
    cairo_surface_t* backSurface_;   // 后端缓冲区
    cairo_surface_t* frontSurface_;  // 前端（窗口 surface）
    std::vector<Rect> currentDirty_; // 当前帧的脏区
};
```

`beginPaint` 返回的 Painter 绑定到 `backSurface_`，控件的绘制操作都作用在这个后端缓冲区上。`endPaint` 把脏区对应的像素从 `backSurface_` 拷贝到 `frontSurface_`。

#### 脏区管理

在 Widget 里加两个方法：

```cpp
class Widget {
public:
    // 标记整个控件区域为脏，触发重绘
    void repaint();

    // 标记控件为需要更新，但延迟到下一帧再合并脏区
    void update();

    // ...
};
```

`repaint()` 和 `update()` 的区别就跟 Win32 里 `InvalidateRect(hwnd, NULL, FALSE)` 和 Qt 里 `QWidget::update()` 的区别一样——前者立即请求重绘，后者把请求缓存起来等到下一帧合并处理。在我们的实现里，`update()` 只是往 Application 的脏区列表里添加当前控件的全局边界矩形；`repaint()` 是调用 `update()` 之后强制唤醒事件循环（通过往管道写一个字节）。

脏区的合并策略可以简单一些——把所有脏区收集到一个 `std::vector<Rect>` 里，在渲染帧开始时做一次合并。最简单的合并方式是取所有脏区的包围盒（bounding box），虽然可能多画了一些区域，但实现起来简单可靠。一个脏区的示意图大概是这样的：

```
┌──────────────────────────────────┐
│  Window (1920x1080)              │
│                                  │
│    ┌────────┐                    │
│    │ dirty 1│      ┌───────────┐ │
│    │ Button │      │ dirty 2   │ │
│    └────────┘      │  Label    │ │
│                    └───────────┘ │
│  ┌───────────────────────────┐   │
│  │    bounding box (union)   │   │
│  │  ┌────────┐   ┌────────┐ │   │
│  │  │dirty 1 │   │dirty 2 │ │   │
│  │  └────────┘   └────────┘ │   │
│  └───────────────────────────┘   │
│                                  │
└──────────────────────────────────┘
  → 只重绘 bounding box 内的区域
```

#### Application::run() 的进化

从 Stage 2 到现在，我们的 `run()` 一直是 `xcb_wait_for_event` 的阻塞循环。现在它要进化成一个基于 `poll()` 的多路复用循环：

```cpp
int Application::run() {
    while (running_) {
        // 1. 构造 pollfd 数组
        struct pollfd fds[2];
        fds[0].fd = xcb_get_file_descriptor(conn_);
        fds[0].events = POLLIN;
        fds[1].fd = taskPipeRead_;   // 自管理管道的读端
        fds[1].events = POLLIN;

        // 2. 等待事件，超时 16ms (~60 FPS)
        int ret = poll(fds, 2, 16);

        // 3. 处理 XCB 事件（非阻塞）
        while (auto* event = xcb_poll_for_event(conn_)) {
            dispatchEvent(event);
            free(event);
        }

        // 4. 处理异步任务队列
        processTaskQueue();

        // 5. 如果有脏区，执行渲染
        if (!dirtyRegions_.empty()) {
            auto painter = doubleBuffer_.beginPaint(dirtyRegions_);
            renderAll(painter);  // 遍历控件树绘制
            doubleBuffer_.endPaint();
            dirtyRegions_.clear();
        }
    }
    return 0;
}
```

关键的变化是 `xcb_wait_for_event` 变成了 `xcb_poll_for_event`——前者阻塞直到有事件，后者立即返回（可能返回 `nullptr`）。这样做是因为 `poll()` 已经帮我们做了等待，如果 XCB fd 可读，说明有事件；如果不可读，`xcb_poll_for_event` 返回 `nullptr`，我们直接跳过。

#### AsyncFile 异步文件读取

```cpp
class AsyncFile {
public:
    using ReadCallback = std::function<void(std::vector<uint8_t> data)>;

    // 异步读取整个文件：启动工作线程，读完后通过 postTask 回调
    static void readAll(Application& app,
                        const std::string& path,
                        ReadCallback callback);
};
```

实现思路：

1. `readAll` 启动一个 `std::thread`
2. 在线程里用标准文件 IO 读取文件内容到 `std::vector<uint8_t>`
3. 读取完成后，调用 `app.postTask([callback, data = std::move(data)]() { callback(std::move(data)); })`
4. `postTask` 把回调塞进任务队列，往管道写一个字节唤醒 `poll()`
5. 主线程在 `processTaskQueue()` 里取出回调并执行

#### Application::postTask()

```cpp
class Application {
public:
    // 线程安全的任务投递：从任何线程调用，把 task 排入队列
    // 主线程在下一轮事件循环中执行 task
    void postTask(std::function<void()> task);

private:
    std::mutex taskMutex_;
    std::deque<std::function<void()>> taskQueue_;
    int taskPipeWrite_;  // 管道写端
    int taskPipeRead_;   // 管道读端

    void processTaskQueue() {
        std::deque<std::function<void()>> tasks;
        {
            std::lock_guard lock(taskMutex_);
            tasks = std::move(taskQueue_);
        }
        for (auto& task : tasks) {
            task();
        }
    }
};
```

`postTask` 的实现非常经典——互斥锁保护队列，管道通知唤醒。为什么不直接用 `condition_variable`？因为 `condition_variable` 没法跟 `poll()` 一起用——`poll` 等的是 fd，`condition_variable` 等的是信号量。管道（或者 `eventfd`）是连接两者的桥梁：`postTask` 往管道写一个字节，`poll` 监听管道的读端，一写就读到，事件循环就被唤醒了。

### 验证方法

**验证双缓冲**：写一个包含多个控件的窗口（至少 5 个 Button + 1 个 Label），用一个定时器每 100ms 随机改变某个 Button 的背景色。如果双缓冲工作正常，你应该看不到任何闪烁——画面更新是瞬间完成的。如果双缓冲没生效，你会看到控件在重绘时有明显的白闪。

**验证脏区管理**：在 `beginPaint` 里打印传入的脏区矩形。当只有一个小按钮更新时，脏区应该只包含那个按钮的区域，而不是整个窗口。如果你发现脏区一直是整个窗口大小，说明脏区合并逻辑有问题，或者 `update()` 没有正确计算控件的绝对位置。

**验证异步 IO**：在某个按钮的回调里调用 `AsyncFile::readAll` 读取一个大文件（可以 `dd if=/dev/urandom of=test.dat bs=1M count=50` 生成一个 50MB 的测试文件）。在等待读取完成的过程中，窗口应该保持完全响应——你可以继续拖动窗口、点击其他按钮、调整大小。如果窗口冻结了，说明你的异步实现有问题——可能在回调里做了阻塞操作，或者 `postTask` 没有正确唤醒事件循环。

**验证帧率**：在 `run()` 的循环里加一个帧计数器，每秒打印一次帧数。如果实现正确，空闲时应该约 60 FPS（受 `poll` 的 16ms 超时限制），有大量事件和绘制任务时帧率可能下降但不应该低于 30 FPS。

### 可能踩的坑

**Cairo surface 尺寸不匹配**：双缓冲的后端 surface 大小必须跟窗口大小完全匹配。如果窗口在两次 `beginPaint` 之间被 resize 了，后端 surface 还是旧的尺寸，绘制坐标就会错位，或者 `cairo_paint` 时访问越界。解决方法是在 `beginPaint` 开头检查当前窗口尺寸是否跟 surface 匹配，不匹配就先调用 `resize()`。

**管道死锁**：`postTask` 里的管道写入需要注意一种边界情况——如果管道缓冲区满了（写入速度远快于读取速度），`write()` 会阻塞。在实践中这几乎不会发生，因为管道缓冲区通常是 64KB，每个通知只写 1 字节。但如果你在短时间内 `postTask` 了上万次，理论上可能触发。一个防御性的做法是用 `write(fd, "x", 1)` 忽略返回值，或者在管道读端一次性把缓冲区读空。

**xcb_poll_for_event 的循环**：在 `run()` 里我们用一个 `while` 循环把所有待处理的 XCB 事件都取出来再进入下一轮 `poll`。如果你只取一个就跳出去 `poll`，可能会造成事件积压——X Server 一次性发来 10 个事件，但你每帧只处理 1 个，剩下的都在队列里等着，交互响应就会变慢。

**Worker thread 生命周期**：`AsyncFile::readAll` 启动的线程需要被 `detach` 或者被某个线程池管理。如果用 `detach`，要确保回调 lambda 捕获的对象在回调被执行时还活着。特别是如果你捕获了 `this`（比如控件指针），而这个控件可能在 IO 完成之前就被销毁了——这是一个经典的 use-after-free 陷阱。一个更安全的做法是在回调里先检查对象是否还有效（比如用 `weak_ptr` 或者一个布尔标志位）。

**xcb_flush 的时机**：在 `endPaint` 里把后端缓冲区拷贝到窗口 surface 之后，你需要调用 `xcb_flush(conn_)` 确保 X Server 看到更新。这个调用很容易忘，忘了的症状就是画面更新延迟——你可能画了，但 X Server 还没处理。

### 如果卡住了

**如果你在双缓冲的 Cairo surface 创建上卡住了**：关键的 API 是 `cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height)`，它创建一个纯内存的 image surface。绘制完成后，你需要一个绑定到窗口的 XCB surface（`cairo_xcb_surface_create`），然后把 image surface 作为源，`cairo_set_source_surface` + `cairo_paint` 一次性拷过去。如果不确定怎么做，翻回 Stage 3 的 Cairo 绘图章节，回顾 surface 和 context 的关系。

**如果你在 poll 多路复用上卡住了**：先忘掉 MiniUI，写一个最小的测试程序——创建一个管道，用 `poll()` 同时监听 stdin 和管道的读端。往管道里写东西，观察 `poll` 是否正确返回。确认 `poll` 的基本用法没问题之后，再把 XCB fd 加进来。`xcb_get_file_descriptor()` 就是那个把 XCB 和 `poll` 连接起来的桥梁。

**如果你在 postTask 的线程同步上卡住了**：最简单的实现方案——一个 `std::mutex` 保护 `std::deque<std::function<void()>>`，写入端加锁插入并写管道通知，读取端加锁取出整个队列然后释放锁，在锁外逐个执行任务。这个方案在性能上不是最优的，但对教学级框架来说足够了。如果你想要更高效，可以研究 `eventfd` 替代管道、或者无锁队列，但这些优化在这个阶段不是必需的。

**Win32 程序员的参照系**：如果你在某个概念上纠结，可以对照 Win32 里的等价物来理解：

| MiniUI | Win32 等价 |
|--------|-----------|
| `poll()` + `xcb_fd` + `pipe_fd` | `MsgWaitForMultipleObjectsEx()` |
| `postTask()` | `PostMessage(hwnd, WM_USER+N, ...)` |
| `AsyncFile` | `OVERLAPPED` + `ReadFileEx()` |
| `DoubleBuffer` | `CreateCompatibleDC` + `BitBlt` |
| `repaint()` / `update()` | `InvalidateRect()` |
| `beginPaint(dirtyRegions)` | `BeginPaint()` + `ps.rcPaint` |
| `xcb_poll_for_event` | `PeekMessage()` |
| `processTaskQueue()` | 处理 `WM_USER+N` 消息 |

`MsgWaitForMultipleObjectsEx` 能同时等 Windows 消息和内核对象（事件、管道、完成端口），我们的 `poll()` + XCB fd + pipe fd 做的是一模一样的事——在同一个阻塞调用里同时等待多种事件源。

---

## 下一步

双缓冲和异步 IO 都搞定了，我们的 MiniUI 框架在功能上已经相当完整——控件树、布局、信号、渲染管线、异步加载，一个 GUI 框架该有的核心部件基本齐了。下一阶段（Stage 8）是收官——我们要用整个框架拼出一个迷你文本编辑器，同时引入 C++20 协程（coroutine）来替代手动回调链，让异步代码从"回调地狱"进化成看起来像同步的线性代码。那个阶段是对整个系列的集成考验，也是最有成就感的一步。
