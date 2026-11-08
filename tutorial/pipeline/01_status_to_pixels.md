---
title: 从状态到像素：一次界面更新的全程
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 从状态到像素：一次界面更新的全程

您双击编出来的 `gui_naked`，在窗口里敲了几个字，然后按下了显示按钮。STATIC 标签上的文字当场就换了，可下面那行由程序自己画的字一动不动，还停在启动时写下的那句：“（这行字是本程序自己画的，还没被更新过）”。同一次点击喂给了两个输出，一个动了，而另一个没动。咱们这一篇就沿着动了的那个走：从鼠标左键松开的那一刻，到标签上的那几个像素换了颜色，中间的每一站咱们都停下来看一眼。没动的那个您也别扔，它是咱们在第二站要用到的反面教材。

出发之前咱们把材料认清楚。这一篇要用的代码，在主线（gui/ 目录里从零造框架的教程线）造骨架的时候都见过：裸 Win32 的 demo 在 `src/gui/app/m1a_naked.cpp`，属性与重画的内容在[主线第六篇](../gui/06_property_repaint.md)里讲过。您要是没跟过主线也不要紧，本篇用到的代码都摆在正文里，咱们边走边读。

## 第一站 · 状态换在前面，画不画另说

咱们从点击落进程序的样子说起。点击到达的时候是一条 `WM_COMMAND` 消息，demo 里处理它的分支是这样的：

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) {
        wchar_t text[256];
        GetWindowTextW(g_edit_hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));

        // Output 1 - a system-drawn STATIC. SetWindowTextW invalidates the
        // control itself, so the new text appears with no help from here.
        SetWindowTextW(g_label_hwnd, text);

        // Output 2 - the line painted in WM_PAINT below. Copy the text and
        // stop: no InvalidateRect, on purpose.
        lstrcpynW(g_drawn_text, text, 256);
        return 0;
    }
    break;
```

您把两个输出放在一起看，看出来的差别其实只有一件事。`SetWindowTextW` 把新文字交给了系统控件，重画的事 STATIC 自己知道怎么办。`lstrcpynW` 只是把字符串拷进了全局变量 `g_drawn_text`，拷完它就停手了，没有人知道这块画面已经该换了。数据确实更新了，而屏幕不知道。

等咱们在第一站停下来，确认的事实是：改状态和画画面是两件事，中间隔着的是一条消息。点按钮的代码只需要操心第一件，第二件由另一条通路替咱们走完。咱们接下来要追的就是它。

demo 留下这个破绽是故意的，源码的注释里写着 `no InvalidateRect, on purpose`，主线还拿它当了痛点清单里“状态改了界面不动”问题的标本。本篇也跟着沾了光：一个活的反例摆在手边，比十句正确的代码更能说明通路断了会怎样。至于 `WM_COMMAND` 信封里 `LOWORD` 装控件 ID、`HIWORD` 装通知码的那套读法，咱们在[主线第一篇](../gui/01_naked_win32.md)里已经讲过，这里就不重复了。

STATIC 那一路咱们暂时只看到了一个影子。`SetWindowTextW` 内部走的同样是失效和重画那几步，只是控件自己的代码在系统那边，咱们看不见源码，能看见的只有效果。它和自画的那行字走的是同一条通路，一路咱们看得见，而另一路看不见，偏偏摆在同一次点击里，正好把第一站的边界照清楚了。

主线第六篇把失效自动化的通路修到了漏不掉的程度：属性值变了，往上冒泡请求重画的事由监听器自动完成，您写值的时候重画就排上了。那套封装本篇就不重讲了，咱们要看的是它脚底下平台层面的通路是什么样子。

## 第二站 · 报失效，把脏区攒起来

状态变了，咱们得告诉系统：“这块画面作废了，得重画。”这个动作在 Win32 里的名字叫报失效，用的函数是下面这个：

```cpp
InvalidateRect(hwnd, nullptr, TRUE);
```

三个参数每一个都说了自己的事，咱们挨个看。窗口句柄说的是给谁报。要作废的矩形，传 `nullptr` 进去表示的就是整个客户区，客户区说的就是窗口里去掉标题栏和边框、真正归您画的那一块。擦背景标记说的是背景怎么处理，它为 `TRUE` 的时候，系统会在重画之前替咱们把这块区域刷一遍背景色。

这个调用是不会立刻触发任何画图动作的。系统做的其实是登记：把这块矩形并入这个窗口的更新区域（update region，系统替每个窗口维护的一份“哪些地方脏了”的记录）。您在鼠标点击的分支里连着报了三块失效，三块矩形就并成了一份，后面的处理也只统一来一次。攒批是失效通路天生的脾气，咱们往下走会看到它为什么被设计成这样。

在主线第六篇造出的那个框架版本里，这次调用是从很远的地方走过来的，咱们直接看它落地的最后一行。`Widget::invalidate()` 的调用沿着父链一路走到根节点，根上的钩子再调窗口的失效方法：

```cpp
void Window::invalidate() {
    InvalidateRect(hwnd_, nullptr, TRUE);
}
```

节点对窗口和平台是一无所知的，它能做的也只是往上冒泡，冒到了根上才碰到上面那一行。整个客户区一起失效是主线的简化处理，因为节点拿到的矩形用的是父节点的坐标系，它自己算不出落在屏幕上的位置。更精细的脏区算法，咱们留到本线的第五篇《失效、脏区与重绘》里专门讲。本线说的就是您正在读的 pipeline 教程线，后面的篇名也都按它算。

这里咱们停一秒，回头看 demo 里那行没动的字。它没动的原因就是通路断了：`lstrcpynW` 拷完了字符串，从头到尾都没有一句这样的调用，更新区域里干干净净的，系统也就不知道有活要干了。

## 第三站 · 等消息循环闲下来

脏区记上了，重画发生在什么时候？咱们接着往下看：发生在消息循环闲下来的时候。

`WM_PAINT` 有一条很特殊的脾气：咱们找不到任何把它投进队列的代码，当然也没有哪段代码调用过 `PostMessage`，它是系统当场生成的，而且只在队列里没有别的消息的时候才生成。这样的条件并不是咱们的概括，MSDN 对 `WM_PAINT` 的描述原文就是这么写的。您要是排在队列里的是一串键盘输入和鼠标移动，重画也就一直等着了。

这个脾气解释了上一站的攒批为什么成立。咱们一秒钟内报多少次失效都没关系，反正重画要等队列清空了才发生，到那时候脏区早就并成一份了，画一次也就完事了。

也有不等的时候。您看 demo 在显示窗口之后紧跟了一句：

```cpp
ShowWindow(hwnd, nShowCmd);
UpdateWindow(hwnd);
```

`UpdateWindow` 的做法是绕开队列，更新区域非空的时候，它就直接把 `WM_PAINT` 发给了窗口过程。窗口刚显示出来的第一帧等不起，用户看到的还是一片空白，所以咱们让它插了一次队。这样的行为在 MSDN 里写得明明白白，咱们照着文档陈述。

咱们把 demo 的消息循环也认一遍，重画最终要靠的就是这个循环的放行：

```cpp
MSG msg{};
while (true) {
    const BOOL kResult = GetMessageW(&msg, nullptr, 0, 0);
    if (kResult == 0) {
        break;  // WM_QUIT
    }
    if (kResult == -1) {
        return 1;  // failure
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
}
```

`GetMessageW` 取的是消息，`DispatchMessageW` 把消息递给了窗口过程，咱们就看着这个环一圈一圈转下去。队列空了，而脏区还在，`WM_PAINT` 到这时候才被生成了出来，走进了 `WndProc` 的 `switch`。

`TranslateMessage` 那一句咱们也认一下，它跟重画没有直接的关系，干的是把按键消息翻译成字符消息的活。有意思的地方在于它会往队列里补投 `WM_CHAR` 一类的消息，占的正是 `WM_PAINT` 等待的通道，所以它也站在咱们的时间线里。

这样的设计对咱们日常写法有一条直接的提醒：消息的处理分支里，什么样的重活都别干。您要是在一个点击处理里算了半秒，排在其后的重画就得跟着等半秒，用户看到的就是界面卡了半秒。其实消息循环是一条单行道，而重画排在队尾，前面一堵住它就成了第一个遭殃的。

## 第四站 · BeginPaint 交出脏区

处理 `WM_PAINT` 的一对固定写法就是 `BeginPaint` 加 `EndPaint`，咱们在 demo 里看到的骨架是这样的：

```cpp
case WM_PAINT: {
    PAINTSTRUCT paint{};
    HDC         window_dc = BeginPaint(hwnd, &paint);
    // ...画图...
    EndPaint(hwnd, &paint);
    return 0;
}
```

`BeginPaint` 做的几件事，每一件都值得咱们停下来看。

它交给咱们一个设备上下文（HDC，您可以把它理解成一套能往某个表面上画图的画笔套餐，里面装着画笔、画刷、字体这些当前的设置）。它还把裁剪区域设成了更新区域，也就是说您拿这个 HDC 画图的时候，画到没脏的地方会被自动裁掉，画了也是白画。最后它把 `PAINTSTRUCT` 填好了还给您，这个结构体在 SDK 里的样子是这样的：

```cpp
typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT;
```

前三个字段是给咱们的：`hdc` 就是您手里那个设备上下文，`rcPaint` 是包住整个更新区域的最小矩形，`fErase` 说的是背景擦没擦。后面的三个字段归系统内部使用，咱们不动它。

您翻遍 demo 也找不到 `rcPaint` 的使用，这不是您漏看了。demo 每次都重画的是整个客户区，咱们拿不拿那个最小矩形都行，它跟主线框架里的粗粒度是配套的取舍（整个客户区一起失效）。等脏区算精细了，只重画 `rcPaint` 包住的那一块才有意义，那就是本线第五篇的事了。

还有一件它顺手做掉的事，咱们得点出来：更新区域在这里被清空了。文档里给这一步起的名字叫验证（validate）。清空发生在调 `BeginPaint` 的那一刻，所以只要进了 `BeginPaint`，画完这一次就不会再收到连环的 `WM_PAINT`，哪怕后面漏了 `EndPaint` 也一样。真要让消息反复重来的话，得是连 `BeginPaint` 都没调的情形，那时候的更新区域一直非空，系统也就一直生成下去了。`EndPaint` 管的是另一头的收尾，它标记的是这次绘制的结束，把 `BeginPaint` 交出来的设备上下文还回系统，漏调它的代价是那个 DC 一直还不回去，攒下去被吃掉的就是系统资源了。

demo 里还有一条跟它打配合的消息，咱们一起看了。`WM_ERASEBKGND` 的分支直接返回 1，把擦背景的活拒绝了：

```cpp
case WM_ERASEBKGND:
    // Claim the erase and do nothing. The class brush set in wWinMain
    // never runs now: WM_PAINT below owns the whole client area.
    return 1;
```

拒绝的理由就写在注释里：整个客户区都归了 `WM_PAINT` 管，擦背景的活它自己顺手干了，系统再擦一遍反而多出一道闪烁。您拖窗口右边框的时候画面不闪，功劳的一半要记在它头上。

这里咱们把第二站埋下的那个参数接上。`InvalidateRect` 的第三个参数 `TRUE` 并不会当场擦背景，它只是把要擦的请求记进了更新区域，真正的擦背景动作要等系统处理到擦除消息的时候才发生。demo 在 `WM_ERASEBKGND` 里返回 1 把活拒了，所以背景的填充实际挪进了咱们自己的 `FillRect`，参数和拒绝两头是接得上的。

## 第五站 · 像素写进哪块内存

现在画笔在咱们手里了，真正把像素写出来的代码在 `WM_PAINT` 分支中间。这段咱们按逻辑分成三截看。

咱们动手搭一块画板：

```cpp
// Double buffer: compose the frame in an off-screen bitmap, then blit
// it in one go. Painting straight into the window DC leaves a visible
// gap between filling the background and drawing the text.
RECT client{};
GetClientRect(hwnd, &client);
HDC     buffer_dc = CreateCompatibleDC(window_dc);
HBITMAP buffer    = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
HGDIOBJ previous  = SelectObject(buffer_dc, buffer);
```

`CreateCompatibleDC` 造的是一个内存里的设备上下文，`CreateCompatibleBitmap` 造的是一块和窗口同样大小的内存位图，`SelectObject` 把位图装进了上下文。到这里咱们手里有两块可以画的表面：`window_dc` 连着的是窗口，`buffer_dc` 连着的是一块谁也看不见的内存。

然后咱们把整帧画面画在内存里：

```cpp
// WM_ERASEBKGND above refused to erase, so the frame starts here.
FillRect(buffer_dc, &client,
         reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)));

// The line this program paints itself.
SetBkMode(buffer_dc, TRANSPARENT);
DrawTextW(buffer_dc, g_drawn_text, -1, &g_drawn_rect,
          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
```

`FillRect` 负责刷的是背景，`DrawTextW` 把 `g_drawn_text` 里的字符串画进 `g_drawn_rect` 那块矩形。参数里的 `-1` 说的是字符串以空字符结尾，长度的事情交给函数自己去数。剩下四个标志位管的是左对齐、单行、垂直居中的排版，放不下的时候结尾还会补上省略号。您注意此刻像素落在哪：落在了内存位图上，而屏幕上什么都没发生。

有一个细节也值得咱们停下看。`CreateCompatibleBitmap` 的宽高参数直接用了 `client.right` 和 `client.bottom`，把 `client.left` 减掉的那一步也省了，这样的写法看着像偷懒，其实是对的：`GetClientRect` 还回来的矩形左上角永远是原点，宽和高其实就是 right 和 bottom 本身。咱们读别人的绘图代码的时候，这样的写法值得停下来问一句为什么。

最后咱们把整帧一次性搬上窗口：

```cpp
BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);

SelectObject(buffer_dc, previous);
DeleteObject(buffer);
DeleteDC(buffer_dc);
```

`BitBlt` 做的是块拷贝，把内存位图整块搬到窗口的设备上下文上，`SRCCOPY` 是搬运的方式（源覆盖目标）。在内存里拼好了整帧再一次搬过去，这样的套路就叫双缓冲，为的是不让用户看到“背景刷了一半、字还没画上去”的中间态。搬完咱们把位图和上下文还回系统。书架（native_win32 目录里 Windows 原生绘图的参考线）的[GDI 双缓冲一章](../native_win32/23_ProgrammingGUI_NativeWindows_GDI_DoubleBuffer.md)把这个套路单独讲了一整篇，[HDC 一章](../native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC.md)则把设备上下文这个概念摊开了讲，咱们把链接挂在这里，本篇就不重讲它了。

您也许想省掉这块画板、直接画到 `window_dc` 上，demo 的注释里写了后果：直画会露出“背景刷完、字还没上去”的中间态，窗口越大的时候越明显。双缓冲多花的本钱是一块内存位图，换来的是整帧一起上屏。

那么 `window_dc` 的终点就是屏幕吗？其实不是。它算全程里最反直觉的一站了，咱们干脆用整个下一站讲它。

## 第六站 · 窗口 DC 的背后不是屏幕

`BitBlt` 把像素搬进了 `window_dc`，画面却停在了半路。而从 Windows Vista 起，每个窗口的画面都会走一块由桌面窗口管理器（DWM、Desktop Window Manager、系统里专职合成画面的组件）掌管的内存表面，书面上它的名字叫重定向表面（redirected surface）。您可以把它想成系统替每个窗口存下的一张成品照片。咱们画图的终点就是照片本身，照片什么时候上屏幕、和谁拼在一起上屏幕，都是 DWM 的事。

有一个您自己几十秒就能做的观察支持这件事。您拿一个别的窗口把咱们的窗口整个盖住、再拖开，被盖住的那部分画面直接就回来了，您看不到一次重画的闪烁。照 DWM 的模型这也说得通：画面在系统的手里存着一整份，露出来的时候 DWM 把存着的照片重新拼上屏就行，不必劳驾咱们再画一遍。Vista 之前是没有这一层的，窗口被盖住再露出来的话，重画就免不了了，那个年代的老书里写的“露出就重画”讲的就是旧世界的事了。

咱们这个 demo 和记事本、浏览器同屏的时候，拼桌这件事也一样：谁也不画谁的领地，各管各的表面，收上来拼成整屏的活全在 DWM 一家手里。这就是为什么隔壁窗口在放视频，咱们这边画自己的文字，两边也就互不添乱了。

等咱们走到这里，一条边界自然就浮出来了：咱们的代码跑在 CPU 上，一笔一笔把像素写进自己窗口的表面。而 DWM 跑在 GPU 上，把屏幕上所有窗口的表面拼成整屏的一帧送出去。画图归了进程，合成归了系统，两边各干各的，中间交接的就是重定向表面。往后在本线里咱们还会反复遇到它，GDI、Direct2D、Skia 讲到底回答的也都是同一个问题，也就是怎么把咱们这一侧的表面填得又快又好。

边界带来的另一个后果是：咱们这侧的重画频率和屏幕的刷新频率是解开的。屏幕按自己的节奏一帧一帧刷新，DWM 每次拿各窗口表面当时的最新内容去拼。咱们这次 `WM_PAINT` 慢了半拍，耽误的只是这一帧的内容更新，把屏幕撕出口子的事不会发生。经 DWM 合成上屏的内容没有撕裂这回事，撕裂只出现在绕开合成、直写屏幕的路径上。帧和刷新怎么对齐、垂直同步在其中干什么，下一篇咱们就讲它们。

DWM 内部怎么调度、什么时候取表面、怎么跟屏幕的刷新节奏对上，这些细节文档里当然没有写死，笔者也没有去验证它的内部时序，咱们能拿到的模型就是“窗口画面进表面、再由 DWM 合成”。对写应用的咱们来说，这个模型已经够用了。

## 三段分工

等咱们把全程走完再回头一看，一次界面更新其实分成三段，每段有各自的责任和各自的触发时机。

| 段 | 谁在干活 | 触发时机 | 本篇对应的站 |
|:---|:---|:---|:---|
| layout（布局） | 咱们的代码，算每个东西放哪 | `WM_SIZE`，窗口尺寸变了 | 站外，主线第五篇 |
| paint（绘制） | 咱们的代码，把状态变成像素 | `WM_PAINT`，更新区域非空且队列空闲 | 第一到第五站 |
| compose（合成） | DWM，把各窗口的画面拼上屏 | 屏幕每次刷新 | 第六站 |

demo 里 layout 对应的是 `WM_SIZE` 分支里那句 `layout_controls(hwnd)`，窗口一缩它重算的就是三个控件的位置。paint 对应的是 `WM_PAINT` 分支，咱们刚才逐段读过。compose 对应的是 DWM，咱们的代码里一句都没有，合成这件事本来就不归咱们进程管。

这三段谁也不越谁的界。layout 产出的只有位置和尺寸，像素怎么写它不碰。paint 眼里只有自己窗口的那块表面，旁边有没有别的窗口，它其实是不知道的。compose 就更省事了，连画面内容的含义都不问，图标、文字、一张照片到了它手里全是同样的一堆像素。您以后读任何 GUI 框架的渲染代码，只要把看到的东西归到三段中的一段，迷路的概率就会小很多。

咱们本线的路线图也顺着这三段铺开。GDI、Direct2D、Skia、文本讲的那几段，换的都是 paint 这一段里的工具，DWM 和 DirectComposition 的几篇，讲的则是 compose 那一段。layout 那一段主线已经带着咱们亲手造过一遍，本线就不重造了。

这个分法也顺手解释了咱们开头那个没动的字。它卡在 paint 的门口：状态换了（第一站），报失效的事没人做（第二站），所以第三到第五站全都没开动，第六站的合成倒照常每帧在转，只是拼上去的还是旧画面。主线第六篇在框架层把报失效自动化了，失效和脏区怎么在平台层查到底，本线第五篇接了这个活。

## 您可以自己动手的三处

等您读到这儿，有几件事值得您亲手做一遍。

1. 在 `WM_COMMAND` 分支的 `lstrcpynW` 后面补一句 `InvalidateRect(hwnd, &g_drawn_rect, TRUE)`，重新编译再点按钮。那行自己画的字这次会跟着动，因为断掉的通路接上了。这句手工补丁正是主线第六篇绕开的地方：它把失效接线挪进了属性那一层，让这一句从此不需要人记。
2. 拿一个别的窗口把咱们的窗口整个盖住再拖开，注意看被盖住的那块。画面直接回来，没有重画的闪烁，您看到的就是第六站讲的那个模型在干活。
3. 把 `UpdateWindow` 那句注释掉再跑。第一帧照样出现，因为它只是让第一帧插一次队，注释掉之后重画退回排队，循环一闲下来照样发。差别可能不明显，您得知道自己在看什么。

## 下一站

这一篇咱们只是把全程过了一遍，每一站咱们都停得很短。后面的每一站都有自己的专篇：脏区怎么攒、怎么算精细，咱们留给了[失效、脏区与重绘](05_invalidation_dirty_region.md)那一篇。HDC 和 GDI 的对象模型，您到[HDC 与 GDI 对象模型](04_gdi_objects.md)那一篇里读。DWM 的合成细节，您看[DWM：重定向表面与窗口合成](30_dwm_composition.md)那一篇。主线那边的[属性与重画](../gui/06_property_repaint.md)管的是框架层怎么让报失效自动发生，咱们正好拿它接着本篇第二站往下读。

下一篇咱们贴着屏幕看：[屏幕、帧与垂直同步](02_screen_frame_vsync.md)。合成出来的帧怎么跟上显示器的刷新节奏，掉帧和撕裂是从哪来的，咱们到那一篇里见。
