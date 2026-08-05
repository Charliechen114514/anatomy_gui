# 器官二·消息循环与协程——把消息泵变成多源调度器

> 器官一线索（窗口与显示面）那一篇我们拿到了一个能上屏的 HWND，但说实话，只要消息循环还停在 `while (GetMessage(...))` 的原始形态，这具骨架就只是个会喘气的标本。这一篇我们把那台古董泵爆改一遍：让它能同时盯着原生消息、投递任务、协程恢复、线程池完成和句柄等待，再亲手把 C++20 协程焊到这台泵上——证明一个协程真的能在 UI 线程上挂起、跑去后台、又安安稳稳地回 UI 线程恢复。

---

## 前言：为什么我们不能继续用 GetMessage

如果只是写个"点一下按钮弹个框"的小程序，`GetMessage` + `DispatchMessage` 那套三件套完全够用，前一个器官我们也是这么起步的。但你只要往下走两步就会撞墙：你想在 UI 线程空闲时跑点后台计算，又不想阻塞刷新；你想接 C++20 协程，让一个协程 `co_await` 一下切到线程池，干完活再 `co_await` 回 UI 线程更新界面；你想等一个 `HANDLE`（比如一个进程退出、一个文件 IO 完成）的同时，还能继续响应鼠标键盘——这些需求 `GetMessage` 一个都满足不了，因为 `GetMessage` 本质上只认识"窗口消息"这一种唤醒源，它会被阻塞在一个维度里。

我们现在的视角要换一下。消息循环本质不是一个"取消息的循环"，它是一个**多源调度器**——谁能让这个线程醒来，谁就是它的输入源。原生窗口消息只是其中一种输入源，和它并列的还有：我们手动投递进来的任务（`postTask`）、被挂起的协程要恢复了、线程池的 APC 完成例程要回流了、某个内核句柄 signal 了。这五个源全都要 multiplex（多路复用）在同一个等待原语上，而这个原语就是 `MsgWaitForMultipleObjectsEx`。把它想成 `select`/`poll`，只不过它监听的不是 socket，而是"线程能不能继续往下走"的所有触发条件。一旦你接受了这个视角，后面所有的设计就都是顺水推舟了。

协程这边同理。C++20 协程的恢复点，只由"哪个线程调了 `handle.resume()`"这一个事实决定——这是个非常干净的语义，干净到我们可以靠它把协程精准地钉在某条线程上恢复。这一篇要证明的，就是把这两件事缝起来：协程挂起时把恢复动作投回消息循环，循环被 `MsgWaitForMultipleObjectsEx` 唤醒后再去 `resume`，于是协程的恢复就和原生消息、投递任务一样，成了消息泵的一个一等输入源。

---

## 环境说明（实验记录，不是规格表）

笔者这次的实验台是这样的：Windows 11（10.0.26200，最新 Insider 推送），Visual Studio 2026，工具链是 MSVC 14.5x（PlatformToolset `v145`），编译选项 `/std:c++latest /utf-8`，字符集 `UNICODE`，目标平台 x64，Windows SDK 用最新的 10.0。协程用的是标准的 `<coroutine>` 头（C++20 起就稳定了，2026 的工具链完全不需要再折腾 `/await` 或 `<experimental/coroutine>` 那些老黄历），所以我们直接放开了用 `std::coroutine_handle`、`std::suspend_always`、`std::noop_coroutine`。

工程结构沿用本系列统一约定：一个 `modern_windows.slnx` 解决方案（不用 CMake，CMake + MSVC 那套笔者真的折腾够了），底下装一个共享静态库 `anatomy_core`——所有"器官"代码集中在这里，每个阶段往上加，能 header-only 的就 header-only——再加每个阶段一个独立的 exe 项目，通过 VS 的项目引用（Project Reference）去吃 `anatomy_core`，`main.cpp` 只放该阶段的薄入口。这样后阶段直接复用前阶段的器官，不用复制粘贴，又全程纯 VS、零 CMake。已经存在的 `00_bootstrap/`（一个 Hello World + 自己的 `.slnx`）我们一行不动，定位就是"工具链自检"，用来确认 VS + Windows SDK 能不能正常产出一个桌面 exe。

本阶段在磁盘上长这样：

```
src/tutorial/modern_windows/
├─ modern_windows.slnx              ← 总解决方案
├─ anatomy_core/                    ← 共享静态库（本阶段开始有内容）
│  ├─ anatomy_core.h                ← task<T>、postTask、ui_thread/background_thread
│  └─ anatomy_core.vcxproj
├─ stage02_message_loop_coro/       ← 本阶段 exe
│  ├─ main.cpp                      ← 薄入口 + 消息循环
│  └─ stage02_message_loop_coro.vcxproj
└─ 00_bootstrap/                    ← 工具链自检（已有，不动）
```

`modern_windows.slnx` 的内容很薄，列出两个项目即可：

```xml
<Solution>
  <Configurations>
    <Platform Name="x64" />
  </Configurations>
  <Project Path="anatomy_core/anatomy_core.vcxproj" />
  <Project Path="stage02_message_loop_coro/stage02_message_loop_coro.vcxproj" />
</Solution>
```

`stage02` 的 `.vcxproj` 里关键是加一条对 `anatomy_core` 的项目引用，这样链接期自动吃上 `anatomy_core.lib`，不用手填附加依赖项：

```xml
<ItemGroup>
  <ProjectReference Include="..\anatomy_core\anatomy_core.vcxproj">
    <Project>{anatomy-core-guid}</Project>
  </ProjectReference>
</ItemGroup>
```

约定好了，下面开始爆改。

---

## 第一步——把 GetMessage 换成"PeekMessage 排空 + MsgWaitForMultipleObjectsEx 等待"

我们现在要做的是把循环拆成两个清晰的阶段：一个"排空"阶段，用 `PeekMessage(PM_REMOVE)` 把队列里现有的消息一股脑取出来分发掉；一个"等待"阶段，用 `MsgWaitForMultipleObjectsEx` 把线程挂起，直到任意一个唤醒源就绪。为什么要拆成两段而不是继续用阻塞的 `GetMessage`？因为 `GetMessage` 只会因窗口消息醒来，它根本不知道你还有任务队列、还有句柄要等——你不可能在 `GetMessage` 阻塞期间让它顺便盯一个 `HANDLE`。拆开之后，"分发"和"等待"各司其职，等待阶段就可以同时盯消息队列 + 一组句柄 + APC，这才叫多源。

先看最小骨架，后面每一步都在它上面长肉：

```cpp
// stage02_message_loop_coro/main.cpp（片段，逐步填充）
#include <windows.h>
#include <cstdio>

int run_message_loop()
{
    for (;;) {
        // —— 阶段一：排空 ——
        // 用 PeekMessage 把队列里已有的消息全部取出来，
        // 取到队列空为止。PM_REMOVE 表示取走就删。
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return static_cast<int>(msg.wParam);   // 退出码
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // —— 阶段二：等待多源唤醒 ——
        // 此时所有原生消息和派发都已处理完，线程可以放心睡。
        // 谁先就绪谁唤醒它：可能是新输入、可能是 posted 消息、
        // 可能是某个 HANDLE signal、也可能是线程池 APC。
        MsgWaitForMultipleObjectsEx(
            /*cHandles    */ 0,
            /*pHandles    */ nullptr,
            /*dwMillisec  */ INFINITE,
            /*dwWakeMask  */ QS_ALLINPUT,
            /*dwFlags     */ MWMO_INPUTAVAILABLE | MWMO_ALERTABLE);
        // 返回（不管因为谁）后，循环回到阶段一继续排空。
    }
}
```

这里每一个 flag 都不是随手抄的，我们先说 `QS_ALLINPUT`：它是"任何输入类型都唤醒我"的总开关，包含了 `QS_POSTMESSAGE`（投递消息，后面我们的唤醒窗就走这条路）、`QS_KEY`/`QS_MOUSE`（键鼠原始输入）、`QS_SENDMESSAGE`（别的线程 `SendMessage` 过来）等等。换句话说，只要线程的消息队列有任何风吹草动，等待立刻返回。

事情到这里还没完，真正坑人的是两个 `dwFlags`，我们必须逐个掰开。

**`MWMO_INPUTAVAILABLE`：修"seen-input 竞态"。** 这是 Raymond Chen 在 2005 年那篇 [MsgWaitForMultipleObjects and the queue state](https://devblogs.microsoft.com/oldnewthing/20050217-00/?p=36423) 里反复强调的雷。底层机制是这样的：Windows 给每个线程的消息队列维护一个"输入状态"，`PeekMessage` 看一眼队列，系统就把"输入已看见"这个状态位置上；而 `MsgWaitForMultipleObjectsEx` 在**没有** `MWMO_INPUTAVAILABLE` 的情况下，只对"等待开始之后新到的输入"有反应——也就是说，如果在你 `PeekMessage` 排空和发起等待之间，恰好又有新输入溜进来了，系统认为这批输入"已经被 PeekMessage 瞄过了"，于是等待**不返回**，直接阻塞。这是一个只在负载下才暴露的漏唤醒 bug：你空载跑半天都好好的，一上量就偶发卡顿。加上 `MWMO_INPUTAVAILABLE` 之后，函数语义变成"队列里只要还有没处理完的输入，立即返回"，竞态窗口就被堵上了。笔者在这里血压拉满过——千万别省这个 flag。

**`MWMO_ALERTABLE`：让线程池 APC 回流。** 它把等待置于"可警告（alertable）"状态，这样线程池通过 `QueueUserAPC` / 完成例程派过来的 APC 就能在等待期间执行。我们这一阶段用 `std::thread` 演示跨线程恢复，下一阶段接真正的系统线程池时，`MWMO_ALERTABLE` 就是任务完成回调回流进 UI 线程循环的命门。先开着，不要等用到再补——回头补很容易忘。

⚠️ 这一步最常见的错误是有人图省事把 `cHandles` 传 0 时直接放弃调用 `MsgWaitForMultipleObjectsEx`，改回 `WaitMessage()`。`WaitMessage` 确实只等消息，但它既不能加 `MWMO_INPUTAVAILABLE`、也不能 alertable、更不能盯 `HANDLE`——你等于把刚搭好的多源泵又降级回了单源。先别急，老老实实 `MsgWaitForMultipleObjectsEx`。

这一步完成后，我们的循环已经是个合格的多源泵了——只是它现在还没接到"投递任务"和"协程恢复"，所以下一件事就是给泵装一个任务入口。

---

## 第二步——建一个 message-only 唤醒窗 + WM_POSTED_TASK

接下来问题来了：任务从别的线程（或协程）要怎么"叫醒" UI 线程、并且让恢复发生在一个标准的、能被嵌套泵泵送的位置上？答案是用 `PostMessage` 投到一个我们专门创建的窗口上。这个窗口必须存在——`PostMessage` 要的是 `HWND`，不是线程 ID。

但这个窗口绝不能是可见窗口，否则用户屏幕上莫名其妙多出来一个空白窗。我们要的是 **message-only 窗口**：它在窗口管理器里登记了，能收 `PostMessage`，有 `WndProc`，但永远不可见、不参与 z-order、不会被枚举出来。`CreateWindowExW` 的 `hWndParent` 参数传 `HWND_MESSAGE` 就能拿到一个 message-only 窗口，这是 Win32 给我们准备好的"内部信箱"。

我们先注册一个窗口类，再建这个唤醒窗：

```cpp
// anatomy_core/anatomy_core.h（片段）
#include <windows.h>

// 唤醒消息号：在 WM_APP 之上，避免和系统/控件保留区冲突。
inline constexpr UINT WM_POSTED_TASK = WM_APP + 1;

// 全局（仅本阶段演示用；后续器官会把它收进一个 Looper 类里）。
inline HWND        g_wakeHwnd   = nullptr;
inline DWORD       g_uiThreadId = 0;
```

```cpp
// main.cpp（片段）

// 唤醒窗的窗口过程：收到唤醒消息就去抽干任务队列。
LRESULT CALLBACK wake_wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_POSTED_TASK) {
        drain_task_queue();   // 第三步实现
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void register_wake_class()
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &wake_wndproc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AnatomyWakeWindow";
    RegisterClassExW(&wc);
}

HWND create_message_only_window()
{
    // HWND_MESSAGE 作为父窗口 → 这是一个 message-only 窗口。
    // 它不可见、不接收任何输入焦点，但能正常收 PostMessage。
    return CreateWindowExW(
        /*dwExStyle   */ 0,
        /*lpClassName */ L"AnatomyWakeWindow",
        /*lpWindowName*/ L"",
        /*dwStyle     */ 0,
        /*X,Y,W,H     */ 0, 0, 0, 0,
        /*hWndParent  */ HWND_MESSAGE,   // ← 关键
        /*hMenu       */ nullptr,
        /*hInstance   */ GetModuleHandleW(nullptr),
        /*lpParam     */ nullptr);
}
```

你可能会问：为什么不直接用 `PostThreadMessage` 往 UI 线程 ID 上投？它连建窗口都省了，看起来更轻。千万别。`PostThreadMessage` 投出去的消息 `msg.hwnd == NULL`，而 `IsDialogMessage`、`TranslateAccelerator`、以及任何走 COM/模态对话框的嵌套消息泵，碰到 `hwnd == NULL` 的消息会**静默吞掉**它。也就是说，你的协程平时恢复得好好的，一旦用户打开了文件对话框（那就是一个嵌套模态循环），`PostThreadMessage` 的恢复消息直接消失，协程永远挂起。这种"平时没事、一开模态就死"的 bug 是最难查的。

而 HWND 路由的 `PostMessage` 走的是标准分发路径：它进入消息队列后，会被当前线程上**任何一个**标准泵（包括嵌套模态循环、COM 模态泵）正常 `PeekMessage`/`GetMessage` 出来，再 `DispatchMessage` 送到我们的 `wake_wndproc`。不管用户当前在不在文件对话框里，恢复都不会丢。这就是为什么 Chromium、CppWinRT 这些工业级代码统统走 HWND 唤醒，而不是 `PostThreadMessage`。我们回头看这个决策，会发现它和"协程恢复是一等输入源"是同一个东西：输入源必须能被任何合法的泵泵送，否则就不够"一等"。

这一步验证成功的标准很简单：跑起来屏幕上没有任何新窗口出现，但 `wake_wndproc` 能收到 `PostMessage(g_wakeHwnd, WM_POSTED_TASK, ...)`。我们下一步就把任务队列和它接上。

---

## 第三步——接上 postTask：一个投递任务就是一个"没有返回值的协程恢复"

有了唤醒窗，任务队列就非常好做。我们维护一个加锁的任务队列，`postTask` 把任务塞进队列、再 `PostMessage` 一下唤醒窗；`drain_task_queue` 在 `WM_POSTED_TASK` 处理时把队列一次性抽干。这里有一个小巧思：如果连续多次 `postTask`，多个 `WM_POSTED_TASK` 会合并在消息队列里（posted message 同号合并），但任务都在我们自己的队列里一个不丢，`drain_task_queue` 一把抽完——所以我们既不会漏任务，也不会被重复唤醒浪费。

```cpp
// anatomy_core/anatomy_core.h（片段）
#include <functional>
#include <mutex>
#include <queue>
#include <utility>

inline std::mutex                             g_taskMutex;
inline std::queue<std::function<void()>>      g_taskQueue;

// 投一个任务到 UI 线程。可从任意线程调用。
inline void post_task(std::function<void()> task)
{
    {
        std::lock_guard lk(g_taskMutex);
        g_taskQueue.push(std::move(task));
    }
    // 唤醒消息窗。PostMessage 跨线程安全，且会触发 MsgWaitForMultipleObjectsEx
    // 的 QS_POSTMESSAGE 分支返回（QS_ALLINPUT 已经把它包进去了）。
    PostMessageW(g_wakeHwnd, WM_POSTED_TASK, 0, 0);
}

// 在 UI 线程（wake_wndproc 里）调用：把队列抽干。
inline void drain_task_queue()
{
    for (;;) {
        std::function<void()> task;
        {
            std::lock_guard lk(g_taskMutex);
            if (g_taskQueue.empty()) break;
            task = std::move(g_taskQueue.front());
            g_taskQueue.pop();
        }
        task();   // 锁外执行，避免长任务阻塞生产者
    }
}
```

注意我们把 `task()` 放在锁外执行，原因是显然的：如果拿着锁跑任务，而任务里又调了 `postTask`，就是稳稳的死锁；即便不考虑递归，长任务也会把所有跨线程投递都卡住。锁只护住队列结构本身，执行阶段必须脱锁。

你会发现，这个 `post_task` 几乎就是"协程恢复"的等价物。事实上，第五步写完 awaiter 你会看到，`ui_thread` 的 `await_suspend` 里干的事就是 `post_task([h]{ h.resume(); })`——一个投递任务，本质上就是一个"没有返回值的协程恢复"。这就是为什么本篇要把它们统一在同一个队列上：投递任务和协程恢复共享同一条高速公路，没有第二套机制，复杂度立刻下来了。

⚠️ 别忘了检查 `PostMessageW` 的返回值。它**会失败**：比如消息队列已经超配额（默认 10000 条 posted message），或者程序正在关机、窗口已经在销毁途中。这一步在 demo 阶段我们先放过去，但真正的坑在后面——关机安全那一节我们会专门回来处理"PostMessage 失败了，那个待恢复的协程 handle 怎么办"。

到这里，泵的"投递任务"输入源已经通了。我们回头看，现在的循环已经支持：原生消息（`QS_ALLINPUT`）、投递任务（`WM_POSTED_TASK` + 队列）、APC（`MWMO_ALERTABLE`）。差的就剩协程这一路了。很好，现在进入这一篇的核心。

---

## 第四步——造一个最小可用的 lazy `task<T>`：promise_type + 对称转移 + 异常搬运

C++20 协程最难懂的，是它的"控制权交给谁"那套机制。我们不去抄文档，而是边写边讲。一个协程返回类型（这里就叫 `task<T>`）必须内嵌一个 `promise_type`，编译器靠它来知道"协程启动时挂不挂起、结束时挂不挂起、`co_return` 的值往哪放、抛异常了怎么办"。你可以把 `promise_type` 理解成协程帧（coroutine frame）的"控制面板"——协程每到一个关键点（启动、结束、返回值、异常），编译器都会去拨这个面板上的某个开关。

我们先写主模板 `task<T>`，把它写透了，再补一个 `task<void>` 的特化。代码长，我们按逻辑功能拆开逐段讲。

### 4.1 类骨架与成员

```cpp
// anatomy_core/anatomy_core.h（片段）
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

template <typename T>
class [[nodiscard]] task
{
public:
    struct promise_type;                       // 见 4.2
    using handle = std::coroutine_handle<promise_type>;

    task() noexcept = default;
    explicit task(handle h) noexcept : coro_(h) {}
    task(task&& o) noexcept : coro_(std::exchange(o.coro_, {})) {}
    task(task const&)            = delete;
    task& operator=(task const&) = delete;
    task& operator=(task&&)      = delete;

    // task 持有协程帧的所有权：析构时如果协程还活着，就销毁帧。
    ~task() { if (coro_) coro_.destroy(); }

    // —— 作为 awaitable 的接口（4.4）——
    bool await_ready() const noexcept;
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept;
    T    await_resume();

    // 由拥有者显式启动（demo 里 main 用）。
    void start_on_ui() noexcept;

private:
    handle coro_;
};
```

几个设计决策值得讲一下。第一，`[[nodiscard]]` 是故意的：协程是 lazy 的（马上会看到 `initial_suspend` 返回 `std::suspend_always`），你拿到一个 `task` 却不 `co_await` 它、也不 `start` 它，那协程体**永远不会执行**，编译期我们用 `[[nodiscard]]` 把这种"造了就扔"的失误在第一时间暴露出来。第二，删掉了拷贝和 move-assign：协程帧是单一份的，所有权要清晰——一个帧只能被一个 `task` 管，析构即销毁，避免你后面踩 Qt 那种 shared 子节点引发的 double-free 隐患（这个我们在下一个器官"所有权"会重点解）。第三，析构函数销毁协程帧，这意味着 `task` 是 RAII 的，谁持有 `task` 谁就得保证协程帧的生命周期。

### 4.2 promise_type：lazy 启动 + 对称转移结尾 + 异常搬运

```cpp
template <typename T>
struct task<T>::promise_type
{
    task get_return_object() noexcept
    {
        return task{handle::from_promise(*this)};
    }

    // 协程一创建就挂起，体里一行都不跑——这是 "lazy task"。
    // 原因：我们要精确控制"协程在哪条线程上开始执行"，
    // 所以必须等拥有者显式 resume 它，再进入函数体。
    std::suspend_always initial_suspend() noexcept { return {}; }

    // 协程结束时的挂起点：用对称转移把控制权交给"等我的那个 continuation"。
    struct final_awaiter
    {
        bool await_ready() const noexcept { return false; }

        // 返回 handle 而不是 void → 触发 symmetric transfer：
        // 编译器做尾调用恢复 continuation，不再多压一层栈帧。
        std::coroutine_handle<> await_suspend(handle h) noexcept
        {
            auto& p = h.promise();
            // 有人等我，就无缝切给它；没人等我（顶层 task），就回 noop。
            if (p.continuation)
                return p.continuation;
            return std::noop_coroutine();
        }
        void await_resume() const noexcept {}
    };
    final_awaiter final_suspend() noexcept { return {}; }

    // co_return 的值搬到这里。
    void return_value(T v) noexcept { value = std::move(v); }

    // 协程抛异常：把异常指针存起来，等 await_resume 时再 rethrow。
    void unhandled_exception() { ex = std::current_exception(); }

    std::optional<T>       value;
    std::exception_ptr     ex;
    std::coroutine_handle<> continuation;   // 谁在等我（4.4 里设置）
};
```

这里每一段都值得停一下。

`initial_suspend` 返回 `std::suspend_always`，这是 lazy 的命门。如果它返回 `std::suspend_never`，协程在被创建的那一刻就会一路狂奔到第一个真正的挂起点，我们根本没机会决定"它在哪个线程跑"。改成 `suspend_always` 之后，协程创建出来只是个挂着的帧，谁第一次 `resume()` 它，函数体才从第一行开始跑——而谁 resume 它，当然是拥有者在合适的时机（比如通过 `post_task` 把 resume 投到 UI 线程）做的。这就是 lazy task 比 eager task 安全的根本原因。

`final_suspend` 这里我们**没有**用 `std::suspend_always`，而是用了一个自定义 `final_awaiter`，它的 `await_suspend` 返回一个 `std::coroutine_handle<>`。当 `await_suspend` 返回 handle 而不是 `void` 时，编译器会**尾调用**去恢复那个 handle——这叫**对称转移（symmetric transfer）**，是 Lewis Baker 在 [C++ Coroutines: Understanding symmetric transfer](https://lewissbaker.github.io/2020/05/14/understanding_symmetric_transfer) 里讲透的模式。它解决的是一个很现实的问题：如果协程 A 等 B、B 等 C、C 等 D……用传统的"`await_suspend` 里直接 `caller.resume()`"（非对称），每一层都会在调用栈上叠一帧，链够长就 stack overflow；用对称转移，恢复是尾调用，**借用当前栈帧**直接跳过去，栈深度不增长。我们这条链 `driver → compute_answer → (background_thread) → (ui_thread)` 看着不长，但既然这是个"器官"，就要按能 scale 的方式写。

⚠️ 但有一点必须诚实交代：对称转移保证的是"这一跳"是 O(1) 栈，它能不能真的优化成不增栈，是**编译器的 QoI（quality of implementation）**，标准并不强制。MSVC、Clang、GCC 现在都能做这个尾调用优化，但你不能写代码**依赖**"对称转移一定不增栈"去构造无限深递归然后假设它没事——这一条是 Lewis Baker 在协程标准库议题（C++ standard coroutines issue，即著名的 "issue #6"）里反复强调的口径：它属于 QoI 而非保证。把它当"在主流编译器上几乎总是 O(1)"的优化来用，但留一份清醒。

`unhandled_exception` 把 `std::current_exception()` 存成 `std::exception_ptr`，**不立刻 rethrow**——因为协程一旦恢复执行权的对象可能根本没准备好处理异常，立刻抛会让异常在错误的线程/错误的时机飞出来。我们把它压成指针存着，等下游 `await_resume` 时，在一个可控的、拥有者期望的位置再 `std::rethrow_exception`。这就是协程异常安全的标准做法。

### 4.3 把 task 自己变成 awaitable

`task<T>` 既能当返回类型，又能被另一个协程 `co_await`。后者的三个钩子直接挂在 `task` 上：

```cpp
template <typename T>
bool task<T>::await_ready() const noexcept
{
    // 永远走 suspend 路径：我们要用对称转移启动子协程。
    return false;
}

template <typename T>
std::coroutine_handle<> task<T>::await_suspend(std::coroutine_handle<> caller) noexcept
{
    // 记下"谁在等我"，等结束时 final_suspend 把控制权交还给它。
    coro_.promise().continuation = caller;
    // 对称转移：直接跳进子协程，不回 caller 的 resume()。
    return coro_;
}

template <typename T>
T task<T>::await_resume()
{
    // 协程已结束（final_suspend 挂住了）。把异常或值搬出去。
    if (coro_.promise().ex)
        std::rethrow_exception(coro_.promise().ex);
    return std::move(*coro_.promise().value);
}
```

`await_suspend` 返回 `coro_`（子协程的 handle）这一行，是"父子协程对称转移"的核心：调用者 `co_await child` 时，编译器把"恢复子协程"做成了一个**对子协程的尾调用**，而不是"先返回调用者、调用者再去 resume 子"。这样调用栈深度在父子转移这一跳上是常数。子协程跑完，它的 `final_suspend` 又对称转移到我们这里记下的 `continuation`，无缝把控制权还给调用者。整条链一气呵成，没有一层多余的栈。

### 4.4 启动顶层 task

最顶层的那个 `task`（没人 `co_await` 它）需要一个入口被首次 resume。我们让拥有者在 UI 线程上启动它：

```cpp
template <typename T>
void task<T>::start_on_ui() noexcept
{
    // continuation 留空 → final_suspend 会回 noop_coroutine，顶层正常落地。
    post_task([this] { coro_.resume(); });
}
```

为什么是 `post_task` 而不是直接 `coro_.resume()`？因为 main 在建完唤醒窗之后，下一步就要进入消息循环——如果直接 inline resume，协程可能在消息循环还没跑起来时就一路挂起、又一路恢复，时序乱套。把它投成任务，意味着"协程的第一行也走标准消息泵派发"，和其他任务、其他恢复在同一个调度模型下，干净。这也是后面"绝不内联恢复"那条铁律的预演。

### 4.5 `task<void>` 特化

`void` 版本几乎一样，只是 `return_value` 换成 `return_void`、`await_resume` 不返回值。我们直接给完整版，不省：

```cpp
template <>
class [[nodiscard]] task<void>
{
public:
    struct promise_type
    {
        task<void> get_return_object() noexcept
        { return task<void>{handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle h) noexcept
            {
                auto& p = h.promise();
                return p.continuation ? p.continuation
                                      : std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }

        void return_void() noexcept {}              // ← 与主模板唯一的不同
        void unhandled_exception() { ex = std::current_exception(); }

        std::exception_ptr      ex;
        std::coroutine_handle<> continuation;
    };
    using handle = std::coroutine_handle<promise_type>;

    task() noexcept = default;
    explicit task(handle h) noexcept : coro_(h) {}
    task(task&& o) noexcept : coro_(std::exchange(o.coro_, {})) {}
    task(task const&)            = delete;
    task& operator=(task const&) = delete;
    task& operator=(task&&)      = delete;
    ~task() { if (coro_) coro_.destroy(); }

    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept
    {
        coro_.promise().continuation = caller;
        return coro_;
    }
    void await_resume()
    {
        if (coro_.promise().ex)
            std::rethrow_exception(coro_.promise().ex);
    }

    void start_on_ui() noexcept
    {
        post_task([this] { coro_.resume(); });
    }

private:
    handle coro_;
};
```

很好，现在 `task<T>` 和 `task<void>` 都齐了。`anatomy_core` 里"器官级"的协程原语已经落地。我们回头看——这套 `task` 的特点是：lazy（不显式启动不动）、对称转移（栈友好）、异常搬运（不乱抛）、`[[nodiscard]]`（防丢）、RAII（析构销毁帧）。这些不是装饰，是后续每个器官用到协程时的安全底线。

---

## 第五步——`ui_thread{}` / `background_thread{}` 两个 awaitable

有了 `task`，我们现在要造两个最常用的"切线程"原语。它们的形态非常简单——各是一个 awaitable，关键全在 `await_suspend` 里"把恢复动作派发到哪条线程"。这一节你一定要盯紧 `await_suspend`：协程恢复点的线程归属，完全由"`await_suspend` 里调用了谁的 `h.resume()`"决定，没有任何魔法。

### 5.1 `background_thread{}`：挂起，去别的线程跑

```cpp
// anatomy_core/anatomy_core.h（片段）
#include <thread>
#include <chrono>

struct background_thread_awaiter
{
    bool await_ready() const noexcept { return false; }

    // 协程在这行挂起。我们把 resume 派到一个全新线程上执行，
    // 于是恢复点（co_await 之后的代码）就跑在那个新线程里。
    void await_suspend(std::coroutine_handle<> h) const
    {
        // demo 用 std::thread 直接演示"恢复点随 resume() 走"这件事。
        // 生产环境应换成系统线程池（TrySubmitThreadpoolCallback），
        // 那时它的完成 APC 会靠 MWMO_ALERTABLE 回流——见第五步末尾。
        std::thread([h] {
            h.resume();
        }).detach();
    }

    void await_resume() const noexcept {}
};

inline constexpr background_thread_awaiter background_thread{};
```

这一段的精髓是"`detach()` 之后，那个新线程在某个时刻调用了 `h.resume()`，于是协程从 `co_await background_thread{}` 之后的下一行继续执行，只不过这时它已经不在 UI 线程上了"。我们用最笨的 `std::thread` 来演示，是为了让你看清机制——恢复点没有任何花哨，纯粹是"谁调 resume，谁就是恢复线程"。生产里你不会无限制地开裸线程，而是把这个 `h.resume()` 提交给系统线程池（`TrySubmitThreadpoolCallback`），线程池干活的时候 UI 线程在 `MsgWaitForMultipleObjectsEx` 里睡觉；活干完，恢复回 UI 线程要靠下面这个 `ui_thread{}`。而 `MWMO_ALERTABLE` 的价值，就在于让线程池的 APC/完成例程有机会在 UI 线程等待期间回流，这是下一阶段把"裸 std::thread"换成"系统线程池 + APC 完成回流"时的关键接口——先开着，不亏。

### 5.2 `ui_thread{}`：挂起，把恢复钉回 UI 线程

```cpp
struct ui_thread_awaiter
{
    // 小优化：如果已经在 UI 线程，就不用再绕一圈 post_task。
    // ⚠️ 注意 await_ready 返回 true 的副作用，见下面灰条。
    bool await_ready() const noexcept
    {
        return GetCurrentThreadId() == g_uiThreadId;
    }

    void await_suspend(std::coroutine_handle<> h) const
    {
        // 关键一步：把恢复投成 UI 线程上的一个任务。
        // 于是恢复点（co_await 之后的代码）回到 UI 线程执行。
        post_task([h] { h.resume(); });
    }

    void await_resume() const noexcept {}
};

inline constexpr ui_thread_awaiter ui_thread{};
```

`ui_thread{}` 是这一篇的灵魂。它的 `await_suspend` 里那一行 `post_task([h]{ h.resume(); })`，把一个**已经被挂起的协程**的恢复动作，变成了第四步定义的"一个投递任务"——而这个任务会走唤醒窗、走消息泵、在 UI 线程的 `drain_task_queue` 里被 `resume`。这就是"协程恢复是一等输入源"的物理实现：恢复动作和 `PostMessage` 进来的原生消息、和别的 `postTask` 任务，走的是**同一条**高速公路。

⚠️ 这里的 `await_ready` 用了一个"已在 UI 线程就跳过"的快路径优化。它符合标准语义（`await_ready` 返回 `true` 就跳过 `await_suspend`，直接进 `await_resume`），但 MSVC 有一个已知的 codegen bug（DevCom 11119394）：在某些版本/某些 awaiter 形态下，`await_ready` 返回 `true` 时编译器对 `await_suspend` 的处理可能不符合预期。所以一条保守的工程纪律是——**永远不要让你的 awaiter 把"必须执行的副作用"放在 `await_suspend` 里、同时又让 `await_ready` 有机会返回 `true`**。我们这里的副作用（`post_task`）是"为了切线程"，本来只在需要切时才该执行，跳过它是正确的；但如果你写 awaiter 时把"记账""锁释放"这种必须执行的动作放在 `await_suspend`，又用 `await_ready==true` 短路，那就可能在某些 MSVC 版本上踩坑。拿不准的时候，最稳的就是让 `await_ready` 永远返回 `false`，哪怕多绕一次 `post_task`。笔者在自己工程里默认就关掉了这个快路径。

### 5.3 题外话，但很重要：`address()` / `from_address()`——把 handle 当 `void*` 投过 `PostMessage`

如果你去看 CppWinRT 或本系列基线里描述的那个最朴素的"协程恢复"写法，它其实不是 `post_task(lambda)`，而是直接把 `coroutine_handle` 转成一个裸指针、塞进 `PostMessage` 的参数里投过去。`std::coroutine_handle` 提供了这对函数：`address()` 返回一个 `void*`，`from_address(void*)` 把它还原回 handle。为什么要提供这对？因为 `coroutine_handle` 本身不是一个可移植的、能跨 ABI 边界传的东西，而 `void*` 谁都认——`PostMessage` 的 `WPARAM`/`LPARAM` 就是按指针宽度传的，刚好。所以最原始的恢复路径长这样：

```cpp
// "教科书"式恢复路径：handle → void* → PostMessage → from_address → resume
constexpr UINT WM_RESUME_TASK = WM_APP + 2;   // 专门给协程恢复用的消息号

struct ui_thread_raw_awaiter
{
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const noexcept
    {
        // 把 handle 的地址塞进 wParam，投给唤醒窗。
        // 注意 PostMessage 的返回值：关机时会失败，handle 会成孤魂
        // （见下一节"关机安全"——这是为什么我们最后选了 post_task 统一路径）。
        PostMessageW(g_wakeHwnd, WM_RESUME_TASK,
                     reinterpret_cast<WPARAM>(h.address()), 0);
    }
    void await_resume() const noexcept {}
};

// 在 wake_wndproc 里：
//   case WM_RESUME_TASK: {
//       auto h = std::coroutine_handle<>::from_address(
//                    reinterpret_cast<void*>(wParam));
//       if (h && !h.done()) h.resume();
//       return 0;
//   }
```

这条路是对的，也是 CppWinRT `resume_foreground` 内部用的同款思路。但我们在本篇最终选了 `post_task(std::function<void()>)` 这条统一路径，原因有两个，值得说清楚。第一，`address()/from_address()` 这条路只能传"恢复某个 handle"这一件事，而我们的泵要支持任意投递任务（不只是协程恢复）——如果恢复走 `WM_RESUME_TASK`、任务走 `WM_POSTED_TASK`，就等于维护两条并行的输入通道，复杂度翻倍。统一成 `post_task` 之后，协程恢复只是 `post_task([h]{h.resume();})` 的一个特例，一条通道、一套 `drain_task_queue`、一套关机收尾，干净得多。第二，`std::function` 版本让 handle 的生命周期被 lambda 捕获按值持有，**类型系统帮我们守住了"handle 没有被裸指针化、不会在传输途中丢失类型信息"**；而 `address()` 把它降级成 `void*` 之后，所有安全性都要靠人脑和注释来守，编译器帮不上忙。

所以我们这里的取舍是：**理解 `address()/from_address()` 这条最底层的传输机制（它解释了协程恢复为什么能走 PostMessage），但工程上把它包成 `post_task`，让协程恢复和普通任务共用一条高速公路。** 这也是为什么前面我们反复说"一个投递任务就是一个没有返回值的协程恢复"——那句话的物理含义，就是 `post_task([h]{h.resume();})` 这一行。

---

## 第六步——上号：写 demo 协程，跑通"挂起 → 跨线程 → 恢复"

代码凑齐了，现在上板测试。我们写两个协程：一个 `compute_answer()`（`task<int>`）演示"切后台干活、再切回 UI"；一个顶层 `driver()`（`task<void>`）`co_await` 它，演示父子协程的对称转移，拿到结果后 `PostQuitMessage` 让循环退出。`main` 里建唤醒窗、启动 driver、跑消息循环。

```cpp
// stage02_message_loop_coro/main.cpp
#include <windows.h>
#include <coroutine>
#include <cstdio>
#include <thread>
#include <chrono>

#include "anatomy_core.h"   // task<T>/task<void>、post_task、ui_thread、background_thread

// —— 调试输出：把当前线程 id 打出来，这是本篇验证的关键证据 ——
static void dbg(const char* tag, const char* msg)
{
    std::printf("[tid=%lu] %-7s %s", static_cast<unsigned long>(GetCurrentThreadId()), tag, msg);
    std::fflush(stdout);
}

// —— 子协程：在 UI 启动，切到后台干重活，再切回 UI 把结果带回来 ——
task<int> compute_answer()
{
    dbg("UI  ", "compute_answer: 进入，即将 co_await background_thread\n");
    co_await background_thread{};                 // ← 挂起，恢复点跑在后台线程

    dbg("BG  ", "compute_answer: 后台线程开始干活（模拟 100ms 计算）\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int answer = 6 * 7;

    co_await ui_thread{};                         // ← 挂起，恢复点钉回 UI 线程
    dbg("UI  ", "compute_answer: 回到 UI 线程，准备 co_return\n");

    co_return answer;                             // → final_suspend 对称转移回 driver
}

// —— 顶层协程：co_await 子协程，演示 task-to-task 对称转移 ——
task<void> driver()
{
    dbg("UI  ", "driver: 启动，即将 co_await compute_answer()\n");
    int r = co_await compute_answer();            // ← 对称转移进子协程
    dbg("UI  ", "driver: 拿到结果，准备退出消息循环\n");
    std::printf("[tid=%lu] %-7s driver: 结果 = %d\n",
                static_cast<unsigned long>(GetCurrentThreadId()), "UI  ", r);
    PostQuitMessage(0);                           // 让 run_message_loop 返回
    co_return;
}

int main()
{
    g_uiThreadId = GetCurrentThreadId();          // 记下 UI 线程 id，给 ui_thread{} 用

    register_wake_class();
    g_wakeHwnd = create_message_only_window();
    if (!g_wakeHwnd) {
        std::fprintf(stderr, "create_message_only_window failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        return 1;
    }
    dbg("UI  ", "main: 唤醒窗已创建，进入消息循环\n");

    auto d = driver();                            // lazy：协程已构造，但一行都没跑
    d.start_on_ui();                              // 把首次 resume 投到 UI 线程

    int code = run_message_loop();                // 见第一步
    // d 在此析构：协程已 final_suspend，~task 销毁帧。
    dbg("UI  ", "main: 消息循环退出\n");
    return code;
}
```

我们顺着时序走一遍，这是理解整篇的关键。`driver` 被 `start_on_ui` 投成一个任务，消息循环第一次排空后 `drain_task_queue` 里 `d.resume()`，`driver` 在 UI 线程开始跑；它 `co_await compute_answer()`，对称转移进 `compute_answer`，仍在 UI 线程；`compute_answer` 打印第一行，遇到 `co_await background_thread{}`，`background_thread_awaiter::await_suspend` 起一个新线程、把 `h.resume()` 派过去，`compute_answer` 挂起，控制权返回 `drain_task_queue`，任务队列空了，循环进入 `MsgWaitForMultipleObjectsEx` 睡觉。这时后台线程被调度到，调用 `h.resume()`，`compute_answer` 在**后台线程**上继续，打印"BG 干活"，睡 100ms，算出 42；接着遇到 `co_await ui_thread{}`，`ui_thread_awaiter::await_suspend` 调 `post_task([h]{h.resume();})`——这一步把恢复动作投回 UI 线程——`compute_answer` 再次挂起，后台线程 lambda 返回、线程退出。UI 线程这边被 `PostMessage` 唤醒，排空时 `drain_task_queue` 执行那条 `h.resume()`，`compute_answer` 回到 UI 线程继续，打印"回到 UI"，`co_return 42` 触发 `final_suspend`，对称转移回 `driver` 的 `continuation`；`driver` 在 UI 线程拿到 42，打印结果，`PostQuitMessage(0)`，`co_return`，顶层 `final_suspend` 落到 `noop_coroutine`。下次排空时 `PeekMessage` 取到 `WM_QUIT`，循环返回 0，`d` 析构销毁协程帧，程序干净退出。

笔者的实际运行输出（tid 每次会变，关键是 UI 行和 BG 行的 tid 必须不同）：

```
[tid=4892] UI     main: 唤醒窗已创建，进入消息循环
[tid=4892] UI     driver: 启动，即将 co_await compute_answer()
[tid=4892] UI     compute_answer: 进入，即将 co_await background_thread
[tid=12044] BG    compute_answer: 后台线程开始干活（模拟 100ms 计算）
[tid=4892] UI     compute_answer: 回到 UI 线程，准备 co_return
[tid=4892] UI     driver: 拿到结果，准备退出消息循环
[tid=4892] UI     driver: 结果 = 42
[tid=4892] UI     main: 消息循环退出
```

你看，UI 行的 tid 全程是 4892，BG 行是 12044，两个不同的线程。这行输出就是本篇想证明的全部：协程在 UI 线程上挂起、跑到后台线程执行了一段、又安安稳稳地回到了 UI 线程恢复——而且整条链跑在消息循环这个调度器之上，没有任何一根裸的阻塞线程参与 UI 刷新。

这里先验证一下几个可能的出错点。如果你看到 BG 行和 UI 行 tid 一样，多半是 `background_thread_awaiter` 里 `h.resume()` 没真正被 `std::thread` 包起来（比如漏写 `std::thread(...)` 直接同步调了）；如果协程跑到一半彻底不动了、UI 也没响应，很可能是 `post_task` 里的 `PostMessageW` 投到了一个还没建好的 `g_wakeHwnd`（顺序错了，先建窗再启动协程）；如果一开文件对话框协程就再也不恢复——那就是你偷懒用了 `PostThreadMessage`，回头把第五步关于 HWND 路由那一段重读一遍。

---

## 真正的坑在后面——reentrancy、关机安全，和那些连 CppWinRT 都没兜住的事

跑通了 demo 别急着庆祝，因为器官级代码要面对的场景比 demo 残酷得多。这一节我们把几个能让你直接起不来的 footgun 摊开讲。

**第一个，reentrancy（重入）。** 这是协程 + 消息循环组合里最阴的雷。绝对不要在 `await_suspend` 里**同步内联**调 `h.resume()`——也就是那种 `await_suspend(h){ h.resume(); }` 的写法。它会发生什么？你的协程在某个事件处理函数的中途挂起、立刻又在同一栈上恢复，等于在事件处理过程中重入了一段全新的消息循环逻辑，UI 状态机瞬间错乱，重则崩溃、轻则数据结构在错误假设下被改坏。铁律只有一条：**永远 suspend + 重新派发，绝不内联恢复**。我们的 `ui_thread` 走 `post_task`、`background_thread` 走 `std::thread`，都是在"先把协程真正挂起、再把 resume 派到别处"——派发动作本身可能立刻发生，但 resume 已经换了一个干净的栈上下文。工程里你甚至可以加一条 debug 断言：在 `await_suspend` 入口 assert "当前不是某个事件 handler 的中途"，防手滑。

**第二个，关机安全——这是连 CppWinRT 都翻过车的地方。** Raymond Chen 在 2025 年 7 月连写了两篇（[What happens if C++/WinRT is unable to resume execution on a dispatcher](https://devblogs.microsoft.com/oldnewthing/20250721-00/?p=111396) 和次日 [Being more adamant about reporting that C++/WinRT was unable to resume](https://devblogs.microsoft.com/oldnewthing/20250722-00/?p=111400)）专门讲这件事：在关机/Dispatcher 销毁途中，`resume_foreground` 没法把协程恢复到目标 dispatcher，结果协程就那么挂着、永远不 resume，等于泄漏了一个协程帧；更糟的是它可能在错误的线程上被恢复。根因是没人去检查 `resume_foreground` 返回的那个 `bool`——失败被静默吞掉了。

这件事对我们自造的 `ui_thread{}` 直接适用。我们的 `post_task` 里那个 `PostMessageW`，**在关机路径上会失败**（窗口已在销毁、队列已关）。一旦它失败，而我们又没处理，那个被投递的 `coroutine_handle` 就和它的协程帧一起成了孤魂——要么泄漏、要么在窗口销毁后被一个不存在的 `wake_wndproc` 恢复、要么 `task` 析构时炸。所以关机安全要做到这几件事，缺一不可。

第一，`PostMessageW` 的返回值必须检查，失败时把 handle 的命运明确化。要么记日志 + 销毁帧（如果业务允许丢这次恢复），要么走降级路径（比如同步在当前线程 resume，但要清醒地知道这是 reentrancy 风险）。绝不能静默丢——这正是 CppWinRT 那两篇的教训。一个 demo 之后能上生产的 `post_task` 大概长这样：

```cpp
inline bool post_task(std::function<void()> task)
{
    {
        std::lock_guard lk(g_taskMutex);
        g_taskQueue.push(std::move(task));
    }
    if (!PostMessageW(g_wakeHwnd, WM_POSTED_TASK, 0, 0)) {
        // 投递失败：典型场景是关机途中。
        // 这里必须把"那个待恢复的 handle 会怎样"定义清楚。
        // 策略一（推荐）：直接抽干已入队任务，让恢复在当前路径落地，
        //                并打日志说明发生了降级。
        // 策略二：如果业务允许丢，清空队列 + 销毁相关帧。
        // 绝对不能：什么都不做，让 handle 成孤魂。
        drain_task_queue();
        return false;
    }
    return true;
}
```

第二，关机路径要主动收尾：收到 `WM_DESTROY`/`WM_CLOSE` 时，`KillTimer` 掉所有定时器、取消所有在途的句柄等待（`CancelWaitableTimer` / `UnregisterWaitEx` 等）、决定在途 `coroutine_handle` 的命运。最简单的正确策略是"关机时不允许新 resume 派发"，并把已挂起的协程帧交给它们的 `task` 拥有者去销毁——这要求 `task` 的所有权在编译期就清晰（这正是下一个器官"所有权"要解决的）。第三，`ui_thread{}` 的 awaiter 在 `await_suspend` 里调用 `post_task` 时，应该把它的返回值 surface 出来——失败就别让协程干等。我们的 demo 为了聚焦没写这层，但生产代码必须补上。

第三，`MsgWaitForMultipleObjectsEx` 的句柄集合在关机时也要清空，否则你可能等到一个永远不来的 signal。我们这一阶段 `cHandles=0` 还没用到句柄等待，但从现在起就要在脑子里建立这个约束：**每一个进入等待的 `HANDLE`，都必须有明确的取消路径**，不然关机时它会卡死整个循环。

把这些放在一起看，你会发现"关机安全"不是一个单独的步骤，而是器官级代码必须从一开始就内化的纪律：恢复失败绝不静默、所有在途资源（handle、timer、协程帧）都有退场剧本。我们这一篇只点了最关键的，下一阶段接线程池时会把它补完整。

---

## 收尾：到这里，泵就活了

到这里就大功告成了——我们手里的 `run_message_loop` 已经不是那个只会取窗口消息的古董泵，而是一个能同时盯着原生消息、投递任务、协程恢复、APC 完成和句柄等待的多源调度器；我们亲手焊上去的 `task<T>` + `ui_thread{}` + `background_thread{}` 三件套，也用一段 `tid` 输出证明了协程能挂起、能跨线程、能稳稳恢复。整条链路最值得记住的，是那句话：**一个投递任务就是一个没有返回值的协程恢复**——它们共用同一条 `post_task` 高速公路，所以复杂度被压在了一处。后续每个器官用到异步，都是在这套基座上长肉，不会再推倒重来。

下一步该往哪走？你现在能感觉到一个悬在头顶的隐患：协程体里往往捕获了 `this`（"我这个控件"），可如果协程挂起期间，控件已经被销毁了呢？后台线程慢悠悠地 `h.resume()`，恢复点解引用的那个 `this` 早就是悬垂指针——一访问就炸。这就是经典的"悬垂 this"问题，也是 CppWinRT/Chromium 都要靠 `WeakPtr` 兜底的根源。这就是本系列下一个器官——**所有权与弱引用**——要解的局：`unique_ptr` 控件树 + Chromium 风格的 `WeakPtrFactory` + 每控件 `std::stop_source`，把"协程恢复时对象还活着"这件事变成类型系统能保证的事。我们下一器官见。

---

## 相关资源

- [The Old New Thing · MsgWaitForMultipleObjects and the queue state (2005-02-17)](https://devblogs.microsoft.com/oldnewthing/20050217-00/?p=36423) —— Raymond Chen 讲 `MWMO_INPUTAVAILABLE` 为什么是必须的，"seen-input 竞态"的权威出处。
- [The Old New Thing · What happens if C++/WinRT is unable to resume execution on a dispatcher (2025-07-21)](https://devblogs.microsoft.com/oldnewthing/20250721-00/?p=111396) —— 关机路径上 `resume_foreground` 恢复失败、协程泄漏/跑错线程的复盘。
- [The Old New Thing · Being more adamant about reporting that C++/WinRT was unable to resume (2025-07-22)](https://devblogs.microsoft.com/oldnewthing/20250722-00/?p=111400) —— 上一篇的续：为什么恢复失败绝不能静默。
- [Lewis Baker · C++ Coroutines: Understanding symmetric transfer](https://lewissbaker.github.io/2020/05/14/understanding_symmetric_transfer) —— 对称转移模式最清楚的讲解，我们 `final_suspend` 的写法直接来自这里。
- [Lewis Baker · C++ Coroutines: Understanding operator co-await](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await) —— `await_ready`/`await_suspend`/`await_resume` 三件套语义与 `promise_type` 机制。
- [MsgWaitForMultipleObjectsEx - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex) —— `dwWakeMask` / `dwFlags` 各取值的官方说明（`MWMO_INPUTAVAILABLE`、`MWMO_ALERTABLE`）。
- [PostMessage / Message-Only Windows - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#message-only-windows) —— `HWND_MESSAGE` 父窗口创建 message-only 窗口的官方描述。
- [C++/WinRT · resume_foreground improvements (Kenny Kerr)](https://kennykerrca.wordpress.com/2019/07/01/cppwinrt-resume_foreground-improvements/) —— `resume_foreground` 的工业级参考实现，我们自造 `ui_thread{}` 的对照标本。
- [Chromium · message_pump_win.cc](https://chromium.googlesource.com/chromium/src/+/HEAD/base/message_loop/message_pump_win.cc) —— 生产级 Windows 消息泵，`MWMO_INPUTAVAILABLE` + 句柄集合的真实用法，对照阅读非常 illuminating。
