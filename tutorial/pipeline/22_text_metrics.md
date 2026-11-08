---
title: 测量、命中测试与光标
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 测量、命中测试与光标

按[篇目索引](index.md)的登记，本篇在文本段里排的是末位。前一篇[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)交给咱们的，是一份排好行的版式实体 `IDWriteTextLayout`。文本段到这里收了尾，咱们进的是 Direct3D 段的[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)，本篇收尾时咱们再过去。

标题里的“光标”咱们得跟鼠标的那个分清楚。编辑器里一闪一闪的竖线，英文里叫的是 caret，说的就是插入点。随鼠标移动的箭头叫指针，它的英文名是 cursor。书架（也就是 `tutorial/native_win32/` 目录下那套带完整示例的老教程）的[图标与光标资源一章](../native_win32/14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md)讲的是指针，咱们本篇讲的几乎全是 caret，您读到后面拿不准的时候，回到咱们这句来对一下就好。

版式排好了也只是能画，可编辑器的活儿有一半在画的外面：一段字实际占了多宽多高，鼠标点下去落在第几个字上咱们得能答，插入点的落点在哪一个坐标，咱们也得答得上来。这些问题的答案全在 layout 的查询方法里，咱们今天把 `GetMetrics`、`GetLineMetrics` 与两个方向相反的 `HitTest` 认全，收尾再拼一个能点着定位光标的小查看器。

## GetMetrics：整块版式的尺寸

量整块版式的入口是 `IDWriteTextLayout::GetMetrics`，它往咱们递过去的 `DWRITE_TEXT_METRICS` 结构体里填数。书架的排印章（名字沿用上一篇《DirectWrite：TextLayout 全解》的叫法，就是[DirectWrite 排印章](../native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography.md)）给过一份带注释的调用，字段咱们直接看：

```cpp
DWRITE_TEXT_METRICS metrics{};
layout->GetMetrics(&metrics);

// metrics.width        - actual width of the laid-out text
// metrics.height       - actual height of the laid-out text
// metrics.widthIncludingTrailingWhitespace - width incl. trailing spaces
// metrics.layoutWidth  - the constraint width passed to CreateTextLayout
// metrics.layoutHeight - the constraint height passed to CreateTextLayout
// metrics.lineCount    - number of lines that made it into the layout
```

`width` 与 `height` 说的是排完版之后文字实际占了的尺寸，单位还是咱们认过的 DIP，也就是[第三篇](03_dip_and_dpi.md)里的设备无关像素，它跟 Direct2D 的坐标是直通的。`layoutWidth` 与 `layoutHeight` 记的则是容器的尺寸，也就是咱们递给 `CreateTextLayout` 的那一对约束，两对数各记各的，下面咱们马上能看到它们分家的场合。

分家的场合就是断行。约束宽度装不下一整行的时候，引擎会在合适的位置把行断开，行数多了，`height` 自己就长了，`lineCount` 也跟着涨了，整个活儿咱们一行加法都不用写。上一篇的动手清单里留过一道把最大宽度从 300 改到 120 的题，您要是做过，同一份文案就断成了几行，这时您数画面里的行数，`lineCount` 报的也就是同一个数。

`width` 与 `widthIncludingTrailingWhitespace` 的差别只有一份尾部空格。文案的末尾要是挂着空格，`width` 量到的是最后一个看得见的字，而带尾部空格的那个数把空格也算进去了。您要是给文本框画底色框，框宽取看得见的那个就好，空格不该撑出一条空白的尾巴。

上一篇咱们还交代过一头脾气，在这里回收：约束高度不够的时候，放不下的行不会被排进版式，`height` 只数真正排进去的那些行。所以咱们拿 `GetMetrics` 的数当“画面上有什么”的依据是可靠的，它从来不会把没画的东西报成画了。

## GetLineMetrics：逐行的几何

整块的尺寸有时候不够细，咱们会想知道每一行自己的事：它多高、基线在哪儿、装了几个字。基线咱们在[Skia 的文本一篇](15_skia_text.md)里认过，说的就是字母站立的那根看不见的线。逐行的几何得走 `IDWriteTextLayout::GetLineMetrics`，它的调用是两段式的，书架排印章给的样板咱们照着摆：

```cpp
// Pass 1: how many lines are in the layout at all.
DWRITE_TEXT_METRICS text_metrics{};
layout->GetMetrics(&text_metrics);
UINT32 line_count = text_metrics.lineCount;

// Pass 2: one DWRITE_LINE_METRICS per line, actual count comes back.
std::vector<DWRITE_LINE_METRICS> lines(line_count);
UINT32 actual = 0;
layout->GetLineMetrics(lines.data(), line_count, &actual);
```

头一趟咱们拿的是行数，第二趟咱们按行数备好数组再去要明细，`actual` 交回来的是真填了多少格。每一格里的数长这样：

```cpp
// lines[i].length                  - UTF-16 units covered by this line
// lines[i].height                  - total height of this line
// lines[i].baseline                - baseline, measured from line top
```

`length` 数的还是 UTF-16 单元，它跟 `DWRITE_TEXT_RANGE`（用起点加单元数表示一段文本的结构）的口径是一致的，代理对占两个单元的告诫咱们在[第十九篇](19_chars_and_glyphs.md)里数过，行里躺着的要是 emoji，您数单元的时候得把它记成两个。咱们把结构体里剩下的三个成员也认全。`trailingWhitespaceLength` 记的是行尾空格占的单元，`newlineLength` 记的是行尾换行符占的单元，咱们别忘了换行符自己也占单元，`\r\n` 占的是两个，您算行内位置的时候可别漏了它。`isTrimmed` 标的是这一行有没有被省略号收尾裁过，本篇的示例用不上它。

这些几何什么场合用得上？行号栏是现成的一个：`length` 逐行累加，您就知道第 i 行对应的是文本的哪一段。滚动的长文是另一个：可视区裁掉头几行的时候，前几行的 `height` 加起来就是咱们往上挪的偏移。

## HitTestTextPosition：字符位置换坐标

方向反过来的一对查询登场了。咱们手里数的是“第几个字符”，画到屏幕上要的是坐标，`HitTestTextPosition` 管的就是这一步换算：

```cpp
FLOAT x = 0.0f, y = 0.0f;
DWRITE_HIT_TEST_METRICS htm{};
layout->HitTestTextPosition(
    text_position,   // which UTF-16 position we ask about
    FALSE,           // isTrailing: leading or trailing edge
    &x, &y, &htm
);
```

头一个参数是咱们关心的字符位置，第二个参数 `isTrailing` 挑的是光标停在字符的哪一边，`FALSE` 要的是前边，`TRUE` 要的是后边。交回来的 `x` 与 `y` 就是插入点停靠的位置，咱们拿它当竖线的左上角用。附带的 `DWRITE_HIT_TEST_METRICS` 里还有一整份几何：`textPosition`、`length`、`left`、`top`、`width`、`height`，外加记方向的 `bidiLevel`。咱们画光标的时候，竖线的高度取的就是 `htm.top` 与 `htm.height`，它比返回的 `y` 更直接。

`isTrailing` 为什么得咱们自己挑？因为同一个字符位置能停两条光标，一条贴的是前边，一条贴的是后边，中文里咱们感觉不到它的歧义，可文字一混方向它就冒头了。两个词说的都是阅读方向上的前后，从右往左读的段落里，前边就换到了画面的右侧。`htm.bidiLevel` 记的就是方向的信息，偶数的行从左往右，奇数的行从右往左，[讲整形的那一篇](20_shaping.md)（整形就是 shaping，把字符排成字形的过程）里咱们见过 BiDi 的嵌套，这里的层级就是那里讲过的那套编号，现在派上了用场。咱们画竖线的时候不用自己算方向，光标贴哪一侧在您把 `isTrailing` 定下来的那一刻就定了，引擎给的 `x` 已经按方向摆好了。

还有一处的歧义连中文也躲不开，它说的就是连字跟代理对。文本位置是按 UTF-16 单元数的，可 fi 连字在画面上是一个拆不开的整体，emoji 的代理对也是一个整体，位置落在它们中间的时候，画面上其实找不到能停的中间。[整形一篇](20_shaping.md)里咱们说过 cluster 的映射管的就是这里：光标按簇的边取位置，跳过簇中间的单元，用户看到的插入点就永远贴在一个看得见的边界上。咱们不用手工去查映射表，下面讲的 `HitTestPoint` 交回来的 `length` 会告诉咱们这一簇占几个单元。

## HitTestPoint：坐标换字符位置

鼠标点下去给咱们的是坐标，方向就反过来了，咱们得拿着坐标去找第几个字符，干这活的入口是 `HitTestPoint`，书架排印章在命中测试小节里给的就是它：

```cpp
BOOL is_trailing = FALSE;
BOOL is_inside   = FALSE;
DWRITE_HIT_TEST_METRICS htm{};
layout->HitTestPoint(x, y, &is_trailing, &is_inside, &htm);

// htm.textPosition - the cluster the point landed on (0-based)
// htm.length       - UTF-16 units of that cluster
```

`isInside` 说的是点有没有落在版式的范围里，范围外的点它也接，只是这个标志给您机会判掉。`isTrailingHit` 是光标定位的判官：点落在字符的前半边，它给的是 `FALSE`，落在后半边给的才是 `TRUE`，书架排印章的原话就是拿它决定光标放字符前还是字符后。

咱们把它拼成插入点位置，一行也就够了：

```cpp
UINT32 caret_pos = htm.textPosition + (is_trailing ? htm.length : 0);
```

加法里的 `htm.length` 正是上一节咱们留的口子：命中的可能是一整簇，点在簇的后半边时，插入点得跳到整簇的后面，连字中间也就不留缝了。两次 hit test 在这里接成了完整的一来一回：`HitTestPoint` 把屏幕上的点翻译成位置，`HitTestTextPosition` 把位置翻译回屏幕上的线，编辑器的光标逻辑，骨架就是咱们手里的这两块拼起来的。

## 示例：点哪儿，光标落哪儿

咱们把点与线的换算拼成一个小查看器（按理说能跑）：窗口里一段中英混排，您鼠标点哪儿，光标就竖到那个字的边上。下面的代码是笔者按 `dwrite.h` 与 `d2d1.h` 的声明组织的骨架，它还没有经过编译的验证，参数的名字与次序请您以手头的 SDK 为准。工厂、format、layout 的创建在[上一篇](21_directwrite_layout.md)里完整走过，咱们不再贴一遍，只看新增的部分。

设备无关的家当还是那几样，咱们多添两个全局：插入点的位置，加上光标要不要显示的开关。

```cpp
// Skeleton, not compiled.
static IDWriteTextLayout* g_layout = nullptr;   // from the TextLayout article

static const wchar_t* g_text =
    L"点一下任意位置，光标就落到那个字边上。DirectWrite 负责算。";
static const D2D1_POINT_2F kOrigin = {20.0f, 20.0f};

static UINT32 g_caret_pos   = 0;
static bool   g_caret_shown = false;
```

`kOrigin` 是咱们递给 `DrawTextLayout` 的版式起点，它在 hit test 里有一个要紧的角色：鼠标给咱们的是客户区坐标，而 layout 认的是自己版式内的坐标，两套坐标差的就是这个起点，换算的方向您可别弄反了。

点击的活儿挂在 `WM_LBUTTONDOWN` 上，咱们把三步连起来看：

```cpp
// Skeleton, not compiled. GET_X_LPARAM needs <windowsx.h>.
void OnLeftButtonDown(HWND hwnd, LPARAM lParam, float dpi_scale) {
    // Client pixels -> DIP, the drill from the DIP & DPI article.
    float px = GET_X_LPARAM(lParam) / dpi_scale;
    float py = GET_Y_LPARAM(lParam) / dpi_scale;

    // Client DIP -> layout coordinates: subtract the draw origin.
    float lx = px - kOrigin.x;
    float ly = py - kOrigin.y;

    BOOL is_trailing = FALSE;
    BOOL is_inside   = FALSE;
    DWRITE_HIT_TEST_METRICS htm{};
    g_layout->HitTestPoint(lx, ly, &is_trailing, &is_inside, &htm);

    g_caret_pos   = htm.textPosition + (is_trailing ? htm.length : 0);
    g_caret_shown = true;
    InvalidateRect(hwnd, nullptr, FALSE);
}
```

DPI 的换算系数怎么来，[第三篇](03_dip_and_dpi.md)里给过它的路数。`HwndRenderTarget` 建立的那套在[第六篇](06_d2d_targets.md)，咱们都按旧路引用。点击处理本身没有新东西：咱们把坐标挪进版式，翻译的活儿交给 hit test，加法的结果落到 `g_caret_pos` 上，客户区也标成了失效，重画就等着消息循环来催了。

画的部分里，咱们把光标画成一条一 DIP 宽的竖矩形：

```cpp
// Skeleton, not compiled.
void OnPaint(ID2D1RenderTarget* target,
             ID2D1SolidColorBrush* text_brush,
             ID2D1SolidColorBrush* caret_brush) {
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));
    target->DrawTextLayout(kOrigin, g_layout, text_brush,
                           D2D1_DRAW_TEXT_OPTIONS_NONE);

    if (g_caret_shown) {
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS htm{};
        g_layout->HitTestTextPosition(
            g_caret_pos, FALSE, &x, &y, &htm);

        // One DIP wide, one line tall, at the origin plus (x, htm.top).
        target->FillRectangle(
            D2D1::RectF(kOrigin.x + x,
                        kOrigin.y + htm.top,
                        kOrigin.x + x + 1.0f,
                        kOrigin.y + htm.top + htm.height),
            caret_brush);
    }
    HRESULT hr = target->EndDraw();
    // D2DERR_RECREATE_TARGET handling: see the render-target article.
}
```

竖线的横坐标咱们用返回的 `x` 加上起点，高度与纵坐标用 `htm` 的行几何，取法就是咱们在 `HitTestTextPosition` 一节里说过的那套。`isTrailing` 这里写死了 `FALSE`，因为点击的时候 `g_caret_pos` 已经按 `is_trailing` 跳过簇了，画的时候再取前边，落点贴的就是簇的边界。这个写法在示例的纯中英文案（都是左到右的文字）上是稳的，混方向的场合笔者没验证过，您真遇上 BiDi 混排的时候请实测。预期的画面是：深灰的底上有一行字，您点过去，白线就竖在您点的位置上，线的顶底正好卡在那一行的行高里。画面是咱们按代码推出来的预期，真跑起来的截图笔者手里没有，您接进项目里对一眼，就知道推没推对了。

## 闪的节奏归谁管

光标是会闪的，可本篇的代码画的是一条不动的线，这里咱们把边界画清楚。位置是排版的活儿，本篇已经管到了。闪是时间的活儿，路子倒是有两条。

一条是系统 caret：`CreateCaret` 给窗口挂上一条系统替咱们维护的插入线，`SetCaretPos` 挪它的位置，显隐的开关是 `ShowCaret` 与 `HideCaret`，闪的节奏系统也就包了。另一条是咱们自己拿 timer 翻转一个布尔值，每翻一次咱们就 `InvalidateRect` 一回，画的时候按开关决定竖线画不画，书架的[定时器一章](../native_win32/57_ProgrammingGUI_NativeWindows_Timer.md)把 `WM_TIMER` 的脾气讲得很全，它的精度撑一条光标是足够的。书架里咱们没找到 caret 的专门章目，`CreateCaret` 一家子的名字出自 SDK 的常识，签名与细节还请您对着官方文档核一遍再用。

咱们的小查看器为什么不带闪？因为竖线画在哪儿，全由 `HitTestTextPosition` 说了算，这个因果的链条里不掺时间，咱们读着也就清爽。您真要接系统 caret，`SetCaretPos` 吃的是整数像素，DIP 换回像素的乘法可别省，那是[第三篇](03_dip_and_dpi.md)里练过的旧路。

## 选区：两端 hit test 围出矩形

咱们选中一段字，底色块怎么来？思路跟光标是同一副：拖动的起点与终点咱们各做一次 `HitTestPoint`，得到的是两个位置，围出来的范围就是要高亮的区间。区间的矩形咱们不用自己拼，`IDWriteTextLayout` 还备了一个查询 `HitTestTextRange`，您给它范围，它交回来的是一行一块的几何：

```cpp
// Skeleton, not compiled. sel_start / sel_end came from two
// HitTestPoint calls: mouse down, then mouse up.
UINT32 lo  = sel_start < sel_end ? sel_start : sel_end;
UINT32 hi  = sel_start < sel_end ? sel_end : sel_start;
UINT32 len = hi - lo;   // works for both drag directions

DWRITE_HIT_TEST_METRICS hits[8];
UINT32 actual = 0;
g_layout->HitTestTextRange(
    lo, len,
    kOrigin.x, kOrigin.y,   // origin: rects come back in client DIP
    hits, 8, &actual);

for (UINT32 i = 0; i < actual; ++i) {
    target->FillRectangle(
        D2D1::RectF(hits[i].left, hits[i].top,
                    hits[i].left + hits[i].width,
                    hits[i].top + hits[i].height),
        selection_brush);   // semi-transparent, drawn UNDER the text
}
```

范围跨了几行，交回来的就是几块，每一行摊的是一块矩形。所以数组的格数得按行数留，`actual` 报的是实际用了几块，范围把数组撑爆的时候它会告诉咱们没装下。骨架里咱们留了 8 格，示例的文案是够用的。起点坐标咱们直接传进了 `HitTestTextRange`，您把它跟 `HitTestPoint` 那头要减起点的做法相对着记：这一头是加，交回来的矩形已经是客户区 DIP 的坐标了。

画的次序有讲究：半透明的高亮咱们画在 `DrawTextLayout` 之前，文字也就浮在底色上面了。要是画反了，字也跟着被罩上了一层半透明，看着就脏了。拖选的时候鼠标经常拖出文字的范围，`SetCapture` 把后续的鼠标消息都引到咱们窗口的办法，书架的[命中测试一章](../native_win32/50_ProgrammingGUI_Graphics_CustomCtrl_HitTest.md)里有整节的讲解。

## 行高从哪儿来

光标与选区都用上了行高，咱们把行高的老底翻出来看一眼。字体的度量里躺着三个数：ascent 是基线以上的高度，descent 是基线以下垂下去的深度，行距（leading）是排版建议两行之间再垫的空隙，名字来自铅字时代垫在行间的那根铅条。一套行高就是把三个数加出来的和。[Skia 的文本一篇](15_skia_text.md)里咱们算过同一道题：`SkFontMetrics` 的 `fAscent` 是负数、`fDescent` 是正数，行高也就写成了 descent 减 ascent 再加 leading。DirectWrite 的 `DWRITE_FONT_METRICS` 换了一副记法：

```cpp
// Skeleton, not compiled. Field names from DWRITE_FONT_METRICS.
DWRITE_FONT_METRICS fm{};
// font is an IDWriteFont*; the collection walk is outlined below.
font->GetMetrics(&fm);

float em          = static_cast<float>(fm.designUnitsPerEm);
float line_height = static_cast<float>(
    fm.ascent + fm.descent + fm.lineGap) * font_size / em;
```

ascent 与 descent 记的是不带符号的正数。lineGap 咱们得单独交代一句：它带符号，咱们见到的值多为零或正，偶尔才是负的（为负的时候，推荐的行距比字符盒高还要矮），咱们照样用加法就行。量出来的单位是字体的设计单位，咱们得乘字号再除以每 em 的设计单位数，才换得回 DIP 的口径。Skia 那边 `getMetrics` 已经按字号换算好了，DirectWrite 却把换算留给了咱们，您两头对数的时候，口径差的就是这里，对照着记也就不容易混了。

拿到 `IDWriteFont` 本身的链路不长：入口是 `IDWriteFactory::GetSystemFontCollection`，按家族名找到的是 `IDWriteFontFamily`，咱们再从家族里取出字体对象。书架的排印章没铺过取字体的路，本篇的示例也用不上它，咱们真要自己量字体的时候，您按这几个接口名去查官方文档就对。

实用上还有一条近路：咱们根本不用自己加。`GetLineMetrics` 交回来的每一行都带着算好的 `height` 与 `baseline`，区间字号混排的时候行高各是各的，那套加法引擎已经替咱们做过一遍了。字体度量的价值在讲清来历，您真写编辑器的时候，行几何咱们从 line metrics 里拿就好，两套数各有各的岗位。

## 组串的下划线：输入法的预告线

最后一样东西您在中文输入法上天天见：打拼音的时候，您候选的字还没确认，界面上却已经浮出一串带下划线的字，它叫的就是组串（composition string），说的是输入法正在组织的临时文本。它最终会被确认的正式文字替换，所以在确认之前，咱们给它的待遇是“看得见但不算数”的样式：内容咱们照排，下面再垫一条下划线当它的预告。

版式层面的做法是现成的，区间样式本来就是 layout 的本事，咱们直接用：

```cpp
// Skeleton, not compiled. comp_start / comp_len track the IME
// composition range inside the text buffer.
DWRITE_TEXT_RANGE comp = {comp_start, comp_len};
g_layout->SetUnderline(TRUE, comp);
```

咱们写 `SetUnderline` 这一行不费事，真难的是 `comp_start` 与 `comp_len` 从哪儿来。组串的范围由输入法通过 `WM_IME_*` 一类消息报给窗口，Imm 系列的 API 管取串，那是一整块输入法的地界，咱们讲的渲染管线不往那边走。下划线怎么设，本篇讲清了，范围怎么跟输入法要，您按 `WM_IME_*` 与 Imm 系列的接口名去查官方文档就对，本篇的止步点也就画在这里。

## 动手清单

活儿其实都不难，原料全是本篇与书架里现成的东西，您改起来都不出几行：

1. 咱们把示例的 `isTrailing` 从写死的 `FALSE` 换成点击时存的 `is_trailing`，对同一个字的前半边与后半边各点一次，光标挪没挪半格，您也就看出这个参数管什么了。
2. 文案尾巴上添两三个空格，咱们把 `width` 与 `widthIncludingTrailingWhitespace` 打出来比一比，两个数的差就是尾部空格占的宽。
3. 把 `CreateTextLayout` 的最大宽度从 300 压到 120，`lineCount` 涨了，咱们再逐行打 `GetLineMetrics` 的明细，每行的 `length` 与 `height` 各是多少，断行的脾气也就落在您自己的输出里了。
4. 在文案里混一个 emoji，咱们对它的前半边与后半边各点一次，`htm.length` 报的是 2，插入点一跳就是两个单元，代理对的告诫就成了您亲眼见过的东西。
5. 给示例加上 `WM_MOUSEMOVE` 里按住左键拖动的分支，起点终点的两个位置喂给 `HitTestTextRange`，半透明选区就出来了。画在 `DrawTextLayout` 之前，咱们把次序反过来的样子也看一眼。
6. `g_caret_pos` 传成文本的长度（末尾再停），`HitTestTextPosition` 给的 x 落在哪儿，您打出来看看，末尾光标的行为也就有数了。

## 本篇的术语速查

| 术语 | 一句话的意思 |
|:--|:--|
| `GetMetrics` / `DWRITE_TEXT_METRICS` | 整块版式的实际尺寸、容器约束与行数 |
| `GetLineMetrics` / `DWRITE_LINE_METRICS` | 逐行的高度、基线与单元数，两段式调用 |
| `HitTestTextPosition` | 字符位置换坐标，光标竖线画在哪 |
| `HitTestPoint` | 坐标换字符位置，点选定位的入口 |
| `HitTestTextRange` | 一个范围换一行一块的矩形，选区高亮的原料 |
| `DWRITE_HIT_TEST_METRICS` | hit test 交回的几何包，含 `bidiLevel` |
| `isTrailingHit` / `isTrailing` | 光标停在簇的前边还是后边，阅读方向说了算 |
| caret | 插入点竖线，它跟鼠标指针（cursor）是两样东西 |
| `DWRITE_FONT_METRICS` | ascent、descent 无符号正数，lineGap 带符号，设计单位，除以 em 换算 |
| 组串 | 输入法未确认的临时文本，`SetUnderline` 垫预告线 |

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `DWRITE_TEXT_METRICS` 里哪两个数记的是实际尺寸，哪两个记的是容器约束？断行变多的时候，谁会跟着变？
2. `GetLineMetrics` 为什么是两段式调用？`actual` 交回来的是什么？
3. `HitTestTextPosition` 的第二个参数挑的是什么？同一个字符位置为什么能对应两条光标？
4. `htm.bidiLevel` 的奇偶说的是什么？从右往左的行里，“后边”在画面的哪一侧？
5. `HitTestPoint` 交回的 `length` 在算插入点时干什么用？连字为什么不能停在中间？
6. 光标的横坐标与高度分别取自 hit test 交回的哪两个东西？
7. 闪的节奏有几条路？它们各自归谁管？
8. 选区高亮的矩形谁替咱们围？画的次序为什么在 `DrawTextLayout` 之前？
9. `DWRITE_FONT_METRICS` 与 `SkFontMetrics` 的 ascent、descent，符号与单位各差在哪儿？
10. 组串的下划线用什么设？范围从哪儿来、本篇的止步点画在哪里？

## 下一步

文本的四篇到这儿就走完了。字符与字形的编号、整形的 cluster 映射、layout 的排版，再加上今天的测量命中，咱们手里的这些已经够撑起一个能点能选的文本视图了。按[篇目索引](index.md)的登记，下一站是[GPU 管线：从顶点到像素的各阶段](23_gpu_pipeline.md)，咱们从 CPU 的排版桌上抬起头，去看看一块像素在显卡里都经历了什么。您要是想趁热把文本的这套东西用熟，动手清单里选区的那一档值得做完。
