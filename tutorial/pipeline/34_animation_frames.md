---
title: 动画与帧调度
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 动画与帧调度

[屏幕、帧与垂直同步](02_screen_frame_vsync.md)的结尾咱们立过一个问题：动画想要匀速地动，咱们就不能指望“消息来了就画”，得按屏幕的节拍自己安排重画的时机，节拍器怎么搭，当时咱们把它留给了本篇。今天咱们就来把它接住。

什么样的定时器当得了动画的节拍器，一帧的预算怎么花，咱们挨个摊开。贯穿本篇的主线也只有一条，咱们要让画面动起来，错误的姿势有它的下场，正确的姿势也有它的位置。

按编号的顺序排下来，本篇的上一篇是[《透明、阴影、模糊》](33_transparency_effects.md)。[《DWM：重定向表面与窗口合成》](30_dwm_composition.md)的结尾已经把 visual 树的名字立住了，本篇只借它的名字和最小的一套骨架，全套的搭建细节，咱们到[《DirectComposition：visual 树》](31_directcomposition.md)里看。

## WM_TIMER 为什么扛不动动画

咱们从人人都会写出的那个版本看起。手头有一块会动的界面时，最省事的做法就是拿 SetTimer 起一个定时器，每隔十几毫秒推进一下自己的状态，再补上一句失效的登记。它的骨架长成下面这样，这是讲解用的示意，咱们没有在本仓库里编译过：

```cpp
// The metronome everyone reaches for - skeleton, not compiled here.
const UINT_PTR kAnimTimer = 1;
SetTimer(hwnd, kAnimTimer, 16, nullptr);   // "16 ms" - as if we got it

// In the window procedure:
case WM_TIMER:
    if (wParam == kAnimTimer) {
        advance_animation();               // move the state one tick
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    break;
```

咱们看着它挺像那么回事，微软的公开文档却早就把它的底细交代了。

头一处毛病出在分辨率上：按 SetTimer 文档的说法，系统定时器的默认分辨率约为 15.6 毫秒，咱们在参数里写的 16，系统实际上兑现不了，到点的间隔围着 15.6 毫秒打转，折算下来的每秒拍数大约是 64，拍与拍的间隔还并不均匀。

第二处毛病出在消息的脾气上：文档写明 WM_TIMER 是一条低权重的消息，只在队列里没有别的消息时才被生成，而多次到点也积不成几条，而是并成一条发过来。队列忙起来的时候，咱们丢的就是拍子。

第二篇《屏幕、帧与垂直同步》里立过两座钟的说法，放到咱们这儿就是屏幕一座、定时器一座，咱们把上面两处毛病放回去，形状就都现出来了。60 Hz 的屏幕每 16.7 毫秒取一次内容，定时器却拍着自己的 15.6 毫秒走，两座钟的节拍对不上，DWM 取内容的时刻与咱们更新的时刻一会儿赶上了、一会儿错过了，咱们看到的画面就一顿一顿。

咱们把卡顿感的来源说得更准一点：它多半来自拍子没对齐，画的快慢倒在其次。第二篇管节奏匀称的说法叫帧节奏，本篇治的就是它。

定时器的分辨率也不是提不上去。winmm 的 timeBeginPeriod(1) 能把系统定时器的节拍提到 1 毫秒一档，SetTimer 也跟着变准了，不过文档也写明了代价，全局的功耗，系统层面也不鼓励咱们这么干。何况它治得了分辨率的病，治不了低权重和合并的病：队列忙起来的时候，WM_TIMER 还是照样垫底的。咱们的节拍器要想稳，还得换个对准屏幕的源头。

## 一帧的预算，以及超支的样子

预算的数字咱们在第二篇算过：60 Hz 的屏幕一个刷新周期约 16.7 毫秒，咱们的动画想跟上它，一帧的全部活儿就得装进这 16.7 毫秒里。

活儿都有哪些呢？咱们数得出来的有推进状态、组帧、画进表面，外加 30 篇讲过的合成那一程。合成拥堵的时候，头一个受害的就是动画，这一点 30 篇讲开销时咱们记下过。

超支的下场咱们在第二篇也见过：开了垂直同步之后，帧率的数字只在整倍数之间跳，60 掉到 30 是一步到位的。用户看到的跳帧，观感上的说法就是卡。

这里咱们把机制再往前推半步：卡的观感主要来自忽快忽慢。人眼对匀速的低帧率有相当的容忍，对节奏的抖动却很挑剔，一拍 10 毫秒一拍 25 毫秒的动画，看着比稳定 25 毫秒一拍的还要糟。所以帧调度的目标有两层：一帧的活儿装进预算，拍与拍的间隔还要匀。

咱们想确认拍子匀不匀的时候，光靠眼睛是说不清的，第二篇介绍过的 PresentMon 能把每个呈现事件的时间戳记下来，咱们拿它看间隔的分布，间隔抖不抖也就一目了然了。量节奏的家什和量平均帧率的家什是同一件，看的角度却要换到间隔的分布上。

## 让画面动起来的三条路

节拍器的问题立住了，咱们把视野放大一层：拿到一个会动的需求时，驱动它的方式其实有三条路，走哪条路取决于动画的本性。三条路咱们各认一遍，本篇后半段的骨架会挑着走。

咱们头一个要认的是失效驱动，也就是 UI 的常态。界面上的画面绝大多数时刻是静止的，状态变了才需要重画，[失效、脏区与重绘](05_invalidation_dirty_region.md)整篇讲的机制就是为它设计的。咱们把动画放进这个框架，等于宣告了状态每一拍都在变，于是咱们一拍报一次失效、跟着画一次新画面。零散的、跟着输入走的动画，比如按钮按下的反馈，走失效驱动的路子最合身。

另一条路走的是连续帧循环，游戏的活法。第二篇的游戏骨架就是一个 while 循环，一圈一圈地转，每一圈画的都是一帧新画面，末了的 Present 等的就是垂直同步。窗口程序没有那样的循环，咱们等价的写法是让节拍器每拍主动地报一次失效，让 WM_PAINT 一拍一拍地来。全屏连续变化的内容（粒子和视频那一类）只能走它。

咱们要认的第三条路是合成器动画，30 篇的结尾咱们见过它的预告。画的内容只画一次就够了，动的只是呈现它的参数：位置、透明度、缩放。参数的每一拍插值由 DWM 在合成时完成，而应用完全不参与每一帧。系统的最小化动画吃的就是同一碗饭，30 篇里咱们亲眼看过：动画全程应用一帧都没画。应用侧的入口是 DirectComposition（下文简称 DComp）的属性动画，本篇的骨架部分咱们上手试它。

## 连续帧循环的节拍器

连续帧循环最要紧的一环是节拍器。SetTimer 已经指望不上了，咱们手里能对准节拍的现成家什有两件：一件是高分辨率的等待定时器，另一件是 DWM 自己给的 DwmFlush。

咱们要认的头一件家什是等待定时器，造它的函数叫 CreateWaitableTimerExW。Windows 10 的 1803 版起，它多了一个 CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 标志，文档写明的说法是，到点的误差能压到微秒一档。

咱们拿它当节拍器，再配上让消息循环照常呼吸的 MsgWaitForMultipleObjectsEx，它的骨架长这样（讲解示意的骨架，咱们没有在本仓库编译验证）：

```cpp
// A high-resolution waitable timer as the animation metronome.
// Skeleton for illustration - not compiled in this repo.
HANDLE timer = CreateWaitableTimerExW(
    nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
    TIMER_MODIFY_STATE);

LARGE_INTEGER due{};
due.QuadPart = -16667;   // 100-ns units: roughly one 60 Hz tick
SetWaitableTimer(timer, &due, 16, nullptr, nullptr, FALSE);

// Inside the message loop:
DWORD wake = MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, 0);
if (wake == WAIT_OBJECT_0) {
    advance_animation();                    // one tick of state
    InvalidateRect(hwnd, nullptr, FALSE);
}
// else: a message arrived - run the pump as usual.
```

骨架里有两处值得咱们停下来看。due.QuadPart 传的是负数，等待定时器的约定是负数按相对时间算，它认的单位是 100 纳秒，-16667 折出来大约是一拍的 16.7 毫秒。

咱们再看 MsgWaitForMultipleObjectsEx 那一行，它是这套写法的要害：它等的要么是定时器到点，要么是消息的进队，所以两头谁来了都醒，动画的节拍和消息的呼吸就都不耽误了。

咱们认的另一件家什叫 DwmFlush，30 篇里咱们见过它：它等到 DWM 把队列里的呈现请求处理完了才返回。拿它当节拍器的用法是这样的：画完了一帧就调一句，函数返回的时候合成刚落地，紧接着画的才是下一拍，节奏就跟合成的班车对上了。

它的代价是等待的工夫里线程被占住，消息的循环得另想办法转，两件家什是各管一摊的，您按自己程序的形状挑。DComp 的世界里还有一位 DCompositionWaitForCompositorClock，名字里就写着等合成器的时钟，文档给它的定位正是动画对节拍，笔者没有逐机验证过它的手感，您真要用请以文档为准。

画具要是换成了 DirectX，咱们反而不用另造节拍器：Present 的第一个参数 SyncInterval 传 1，翻页等的也就是下一个消隐期了。第二篇讲过的机制在[交换链与呈现](27_dxgi_swapchain.md)里落成完整的 API，咱们不重走。

## 缓动：匀速为什么看着假

节拍稳了，咱们看动的东西本身。最简单的动画是线性插值：咱们设一个起点 a 和一个终点 b，进度 t 走的区间是 0 到 1，任意时刻的值就是 `v = a + (b - a) × t`。数学上它是挑不出毛病的，咱们的眼睛却觉得别扭。

咱们看到的匀速面板就是这样一副机械的样子，机械感的来源在两头：起步的那一瞬速度会从零直接跳到满速，收尾的那一瞬又从满速直接归零，真实世界里没有东西是这么动的。

咱们修两头用的就是缓动曲线。进场动画的常客是三次的 ease-out，它的式子是 `v = a + (b - a) × (1 - (1 - t)³)`。

t 小的时候导数大，所以起步冲得快，t 逼近 1 的时候曲线越来越平，收尾稳稳地落住。它适合进场的道理也是物理的：物件受着摩擦滑到您面前的时候，速度本来就是衰减的，眼睛对衰减的减速有预期，曲线给的减速跟预期对上了，观感就顺了。

咱们再看 ease-in，它的脾气正好反过来，它的收尾最猛，适合退场的场面——要走的东西就让它走得利索。两头都要照顾的场合还有 ease-in-out，它的脾气是起步加速、收尾减速，多项式 `3t² - 2t³` 的形状就是它的代表。

咱们把视线挪到 Web 那边，ease-out 在那儿也占着默认的位置：CSS transition 的缺省曲线 ease，本身就是一条起步快、收尾慢的贝塞尔。同行们在无数界面上试出来的经验，与咱们的式子是同一个方向，两边也就互为印证了。

## 改布局，还是改 transform

同样的滑动面板，咱们动它的方式有两种，代价上却差着一整条的管线。

头一种咱们动的是布局：面板的位置是布局算出来的，咱们每一拍都重跑一遍布局，布局把它挪到新的格子上，跟着来的才是失效、重画、合成。另一条主线教程（教咱们亲手造窗口和控件的一条线）的第一篇[《具象 · 脱掉封装，把窗口立起来》](../gui/01_naked_win32.md)里咱们见过的 layout_controls 链路，每一拍都走着它的全程。GUI 框架里改一个 left 属性触发的就是它。

另一种咱们动 transform：咱们的画面只画进表面一次，动的只是呈现它的位置参数，按新位置拼接的活归合成器，拿的是现成的照片。30 篇讲拖动窗口时咱们看过同款机制：照片的一个像素都没变，变的只是相框的摆位，GPU 的搬运替下了 CPU 的重画。

咱们把同样的思路搬到应用侧：改 transform 的做法不触发布局，重画也一并省了，每一拍要跑的只剩合成器那一程。

Web 前端攒了多年的通识也站在同一边：CSS 动画里 transform 和 opacity 是合成器友好的属性，动画的执行也能交给合成线程，改 top、left、width 的下场就是回流加重绘。两边的用词不一样，而机制却是同一副：咱们能只动呈现参数的事，就不必劳驾布局和绘制了。

边界咱们也交代清楚：transform 省的是布局和绘制两程，合成的那一程是躲不掉的。半透明的面板在别的窗口上滑过的时候，逐像素的混合每帧照做，30 篇讲过的合成开销一分不少。另外 transform 动的是刚体，面板若要随进度改它的尺寸、圆角，那些变化还是得回到绘制的路上去。

## 一拍里的变化，攒成一次失效

走失效驱动的动画，还有一笔工要咱们自己做。第五篇《失效、脏区与重绘》讲过平台层的攒批：失效干的活是登记，多次的登记并成一份更新区域，WM_PAINT 等队列空了才来。

动画框架在应用层做的是咱们同款的事：一拍的工夫里，十几个动画的属性各自往前推进，新旧的位置各不相同，框架把变化全都攒了起来，最后一并成一次失效的登记。登记的活便宜，重画的活贵，所以攒批图的是把贵的活从十次压到一次。

```cpp
// One tick: advance every animation, coalesce, invalidate once.
// Skeleton for illustration - not compiled in this repo.
void animation_tick(HWND hwnd) {
    RECT dirty{0, 0, 0, 0};
    for (Animation& anim : active_animations) {
        const RECT old_rect = anim.current_bounds();
        anim.advance_one_tick();
        const RECT new_rect = anim.current_bounds();
        UnionRect(&dirty, &dirty, &old_rect);   // where it was
        UnionRect(&dirty, &dirty, &new_rect);   // where it is now
    }
    InvalidateRect(hwnd, &dirty, FALSE);        // one registration
}
```

咱们要把旧的位置和新的位置都并进去，因为重画要擦掉的是旧的位置，要画上的是新的位置，缺了旧的那一块，上一拍的身影就留在屏幕上了。

咱们做单个动画的时候，攒批的用处看着不大，界面上几十个动画同跑的时候，一次失效和几十次失效的差别就实打实了。第五篇说过攒批是失效通路天生的脾气，框架层做的只是不跟它对着干。

## 同一个滑动面板的两版骨架

收尾咱们把两条路各写一个骨架，素材是同一个需求：一块宽 300 像素的面板要从窗口的左边滑进来，动画的全长是 300 毫秒，缓动用三次的 ease-out。

重画版走的是失效驱动，节拍器用的是前面那套高分辨率的等待定时器，面板的每一拍都画在新的偏移上（讲解示意的骨架，咱们没有在本仓库编译验证）：

```cpp
// Repaint version: the panel is drawn at a new offset every tick.
// Skeleton for illustration - not compiled in this repo.
struct Slide {
    float progress = 0.0f;   // 0..1

    float offset_x() const {  // cubic ease-out: fast start, soft landing
        return -300.0f + 300.0f * (1.0f - powf(1.0f - progress, 3.0f));
    }
};

void panel_tick(HWND hwnd, Slide& slide) {
    const int old_x = static_cast<int>(slide.offset_x());
    slide.progress = min(1.0f, slide.progress + 16.7f / 300.0f);
    const int new_x = static_cast<int>(slide.offset_x());

    // Sliding in from the left: old_x <= new_x, so the union is
    // one rectangle covering both the trail and the new position.
    RECT dirty = {old_x, 100, new_x + 300, 400};
    InvalidateRect(hwnd, &dirty, FALSE);
}
// WM_PAINT then draws the panel at slide.offset_x(), full frame as usual.
```

DComp 版是咱们把面板画进表面，剩下的活就交给合成器了。属性动画的入口是 DCompositionCreateAnimation 造出来的 IDCompositionAnimation，咱们往里面描述值随时间怎么走，再把动画挂到 translate transform 的 OffsetX 上，Commit 一交了之后，每一拍的插值全归了 DWM，咱们这边连一条 WM_PAINT 都不会再收到（同样是讲解骨架，咱们没有编译验证它）：

```cpp
// DirectComposition version: animate the transform, not the content.
// Skeleton for illustration - not compiled in this repo.
ComPtr<IDCompositionAnimation> anim;
DCompositionCreateAnimation(anim.GetAddressOf());

// Offset-x ramps from the base value (-300) to 0 over 300 ms.
// Ease comes from the same object: AddCubic appends a cubic-bezier
// shaped segment, AddSinusoidal a sinusoidal one - parameter details
// are the documentation's business, not this skeleton's.
anim->AddLinear(0.0, 0.3, 0.0);

ComPtr<IDCompositionTranslateTransform> translate;
device->CreateTranslateTransform(translate.GetAddressOf());
translate->SetOffsetX(-300.0f);        // base value
translate->SetOffsetX(anim.Get());     // hand the curve to the animator

visual->SetTransform(translate.Get());
target->Commit();   // from here on, DWM interpolates every frame
```

挂曲线的那两行值得咱们多看一眼：SetOffsetX 的重载有两个，其中的一个收定值，另一个收的才是 IDCompositionAnimation。DirectComposition 的标量属性普遍是这么成对设计的，动画在它的眼里只是属性的另一种赋值方式。WinRT 侧的 Windows.UI.Composition 是同一思想的近亲，关键帧动画和现成的缓动函数都替您包好了，本篇的正文不展开它，您混个脸熟就行。

两版的差距咱们摆成一张表，您对着看更直白：

| | 重画版 | DComp transform 版 |
|:--|:--|:--|
| 每一帧谁在算 | 应用，节拍器一拍一拍地催 | DWM，应用只交一次曲线 |
| 每拍应用的活 | 推进状态、报失效、整帧重画 | 无 |
| 动的内容怎么上屏 | 失效、WM_PAINT、贴帧、合成 | 合成器按 OffsetX 插值 |
| 前置家什 | 双缓冲加节拍器 | visual 树加 D3D 设备，《DirectComposition：visual 树》那篇的主场 |

咱们把两版的差距再摆开看：不只在 CPU 的工夫。重画版每一拍走过的都是完整的失效加重画的链路，DWM 那头每帧取到的是全新的照片，而 DComp 版的照片始终是同一张，动的只是摆位。

咱们需要交代的前置是：DComp 骨架省略了整套设备的搭建和 WS_EX_NOREDIRECTIONBITMAP 的窗口改造，那些活是[《DirectComposition：visual 树》](31_directcomposition.md)的正题，本篇的骨架只管把动画挂上去的形状画对。

## 您可以自己动手看的一处

您拿本文开头的 SetTimer 骨架真跑一个会动的小球，再用高分辨率的等待定时器换掉节拍器，各跑十秒的时长。小球的速度不用快，您盯着看的不是快慢，看的是匀不匀。换装前后的差别，在多数机器上肉眼就分得出来的。

要是您还想看数字，咱们就请 PresentMon 出场，把两版的呈现事件各录一段，间隔的分布一对，拍子的抖动就有了读数：定时器版的间隔多半散得开，换过之后就该集中了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得回去再看一遍。

1. WM_TIMER 的两处毛病分别是什么？哪一处出在分辨率，哪一处出在消息的脾气？
2. 60 Hz 屏幕上一帧的预算约是多少？超支之后帧率为什么直接腰斩？
3. ease-out 的式子是什么？它适合进场的道理是什么？
4. 改布局的动画每拍要走的链路是哪几程？transform 版省掉了其中的哪几程？
5. 攒批把哪一步从多次压成一次？平台层的哪种机制在做同款的事？
6. DComp 属性动画里，应用交出去的是什么？每一帧的插值由谁完成？

## 下一篇

按[篇目索引](index.md)的登记，下一篇的题目是[《架构对照：场景图、显示列表、即时模式》](35_render_architectures.md)。它要做的事跟本篇的收尾正好衔接：咱们这一路把 GDI、Direct2D、Skia、DComp 各家画具一件件上手摸过，保留模式与即时模式、场景图与显示列表的对照是它的正题。
