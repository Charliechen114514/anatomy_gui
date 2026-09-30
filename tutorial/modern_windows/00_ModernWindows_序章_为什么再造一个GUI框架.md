# 序章——为什么我们要用现代 C++ 在 Win32 上从零造一个 GUI 框架

> 这一篇不写代码细节，先把"我们到底在干嘛、为什么要受这一路的挫、接下来十五站怎么走"这件事说透。如果你是冲着 Win32 那套 API 来的，这一篇告诉你我们打算拿那堆古董 API 重新拼一具什么样的身体；如果你是直接跳进来的，这一篇也能让你明白我们要造的东西长什么样。本系列**不预设你读过任何其它教程**——它是一门独立的、从零开始的现代 GUI 框架课。废话不多说，点火。

## 我们到底在造什么

先把话说在前面：我们这一回折腾出来的东西叫 **ModernWindows**，它不是一个产品，甚至算不上一个能拿去写 CRUD 业务的"框架"。它是一个**教学标本**——一具我们亲手在 Windows 上、用现代 C++ 从零拼出来的 GUI 骨架，存在的唯一目的就是把"任何一个 GUI 框架藏起来的那些假设"一个一个摊开给你看。

为什么要从零造？因为"会用一个框架"和"看穿一个框架"之间隔着一整条鸿沟。你大可以打开 Qt Designer 拖出一个窗口、给按钮点一下槽函数，跑起来皆大欢喜——可一旦出了问题、一旦你想知道"为什么我改个属性它就重画了"、"为什么那个异步操作回来的时候控件已经没了"、"为什么它动起来那么顺而我自己画的就闪"，你就只能隔着几层抽象去猜。这套猜的过程，最快的方式就是自己亲手把那具骨架拼一遍：每一条消息怎么进来、每一帧怎么合成、每一个属性变化怎么传播——全都攥在你自己手里。造完之后，你再看 Qt、看 WPF、看 WinUI3，眼里就不再是魔法，而是一组你已经亲手造过一遍的器官的变奏。

我们现在要做的是，用**现代 C++**（C++20 协程、Concepts、`std::expected`）打底，用**现代 GUI 思想**（细粒度响应式、数据绑定）当中枢神经，用**现代 Win32 栈**（Direct2D 1.1 / DirectComposition / DirectWrite）做肌肉和皮肤，从零拼一具骨架出来。它身上有八个器官——这是任何一个 GUI 框架都绕不过去的同一组结构，我们这篇后面会逐一摊开。重要的是你要先建立一个预期：**这具骨架不是一上来就能跑出漂亮界面的，前面好几篇都在"受挫"**——开窗的那一篇你连个按钮都看不到，只有一个深灰色矩形；消息循环那篇你折腾半天只是为了让一个协程能挂起又恢复。这些挫不是白受的，每一挫都对应一个具体的类、一个具体的概念、一个工业级框架藏在抽象层底下的真问题。我们这一篇的任务，就是先把"每个器官落在哪个类上、为什么它难"这张地图给你。

## 为什么是 Windows、是现代 C++、是从零

你可能会问：想学 GUI 框架内部，干嘛非得在 Windows 上、用 C++20、从零造？这个问题我们拆三条讲。

第一条，**Windows 是把"现代 GUI 合成管线"铺得最完整的桌面平台**。Direct2D 1.1 挂在 D3D11 上、DirectComposition 保留视觉树、DirectWrite 排版——这套"保留模式 + GPU 合成"的范式，是工业界现代 UI 的地基，Windows 恰好把它原原本本暴露给了你。我们不在这套栈上亲手走一遍，就永远只能隔着 Electron、隔着 Qt 的抽象层去猜 GPU 合成到底干了什么。想在 Windows 这个全世界最大的桌面平台上把现代 GUI 摸透，这就是该来的地方。

第二条，**C++20 的协程给消息循环装上了记忆，这是老式回调给不了的**。传统 Win32 的异步写法是回调：发起一个 IO、登记一个回调、这条逻辑链就断了，等系统回头叫你，你已经在另一个函数里了，局部变量、调用栈全没了。协程把这个"断点"变成语言层面的一等公民——`co_await` 挂起，协程帧帮你把整个局部状态存下来，将来在某个点上 `resume`，你接着往下跑，跟什么都没发生过一样。而 Win32 的消息循环天然适合接协程：所有"通知某件事就绪"都被统一抽象成"往这个线程的队列里投一条消息"，我们完全可以把协程的恢复点通过 `PostMessage` 投回去，让挂起的协程在 UI 线程上原地复活。这是本系列的第一条 headline，也是我们选 C++20 的核心理由。

第三条，**细粒度响应式是现代 GUI 的中枢神经，而它有一个不亲手造就看不见的坑**。你大概知道"响应式"是说"数据变了界面自动更新"，但工业级的响应式系统真正难的地方在**依赖追踪**——一个派生属性得知道自己是从谁推出来的，否则一旦出现菱形依赖（`a` 同时影响 `b` 和 `c`，而 `b`、`c` 又共同决定 `d`），你改一次 `a`，`d` 会被反复求值好几次，这叫"毛刺"（glitch）。这个坑不亲手造一遍响应式内核，你根本意识不到它存在。我们这一系列要自己造一个依赖感知的内核，这是第二条 headline。

## 先把环境钉死——像实验记录那样

按照老规矩，动手之前先把工具链和约束钉死，省得后面每一篇都要重复交代一遍。这一节你可以当成整个系列的"环境基线"来对照。

我们这次用 **Visual Studio 2026 + MSVC 14.5x 工具集**，编译选项 `/std:c++latest`（吃 C++23/26 那一波特性，尤其是 `std::expected`、`std::stop_source`、ranges 的完善），外加 `/utf-8`——后者在你源码里写中文注释和字符串字面量的时候是保命设置，不加它 MSVC 默认按系统代码页解释源文件，中文受众迟早要在这上面栽一个"为什么只有我编译出来是乱码"的跟头。字符集一律 `UNICODE`，目标平台 x64，Windows SDK 装最新版（本系列假设 Windows 10/11，因为 DirectComposition 的若干新特性只有在更新版本上才齐活）。

顺便把几个贯穿全系列的符号约定也钉在这一节，后面所有器官代码都照这套来，免得每篇都重复交代。框架统一住在 `mw::` 命名空间里（ModernWindows 的缩写），成员变量带尾下划线（`hwnd_`、`d3d_`），类型名用大驼峰（`RootWindow`、`LayoutStrategy`），模板参数用 Concepts 约束而不是 `typename T` 一把梭。所有 COM 对象一律走 `Microsoft::WRL::ComPtr`，绝不裸 `Release()`；所有可能出错的调用一律返回 `Result<T>`（也就是 `std::expected<T, FrameworkError>`），绝不 `throw`。这几条你现在扫一眼就行，等真正读到代码的时候你会感谢这套约定的——它让每一篇器官专题的代码长得都像同一个人写的，而不是十五个不同人格的拼盘。

这里有一个取舍要先交代清楚：**构建系统我们用 CMake，但产出的东西是 Visual Studio 的 `.sln`**。你大概会问——CMake 配 MSVC 不是出了名地狱吗？是，但那地狱的源头是"在命令行手撸 CMake + IntelliSense 对不上"，而我们绕开了它：`cmake --preset` 跑一次生成 `.sln`，之后全程在 VS 里点开这个 `.sln` 调试，体验和手写工程没差别。况且写一份 `CMakeLists.txt` 比手搓一堆 `.vcxproj` 的 XML 干净太多，加个阶段就一行 `add_subdirectory`。地狱的那一半，绕过去了。构建模型的细节我们留到后面单独一节展开，这里先记住一句话：**一份顶层 `CMakeLists.txt` + 一个 `CMakePresets.json`，底下每个阶段自包含——一个 `stageNN_xxx` 目录自己带一份完整的器官代码 + `main.cpp`，编进一个独立 exe，阶段之间互不依赖**。

## 两条主线——这门课真正想教会你的两件事

动机说完了，我们把这次最想打出来的两张牌再单独拎出来讲透，因为这两条是贯穿后续十几个阶段的灵魂。更重要的是，你要知道这两条主线**各自落在一个具体的类上**——这才是你后面"受挫"时能抓住的把手。

### 主线一：把 C++20 协程接进消息循环

你可以把这条主线理解为"给消息循环装上记忆"，而它在我们代码里最终长成一个叫 `task<T>` 的类、外加两个切线程的小工具 `ui_thread{}` / `background_thread{}`。我们先看它解决什么问题，再看它难在哪。

传统回调式异步最大的痛是"逻辑链断成几截"。协程把它缝起来了：`co_await` 挂起，状态存在协程帧里，将来 `resume` 接着跑。那"将来在哪个点"恢复？在我们的框架里，这个点就是 **UI 线程的消息循环**——协程挂起时把"怎么唤醒我"（也就是 `coroutine_handle::address()`）压成一条 `PostMessage`，投给一个 message-only 的 HWND；这条 HWND 收到自定义消息，就把地址还原成 handle，原地 `resume`。这套玩法就是 CppWinRT 的 `resume_foreground` 的精神，只不过我们用裸 C++ 自己手搓一遍。先看挂起那一侧，混个眼熟：

```cpp
// stage02_message_coroutine/include/mw/await_resume_on_ui.h —— 协程挂起后，把恢复点投回 UI 线程
#include <coroutine>
#include <windows.h>

namespace mw {

// 协程恢复专用的自定义消息号，挂在 WM_APP 段，绝不和系统消息撞车
constexpr UINT kResumeMessage = WM_APP + 0x100;

// 一个 awaiter：co_await 它，协程就挂起，并把"怎么唤醒我"投成一条 PostMessage
struct ResumeOnUiThread {
    HWND resumeTarget_;   // message-only HWND，挂在消息循环里收恢复消息

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        // 协程帧的地址是唯一可以安全跨线程传递的"恢复句柄"
        PostMessageW(resumeTarget_, kResumeMessage,
                     reinterpret_cast<WPARAM>(h.address()), 0);
    }

    void await_resume() const noexcept {}
};

}  // namespace mw
```

恢复那一侧就活在我们的 `WndProc` 里，几行就交代了：

```cpp
LRESULT CALLBACK RootWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == mw::kResumeMessage) {
        // WPARAM 里就是当初 PostMessage 投过来的帧地址，还原成 handle 直接 resume
        auto h = std::coroutine_handle<>::from_address(reinterpret_cast<void*>(wp));
        if (h && !h.done()) h.resume();
        return 0;
    }
    // ... 其余消息照常分发
    return DefWindowProcW(hwnd, msg, wp, lp);
}
```

⚠️ 这里有一个新手特别容易踩的坑，我们提前点一下：`coroutine_handle::address()` 返回的是 `void*`，而 `PostMessageW` 的 `WPARAM` 是个无符号整数类型。x64 上两者都是 8 字节，所以 `reinterpret_cast` 直接来回转是安全的；但你要是哪天脑子一热把地址塞进 `LPARAM` 之外的结构体里再拷来拷去，那很容易在某个中间环节被截断。我们的原则是：**地址只在 `PostMessage` 的那一刻裸奔一次，进门立刻 `from_address` 还原成有类型的 handle**，绝不缓存、绝不二次拷贝。

这条主线做完，难点全聚集在 `task<T>` 这个类上：它得是 lazy 的（不显式启动不动）、得用对称转移保证栈不爆、得把异常压成指针延迟抛、还得在关机时把没恢复完的协程干干净净收尾——后面阶段二会一帧帧拆给你看。这是回调永远给不了你的东西。

### 主线二：细粒度、依赖感知的响应式内核

第二条主线落在一组类上：`Signal<T>`、`Property`、`Computed<T>`、`Effect`。它要修的是一个不亲手造就看不见的坑——毛刺。我们先把毛刺长什么样摆出来，你才明白为什么要造新轮子。

假设我们有一个控件，它有 `width`、`height`，还有一个期望始终满足 `area == width * height` 的派生属性 `area`。在一个"盲广播"的朴素响应式里，你会很自然地这么写：

```cpp
// 朴素响应式是"盲广播"——赋值就通知全部监听者，完全不看依赖关系
class Widget {
    mw::Property<int>   width_{"width"};
    mw::Property<int>   height_{"height"};
    mw::Property<int>   area_{"area"};
};

// 用朴素 Signal/Property 写 area 的联动，只能各连一条
w.width().changed().connect  ([&]{ w.area_ = w.width_.get() * w.height_.get(); });
w.height().changed().connect ([&]{ w.area_ = w.width_.get() * w.height_.get(); });
```

看起来没毛病，对吧？但你现在试着"在一帧里同时把 `width` 和 `height` 都改了"。`width` 变 → `area` 算一次；紧接着 `height` 变 → `area` 又算一次。中间这一次计算的 `area` 值根本不会有人看到，它是纯粹的浪费——这还是最简单的菱形，一旦你的派生属性图变成一张有共享上游的 DAG，盲广播就会让你在每一帧里做一堆根本没人关心的中间求值，毛刺就这么来的。

依赖感知的内核要干的事，本质上就是**让派生属性知道"我是从谁推出来的"**，然后在一次变更里只求值一次、并且按拓扑序求值，谁也不重复算。这套东西在工业界有现成的样板：Solid 的信号图、Vue 3 的响应式追踪、WinUI 的 `DependencyProperty` 依赖属性——它们都在这一关上花了大功夫。我们这次也要自己造一具，把"依赖收集 → 调度求值 → 去重 → 提交"这条管线走通，最终落在 `Signal<T>` / `Computed<T>` / `Effect` 这几个类上。

事情到这里还没完，响应式内核和主线一是要联动的：一个依赖感知的属性系统，最理想的写法是 `co_await` 一个异步源、把结果赋给一个属性、所有依赖它的派生属性自动按拓扑序刷新——异步和响应式是同一根神经的两端，这个联动点我们在阶段七和阶段八会亲手接上。

## 解剖学隐喻与八器官地图

讲完两条主线，我们退一步看全局。这套课叫"GUI 解剖学"，核心方法论就是一个解剖学的隐喻——任何一个 GUI 框架，无论它包装得多魔法，拆开来都是同一组器官。我们从零拼这具骨架，拼的就是这八个器官。

但光说"八个器官"太虚，你必须知道**每个概念到底落在哪个具体的类上、为什么那个类难到要花一整篇去折腾**，否则后面读到每一篇你都会觉得"这玩意儿怎么这么费劲"。下面这张表就是本系列的导航地图，也是你"受挫"时的对照表——读到任何一篇器官专题卡住了，回来看一眼这张表，你就知道当前这个类到底在解决哪一个问题、它难在哪。我们特意把"难点"单列一列，让你提前看见每一关的坎：

| # | 器官（概念） | 我们造的类 | 这一关难在哪（为什么要花一整篇） |
|:--:|:--|:--|:--|
| 1 | 窗口与绘制面 | `Window` + `DrawingSurface` | 一旦走 DirectComposition，"窗口"就不再是 GDI 画面——焊死 GDI 出口、管 D3D device/交换链、把帧挂上合成视觉树，全压在这两个类上 |
| 2 | 事件循环与输入 | `Application::run()` + `task<T>` + `ui_thread{}`/`background_thread{}` | 把 `GetMessage` 古董泵改成多源调度器，还要把协程恢复点焊进消息循环——恢复点归谁管、归哪条线程，是整篇命门 |
| 3 | 控件树与所有权 | `Widget` + `Container` + `WeakPtr<T>`/`WeakPtrFactory<T>` | 异步世界里的悬垂 this：协程挂起期间控件被销毁怎么办。所有权模型选错，后面每一篇都漏风 |
| 4 | 布局 | `LayoutStrategy`（concept）+ `FlexLayout` | 两遍 Measure/Arrange + 脏位失效，写错了就是每帧 O(n²) 重排 |
| 5 | 绘制与脏区 | `Painter`（包 D2D）+ DirectComposition 视觉树 | 保留模式合成器改写了"脏区"的含义：它不再是屏幕 blit 的范围，而是"这一帧重画哪个子矩形" |
| 6 | 对象模型与类型系统 | 类型注册 + `ComPtr` + `Result<T>` | COM 的引用计数怎么和 C++ 对象模型共存；属性/错误怎么统一成一套词汇 |
| 7 | 信号·响应式·绑定 | `Signal<T>` / `Property` / `Computed<T>` / `Effect` | 菱形依赖下的毛刺：盲广播让派生属性一帧算好几次，得造一个依赖感知的内核 |
| 8 | 异步与并发 | `task<T>` + 各种 awaitable + `std::stop_source` | 关机安全：连 CppWinRT 都在关机时泄漏协程——在途的 handle 怎么收尾 |

你看，"窗口"这个概念听上去再朴素不过，可一旦它具体化成 `Window` + `DrawingSurface` 这两个类，背后就藏着焊死 GDI、管 GPU 管线、绑合成树这一整套现代渲染的坎——这就是为什么器官一要花一整篇。同理，"事件循环"听着就是 `while(GetMessage)`，可它具体化成 `task<T>` + 一堆 awaitable 之后，就藏着协程恢复、关机安全这些工业级框架真要面对的问题。**概念越大路货，落到类上越藏着暗坑**——这张表就是帮你把暗坑提前看见。

等这具骨架拼完，我们还会拿它去和 WinUI3、Qt、ImGui 这几个工业标本正面比一次（那是后话，不在十五站主线里）。但本系列自身的目标，就是把这八个器官从零造一遍，让你手里有一具自己拼出来的、看得见每根骨头的标本。

### 两个值得现在就标记的"高潮站"

八器官里有两站是本系列真正的高潮，笔者在这里先给你打个标记，读到的时候多留个心眼，因为这两站的对照在中文技术圈基本没人系统地讲过。

第一站是**器官 5：保留模式 vs 即时模式的正面交锋**。保留模式（我们这具骨架、Win32、GTK、WinUI3 都是）是框架在内存里维护一棵控件对象树和一个脏区，每帧只重画变化的部分；而 ImGui 那种即时模式根本不留树，每一帧你重新用代码把整个界面"画"一遍，没有脏区这个概念。这两种范式各自的支持者在网络上吵了十几年，但很少有人把同一份交互在两套范式下亲手各实现一遍再摆在一起看。器官 5 那一篇我们就干这件事，它会带着脏区可视化和帧率对比，是本系列的旗舰演示。

第二站是**器官 6：对象模型的三国杀**。同一句"一个按钮到底是什么"——在 Win32 里它是"一个 HWND + 一个 WNDPROC"（句柄 + 回调），在 GTK 里它是一个 `GObject`（带引用计数、带属性系统、带信号），在 WinUI3 里它是一个 COM 组件（`IUnknown` 起家的接口体系），而在 ImGui 里它干脆"什么都不是"，按钮只是这一帧调用 `ImGui::Button()` 那一刻的存在。把这四种回答摆在一张桌子上比，你会突然理解为什么各个框架的 API 长成那个怪样子——它们对"对象"这个最根本概念的选择就不一样。

## 我们故意不做什么，以及为什么

教书有一个反直觉的道理：明确告诉你"什么不做"，往往比告诉你"做什么"更能让你看清一个设计。这一节就把我们这一系列里所有**故意不做**的取舍摆出来，每一条都给理由，因为没有理由的"不做"就是偷懒。

**第一，不上 C++20 Modules，老老实实用头文件。** 这一条估计会有朋友跳出来反对——"都 2026 了还不用 modules？" 我们也想用，但工具链真的还没到。即便到了 VS2026，MSVC 编译器前端对 `import std;` 的支持已经能编过，但 **IntelliSense 引擎是个独立的组件，它对 modules 的解析至今还是 experimental 状态**，红波浪线、补全失效、include 和 import 混用的灾难场面此起彼伏。再加上一个教学工程一旦开了 modules，构建顺序、`.ifc` 缓存、跨项目引用的复杂度会陡然上升，这些坑在 2026 这一代工具链上都还有未修的 ICE（内部编译器错误）。我们的目标是把 GUI 框架的器官讲清楚，不是替微软测工具链，所以这一代我们仍然走头文件。

**第二，不把 GDI 当作绘制面。** GDI 这东西在 Win32 教程里是老朋友了，但这一回它只能演配角。原因在于 GDI 是 CPU-only 的，没有硬件加速、没有像样的抗锯齿、没有彩色字体、没有逐像素 alpha——这些恰恰是"现代 GUI 栈"的及格线。更要命的是，我们选了 `WS_EX_NOREDIRECTIONBITMAP` 这条路（它是 DirectComposition 的命门），这个扩展样式的副作用就是**这个 HWND 上彻底画不出 GDI**——没有重定向位面，`BeginPaint` 拿到的就是个空壳，`TextOut` 直接哑火。所以 GDI 在本系列里只会在极个别需要和系统集成的地方客串一下，绝不是绘制主力。

**第三，不用 `shared_ptr` 挂控件树。** 这一条是所有权设计的硬约束。用 `shared_ptr` 挂子节点看起来"自动管理、省心"，但这套方案在"父子互相持有"的场景里是有名 double-free 隐患的——父持有子、子的某些回捕获了父，环状引用一来引用计数就永远归不了零，要么泄漏要么手忙脚乱地 `weak_ptr` 打补丁。我们这次明确走 **`unique_ptr` 持有控件树 + Chromium 风格的 `WeakPtrFactory`**。`WeakPtrFactory` 永远要声明在成员列表的**最后**——成员按声明逆序析构，factory 放最后就是最先析构，先于业务成员把所有 weak 引用作废，这样协程恢复时拿到的弱引用一定已经失效、返回空，不会撞上"半解构的尸体"。每个控件再配一个 `std::stop_source`，给协程发"别干了"的取消信号。这个选择的具体推导，阶段三会一环环拆给你看。

**第四，不依赖 Windows App SDK / WinRT。** 我们的立场是纯 C++ + 纯 Win32 + DirectX。Windows App SDK（WinUI3 那套）确实是"现代 Windows 应用"的官方叙事，但它把 C++/WinRT、MSIX 打包、SDK 运行时分发一锅端进来，每一项都是一整层抽象，正好把我们想摊开的"隐藏假设"重新盖回去。我们要的是把每一条消息、每一次合成、每一个 COM 引用计数都攥在自己手里看清楚，所以这一系列拒绝 WinRT，把 CppWinRT 只当**参照模型**来读（尤其是 `resume_foreground` 那一手协程调度，我们前面就提过要照着它的精神手搓）。

## 构建架构——CMake + Ninja，VS Code 里直接干活

环境钉死了，取舍也交代了，现在我们把工程的物理结构讲清楚。这是这一篇里最"工程"的一节，但也是最值钱的一节之一，因为它决定了后面十四个阶段我们怎么往前推进而不被工程结构拖累。

整套构建由**一份顶层 `CMakeLists.txt` + 一个 `CMakePresets.json`** 描述。我们用 **Ninja 生成器**（不是 Visual Studio 生成器）——一来 Ninja 快，二来只有 Ninja/Make 这类生成器才会产出 `compile_commands.json`，而这玩意儿是 IntelliSense 的命脉：没有它，编辑器不知道每个文件用什么宏、什么 include 路径编译，补全和跳转全废。所以在 VS Code 里配合 CMake Tools 扩展，选一下 preset 就能配置/构建/调试，全程不用离开编辑器。这份顶层 `CMakeLists.txt` 其实很短：

```cmake
# src/tutorial/modern_windows/CMakeLists.txt
cmake_minimum_required(VERSION 3.26)

project(ModernWindows LANGUAGES CXX)

# 全系列工具链基线：拉到最新标准 + UTF-8 源码/执行字符集 + UNICODE
# 放顶层，所有子目录的 target 自动继承，避免某个 stage 漏掉 /utf-8 导致中文乱码
add_compile_options(/std:c++latest /utf-8)
add_compile_definitions(UNICODE _UNICODE)

add_subdirectory(00_bootstrap)          # 工具链自检
add_subdirectory(stage01_window_surface)   # 每个阶段自包含，写好一个加一行
# 后续：add_subdirectory(stage02_message_coroutine) ...
```

几个决策值得讲一下。第一，`/std:c++latest` 和 `/utf-8` 放在顶层 `add_compile_options`，所有子目录的 target 自动继承——这样不会出现“某个 stage 忘了 `/utf-8`、中文注释编成乱码”这种灵异事件。第二，**每个 stage 自包含**：一个 `stageNN_xxx` 目录里自己有 `include/mw/`（接口）+ `src/`（实现）+ `main.cpp`，它的 `CMakeLists.txt` 用 `add_executable` 把这些源文件全编进一个 exe，再 `target_link_libraries` 挂上图形库——没有共享库，阶段之间互不依赖。这么做的代价是阶段间得 copy-forward（后一阶段把前一阶段代码拷过来再加新器官），但收益对教学来说更值：打开任何一个 stage 就能在一个目录里看到从头到尾的全部代码，想改某一阶段去实验也不影响别的阶段。

物理上长这样：

```
src/tutorial/modern_windows/
├─ CMakePresets.json             ← Ninja preset，cmake --preset default 配置
├─ CMakeLists.txt                ← 顶层：project + 全局选项 + add_subdirectory 各 stage
├─ 00_bootstrap/                 ← 工具链自检（已有）
│  ├─ CMakeLists.txt
│  └─ main.cpp
├─ stage01_window_surface/       ← 阶段一（自包含：接口 + 实现 + 入口全在这）
│  ├─ CMakeLists.txt             ← add_executable，把这个 stage 的全部源文件编进一个 exe
│  ├─ include/mw/...             ← 这个 stage 自己的器官接口
│  ├─ src/...                    ← 这个 stage 自己的器官实现
│  └─ main.cpp
├─ stage02_message_coroutine/    ← 阶段二（自带一份代码 + 新器官，自包含）
└─ build/                        ← 构建产物 + compile_commands.json 落这里（已 gitignore）
```

`CMakePresets.json` 也很薄，把“用什么生成器、产物放哪、导不导出 compile_commands”钉死：

```json
{
  "version": 4,
  "cmakeMinimumRequired": { "major": 3, "minor": 23, "patch": 0 },
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Ninja (MSVC) · Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_MAKE_PROGRAM": "${env:ProgramFiles}/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
      }
    }
  ],
  "buildPresets": [
    { "name": "default", "configurePreset": "default" }
  ]
}
```

⚠️ 这里有个一定会有人卡的点：`CMAKE_MAKE_PROGRAM` 那行是**指给 CMake 一个真的 `ninja.exe`**。为什么要指定？因为 CMake 默认会去 PATH 上找 `ninja`，而如果你机器上装过 Chromium 的 `depot_tools`，它会在 PATH 里放一个**没有 `.exe` 后缀的 `ninja` 壳子**（实际是个脚本），CMake 一执行就报 “inappropriate file type or format”，配置直接挂。解法就是像这样用 `CMAKE_MAKE_PROGRAM` 钉死一个真 ninja——Visual Studio 自带一个（路径见上，`Community`/`Professional`/`Enterprise` 按你装的改）。如果你机器上没有这类假 ninja，这一行可以删掉，让 CMake 自己找。

至于 `00_bootstrap`，它的全部存在意义就是“工具链自检”——确认 CMake+Ninja 链路通、`/std:c++latest` 生效、MSVC 14.5x 能编出一个能跑的 exe。它的 `main.cpp` 里放了两个 `static_assert` 卡 C++23 的特性宏（`__cpp_lib_print`、`__cpp_lib_expected`），标准没拉上去就编不过——所以它不只是 "Hello World"，而是带断言的自检。在它编过之前，后面所有器官都是空中楼阁。所以把它当"开机自检"：每一篇器官专题动手之前，如果你连 `00_bootstrap` 都编不过，先别往下走，回去修环境。全局的 `/std:c++latest`、`/utf-8`、`UNICODE` 都在顶层 `CMakeLists.txt` 里对 `00_bootstrap` 生效——这就是整个系列的工具链基线参照。

为了让这一篇不至于全是 talk，我们把后面阶段一会真刀真枪动的那段"绘制面骨架"先剧透一小段，让你感受一下这具骨架的骨头有多硬。注意看里面的扩展样式和交换链类型，这两个就是整个渲染基线的命门：

```cpp
#include <d3d11.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// 第 1 步：开一个 D3D11 device——Direct2D 1.1 的真正绘制面要挂在一个 D3D device 上
ComPtr<ID3D11Device> d3d;
D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
                  D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, nullptr, 0,
                  D3D11_SDK_VERSION, &d3d, nullptr, nullptr);
ComPtr<IDXGIDevice> dxgi;
d3d->QueryInterface(IID_PPV_ARGS(&dxgi));

// 第 2 步：D2D 1.1 的 device + device context（注意是 1.1，不是老的 HwndRenderTarget）
ComPtr<ID2D1Factory1> factory;
D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf());
ComPtr<ID2D1Device> d2dDevice;
factory->CreateDevice(dxgi.Get(), &d2dDevice);
ComPtr<ID2D1DeviceContext> dc;
d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);

// 第 3 步：一个专为 DirectComposition 服务的 FLIP_DISCARD 交换链
DXGI_SWAP_CHAIN_DESC1 desc{
    .Format       = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc   = {1, 0},
    .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount  = 2,
    .Scaling      = DXGI_SCALING_STRETCH,
    .SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD,   // 现代合成栈的唯一正确选择
    .AlphaMode    = DXGI_ALPHA_MODE_PREMULTIPLIED,
};
ComPtr<IDXGISwapChain1> swap;
// 注意：DComp 路线下用 CreateSwapChainForComposition，不是 CreateSwapChainForHwnd
dxgiFactory->CreateSwapChainForComposition(d3d.Get(), &desc, nullptr, &swap);

// 第 4 步：DComp 视觉树，把交换链当作内容挂上去
ComPtr<IDCompositionDevice> dcomp;
DCompositionCreateDevice(dxgi.Get(), IID_PPV_ARGS(&dcomp));
ComPtr<IDCompositionVisual> visual;
dcomp->CreateVisual(&visual);
visual->SetContent(swap.Get());
ComPtr<IDCompositionTarget> target;
dcomp->CreateTargetForHwnd(hwnd_, /*topmost=*/TRUE, &target);
target->SetRoot(visual.Get());
dcomp->Commit();
```

配套的窗口创建那一句，关键是这个扩展样式：

```cpp
// WS_EX_NOREDIRECTIONBITMAP 是 DirectComposition 的命门
// 副作用：这个 HWND 上 GDI 彻底画不出来——所以本系列绘制全程走 D2D/DComp
HWND hwnd_ = CreateWindowExW(
    WS_EX_NOREDIRECTIONBITMAP,
    kClass, L"ModernWindows",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
    nullptr, nullptr, hInstance, nullptr);
```

⚠️ 这里千万要心里有数：`WS_EX_NOREDIRECTIONBITMAP` 去掉的是这个 HWND 的"重定向位面"（redirection surface），也就是 GDI 那套绘制管线的画布。DWM 现在仍然会替你画标题栏和边框（非客户区归它管），但你一旦在客户区上 `GetDC`/`BeginPaint` 再 `TextOut`，得到的就是一片虚无——这不是 bug，是这套样式的设计意图。我们之所以明知有这些副作用还要选它，是因为它换来的是 DirectComposition 保留视觉树、零闪烁、逐像素 alpha 合成——这套东西是现代 Windows 应用视觉质感的根，没有它，Mica 背景、圆角、平滑动画全都做不利索。

最后这块骨架里其实还藏着本系列统一的错误模型，我们也先露个脸，让你对后续每个器官怎么报错有个预期。整个框架的错误统一走 `std::expected<T, FrameworkError>`，再配两个适配函数把 HRESULT 和 Win32 错误码都收进来：

```cpp
// stage01_window_surface/include/mw/platform.h —— 整个框架的错误模型
#include <expected>
#include <windows.h>

namespace mw {

struct FrameworkError {
    enum class Kind { Hresult, Win32, Panic } kind;
    unsigned long code;   // HRESULT 值，或者 GetLastError() 的值
};

template <typename T> using Result = std::expected<T, FrameworkError>;

inline std::expected<void, FrameworkError> from_hresult(HRESULT hr) {
    if (SUCCEEDED(hr)) return {};
    return std::unexpected(
        FrameworkError{FrameworkError::Kind::Hresult, static_cast<unsigned long>(hr)});
}

inline std::expected<void, FrameworkError> from_win32() {
    DWORD e = ::GetLastError();
    if (e == 0) return {};
    return std::unexpected(
        FrameworkError{FrameworkError::Kind::Win32, static_cast<unsigned long>(e)});
}

}  // namespace mw
```

这样一来，后面任何一个器官里的 API 调用，我们都可以写成 `co_await` 友好的 `Result<T>` 链式风格，错误沿着协程自动往上冒，既不用 `throw`（COM 和 Win32 的世界里异常是灾难），也不用满屏幕 `if (FAILED(hr))`。这套错误模型会和主线一的协程、主线二的响应式一起，构成整具骨架的神经系统。

## 十五站路线图

骨架讲清楚了，我们来看接下来到底要走哪十五站。这张表是整个系列的进度地图，每一站都对应一个或几个器官、落到一组具体的类上，每一站都有一个"跑得起来的交付物"——我们这系列不讲完一阶段不撒手，每篇结尾都有一个能双击运行的 exe。

| 阶段 | 主题 | 对应器官 | 一句话交付物 |
|:--:|:--|:--|:--|
| 01 | 窗口与绘制面 | 器官 1 | 一个 `WS_EX_NOREDIRECTIONBITMAP` 窗口 + DComp + D2D1.1，能合成出第一帧彩色画面 |
| 02 | 消息循环与协程 | 器官 2 | `GetMessage` 循环 + `PostMessage` 恢复协程，跑通一个 `co_await` 异步任务 |
| 03 | 控件树与所有权 | 器官 3 | `unique_ptr` 控件树 + `WeakPtrFactory` + `stop_source`，挂上若干子控件 |
| 04 | 布局 | 器官 4 | Concepts 约束的 `LayoutStrategy`，Measure→Arrange 两遍自动排版 |
| 05 | 绘制与合成——保留视觉树 | 器官 5 | 脏区合进 DComp 视觉树，做一次"只重画脏区"的局部刷新 |
| 06 | 对象模型与样式 | 器官 6 | 控件类型系统 + 样式（主题/CSS 化的雏形），COM `ComPtr` 全员就位 |
| 07 | 响应式与绑定 | 器官 7 | 依赖感知的 Property + 数据绑定，菱形依赖不再起毛刺 |
| 08 | 异步与并发 | 器官 8 | 协程 + 后台任务 + `stop_source` 取消，跑一个大文件异步加载 |
| 09 | 文本与 IME（CJK） | 器官 1/5 延伸 | DirectWrite 排版 + TSF（`ITextStoreACP`），中文输入法走通 |
| 10 | 指针输入与手势 | 器官 2 延伸 | WM_POINTER 统一鼠标/笔/触摸 + DirectManipulation 手势 |
| 11 | DPI / 主题 / Mica 背景 | 工程化 | Per-Monitor V2 + 深色模式 + Mica 背景，多显示器混合 DPI |
| 12 | 控件集 | 综合 | Button / Label / Edit / ListView 等一组可复用控件 |
| 13 | 剪贴板与拖放 | 系统集成 | 跨进程剪贴板 + OLE 拖放，跟系统其它应用互通 |
| 14 | 无障碍（UIA） | 一等器官 | UI Automation 作为一等公民，让 Narrator 能读我们的界面 |
| 15 | 安全 / 打包 / 性能分析 | 收尾 | manifest/UAC + 打包 + 性能 profile，给骨架一个"完结撒花" |

这张表你最好存下来，因为后面每开一篇专题，我们都会在开头回指自己处于哪一站、上一站交了什么、这一站要接什么、它落在哪几个类上。阶段一到八是器官主轴，按"窗口→事件→树→布局→绘制→对象模型→响应式→异步"这个内在顺序往下走，每一步都依赖前面；阶段九到十一是横向的"系统级刚需"（中文输入、指针、DPI 主题），它们会把前面的器官顶到真实世界的复杂度上；阶段十二到十四是把骨架补成"能用"的样子（控件、剪贴板、无障碍），其中无障碍我们专门列成一个器官而不是事后补丁；阶段十五收尾。

## 怎么读本系列，以及你需要先会点什么

到这里，动机有了，路线有了，环境钉死了，我们最后说一下"怎么读"和"前置知识"这件事，免得你一头扎进阶段一发现有些前置没补上。

先说清楚一件让人安心的事：**本系列不假设你读过任何 Win32 教程**。HWND 是什么、消息循环怎么转、`UNICODE` 编译下字符串为什么带 `L` 前缀——这些 Win32 概念第一次用到时，我们都会就地用一两句话讲明白，不会甩一句“别处讲过了”让你自己去找。已经熟 Win32 的朋友，这些小段跳过即可；完全没碰过的朋友，跟着读就能把词汇一点点攒起来，不需要先去补课。想深挖某一步时，本系列会把用到的机制就地讲透，不需要先去别处补课——本系列的定位就是从零开始、自包含。

阅读路线也很直接：序章读完，直接进阶段一开窗、阶段二接协程，之后按阶段号顺序往下走就行。中间遇到哪个 API 看不懂，本系列用到哪讲哪，缺什么就地补什么，不要求你先把别处的手册啃完。

本系列**不预设你读过任何其它从零造框架的教程**。每一篇器官专题都会自包含地把"这个器官是什么概念、它落在哪个类上、为什么难"讲清楚，不需要你事先知道别处怎么造的。

前置的官方文档，笔者给你挑了几篇最该先读的，都是 `learn.microsoft.com` 上的入门货色，不厚，但读完你对 Win32 的基本词汇就齐了：

- [Win32 编程入门（Get Started with Win32）](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/) —— 微软官方的 Win32 入门系列，窗口、消息循环、WndProc 一条龙，中文版。
- [窗口对象概览（About Windows）](https://learn.microsoft.com/zh-cn/windows/win32/winmsg/about-windows) —— 把 HWND、父子关系、owning 关系讲得最清楚的一篇。
- [DirectComposition 概览](https://learn.microsoft.com/zh-cn/windows/win32/directcomp/directcomposition-portal) —— 我们整套渲染基线的官方说明，建议至少把"概览"那一节扫一遍。

## 完结撒花——以及，上号

写到这里，序章就差不多交代完了。我们重新捋一遍这具叫 ModernWindows 的骨架要长成什么样：它长在纯 Win32 + 现代 DirectX 这套地基上，用 C++20 协程给消息循环装上记忆，用依赖感知的响应式内核修掉盲广播的菱形毛刺，用 `unique_ptr` + `WeakPtrFactory` 把所有权钉死，用 `std::expected` 把错误收得干干净净，用 Direct2D 1.1 + DirectComposition 把每一帧像素交给 GPU 合成——所有这一切由一份 `CMakeLists.txt` 描述、`cmake --preset` 生成 `.sln` 交给 VS 调试，每加一个器官就多一个 `add_subdirectory`，一路累加。

它不是产品，它是标本。它的意义不在于让你拿它去写一个能上架的应用，而在于等你读完这十五站，再看 WPF、看 WinUI3、看 Qt、看任何一个 GUI 框架，你眼里都不再是魔法，而是一组你已经亲手造过一遍的器官的变奏。

那就这样，工具链自检的 `00_bootstrap` 你确认能编出 `Hello World` 之后，我们就直接进阶段一——开窗、挂 DComp、合出第一帧。上号。

---

**相关资源**

- [Microsoft Learn · Win32 编程入门（learnwin32）](https://learn.microsoft.com/zh-cn/windows/win32/learnwin32/)
- [Microsoft Learn · 窗口对象概览（About Windows）](https://learn.microsoft.com/zh-cn/windows/win32/winmsg/about-windows)
- [Microsoft Learn · DirectComposition](https://learn.microsoft.com/zh-cn/windows/win32/directcomp/directcomposition-portal)
- [Microsoft Learn · Direct2D 概览](https://learn.microsoft.com/zh-cn/windows/win32/direct2d/the-direct2d-api)
- [Microsoft Learn · IDXGIFactory2::CreateSwapChainForComposition](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1-2/nf-dxgi1-2-idxgifactory2-createswapchainforcomposition)
- [Microsoft Learn · WS_EX_NOREDIRECTIONBITMAP（扩展窗口样式）](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- [MSDN Magazine（存档）· Kenny Kerr：用 Windows 合成引擎做高性能窗口分层](https://learn.microsoft.com/en-us/archive/msdn-magazine/2014/june/windows-with-c-high-performance-window-layering-using-the-windows-composition-engine) —— DirectComposition + `WS_EX_NOREDIRECTIONBITMAP` 这套玩法的经典参考文献，强烈推荐。
