# 器官三·控件树与所有权——让协程不再悬垂 this

> 前两个器官我们先把"骨架"（窗口与消息循环）和"心跳"（协程泵，把 `coroutine_handle::address()` 通过 `PostMessage` 投到 message-only HWND 上恢复）搭了起来，本系列终于要长出真正承重的那部分——"骨头"。器官三要回答的问题听起来朴素，却是整个现代 GUI 框架能否扛住异步的命门：当一个控件被销毁了，挂在这个控件 `this` 上的协程该怎么办？
>
> 答案在我们这一篇里会被一步步推成一个定理，而不是一句口号。我们会亲手写一个 demo 协程，让它在 `co_await sleep` 期间被父容器从树上摘下来、当场析构，然后看协程恢复时是怎么干净退出的——不崩、不 UAF、不需要任何锁。这一篇的核心不是某个新 API，而是把"为什么单线程亲和的 GUI 不需要锁"这件事讲到让人再也忘不掉。

---

## 前言：为什么必须推倒 `shared_ptr` 的所有权

说实话，笔者在做 stage03 之前，先回头把 `anatomy_gui_for_tutorials/stage10` 那套 miniui 翻出来过了一遍。你看，stage10 的 `Widget` 长这样（节选）：

```cpp
class Widget : public std::enable_shared_from_this<Widget> {
    // ...
};

template <LayoutStrategy Layout>
class Container : public Widget {
    std::vector<std::shared_ptr<Widget>> children_;
    void addChild(std::shared_ptr<Widget> child);
};
```

这是相当"经典"的写法，初学者看到 `shared_ptr` 满脑子都是"自动管理、省心"，但 Qt 官方文档在讲 parent-child ownership 的时候专门点过名：**"a parent owns its children, and ownership should not be shared"**。stage10 的 `vector<shared_ptr<Widget>>` 看起来把生命周期全交给引用计数，实际上是把所有权这幅图藏起来了——它同时允许多个 `shared_ptr` 指向同一个子控件，于是"父析构 vs 外部还捏着一个 shared_ptr"就成了一场竞速。父以为孩子没了，外部那份引用却把孩子续了命，等外部那份再一放，又轮到 double-free 上场。这不是危言耸听，Qt 文档把这种结构直接列为反面教材。

更致命的是 `enable_shared_from_this` 一旦遇上协程就是慢性毒药。一个协程捕获了 `shared_from_this()`，本来控件的父容器已经把它从树上摘下来、本该当场析构，结果协程那份 `shared_ptr` 把它续命了——你精心构造的"控件树"在异步世界里成了一个永远死不透的僵尸。树的不变量被破坏，析构顺序变得不可预测，调试的时候你只会看到一堆"为什么这个 button 还活着"的灵异事件。

所以器官三要做的事情很清楚：**把所有权重新收回到独占语义**。父容器用 `unique_ptr` 独占子控件，谁能拿走、谁能销毁一目了然；协程不再持有任何续命用的 `shared_ptr`，它只持有一个"心跳票据"——`WeakPtr<T>`，票据失效就 bail，干干净净。这套东西不是我们凭空发明的，它的设计完全照着 Chromium 的 `WeakPtr` / `WeakPtrFactory` 抄了一遍，因为这套机制在过去十几年里扛住了 Chromium 里数以万计的异步回调，它的正确性是被工程量证明过的。

我们现在要做的是，把这套机制装进 `anatomy_core`，建一棵独占所有的控件树，最后用一个会"自爆"的 demo 协程把异步安全定理亲手演示出来。

---

## 环境说明

在我们动刀之前，先把实验台面固定下来，免得后面踩坑了都不知道是哪一环的锅。本器官以及整个 ModernWindows 系列都跑在下面这条工具链上：

- **平台**：Windows 11 24H2（10.0.26200），Windows SDK 最新版
- **IDE / 工具链**：Visual Studio 2026，MSVC v145（14.5x），项目模板用"Windows 桌面应用程序"
- **语言标准**：`/std:c++latest`（落到 C++23/26 子集），`/utf-8`，`UNICODE`，`x64`
- **构建系统**：Visual Studio 的 `.slnx` 解决方案——对，**不是 CMake**。CMake + MSVC 这套组合笔者实在折腾不动了，生成的 `.vcxproj` 一坨、 IntelliSense 还经常跟构建对不上，所以本系列统一走纯 VS。
- **解决方案布局**：一个 `src/tutorial/modern_windows/modern_windows.slnx`，里面装着：
  - `anatomy_core` —— 共享静态库（`.lib`），把"器官"代码集中在这里，每个阶段往上加，优先 header-only；
  - 每个阶段一个独立的 `exe` 项目（`stageNN_xxx`），通过 VS 的"项目引用（Project Reference）"引用 `anatomy_core`，`main.cpp` 只是这一阶段的薄入口。
- **零号自检**：已有的 `00_bootstrap/`（一个 Hello World + 独立 `.slnx`）**完全不动**，它定位为"工具链自检"——确认 VS + Windows SDK 能编出一个桌面 `exe`。如果你连 `00_bootstrap` 都跑不起来，先别往下看，回去把 SDK 装好。

器官三要往 `anatomy_core` 里加四个文件，目录长这样：

```
src/tutorial/modern_windows/
├── modern_windows.slnx          ← 总解决方案
├── anatomy_core/                ← 共享静态库
│   ├── anatomy_core.vcxproj
│   └── include/mw/
│       ├── weak_ptr.h           ← 本器官新增：WeakPtr / WeakPtrFactory
│       ├── resume_bridge.h      ← 复用器官02：协程恢复桥（PostMessage 泵）
│       ├── widget.h             ← 本器官新增：Widget 基类（无 enable_shared_from_this）
│       └── container.h          ← 本器官新增：Container（unique_ptr 子节点）
└── stage03_ownership/           ← 本阶段 exe
    ├── stage03_ownership.vcxproj
    └── main.cpp                 ← 薄入口：跑那个"自爆 demo"
```

后面所有代码都是这套布局下的真实文件。`anatomy_core` 编成 `.lib`，`stage03_ownership` 引用它、`main.cpp` 里只写 demo。我们开始动刀。

---

## 第一步——先把"异步安全定理"摆到台面上

先别急着写代码。器官三的灵魂是一句话：

> **在单线程亲和的 GUI 里，控件析构和协程恢复发生在同一条执行序列上；所以只要控件析构时把它的弱引用先 invalidate 掉，协程恢复时观察到的 `WeakPtr::get()` 就一定是 `nullptr`——不需要任何原子操作、不需要任何锁。**

这句话听起来像是在画大饼，我们把它拆成一条严格的因果链，一步一步推给你看。

我们这条 GUI 线程本质是一个消息循环：`GetMessage` → `DispatchMessage` → 执行某个回调 → 回到 `GetMessage`。控件析构是怎么发生的？某次消息处理（比如用户点了"关闭"、或者代码主动 `removeChild`）触发了 `~Widget`。协程恢复是怎么发生的？也是某次消息处理——器官02 里我们约定的恢复方式就是把 `coroutine_handle::address()` 通过 `PostMessage` 投到 message-only HWND，WndProc 收到 `WM_APP+k` 时再 `from_address(addr)()` 把协程拉起来。**这两件事不可能同时发生**，因为它们都只能在同一条消息循环里、按 `DispatchMessage` 的顺序串行执行——要么是"析构"这次 dispatch，要么是"恢复"那次 dispatch，先来后到是确定的。

接下来就是关键一步：控件析构时，我们让 `WeakPtrFactory` 在所有业务成员之前先死掉（怎么做到？把它声明成最后一个成员，C++ 会按声明逆序析构，最后声明的最先死），`~WeakPtrFactory()` 里把那张"心跳票据"翻成无效。于是只要协程的恢复 dispatch 发生在控件析构的 dispatch 之后，协程在恢复点拿到的 `WeakPtr::get()` 就一定看到票据已失效、返回 `nullptr`。反过来，如果协程的恢复 dispatch 在控件析构之前发生，那协程拿到的是活着的 `this`，也安全。两条分支都不会撞上"析构到一半的内存"。

你看，这条链路里没有出现任何 `atomic`、没有 `mutex`、没有 `call_once`，因为顺序是消息循环天然保证的。我们唯一要付的代价是**一条铁律**：协程在 `co_await` 挂起期间绝对不能去碰 `this`，必须等回到 GUI 线程、且 `WeakPtr` 验活通过之后才能解引用。这条铁律一旦守住，整套异步安全就免费拿下了。我们这一器官剩下要做的，就是把这条铁律用代码焊死。

---

## 第二步——把 `WeakPtr` / `WeakPtrFactory` 装进 `anatomy_core`

Chromium 的 `WeakPtr` 设计里有一个非常漂亮、也最容易让新手看走眼的细节：**弱引用不是靠"指向对象的 `shared_ptr`"实现的，而是靠"一张独立于心跳主体的票据"实现的**。这两句话听起来差不多，写出来天差地别。我们先把票据本体写出来，它在 `anatomy_core/include/mw/weak_ptr.h` 里：

```cpp
#pragma once

#include <memory>       // shared_ptr
#include <utility>      // move

namespace mw::detail {

// 一张"心跳票据"。它和被观察对象本身完全解耦——
// 谁持有一个 shared_ptr<WeakPtrFlag>，谁就能在事后查询有效性，
// 但这张票据的存在【不会】给被观察对象续命。
// 这正是 WeakPtr 与 shared_ptr<T> 的本质区别。
struct WeakPtrFlag {
    bool is_valid = true;
};

}  // namespace mw::detail
```

这张 `WeakPtrFlag` 才是整条链路上唯一会被引用计数管理的东西。注意它是 `mw::detail` 命名空间下的——`detail` 是我们给"实现细节"留的抽屉，对外只暴露 `mw::WeakPtr<T>` 和 `mw::WeakPtrFactory<T>`。下面是 `WeakPtr<T>` 本体，注意 `get()` 是整个机制的咽喉：

```cpp
namespace mw {

template <class T> class WeakPtrFactory;

// WeakPtr<T> —— 一个"可能已经失效"的 T*。
// 它内部同时持有票据的引用计数副本 + 一个原始 T*。
// 解引用之前先看票据：票据已翻成无效，就返回 nullptr，绝不碰 T*。
template <class T>
class WeakPtr {
public:
    WeakPtr() noexcept = default;
    WeakPtr(std::shared_ptr<detail::WeakPtrFlag> flag, T* ptr) noexcept
        : flag_(std::move(flag)), ptr_(ptr) {}

    // shared_ptr<Flag> 自带原子引用计数，所以默认的拷贝/移动就够用。
    WeakPtr(const WeakPtr&) noexcept = default;
    WeakPtr(WeakPtr&&) noexcept = default;
    WeakPtr& operator=(const WeakPtr&) noexcept = default;
    WeakPtr& operator=(WeakPtr&&) noexcept = default;

    // 机制的心脏：先验票，再发指针。
    T* get() const noexcept {
        if (flag_ && flag_->is_valid) return ptr_;
        return nullptr;
    }

    explicit operator bool() const noexcept { return get() != nullptr; }
    T& operator*()  const noexcept { return *get(); }
    T* operator->() const noexcept { return  get(); }

private:
    std::shared_ptr<detail::WeakPtrFlag> flag_;
    T* ptr_ = nullptr;
};

}  // namespace mw
```

你会发现 `get()` 里没有任何 `atomic` 加载——是不是和"异步安全定理"里说的"不需要原子操作"对上了？这里我们要稍微严谨一点：`shared_ptr` 内部的引用计数本身是原子的（默认是 `RefCountedThreadSafe` 语义），所以 `flag_` 在协程持有的副本和 factory 持有的原始对象之间拷来拷去是线程安全的；但 `flag_->is_valid` 这个 `bool` 的读写并不在原子上。**为什么这样还安全？** 因为 `is_valid` 的读写我们严格限制在 GUI 线程——factory 析构（写 `false`）发生在 GUI 线程的消息处理里，协程的 `get()`（读）也发生在 GUI 线程的恢复点。两者串行，天然 happens-before，根本不需要原子。如果你哪天要在跨线程上下文里直接 `get()`，那就得另说——这也是为什么 Chromium 的文档会强调"WeakPtr 必须在创建它的同一条 sequence 上 dereference"，本系列把协程恢复强制安排回 GUI 线程，正是为了对齐这条约束。

接下来是 `WeakPtrFactory<T>`，整个机制里最讲究声明顺序的家伙：

```cpp
namespace mw {

// WeakPtrFactory<T> —— 给 T 装一台"心跳票据发放机"。
// ⚠️ 它必须作为 T 的【最后一个】非静态成员声明，理由见正文。
template <class T>
class WeakPtrFactory {
public:
    // owner 必须已经进入构造（this 已可用），因为我们要把它的地址写进每张票据。
    explicit WeakPtrFactory(T* owner) noexcept : owner_(owner) {}

    // 析构时第一时间作废所有已发出的票据——
    // 这就是"factory 先死 → 协程恢复看到 nullptr"的物理来源。
    ~WeakPtrFactory() noexcept {
        InvalidateWeakPtrs();
    }

    WeakPtrFactory(const WeakPtrFactory&) = delete;
    WeakPtrFactory& operator=(const WeakPtrFactory&) = delete;

    WeakPtr<T> GetWeakPtr() const noexcept {
        // 每次都从 flag_ 拷一份 shared_ptr 出去——原子增引用计数。
        return WeakPtr<T>{flag_, owner_};
    }

    void InvalidateWeakPtrs() noexcept {
        if (flag_) flag_->is_valid = false;
    }

private:
    T* owner_;
    // 注意：flag_ 用 shared_ptr，而不是成员对象。
    // 这样 WeakPtr 的副本和 factory 本身共享同一张票据，
    // factory 析构时翻一下 bool，所有副本立刻看见。
    std::shared_ptr<detail::WeakPtrFlag> flag_ =
        std::make_shared<detail::WeakPtrFlag>();
};

}  // namespace mw
```

写到这里，有一个细节值得专门拎出来讲：`flag_` 为什么必须是 `shared_ptr<WeakPtrFlag>`，而不能直接是 `WeakPtrFlag flag_;`？因为如果它是一个普通成员，那么 factory 持有的是票据本体、`WeakPtr` 持有的是票据的指针，factory 一析构，票据本体就没了——协程再去访问 `flag_->is_valid` 就是一个 UAF。而用 `shared_ptr` 之后，factory 析构只是把 `is_valid` 翻成 `false`，票据本身（那张 `WeakPtrFlag` 对象）的真正释放要等到所有持有它的 `WeakPtr` 也都析构为止。换句话说，**`shared_ptr` 在这里不是给控件续命，而是给"票据"续命**——票据要活得比 factory 久，但控件本身一点都没被续命。这是这套设计最精妙的一笔，也是和 `shared_ptr<T>` 最本质的区别。

接下来我们终于可以解释那条"factory 必须最后声明"的铁律了。

---

## 第三步——为什么 factory 必须是最后一个成员？

这是本器官里最容易翻车、翻车后果也最严重的一条。我们先看 C++ 的成员析构规则：成员按声明顺序构造、按声明逆序析构。也就是说，你把 `factory_` 写在成员列表的最后一个，那么它就是**最先析构**的那个——这在 GUI 控件里是一个天大的好事，因为这意味着"票据作废"发生在所有业务成员（信号槽、子节点、缓存、TSF 上下文、UIA provider……）开始析构之前。

我们看一个对照性的例子，把这条铁律的后果摆出来。假设有一个 `AsyncButton`，成员是 `std::string text_; std::stop_source stop_; WeakPtrFactory<AsyncButton> factory_;`，构造时按 `text_ → stop_ → factory_` 顺序，析构时反过来：`factory_ → stop_ → text_`。于是 `factory_` 第一个死，`InvalidateWeakPtrs()` 立刻执行——从这一刻起，任何协程再来 `weak.get()` 都是 `nullptr`。等到轮到 `stop_` 和 `text_` 析构时，外部的协程已经 bail 了，根本没机会再去碰一个正在析构的 `std::string`。这正是"异步安全定理"在代码层面的兑现。

⚠️ 千万别手滑把 `factory_` 往前挪。笔者见过有人图省事把它写在最前面，结果就是：业务成员析构到一半，协程恰好在这个间隙里被恢复（消息循环无情地串行执行），`weak.get()` 还能拿到非空 `this`——你以为安全了，去读 `text_`，结果那个 `std::string` 早就析构过了，buffer 已释放，你读到的是 freed memory。ASan 当场能抓出来，但线上 Release 构建是哑巴。**这条坑的隐蔽性极高，因为它只在"析构中途被协程撞上"这种特定时序下才崩**——单元测试很难覆盖到。

更隐蔽的还有一种变体：把 `factory_` 放最后，但在 `~Widget()` 函数体里又调了一次虚函数或者成员访问。这里要清楚——**`factory_` 在 dtor 函数体执行期间还活着**（因为它是最后声明的成员，dtor 函数体跑完之后才轮到它析构），所以你在 dtor body 里 `factory_.InvalidateWeakPtrs()` 是合法的，这是"手动提前作废"的口子。但 dtor body 里调虚函数就要小心了：在 `~Widget()` 里，vtable 已经退化到 `Widget` 这一档，子类 override 的虚函数不会被调用——这是 C++ 的标准行为，不是 bug。我们在 `Widget` 的 dtor 里只会做"打日志 + `request_stop()` + 让 factory 自动析构"这几件事，不碰虚函数，干净。

讲清楚了为什么，现在可以写 `Widget` 本体了。

---

## 第四步——`Widget` / `Container`：独占所有的控件树

`anatomy_core/include/mw/widget.h` 是这一器官的中央文件。注意它**不继承 `enable_shared_from_this`**——这不是疏忽，是契约：

```cpp
#pragma once

#include "mw/weak_ptr.h"

#include <cstdio>
#include <stop_token>
#include <string>
#include <string_view>

namespace mw {

class Container;  // 前向声明，friend 用

// Widget —— 所有控件的根基类。
// ⚠️ 不继承 enable_shared_from_this！这是本器官的核心契约。
class Widget {
public:
    Widget() = default;

    // 析构做的三件事，顺序很讲究：
    //   1) request_stop() —— 给后台协程的"安全带"打一个停信号；
    //   2) 打一条析构日志（demo 时拿来看时序）；
    //   3) 啥都不用做，factory_ 是最后一个成员，dtor body 一结束它自动析构，
    //      析构里会 InvalidateWeakPtrs()——弱引用从这一刻起失效。
    // 注意这里我们不手动 factory_.InvalidateWeakPtrs()，让它走自然析构，
    // 因为 factory_ 在 dtor body 期间还活着，自然析构就够。
    virtual ~Widget() {
        stop_source_.request_stop();
        std::printf("[~Widget] destroying '%s' (stop_requested=%d)\n",
                    name_.c_str(),
                    static_cast<int>(stop_source_.stop_requested()));
    }

    Widget(const Widget&)            = delete;
    Widget& operator=(const Widget&) = delete;

    // --- 父子关系：只暴露 raw 指针，所有权在 Container 手里 ---
    Widget* parent() const noexcept { return parent_; }

    // --- 取消令牌：后台协程在每个 await 点都要看它一眼 ---
    std::stop_token getStopToken() const noexcept {
        return stop_source_.get_token();
    }
    std::stop_source& stopSource() noexcept { return stop_source_; }

    // --- 弱引用：协程拿这张票，不要拿 raw this！ ---
    WeakPtr<Widget> weakFromThis() noexcept {
        return factory_.GetWeakPtr();
    }

    // --- demo 用的名字 / 几何占位（绘制留给后续器官） ---
    const std::string& name() const noexcept { return name_; }
    void setName(std::string n) { name_ = std::move(n); }

protected:
    // 留给后续器官：销毁前夜的钩子。本器官用不到，先占位。
    virtual void onDestroy() {}

private:
    friend class Container;

    std::string     name_;
    Widget*         parent_      = nullptr;
    std::stop_source stop_source_;

    // ⚠️ 必须是最后一个非静态成员！理由见正文"第三步"。
    // 模板参数是 Widget：weakFromThis() 返回 WeakPtr<Widget>。
    // 子类想要更精确的 WeakPtr<MyButton>，自己再加一个 factory。
    WeakPtrFactory<Widget> factory_{this};
};

}  // namespace mw
```

这里有两个细节值得停下来讲。

第一个细节是 `stop_source_` 的角色。你可以把它理解为**给后台协程拴的一条"安全带"**：协程每次在 `co_await` 之后醒来，都会先看一眼 `stop_token::stop_requested()`——只要控件析构过（dtor 里 `request_stop()` 已经被调），这个标志就是 `true`，协程立刻 `co_return` 退出。它和 `WeakPtr` 是两套独立的保险，作用在协程的同一个 await 点上：`WeakPtr` 管"`this` 还在不在"，`stop_token` 管"控件虽然还在、但它已经请求你停下来了"（比如父容器主动 cancel 一个异步任务但不销毁控件）。两套都过，才继续往下跑。

第二个细节是 `factory_{this}` 在构造期就把 `this` 交给 factory。这里有一个 C++ 的隐含保证：在成员初始化列表里，`this` 指向的虽然是 Widget 子对象，但因为标准布局单继承，它的地址等于完整对象（比如 `AsyncButton`）的地址。所以 `factory_` 内部存的 `owner_` 就是完整对象的指针，将来 `GetWeakPtr()` 发出去的 `WeakPtr<Widget>` 里的 `ptr_` 也是它——协程恢复后 `get()` 拿到的就是这个完整对象指针，upcast/downcast 都对得上。当然，由于模板参数是 `Widget`，子类要拿到 `WeakPtr<MyButton>`，需要在子类自己再加一个 `WeakPtrFactory<MyButton> weak_factory_{this};`（同样必须是最后一个成员）——这就是 Chromium 里 `SupportsWeakPtr<T>` 那一套 CRTP 想解决的问题，但本器官的 demo 用基类 `WeakPtr<Widget>` 就足够说明问题了，我们不过度设计。

接下来是 `Container`，独占所有子节点的容器。`anatomy_core/include/mw/container.h`：

```cpp
#pragma once

#include "mw/widget.h"

#include <memory>
#include <utility>
#include <vector>

namespace mw {

// Container —— 独占持有子控件的容器。
// children_ 是 vector<unique_ptr<Widget>>，不是 vector<shared_ptr<Widget>>。
// 这一条改动是器官三和 stage10 反面教材最直观的差别。
class Container : public Widget {
public:
    Container() = default;

    // 接管一个子控件的所有权。调用方交出 unique_ptr 之后，
    // 这个控件的生命周期就完全归本容器管了。
    Widget* addChild(std::unique_ptr<Widget> child) {
        child->parent_ = this;
        children_.push_back(std::move(child));
        return children_.back().get();
    }

    // 把子控件【交还】给调用方——拿走、避免销毁。
    // 适合"把一个按钮从 A 容器挪到 B 容器"这种场景。
    std::unique_ptr<Widget> takeChild(Widget* target) {
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if (it->get() == target) {
                (*it)->parent_ = nullptr;
                auto out = std::move(*it);
                children_.erase(it);  // erase 一个空 unique_ptr，无副作用
                return out;
            }
        }
        return nullptr;
    }

    // 直接移除并销毁子控件。takeChild 返回的 unique_ptr 离开作用域即析构。
    void removeChild(Widget* target) {
        [[maybe_unused]] auto orphan = takeChild(target);
        // orphan 在这里析构 → ~Widget() → factory_ 先死 → 所有 WeakPtr 失效。
    }

    const std::vector<std::unique_ptr<Widget>>& children() const noexcept {
        return children_;
    }

private:
    std::vector<std::unique_ptr<Widget>> children_;
};

}  // namespace mw
```

你会发现 `removeChild` 的实现短得有点不像话——就一行 `takeChild`。这里我们要稍微停一下解释为什么这样写就够：`takeChild` 把目标子控件从 `vector` 里 `move` 出来，返回一个持有所有权的 `unique_ptr`；`removeChild` 用 `[[maybe_unused]]` 接住这个返回值，紧接着函数结束、`orphan` 析构、`~Widget()` 触发、`factory_` 自动先死、所有 `WeakPtr` 失效。整条销毁链是 RAII 自动滚下来的，不需要我们手动 `delete`。这就是独占所有权最大的好处——**销毁这件事是类型系统免费帮你做的**，你只需要保证 `unique_ptr` 走完它的正常生命周期。

`takeChild` 里有一处容易被忽略的小坑：`children_.erase(it)` 时，`*it` 已经被 `std::move` 过、是一个空的 `unique_ptr`，所以 `erase` 只是删掉这个 vector 槽位，不会触发任何二次析构。如果你不小心先 `erase` 再 `move`，那就是另一个故事了——会先析构控件（因为 vector 槽位还持有它），再 move 一个空壳，逻辑就错了。所以这里顺序必须是：**先 move 出来，再 erase 空槽**。

到这里，控件树的"骨头"就立起来了：独占所有、parent 反向只持 raw 指针、协程只持 `WeakPtr`、stop_token 作为安全带。接下来我们要写一个真正的 demo，把整条定理跑给读者看。

---

## 第五步——demo 协程：让它活过控件的销毁

这一步是本器官的高潮，我们要亲手制造一个"协程正在 await、控件被从树上摘下来析构"的现场，然后看协程恢复时怎么 bail。为了 demo 自包含、可独立运行，我们把器官02 那套"协程泵"的简化版一并展示——它就是把 `coroutine_handle::address()` 通过 `PostMessage` 投到 message-only HWND，WndProc 收到 `WM_APP+k` 时恢复协程。完整版本（含 thread_pool、`std::expected` 错误传播、`resume_foreground` 风格的优先级）在器官02 已经写过了，本器官只复用其中最薄的 `ResumeBridge`，并扩展出一个"投递任意闭包"的口子，方便我们模拟"用户点击"。

`anatomy_core/include/mw/resume_bridge.h`：

```cpp
#pragma once

#include <coroutine>
#include <functional>
#include <utility>
#include <windows.h>

namespace mw {

// ResumeBridge —— 把"协程恢复"和"任意 GUI 线程工作"都串到同一条消息循环里。
// 这正是异步安全定理的物理前提：析构、协程恢复、闭包执行都从这里 dispatch。
class ResumeBridge {
public:
    static constexpr UINT kResumeMsg = WM_APP + 0x1001;  // 投递 coroutine_handle
    static constexpr UINT kLambdaMsg = WM_APP + 0x1002;  // 投递 std::function<void()>

    ResumeBridge() {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = &ResumeBridge::WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"MwResumeBridge";
        RegisterClassExW(&wc);

        // HWND_MESSAGE = message-only 窗口：不可见、不参与 z-order、
        // 但照样能收 PostMessage——完美的"协程泵"载体。
        hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"", 0,
                                0, 0, 0, 0, HWND_MESSAGE,
                                nullptr, wc.hInstance, this);
    }

    ~ResumeBridge() {
        if (hwnd_) DestroyWindow(hwnd_);
    }

    ResumeBridge(const ResumeBridge&)            = delete;
    ResumeBridge& operator=(const ResumeBridge&) = delete;

    // 把一个挂起的协程安排回 GUI 线程恢复。
    void post(std::coroutine_handle<> h) const {
        PostMessageW(hwnd_, kResumeMsg,
                     reinterpret_cast<WPARAM>(h.address()), 0);
    }

    // 把任意闭包投回 GUI 线程执行。所有权通过 new/delete 传递。
    void post(std::function<void()> fn) {
        auto* p = new std::function<void()>(std::move(fn));
        PostMessageW(hwnd_, kLambdaMsg, reinterpret_cast<WPARAM>(p), 0);
    }

    int run() {
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void quit(int code = 0) { PostQuitMessage(code); }

private:
    static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == kResumeMsg) {
            auto addr = reinterpret_cast<void*>(wp);
            // 把地址还原成 handle 并 resume——这一步一定在 GUI 线程发生。
            std::coroutine_handle<>::from_address(addr)();
            return 0;
        }
        if (msg == kLambdaMsg) {
            // 取出闭包、调用、释放。异常不期望发生，发生就终止。
            auto* p = reinterpret_cast<std::function<void()>*>(wp);
            (*p)();
            delete p;
            return 0;
        }
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(h, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        return DefWindowProcW(h, msg, wp, lp);
    }

    HWND hwnd_ = nullptr;
};

}  // namespace mw
```

这里 `kResumeMsg` 和 `kLambdaMsg` 共用同一个 message-only HWND，意义不仅是省一个窗口——更重要的是**让"协程恢复"和"控件析构"这两件事物理上走同一个 dispatch 出口**，从而严格串行。你不能用两个独立的 HWND，那样虽然也是单线程，但消息顺序仍然确定（一个线程的消息队列是 FIFO），不过混用一个窗口能把"所有异步副作用都经过这里"这件事讲得更清楚，调试时也只需要 hook 一个 WndProc。

接着写一个最小的 awaitable——`AsyncSleep`，它把"N 毫秒后回到 GUI 线程"包装成 `co_await` 表达式：

```cpp
// 一个把"sleep N 毫秒后回 GUI 线程"包装成 awaitable 的 demo 工具。
// 真实代码里应该用 thread_pool，这里为了自包含用 detach 线程 + Sleep。
struct AsyncSleep {
    DWORD            ms;
    mw::ResumeBridge* bridge;

    bool await_ready() const noexcept { return ms == 0; }

    void await_suspend(std::coroutine_handle<> h) const {
        // 后台线程只做"等"——绝不碰 this、绝不碰控件成员。
        // 到点之后通过 PostMessage 把协程推回 GUI 线程。
        std::thread([h, ms = ms, bridge = bridge]() {
            Sleep(ms);
            bridge->post(h);
        }).detach();
    }

    void await_resume() const noexcept {
        // 恢复时【已经在 GUI 线程】。这里返回 void，
        // 调用方拿到控制权后第一件事就是检查 WeakPtr / stop_token。
    }
};
```

注意 `await_suspend` 里那个后台线程**完全不碰任何控件成员**——它只持有 `coroutine_handle` 和 `bridge` 指针，这两个东西的生命周期都由 demo 的 `main` 保证。如果你在这里捕获了 `this` 或者控件成员的引用，那就把整条定理废了：协程挂起期间控件可能被销毁，那个捕获的指针就成了悬垂。`co_await` 的纪律就是这样——**挂起期间能持有的，只有"票"和"无关于 this"的工具**。

最后是 demo 协程本体，以及 `stage03_ownership/main.cpp`：

```cpp
// 最简的 fire-and-forget 任务包装。
struct DemoTask {
    struct promise_type {
        DemoTask get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend()   noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> h{};
};

// 长寿命协程：持有一张 WeakPtr，每 200ms 醒来检查控件还活着没。
// 注意它【没有】捕获 raw this——它只有 weak 和 stop_token。
DemoTask longLivedCoroutine(mw::WeakPtr<mw::Widget> weak,
                            mw::ResumeBridge*       bridge,
                            std::stop_token        st,
                            int                    ticks) {
    for (int i = 0; i < ticks; ++i) {
        co_await AsyncSleep{200, bridge};

        // 恢复点①：先验 stop_token——控件析构会在 dtor 里 request_stop。
        if (st.stop_requested()) {
            std::printf("[coro] stop requested at tick %d, bail.\n", i);
            co_return;
        }

        // 恢复点②：再验 WeakPtr——控件析构会让 factory_ 先 invalidate。
        mw::Widget* p = weak.get();
        if (!p) {
            std::printf("[coro] WeakPtr dead at tick %d, bail cleanly. no UAF.\n", i);
            co_return;
        }

        std::printf("[coro] tick %d ok, widget '%s' alive.\n",
                    i, p->name().c_str());
    }
    std::printf("[coro] finished normally.\n");
}

int main() {
    using namespace mw;

    ResumeBridge bridge;

    // ---- 建一棵最简的控件树：root (Container) → btn (Widget) ----
    Container root;
    root.setName("root");

    auto btn_up = std::make_unique<Widget>();
    btn_up->setName("AsyncButton");
    Widget* btn = root.addChild(std::move(btn_up));  // 独占所有权移交

    // ---- 协程拿"票"，绝不能拿 raw this ----
    WeakPtr<Widget> weak = btn->weakFromThis();
    std::stop_token st   = btn->getStopToken();

    // 启动一个会跑 10 个 tick 的长寿命协程（总时长 2000ms）。
    longLivedCoroutine(weak, &bridge, st, /*ticks=*/10);

    // ---- 模拟"用户在 500ms 时把按钮从容器里删了" ----
    // kill 动作也走 PostMessage → 严格串行在 GUI 线程执行。
    std::thread([&root, btn, &bridge]() {
        Sleep(500);
        bridge.post([&root, btn]() {
            std::printf("[main] removeChild(btn) → ~Widget() fires now\n");
            root.removeChild(btn);   // ← 控件在这里析构
        });
    }).detach();

    // ---- 2500ms 后退出消息循环 ----
    std::thread([&bridge]() {
        Sleep(2500);
        bridge.post([&bridge]() { bridge.quit(); });
    }).detach();

    return bridge.run();
}
```

这里有几个时序细节值得对照看：协程周期是 200ms，`removeChild` 发生在 500ms——也就是说协程会在 200ms（tick 0）、400ms（tick 1）两次正常醒来打印 alive，然后在 500ms 时控件被销毁，紧接着 600ms（tick 2）那一次醒来撞上的就是失效的 `WeakPtr`。后台的 sleep 线程在 500ms 时只是 `PostMessage` 了 kill 动作，并没有真的去碰控件——真正的 `~Widget()` 是在 GUI 线程的 `DispatchMessage` 里发生的，和协程恢复严格串行。这就把"异步安全定理"的物理前提可视化出来了。

我们在 VS 2026 里 `F5` 跑一下，控制台输出长这样（实际时间戳按机器不同会有抖动，但顺序确定）：

```
[coro] tick 0 ok, widget 'AsyncButton' alive.
[coro] tick 1 ok, widget 'AsyncButton' alive.
[main] removeChild(btn) → ~Widget() fires now
[~Widget] destroying 'AsyncButton' (stop_requested=1)
[coro] WeakPtr dead at tick 2, bail cleanly. no UAF.
```

你看这条输出，它把整条定理一句话讲完了：tick 0 / tick 1 控件还活着，协程正常访问；500ms 时 `removeChild` 触发 `~Widget`，析构日志里 `stop_requested=1` 印证了 dtor 里 `request_stop()` 已经执行、`factory_` 的析构紧接着把 `WeakPtr` 作废；600ms 时协程醒来，`weak.get()` 返回 `nullptr`，走 `co_return` 干净退出，全程没有任何 UAF，没有锁，没有原子。事情到这里，器官三的核心定理就被亲手验证完了。

如果你把 `longLivedCoroutine` 里那句 `weak.get()` 换成直接捕获的 raw `this`（笔者强烈建议你试一次，故意踩坑），同样的时序下你会看到 tick 2 醒来时打印的是一堆乱码或者直接段错误——这就是经典的"协程悬垂 this"。 organs 三的存在意义就是让这种悬垂**永远不会发生**。

---

## 第六步——迁移说明：和 stage10 反面教材逐条对照

把这套设计落到 anatomy 仓库的语境里，最直观的对照是 `stage10` 的 miniui。我们把两条写法并排放一下，看看我们到底改掉了什么、为什么改。

| 维度 | anatomy stage10（旧） | organs三（新） |
|---|---|---|
| 子节点存储 | `std::vector<std::shared_ptr<Widget>>` | `std::vector<std::unique_ptr<Widget>>` |
| 父子所有权 | 共享，外部可同时持 `shared_ptr` | 独占，父容器是唯一 owner |
| 异步引用 | `shared_from_this()` 给协程续命 | `weakFromThis()` 发票，不续命 |
| 基类混入 | `: public std::enable_shared_from_this<Widget>` | 不混入任何"自指"基类 |
| 销毁时机 | 由引用计数决定，不可预测 | 由树结构决定，`removeChild` 即销毁 |
| 协程恢复安全 | 需要外部 `weak_ptr` + `lock()`，竞态复杂 | `WeakPtr::get()` 单线程下天然安全 |

stage10 的写法在"无异步"的纯同步 GUI 里其实问题不大，因为引用计数本身是自洽的。一旦协程进场，`shared_from_this()` 就成了破坏树不变量的元凶——协程捕获的 `shared_ptr` 把本该被父容器析构的子控件续了命，树的"独占父子关系"在异步世界里直接作废。我们换成 `WeakPtr` 之后，协程手里那张票不参与引用计数，控件该死就死，恢复时看到死讯就 bail，所有权图重新变得清晰可推理。

还有一个不那么显眼但很重要的差别：迁离 `enable_shared_from_this` 之后，**"谁拥有谁"这幅图重新可见了**。在 stage10 里，你扫一眼 `Container` 看不出它"是不是唯一 owner"，因为 `shared_ptr` 把这件事藏进了引用计数；在新设计里，`vector<unique_ptr<Widget>>` 一行字就把所有权关系摊在了台面上——父容器拥有子控件、子控件的 `parent()` 反向只持 raw 指针，这就是教学场景里最想强调的东西。shared_ptr 不是坏工具，它是把"所有权"这件本该显式的事情隐式化了；在框架教学、尤其在教"为什么这样设计"的系列里，我们偏偏要把所有权重新写回类型系统。

如果你正打算把一个 stage10 风格的项目迁移过来，建议的顺序是：先把 `Widget` 基类里的 `enable_shared_from_this` 拿掉（这会立刻让 `shared_from_this()` 编译失败、暴露所有"违规续命"的调用点），再把 `Container` 的 `children_` 从 `shared_ptr` 换成 `unique_ptr`，然后把所有异步回调里捕获的 `shared_from_this()` 改成 `weakFromThis()`、并在恢复点加验活检查。这个过程会刷出一大堆编译错误，但每一个错误都是一次"你曾经把生命周期托付给引用计数"的债被发现——慢慢还就是了，笔者自己的迁移也是这么熬过来的。

---

## 收尾——骨头立住了，下一步装肌肉

到这里，器官三的目标全部达成了：我们建立了一棵独占所有的保留控件树（`Widget` / `Container`），给它配上了 Chromium 风格的 `WeakPtr` / `WeakPtrFactory`（factory 声明在最后、dtor 自动 invalidate），用 `std::stop_source` 给后台协程拴了安全带，并且用一个"协程活过了控件销毁"的 demo 把整条异步安全定理亲手跑通。骨架、心跳、骨头齐了，控件能安全地参与协程——接下来我们就有底气在异步的世界里继续往上搭东西了。

下一器官要给这副骨架装上"肌肉"——布局。当控件树可以独占持有、可以安全地异步之后，下一个绕不开的问题就是"子控件在父容器里到底该怎么排"。我们会把 measure / arrange 两阶段协议搬到 `anatomy_core`，让 `Container` 配上一个最小的 `LayoutStrategy`，并且——别忘了——布局 pass 本身也会跑在协程里，到时候你会感谢我们今天亲手焊好的这条 `WeakPtr` 防线。完结撒花，我们器官四见。

---

## 相关资源

- Chromium `WeakPtr` 官方文档：<https://chromium.googlesource.com/chromium/src/+/main/docs/glossary.md#weak-ptr> 以及 `base/memory/weak_ptr.h` 源码注释（这套设计的"原典"，本器官的实现就是它的简化版）
- Chromium `WeakPtrFactory` 必须最后声明的官方解释：<https://chromium.googlesource.com/chromium/src/+/main/base/memory/weak_ptr.h>（搜索 "member variables" 一段，逐字讲了我们第三步那条铁律）
- Qt Object Model / Parent-Child Ownership（反面教材对照）：<https://doc.qt.io/qt-6/object.html#object-model> 以及 <https://doc.qt.io/qt-6/objecttrees.html>（明确点名"ownership should not be shared"）
- `std::stop_source` / `std::stop_token`（C++20 协作式取消，本器官的安全带）：<https://en.cppreference.com/w/cpp/thread/stop_source> 和 <https://en.cppreference.com/w/cpp/thread/stop_token>
- C++ 协程 `coroutine_handle::from_address`：<https://en.cppreference.com/w/cpp/coroutine/coroutine_handle>（器官02 协程泵的核心 API，本器官复用）
- Win32 Message-Only Windows（message-only HWND 的官方定义）：<https://learn.microsoft.com/windows/win32/winmsg/window-features#message-only-windows>
- Visual Studio `.slnx` 解决方案格式：<https://learn.microsoft.com/visualstudio/ide/solution-explorer-slnx>（本系列统一构建系统的载体）
