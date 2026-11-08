---
title: 失效、脏区与重绘
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 失效、脏区与重绘

[第一篇](01_status_to_pixels.md)里咱们把一次界面更新从鼠标松开追到了像素上屏，中间有一程却走得急：报失效的那一段，`InvalidateRect` 的三个参数各认了一句，更新区域露了个面，队伍就往下开了。咱们这一篇回到那里，把那一程摊开成了一整篇。失效怎么报、脏区怎么攒、`WM_PAINT` 什么时候才肯来、来了之后脏区怎么交到您手上，咱们一段一段看，篇尾还给您备了一件小仪器：把脏区的形状涂上颜色，它就自己站到了屏幕上。

咱们对失效并不陌生。这套教程其实分两条线，讲框架层的主线在 gui 目录里，讲平台层的本线在 pipeline 目录里，您眼下读的就是本线的一篇。主线第六篇（[属性与重画](../gui/06_property_repaint.md)）把记得报失效这件事做成了框架层的自动接线：写值的调用沿父链一路冒到了根上，最后落到的就是 `InvalidateRect(hwnd_, nullptr, TRUE)` 那一行。本篇要看的正是它脚下平台层的整套机关：报进去的东西登记在哪，登记了之后由谁来消费，消费完了又怎么销掉。本线的第一篇还留过一句话，说更精细的脏区算法留到第五篇细讲，眼下您读到的正是它。

## InvalidateRect：三个参数各管一件事

咱们把起点放回第一篇里那句手工补丁上。demo 是从[主线第一篇](../gui/01_naked_win32.md)起一路跟到本线的那个示例程序，窗口里有一行它自己画的字，字的内容存在 `g_drawn_text` 里，占的画面记在 `g_drawn_rect` 里。那行字之所以不动，是因为 `lstrcpynW` 拷完了字符串就停了手，给它补上了这一句，通路就接上了：

```cpp
InvalidateRect(hwnd, &g_drawn_rect, TRUE);
```

窗口句柄说的是给谁报，矩形说的是哪一块作废，头两个参数没什么可展开的。值得咱们停下来的是第三个参数 `bErase`，它管的是背景，而且管的方式很有分寸。

`bErase` 传 `TRUE` 的时候，系统其实不会当场擦任何东西。它做的只是把“这块的背景要擦”的请求一并记进更新区域，真正的擦除要等系统处理到那一步才发生。干活的执行者是谁？是窗口过程里的 `WM_ERASEBKGND` 分支，它用的刷子就是咱们注册窗口类时交出去的 `hbrBackground`。您可以翻回[主线第一篇](../gui/01_naked_win32.md)看那个 demo，它的 `WM_ERASEBKGND` 分支就两行：

```cpp
case WM_ERASEBKGND:
    // Claim the erase and do nothing. The class brush set in wWinMain
    // never runs now: WM_PAINT below owns the whole client area.
    return 1;
```

返回 1 等于向系统申报了“擦过”，背景的填充也就挪进了 `WM_PAINT` 自己的 `FillRect`。所以 `TRUE` 记下的请求，最后是被咱们自己在 `WM_PAINT` 里的那句 `FillRect` 干掉的。您回头把参数和拒绝的两头对一对，就会发现它们是接得上的，谁的登记都没有落空。

`bErase` 传 `FALSE` 就更直白了：连擦除的请求都不记，重画的时候旧的像素原样留在原地。什么时候用得上？您打算把这一块整个覆盖掉的时候。demo 的双缓冲就是那么一幅画面：离屏位图里组好了整帧，再一次性地贴过去，旧的内容反正会被整个盖掉，再安排一次擦除就成了白干。demo 里参数写的还是 `TRUE`，但配上 `WM_ERASEBKGND` 的拒绝，系统安排的擦除一次也没有发生过，效果跟 `FALSE` 是一样的。

擦除的这一头还有一张回执，咱们把它也认了。`BeginPaint` 填好的 `PAINTSTRUCT` 里有个 `fErase` 字段，它告诉您背景到底擦没擦。具体到 demo 的写法，`WM_ERASEBKGND` 返回 1 等于向系统申报了“擦过”，`fErase` 拿到的就是 `FALSE`，哪怕实际上除了您自己的 `FillRect` 谁也没擦。回执记录的是系统相信了什么，咱们读它的时候得知道它信的是申报。

## InvalidateRgn：失效不挑矩形

矩形是失效里最常用的形状，但它并不是唯一的形状。Win32 对一块任意的画面范围有个通用的描述，名字就叫区域（region 是它的英文名），它的形状可以是矩形、椭圆、多边形，也可以是几个形状拼起来的组合。咱们想让一块不是矩形的画面作废，用的就是它：

```cpp
// An elliptical dirty area: InvalidateRect cannot express this shape.
HRGN ellipse = CreateEllipticRgn(100, 100, 300, 220);
InvalidateRgn(hwnd, ellipse, TRUE);
DeleteObject(ellipse);
```

第三个参数还是原来的 `bErase`，语义跟 `InvalidateRect` 的一模一样。区域造了出来只是递个形状，系统把它并进更新区域之后就不再需要它了，所以下一行就能删。您要是好奇区域还能怎么组合、怎么参与裁剪，书架的[GDI Region 一章](../native_win32/24_ProgrammingGUI_Graphics_GDI_Region.md)专门讲它，本篇只借它的形状一用。书架说的是 native_win32 目录，仓库把 Win32 的原文资料都收在了那里。

为什么要提它？因为更新区域本身就是一个区域。就文档和接口的行为看，更新区域收的就是形状，矩形只是往里并入的特例。咱们接下来要讲的合并性质，说的就是区域与区域的并集，而任意形状也是算数的。

## 更新区域：报多少次，并成一份

现在咱们把主角请出来。更新区域（update region）是系统替每个窗口维护的一份记录，它的内容只有一句：“这一片的画面已经作废，得重画了。”`InvalidateRect` 和 `InvalidateRgn` 做的事都是往更新区域里并入新的形状。您在同一个窗口上连报三块失效，更新区域里留下的就是三块的并集，咱们手里的更新区域从头到尾也只有一份。

咱们拿一次真实的点击走一遍合并。假设一个处理分支接连办了两件事，每件事都各报了一块失效：

```cpp
// Two changes in one handler: two rectangles, one eventual WM_PAINT.
InvalidateRect(hwnd, &title_rect, TRUE);
InvalidateRect(hwnd, &status_rect, TRUE);
```

两句调用执行完了之后，系统的手里并没有两个待办，只有一份并好的更新区域。等队列清空、`WM_PAINT` 生成的时候，`rcPaint` 交给您的就是同时框住两块的包围盒（bounding box，包住一个形状的最小矩形）。您把两块矩形画在纸上错开摆，比如一块放在了左上、一块放在了右下，您就会发现并集本身没多大，包围盒却几乎撑满了整个窗口。重画的裁剪认的是形状，画布的尺寸却跟着包围盒走，而两块离得越远，多出来的面积就越大。所以常见的动画写法会把一轮重画拆成几次小画，每一块各报各的失效，省下的正是包围盒里白算的那部分面积。

有了这样的记录，`WM_PAINT` 的生成条件咱们就能说全了。第一篇讲过其中的一半：它是系统当场生成的消息，而且只在消息队列里没有别的消息时才生成，排位垫底是它出了名的脾气。缺的那一半是：更新区域得是非空的，这是生成的前提。咱们把两个条件合起来读，`WM_PAINT` 的脾气就完整了：队列里没了别的活，而窗口身上还有活没干，它到那时候才会被造了出来。消息队列那一侧更细的门道，书架的[消息机制一章](../native_win32/59_ProgrammingGUI_NativeWindows_MsgMechanism.md)讲得比本篇细，咱们这里只取跟重画有关的部分。

咱们把上面的三个事实摞在一起看，攒批的合理性就自己冒出来了。失效干的是登记的活，登记本身花不了多少工夫。重画要等队列空了才发生，而从登记到队列清空之间的每一次失效，其实都只是在往同一份更新区域里并形状。等 `WM_PAINT` 真的来了，它面对的就是并好的一份，画一次也就完了。第一篇讲攒批的时候说过“失效通路天生的脾气”，脾气就是从上面三条事实里推出来的制度。

那反过来问：失效报大了、报多了，代价落在哪？落在重画的那一头。动画是最容易看清楚的例子。您要是写了一个每帧 `InvalidateRect(hwnd, nullptr, FALSE)` 的动画，报的虽然是“整个客户区”这么一句话，换来的却是每一帧都把整个窗口重画一遍：离屏的位图按整窗分配，`BitBlt` 也按整窗地搬运。拿 demo 那个 720×480 的窗口来算，动画按常见的每秒 60 帧的节奏推，每秒钟就是 60 次整窗的组帧和拷贝。而动画每一帧真正变了的，可能只有一颗来回跳的小球。同样的帧率下，只把小球的新旧两个位置报成失效，重画的面积就从整窗缩到了两小块，成本是跟着面积走的，省下来的也就实打实。

一圈一圈全窗口失效的写法，咱们姑且叫它重绘风暴。名字听着倒是吓人，病灶其实就一句话：报失效的人没想过脏区可以是小的。

咱们在主线第六篇读过那个说法：界面上的每个对象（主线里管它们叫节点）算不出自己在屏幕上的矩形，只好把整个客户区一起报了。精确失效缺的那一步，是节点沿父链把偏移一层层地加上去，算出自己落在窗口里的矩形，那属于主线框架层的活。平台层这边的接口早就替咱们备好了：`InvalidateRect` 收的是矩形，`rcPaint` 还的也是矩形，两头都是现成的，谁来算都不用改动这一层的接口。

## ValidateRect：把一块登记销掉

登记的事情有进就该有出。把一块区域从更新区域里减掉、宣布它不再需要重画的动作，文档里的叫法是验证（validate），咱们在第一篇讲 `BeginPaint` 的时候提过这个词。亲手做验证的函数是一对：`ValidateRect` 收的是矩形，它的兄弟 `ValidateRgn` 收的是区域，传 `nullptr` 进去的含义也一致，指的都是整个客户区。

什么时候需要咱们亲手验证？绘制走了 `BeginPaint` 之外的路径的时候。咱们在第一篇里留过一个事实：要是 `WM_PAINT` 分支里连 `BeginPaint` 都没调，更新区域就一直是非空的，所以系统也就一直把 `WM_PAINT` 生成下去。咱们把那个现场摆出来看一回，它值得您亲眼看一次：

```cpp
case WM_PAINT: {
    // No BeginPaint: the update region stays non-empty, so the system
    // keeps generating WM_PAINT as soon as the queue is idle. It floods.
    HDC hdc = GetDC(hwnd);
    TextOutW(hdc, 20, 20, L"hello", 5);
    ReleaseDC(hwnd, hdc);
    return 0;
}
```

字是画上去了，`WM_PAINT` 却一条接着一条地来，任务管理器里这个进程的 CPU 占用一个劲儿地往上走，窗口收别的消息也跟着迟钝了。修复的路子有两条，咱们一条一条看。头一条是把绘制改回 `BeginPaint` 的正路，让系统替咱们销登记。另一条是保持直画的写法，您在末尾补上一句 `ValidateRect(hwnd, nullptr)`，登记清空了之后，生成的条件就断了。两条路都是通的，不过前提只有一个：登记必须有人来销。验证本来就是 `BeginPaint` 替咱们做的那一步，绕开它的人就得把这步活接回去。

另一个典型的场景跟擦除有关。假如您在 `WM_ERASEBKGND` 里干的远不止擦，而是把整块画面都画好了，登记还挂在系统的手里，它可就没有存在的意义了。这时候您在分支里调一句 `ValidateRect(hwnd, nullptr)` 把登记销掉，后面排着的 `WM_PAINT` 也就不来了。画完了就销，跟干完活撕掉工单的道理是一样的。

还有个小方向值得您留意：`ValidateRect` 传的矩形要是比更新区域还大，销掉的就是整份的登记，系统倒是不会为这事报错，也不会留下什么负的脏区。验证干的事只有“销”，没有欠着的一说。

## BeginPaint：交出脏区，同时清空登记

`BeginPaint` 在第一篇已经过了一遍，咱们这次只盯着它跟脏区有关的两件事看。

头一件是它把脏区交给了咱们。`PAINTSTRUCT` 里的 `rcPaint` 就是更新区域的包围盒，也就是包住整个更新区域的最小矩形。更新区域可以是任意的形状，`rcPaint` 只是把它框起来的框子，框内的面积只会大于等于脏区本身。您拿 `BeginPaint` 给的 HDC 画图时，裁剪的区域会被设成更新区域，画到脏区外面的部分会被自动裁掉，画了也是白画。所以正经的写法是只画 `rcPaint` 框内的内容，框外的一笔都不用碰。

只画框内的东西，写出来是什么样子？咱们拿 demo 的画面改造一下，假设窗口里有三样各自独立的东西，绘制之前咱们挨个问一句：它和 `rcPaint` 有没有交集？

```cpp
case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC         hdc = BeginPaint(hwnd, &ps);

    RECT overlap{};
    if (IntersectRect(&overlap, &ps.rcPaint, &title_rect)) {
        draw_title(hdc);
    }
    if (IntersectRect(&overlap, &ps.rcPaint, &status_rect)) {
        draw_status(hdc);
    }
    if (IntersectRect(&overlap, &ps.rcPaint, &g_drawn_rect)) {
        draw_the_line(hdc, g_drawn_text);
    }

    EndPaint(hwnd, &ps);
    return 0;
}
```

`IntersectRect` 算的是两个矩形的交集，交集为空的时候它返回 0，对应的东西这一轮就不画了。咱们的失效报得越准，`rcPaint` 就跟着小了，被跳过的绘制调用也就越多。您等会儿拿篇尾的小块失效试一试就会看到：脏区只盖住一行字的时候，另外两样的画笔根本就没有出过鞘。

咱们在 demo 里每次都重画整个客户区，那是主线粗粒度失效的配套取舍：脏区一旦小下来，重画也就跟着小了，双缓冲的位图还可以按 `rcPaint` 的尺寸开，不用回回都按整窗来开了。

另一件是它把登记清空了。调用 `BeginPaint` 的那一刻，整个更新区域就被验证掉了，跟咱们上一节手撕工单是同一个动作，只不过系统替咱们撕。连环的 `WM_PAINT` 不会来，画完了这一次，这一轮也就结了。`EndPaint` 管的是收尾的另一半：把设备上下文还回系统的手里。第一篇讲过它的代价：漏了它的调用，被吃掉的就是系统资源，咱们不重讲。

## 队列之外查脏区：GetUpdateRect 与 GetUpdateRgn

咱们不光能在 `WM_PAINT` 里从 `rcPaint` 拿到脏区，出了那个分支照样查得到。本节要认的函数就是干这个的：`GetUpdateRect` 查的是包围盒，`GetUpdateRgn` 查的是形状本身。

```cpp
RECT box{};
if (GetUpdateRect(hwnd, &box, FALSE) != 0) {
    // Something is dirty: box holds its bounding rectangle.
}

HRGN shape = CreateRectRgn(0, 0, 0, 0);
const int complexity = GetUpdateRgn(hwnd, shape, FALSE);
// SIMPLEREGION / COMPLEXREGION / NULLREGION
DeleteObject(shape);
```

`GetUpdateRect` 的返回值非零，意思是更新区域里有了东西，`box` 里填的就是包围盒。再往前一步的是 `GetUpdateRgn`，它把区域的形状装进您递过去的 `HRGN` 里，返回值告诉您形状复杂不复杂：`SIMPLEREGION` 是单个的矩形，`COMPLEXREGION` 是比单个矩形更复杂的形状，`NULLREGION` 是什么也没有。两个函数的身上都带着 `bErase` 参数，管的是查询的当口要不要把擦除一并办了，您不想惊动画面就传 `FALSE`。

查它有什么用？咱们动笔之前得看一眼活有多少。第四篇（[HDC 与 GDI 对象模型](04_gdi_objects.md)）的双缓冲是按整窗开的位图，知道了包围盒，离屏的画布就能按脏区的尺寸开：

```cpp
// Size the off-screen buffer to the dirty box, not the whole window.
RECT box{};
if (GetUpdateRect(hwnd, &box, FALSE) != 0) {
    const int width  = box.right - box.left;
    const int height = box.bottom - box.top;
    // CreateCompatibleBitmap(window_dc, width, height) from here.
}
```

咱们在动画一帧开始之前查上一眼，脏区小就开小的画布。这一省跟失效报得准是一对搭档：一个把登记报小了，一个按登记开了工，两头吃的是同一份信息。

还有一条边界得跟您说清：这对查询函数只查不画，它们触发不了 `WM_PAINT`，也不会替您把图画上去。咱们想知道活有多少，它们是查活的好手，干活还是得回到绘制的通路里去。

## RedrawWindow：不想等队列的话

`WM_PAINT` 排在队尾的这一件事，多数的时候是美德，攒批靠的就是它。可有些场合是等不起的，比如拖拽的反馈：您挪一步鼠标，橡皮筋的框就得立刻出现在新位置，排进队列等一轮的话，视觉上就追不上鼠标了。第一篇见过一个绕开排队的函数 `UpdateWindow`：更新区域非空的时候，它把 `WM_PAINT` 直接送到了窗口过程，第一帧就是这么插的队。它有个更全能的亲戚，就是本节的主角。

`RedrawWindow` 一次调用就能把失效和立即重画都办了，咱们看它的参数：

```cpp
// Invalidate this rectangle and paint it now, skipping the queue:
// the rubber-band box must track the mouse with no lag.
RedrawWindow(hwnd, &rect, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
```

矩形参数给的是范围，区域参数给的是形状，两个都传 `nullptr` 的时候，范围指的就是整个客户区，您要连非客户区一起办的话，咱们还得再添一个 `RDW_FRAME` 标志。真正的话都让标志说了，咱们把常用的几个认一遍：

1. `RDW_INVALIDATE`：把给定范围标成无效，相当于就地办一次失效的登记。
2. `RDW_UPDATENOW`：受影响的窗口在函数返回之前就收到 `WM_PAINT`，不再等队列清空。
3. `RDW_ERASE` 与 `RDW_ERASENOW`：同样的事给 `WM_ERASEBKGND` 办一份，前者排队，后者立刻。
4. `RDW_ALLCHILDREN`：范围扩到子窗口一起处理。
5. `RDW_VALIDATE`：方向反过来，把给定范围验证掉，正好接上前面那对验证函数。

标志是可以拼着用的，上面那行代码就拼了三个：登记、立刻画、连擦除一起安排。一句调用办完了三件事，这就是 `RedrawWindow` 比 `UpdateWindow` 全能的地方：后者的本事只有催，前者连登记都替您写好了。

咱们把边界也提醒一句：`RDW_UPDATENOW` 是留给等不起的场合的，动画的常规帧别去拿它，每帧都强行地插队，等于亲手把攒批的好处扔了。队列排位低看着像是受了委屈，其实是省工的设计，插队只留给真正等不起的场合。

## 被盖住的窗口还画不画

脏区的登记、消费、销毁咱们都过了一遍，还剩一个环境的问题：窗口被别的窗口盖住的时候，这些事还照常发生吗？

咱们在 Vista 之前的老书里能读到那么一种讲法：窗口被盖住的部分，内容是真的丢了，露了出来就得重画，系统还会把被盖住的部分从更新区域里裁掉。那个年代没有替窗口存画的层，屏幕上的每个像素只属于一个窗口，盖住了就是覆盖。老书讲的是旧世界的事，那些说法放在 DWM 之前的世界里都成立。

DWM 的时代换了一副地基。第一篇里咱们亲手观察过：拿一个别的窗口把咱们的窗口整个盖住再拖开，画面直接就回来了，没有一次重画的闪烁。原因是每个窗口的画面都存在自己的重定向表面里（redirected surface 可以想成系统替窗口存的一张成品照片），盖住它的窗口并不会毁掉照片，露出来的时候 DWM 把存着的照片重新拼上屏就行。咱们照这个模型推一遍：被盖住的窗口没有丢任何东西，也就没有了重画的理由。

那么被完全盖住的窗口，还会不会收到 `WM_PAINT`？咱们把知道的和不知道的分开说。知道的：生成的条件是更新区域非空且队列空闲，条件本身其实并不关心窗口看得见看不见。不知道的：系统对完全遮挡、最小化的窗口会不会干脆扣下重画，MSDN 的 `WM_PAINT` 条目没有写死，笔者也没有逐一验证过，咱们不装作知道。合理的猜测是省略会发生，因为对看不见的窗口画图是白画，不过猜测终究代替不了验证。验证的办法本篇刚好备好了，就等着下面的一节用上它。

## 把脏区涂上颜色

咱们给脏区做一件可视化的小仪器，思路只有一句话：`WM_PAINT` 里别急着画正经的内容，把 `ps.rcPaint` 的范围涂上颜色，脏区的形状就自己站到屏幕上了。

```cpp
// A dirty-region visualizer: paint the dirty rectangle itself, cycling
// through colors so each repaint leaves a fresh tint. Watching which
// areas change color shows exactly what the system considered dirty.
case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC         hdc = BeginPaint(hwnd, &ps);

    static const COLORREF kTints[] = {RGB(255, 220, 0), RGB(0, 220, 255),
                                      RGB(255, 0, 200), RGB(0, 255, 120)};
    static int pass = 0;

    HBRUSH brush = CreateSolidBrush(kTints[pass % 4]);
    FillRect(hdc, &ps.rcPaint, brush);
    DeleteObject(brush);
    ++pass;

    EndPaint(hwnd, &ps);
    return 0;
}
```

每一轮的重画换一种颜色，新一轮的脏区会盖掉旧的着色。您看到的色块边界，就是更新区域的包围盒的边界，接连几块旧色没被盖到的地方，就是这一轮不脏的地方。

包围盒终究只是外面的框子，想看脏区的真形状，咱们就得把区域本身取出来涂。这里埋着一个跟 `BeginPaint` 副作用正对上的细节：更新区域在 `BeginPaint` 调用的那一刻就被清空了，取形状的动作得赶在它前面，晚一步取到的就是空。咱们把顺序摆对：

```cpp
case WM_PAINT: {
    // kTints and pass are the ones declared in the bounding-box
    // instrument above - copy them along when this stands alone.
    // Fetch the region BEFORE BeginPaint: BeginPaint clears the update
    // region, so GetUpdateRgn would find nothing after it.
    HRGN dirty = CreateRectRgn(0, 0, 0, 0);
    const int complexity = GetUpdateRgn(hwnd, dirty, FALSE);

    PAINTSTRUCT ps{};
    HDC         hdc = BeginPaint(hwnd, &ps);

    if (complexity == SIMPLEREGION || complexity == COMPLEXREGION) {
        HBRUSH brush = CreateSolidBrush(kTints[pass % 4]);
        FillRgn(hdc, dirty, brush);
        DeleteObject(brush);
        ++pass;
    }
    DeleteObject(dirty);

    EndPaint(hwnd, &ps);
    return 0;
}
```

`FillRgn` 涂的是区域本身，您看到的色块就是脏区的真身，多块分离的失效会显成互不相连的几团：包围盒版的仪器把它们框成一整块，区域版让它们各自现了形。想再看出半透明的叠加痕迹，书架的[AlphaBlend 一章](../native_win32/25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md)教您调透明度，本篇的骨架不变。

仪器有了，拿什么动作喂它？咱们把几件事挨个试，每做完了一件事，就看一眼色块的变化：

1. 在某个分支里手工调一句 `InvalidateRect(hwnd, &small_rect, FALSE)`，色块应该恰好只盖住那块小矩形。
2. 拖动窗口的边框缩放，看色块铺开的范围。
3. 拿一个别的窗口盖住一半再拖开，看那一半会不会换色。
4. 最小化再还原，看整窗换不换色。

按本篇的模型，咱们可以提前猜一遍：手工报的小块必然会换色，登记和消费是通路的基本功。缩放的场景大概率是整窗换色，尺寸变了，内容也就得重排了。盖住再拖开按 DWM 的模型不该换色：照片还存得好好的，没有重画的理由。最小化还原就轮到上一节的那个“不知道”了，正好拿您的眼睛把答案收回来。要是您的观察跟模型对不上，您别急着怀疑自己的眼睛，把哪一步、看到的什么颜色记下来，回头逐条核对本篇的哪一句话没站住，那样的瞬间比验证通过还有价值。

## 下一步

咱们在本篇把第一篇里走得急的那一程摊平了，摊出来的就是下面这五件事：

1. 失效干的活是登记，多次的登记会并成一份更新区域。
2. `WM_PAINT` 要等队列空闲、登记也非空了才来。
3. `BeginPaint` 交出脏区的同时也清空了登记。
4. 验证是咱们亲手销登记的手段。
5. `RedrawWindow` 是明码标价的插队。

主线第六篇在框架层让报失效的事漏不掉，本篇在平台层把脏区的进出两头认齐了。咱们把两篇合在一起，“状态变了屏幕跟着变”这件事从上到下就没有查不到的地方了。

工具箱里的下一件家什也不远了。GDI 的面板和零件咱们在[第四篇](04_gdi_objects.md)认过，脏区怎么省工，本篇也交了底，再往下就是换一套画具了。按篇目索引的排法，下一篇讲 Direct2D 的[工厂、设备与渲染目标](06_d2d_targets.md)。不管咱们换到哪套画具底下，失效和重画的通路都是同一套。
