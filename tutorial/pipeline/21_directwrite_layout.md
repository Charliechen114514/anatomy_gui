---
title: DirectWrite：TextLayout 全解
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# DirectWrite：TextLayout 全解

按[篇目索引](index.md)的登记，本篇在文本段里排的是第三位。前一篇[《整形：连字、kerning、BiDi》](20_shaping.md)的结尾把话留给了咱们：分析、换算、定位那一整套回调加缓冲的活，`IDWriteTextLayout` 全替咱们包了。

后一篇[《测量、命中测试与光标》](22_text_metrics.md)管的正是版式到手之后的那一类查询，咱们到那边接着看。

上一篇咱们拿着 `IDWriteTextAnalyzer` 亲手走过了整形的全程，那副担子的分量，咱们是亲手掂过的。本篇咱们换到正路上来，看看 Windows 把一串字排成版式的官方路数。`IDWriteTextFormat` 是版式的模板，管着字号、字体与对齐这些全局的样子，`IDWriteTextLayout` 把具体的文本灌进模板，交出来的是一份排好了行、可以按区间改样式的版式实体，最后的画活交给 Direct2D。

## 工厂：DirectWrite 的出生地

DirectWrite 有它自己的工厂，接口的名字是 `IDWriteFactory`，咱们在[第六篇](06_d2d_targets.md)里认过 Direct2D 的 `ID2D1Factory`，两位是平行的关系：别的对象都从工厂的生产线上下来。创建它不走 `new` 的路子，靠的是模块级的函数 `DWriteCreateFactory`：

```cpp
#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

IDWriteFactory* dwrite_factory = nullptr;
HRESULT hr = DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(&dwrite_factory)
);
```

头一个参数是工厂的类型，能挑的值只有两个。`DWRITE_FACTORY_TYPE_SHARED` 表示进程与系统里其他用 DirectWrite 的进程共享字体缓存，字体数据在内存里也就留一份就够了，`DWRITE_FACTORY_TYPE_ISOLATED` 则是自起炉灶的独立缓存。书架的[DirectWrite 排印章](../native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography.md)给的态度很干脆：绝大多数场景写 SHARED 就对了，而只有需要加载大量私有字体、又不想与系统缓存互相干扰的工具，咱们才值得给它换 ISOLATED。咱们示例里写的都是 SHARED。

书架指的是 `tutorial/native_win32/` 目录下那套带完整示例的老教程，排印章是它在 DirectWrite 一线的章目，后面的引用咱们都照书架与排印章的叫法来。

COM 的地面规则咱们在第六篇已经铺过了：接口指针用完调它的 `Release()`，创建完了拿 `FAILED` 判返回值，`HRESULT` 的负值代表的则是失败。DirectWrite 也是建在同一套规范上的，咱们那套习惯原样搬过来就能用，本篇也就不再重讲它了。要补的只有链接一句：头文件用的是 `<dwrite.h>`，要链接的是 `dwrite.lib`。

## TextFormat：一段版式的模板

咱们要看版式模板的样子，对着 `CreateTextFormat` 的调用看最直接：

```cpp
IDWriteTextFormat* format = nullptr;
hr = dwrite_factory->CreateTextFormat(
    L"Microsoft YaHei",          // font family name
    nullptr,                     // font collection: system default
    DWRITE_FONT_WEIGHT_NORMAL,   // weight
    DWRITE_FONT_STYLE_NORMAL,    // style: normal / italic / oblique
    DWRITE_FONT_STRETCH_NORMAL,  // stretch
    18.0f,                       // font size, in DIP
    L"zh-CN",                    // locale name
    &format
);
```

参数咱们挨个念。字体家族名就是您在字体菜单里见到的名字，`nullptr` 的字体集合表示用的是系统装的那一套。粗细、样式、拉伸三样描述的是字的样子：粗细从 thin 排到 black 有一串枚举值，样式管的是正体斜体，拉伸管的是宽窄，咱们平时用到的都是 NORMAL 一档的默认值。字号那一栏的单位是 DIP，也就是咱们在[第三篇](03_dip_and_dpi.md)里认过的设备无关像素，18.0f 的字在任何缩放的屏上看起来一样大，换算的活归了引擎。末尾的区域名咱们单留一节讲，它牵着的是后面的字体回退。

模板的脾气咱们得认清一头：字体家族、字号、粗细这些在创建参数里就定死了，创建完了改不了，咱们想换个字号，就只好重造 format 了。段落级别的属性倒是例外：对齐、换行开关这些，创建之后还能用对应的 Set 方法调。

```cpp
format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
```

`SetTextAlignment` 管的是一行字在版式宽度里怎么摆：`LEADING` 贴的是阅读方向的起头，`CENTER` 摆的是中间，`JUSTIFIED` 拉的是两端对齐。值得您留意的还有 `LEADING` 与 `TRAILING` 的措辞：它们说的头和尾都长在阅读方向上，文字从左往右读的时候 leading 落在左边，从右往左读的时候它就换到了右边。上一篇咱们见过 `isRightToLeft` 参数，方向的事在模板层就有了入口：`SetReadingDirection` 定的是字往哪个方向读，`SetFlowDirection` 定的是行往哪个方向摞，两者合起来才是一段文字完整的方向。`SetParagraphAlignment` 管的则是另一头：整块版式在高度里怎么摆，`NEAR` 贴的是顶，`FAR` 贴的是底。

## TextLayout：把文本灌进模板

咱们手里的 format 毕竟只是模板，它到现在连一个字的边都没沾过。把具体的文本灌进模板的调用是 `CreateTextLayout`：

```cpp
const wchar_t* text = L"Windows 把字排好，用的是 DirectWrite。";
IDWriteTextLayout* layout = nullptr;
hr = dwrite_factory->CreateTextLayout(
    text,
    static_cast<UINT32>(wcslen(text)),
    format,
    300.0f,   // max width, in DIP
    100.0f,   // max height, in DIP
    &layout
);
```

参数表排的是文本、长度、模板加一对尺寸上限。尺寸说的是版式的容器有多大：宽 300 DIP 的容器里放不下的一行，引擎会在该断的地方断成两行，高度不够的时候，放不下的行不会被排进版式，咱们在画面上也就看不到它了。您把上面的调用与 `CreateTextFormat` 对比一下就看得出来：format 管的是样子，layout 管的是这一段字在这么大的容器里实际怎么排。

排版的活都有什么？咱们在上一篇亲手干过的那些全在这里面：按 script 切 run、整形换 glyph、算 advance 与 offset、BiDi 的重排，再添上断行与字体回退的活。书架排印章列的清单里有字形选择、双向排列、断行、字体回退四样，头三样咱们在[上一篇](20_shaping.md)里已经把机理看过一遍，回退的事咱们下面单讲。您可以把 layout 想成一位包工头，`IDWriteTextAnalyzer` 那样的车间都归它调度，咱们平时只跟包工头打交道。

format 与 layout 还有一处分工得说清：format 是不可变的模板，一份 format 能灌的文本有无数段，layout 却与具体的文本绑死了，文字内容变了咱们不能在旧 layout 上改字，能走的只有销毁旧的、再建一份新的。书架的排印章在常见问题里专门写过这件事，理由也直白：排版分析是针对具体文字做的，文字换了分析也就得重做。所以咱们工程里的习惯是 format 建一次用到底，layout 的生死跟着文本走。

排出来的版式咱们还能查询。`GetMetrics` 交回的 `DWRITE_TEXT_METRICS` 里有排版之后的实际宽度、高度与行数，高度那一项以布局的高度上限为界，还有记着 BiDi 嵌套深度的 `maxBidiReorderingDepth`，咱们在上一篇讲双向排版时见过它。详细的测量、命中测试与光标是下一篇的整篇主角，咱们在这里只认个脸熟，详细的活留给它。

## 区间格式：DWRITE_TEXT_RANGE

咱们要在同一版式里给不同的字不同的样式，靠的是一个只有两个字段的小结构：

```cpp
struct DWRITE_TEXT_RANGE {
    UINT32 startPosition;
    UINT32 length;
};
```

它记的就是起点加长度，圈住的是一段文字的范围。这里的单位问题，咱们在[第十九篇《字符、码点与字形》](19_chars_and_glyphs.md)数代理对的时候就打过招呼：position 数的是 UTF-16 的单元，一个汉字占的是一单元，一个 emoji 的代理对占的却是两单元。您给含 emoji 的文字算区间的时候，咱们得把两个单元都数进去，不然样式会从半个字的中间劈开。

咱们拿它改样式的调用长成下面这样：

```cpp
// Bold "DirectWrite" (11 units, found at runtime below).
const wchar_t* hit = wcsstr(text, L"DirectWrite");
DWRITE_TEXT_RANGE bold_range = {
    static_cast<UINT32>(hit - text), 11
};
layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, bold_range);

// Make the Chinese part larger.
DWRITE_TEXT_RANGE cjk_range = { 8, 8 };
layout->SetFontSize(22.0f, cjk_range);
```

`SetFontWeight` 一家的方法走的都是同一个路数：一个值配一个范围，只动圈住的那段，圈外的字按模板的默认走。上面加粗的位置咱们没有写死成魔数，用的是 `wcsstr` 现场找的，改文案的时候加粗跟着词走，也就不用重新数位置了。

颜色是区间格式里最别扭的一件事，咱们单拎出来说。`IDWriteTextLayout` 的方法表里有 `SetFontWeight`、`SetFontSize` 一长排，偏偏没有 `SetFontColor`，而 DirectWrite 只管排版、不管画，颜色归的是画的那一头。口子留的是 `SetDrawingEffect`：

```cpp
// red_brush came from the D2D render target; see below.
layout->SetDrawingEffect(red_brush, bold_range);
```

它收的是一个 `IUnknown*`，DirectWrite 排版的时候不解释它，把它原样记了下来，画的时候再递给动手画的那一方。用 Direct2D 的 `DrawTextLayout` 画的时候，D2D 内置的渲染器会把它认成 `ID2D1Brush`，画刷恰好实现了 `IUnknown`，所以您可以直接把 D2D 画刷塞进去。咱们觉得绕的设计，道理要讲到自定义渲染器才看得清：换成咱们自己的渲染器，指针解释成什么就全归咱们说了算。

生命周期上有一处要当心：layout 会对塞进来的对象调 `AddRef`，所以咱们 `SetDrawingEffect` 之后把自己手里的引用 `Release` 掉也不会出事，对象由 layout 替咱们保着。反过来还有一头要记着：画刷要是设备相关的资源，设备丢失又重建了之后，layout 里还挂着旧的引用，咱们得把区间的效果重新设一遍。

## D2D 画它的三条路

版式排好了，画的活归 Direct2D，咱们按从小到大的次序看它的三条路。

头一条的入口是 `ID2D1RenderTarget::DrawText`，咱们叫它小路：您递给它文本、format、一个矩形加一个画刷，它在内部临时排的版，画完也就丢掉了。一行标签、一个按钮上的字，格式单一、内容又不常变的场合走它最省事。同一段字反复画的时候，排版的功夫就白花了，所以它只配干轻活。

正路的入口是 `DrawTextLayout`，咱们示例里走的就是它：

```cpp
render_target->DrawTextLayout(
    D2D1::Point2F(20.0f, 20.0f),  // top-left corner of the layout
    layout,
    default_brush,
    D2D1_DRAW_TEXT_OPTIONS_NONE
);
```

排版在 layout 建的时候已经做过一遍，`DrawTextLayout` 只管把现成的结果画出来，同样的内容画十遍，排版也就只算一回了。参数里的 `default_brush` 管的是没设过 `SetDrawingEffect` 的区间，设过效果的那些段用的是自己挂着的画刷，分工的来路，咱们在上面讲 `SetDrawingEffect` 的时候已经摆过。选项参数里有一个值得记下的：`D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT` 打开的是彩色字体，emoji 的彩色样式靠它才出得来。DrawText 与 DrawTextLayout 都得夹在[第六篇](06_d2d_targets.md)里讲过的 `BeginDraw` 与 `EndDraw` 中间，章法与别的 D2D 绘制调用是一样的。

咱们再往深走一层，遇到的就是自己实现的回调接口 `IDWriteTextRenderer`，咱们叫它深路。咱们的实现坐在回调的位置上，DirectWrite 把版排完了之后，把每个 glyph run 连同它的位置、挂着的 drawing effect 一个个递过来，画什么、怎么画全归了咱们。它的方法表里 `DrawGlyphRun` 是主角，`DrawUnderline`、`DrawStrikethrough`、`DrawInlineObject` 三位当的是配角，管的是下划线、删除线与内嵌对象。上一篇咱们拿 `GetGlyphs` 与 `GetGlyphPlacements` 亲手凑齐过 `DWRITE_GLYPH_RUN` 的原料，当时咱们的说法是把它递给 `ID2D1RenderTarget::DrawGlyphRun`，屏幕上就有字了。自定义渲染器拿到手的正是同一个结构，DirectWrite 等于把咱们上一篇手动干的活全包了，把成品递回来请咱们过目。

什么时候值得走深路？内置渲染器不会替咱们干的活，比如自定义下划线的样式、选中区域画半透明底色、行内嵌图，才轮到咱们自己出场。`DrawTextLayout` 有一个收自定义渲染器的重载，普通款与深路款共用的是同一份 layout。

## 字体回退：缺字的时候谁来补

示例的文案是中英混排，模板指定的家族却只有 Microsoft YaHei 一家。英文它是兜得住的，可要是咱们把家族换成 Segoe UI，汉字它就兜不住了，Segoe UI 家族里没有汉字的字形。DirectWrite 对缺字情况的处理叫字体回退：排版的时候发现当前字体缺某个字的字形，它就按系统的回退映射挑一款装得下的替补字体，缺的字就由替补接着排了。咱们真把家族换过去之后，您在界面上看不到方块，画出来的每个汉字都是回退悄悄补的。

书架排印章给了回退正常工作的两个前提：一头是系统里装着装得下对应字符的字体，另一头的区域设置也得写对。`CreateTextFormat` 末尾写的 `L"zh-CN"` 在这里派上了用场，回退映射的挑选要参考它，中文界面把区域写对了，替补也就挑得准了。想自定义回退的规则也有门路，比如咱们想让日文字符用 Meiryo 而不是 YaHei，接口用的是 `IDWriteFontFallback`，它住的是 `dwrite_2.h`，Windows 8.1 起的系统才提供它。咱们没有那样的需求，知道接口名和它住的文件就行，详细的去处书架排印章里写着。

## DIP：第三篇的预告在这里兑现

[第三篇](03_dip_and_dpi.md)讲坐标的时候咱们预告过：DirectWrite 的字号同样以 DIP 计，18.0f 的字在任何屏上看起来一样大。今天咱们看到的浮点数全落在预告的范围里：

- `CreateTextFormat` 的字号，单位是 DIP
- `CreateTextLayout` 的宽度与高度上限，单位是 DIP
- `GetMetrics` 交回的宽与高，单位还是 DIP
- `DrawTextLayout` 的起点，用的是与 D2D 同一套的 DIP

好处是直通的：layout 量出来的尺寸可以直接喂给 D2D 的矩形用，咱们一个换算都不用做，因为两头共用的是 96 这个基准。150% 缩放的屏上 DPI 是 144，18 DIP 的字号折成物理像素是 27 个，乘法是引擎在内部做的，咱们的代码一行都不用改。

咱们再把第三篇的两套体系对一遍：GDI 那边的坐标是整数像素，布局常数得咱们自己乘 DPI，而 DirectWrite 与 Direct2D 这边，换算的活归了 API 的内部。咱们再回头看上一篇的 `GetGlyphPlacements`，它算出来的 advance 也是按 DIP 计的，单位当然从头到尾也只有 DIP 一种。

## 示例：一段中英混排走正路

咱们把前面散着的调用串成一份能看的骨架。代码的来历得交代在前面：它是笔者按 `dwrite.h` 的声明与书架排印章的调用形状组织的骨架，没有经过编译的验证，参数的名字与次序请您以手头的 SDK 头文件为准。窗口、消息循环与 D2D 渲染目标都是[第六篇](06_d2d_targets.md)与[裸窗口一篇](../gui/01_naked_win32.md)里现成的东西，咱们不重复，只看文本一侧新增的部分。

咱们把设备无关的资源建一次用到底，factory、format、文本与 layout 都算设备无关的一档：

```cpp
// Device-independent, created once. Skeleton, not compiled.
static IDWriteFactory*    g_dwrite  = nullptr;
static IDWriteTextFormat* g_format  = nullptr;
static const wchar_t*     g_text =
    L"Windows 把字排好，用的是 DirectWrite。";
static IDWriteTextLayout* g_layout = nullptr;
```

创建的次序是 factory、format、layout 一路下来，咱们把三步连着看：

```cpp
// Skeleton, not compiled.
void CreateTextResources() {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                        __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&g_dwrite));

    g_dwrite->CreateTextFormat(
        L"Microsoft YaHei", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"zh-CN", &g_format);
    g_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    g_dwrite->CreateTextLayout(
        g_text, static_cast<UINT32>(wcslen(g_text)),
        g_format, 300.0f, 100.0f, &g_layout);
}
```

layout 建好了，区间样式就可以在它身上动手术了。英文词加粗的位置，咱们用 `wcsstr` 现场找，中文部分的起点与长度按文案数 UTF-16 的单元，两处咱们都设上：

```cpp
// Skeleton, not compiled.
void ApplyRangeStyles(ID2D1RenderTarget* target, ID2D1Brush* red) {
    const wchar_t* hit = wcsstr(g_text, L"DirectWrite");
    DWRITE_TEXT_RANGE bold = {
        static_cast<UINT32>(hit - g_text), 11 };
    g_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, bold);
    g_layout->SetDrawingEffect(red, bold);

    DWRITE_TEXT_RANGE cjk = { 8, 8 };   // "把字排好，用的是" by hand
    g_layout->SetFontSize(22.0f, cjk);
}
```

中文区间的 8 和 8 两个数字值得咱们停一下。文案开头的 `Windows ` 占了 8 个单元（七个字母加一个空格），所以咱们把中文的起点定在 8。咱们接着往下数，中文的那一段是七个汉字加一个全角逗号，凑成的正好是 8 个单元，边界就停在 DirectWrite 前面的空格上。这是笔者按文案手数的，您换了自己的文案就得重数，工程里正确的做法是让分词或 token 化的代码算出范围再填进去，手数的写法只配出现在教程里。

咱们把画的部分挂在 `WM_PAINT` 的路数上，D2D 那侧的对象与章法全是第六篇的老朋友：

```cpp
// Skeleton, not compiled.
void OnPaint(ID2D1HwndRenderTarget* target,
             ID2D1SolidColorBrush* black) {
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));

    target->DrawTextLayout(
        D2D1::Point2F(20.0f, 20.0f),
        g_layout, black, D2D1_DRAW_TEXT_OPTIONS_NONE);

    HRESULT hr = target->EndDraw();
    // D2DERR_RECREATE_TARGET handling: see article 06.
}
```

跑起来的画面是这样的：深灰的底上有一行字，英文词 DirectWrite 加了粗、带了红，中间的汉字比两头大一号，行高是按大的字号撑开的。红画刷属于设备相关的资源，命运跟渲染目标绑在了一起，设备丢失又重建了之后，区间效果咱们要重设一遍，这正是咱们在生命周期那段提醒过的活。`EndDraw` 报回 `D2DERR_RECREATE_TARGET` 的处置，第六篇给过完整的回路，骨架里咱们注释指路，就不展开了。

## 动手清单

下面几处动手的活儿，原料都是本篇与书架里现成的东西，您改起来都不出几行：

1. 咱们把示例的家族换成 `L"Segoe UI"` 再看画面，汉字照样显示出来了，您就亲眼见到回退干活的样子了，您再把区域名改掉试一遍，替补挑得准不准也就有了对比。
2. 咱们拿 `GetMetrics` 把 `widthIncludingTrailingWhitespace` 与 `width` 打出来对比，咱们在文案末尾添一个空格，两个值的差就是尾部空白占的宽。
3. 咱们把 `CreateTextLayout` 的最大宽度从 300 改成 120，同一份文案断成了几行，您数一遍画面里的行数，`lineCount` 报出来的数就对上了。
4. 咱们在文案里添一个 emoji，加粗区间的长度忘了算代理对，样式从半个字中间劈开的样子就落在您自己的屏幕上了，[第十九篇](19_chars_and_glyphs.md)的告诫也就见了实物。
5. 咱们给 `DrawTextLayout` 的选项换成 `D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT`，文案里的 emoji 从黑白变成了彩色，开关管的是什么也就看明白了。

## 本篇的术语速查

这些词咱们后面的篇目还要接着用，咱们把它们集中列一份，您忘了的时候回来查：

| 术语 | 一句话的意思 |
|:--|:--|
| `DWriteCreateFactory` / `IDWriteFactory` | DirectWrite 的入口函数与工厂接口，SHARED 共享系统字体缓存 |
| `IDWriteTextFormat` | 版式的模板，字体家族、字号、粗细这些创建参数定死，对齐与方向创建后可用 Set 方法改 |
| `IDWriteTextLayout` | 文本灌进模板得到的版式实体，可按区间改样式 |
| `DWRITE_TEXT_RANGE` | 起点加长度的范围对，单位是 UTF-16 单元 |
| `SetDrawingEffect` | 挂任意 `IUnknown*` 的口子，D2D 内置渲染器把它认成画刷 |
| `DrawText` / `DrawTextLayout` | D2D 画文字的小路与正路，后者复用排版结果 |
| `IDWriteTextRenderer` | 自定义回调接口，glyph run 一个个递给咱们的实现 |
| 字体回退 | 当前字体缺字时系统挑替补字体，区域设置参与挑选 |
| `IDWriteFontFallback` | 自定义回退规则的接口，dwrite_2.h，Windows 8.1 起 |
| DIP | 设备无关像素，DirectWrite 全程的单位，96 为基准 |

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `DWRITE_FACTORY_TYPE_SHARED` 与 ISOLATED 差在哪？书架建议咱们选哪个？
2. format 与 layout 谁是不可变的模板，谁绑死了具体的文本？换文案的时候哪个要重建？
3. `SetTextAlignment` 的 `LEADING` 为什么不说成左对齐？它与阅读方向是什么关系？
4. `CreateTextLayout` 的两个尺寸参数管的是什么？宽度不够的时候引擎做什么？
5. `DWRITE_TEXT_RANGE` 的 position 数的是什么单位？代理对占几个？
6. layout 的方法表里为什么没有 `SetFontColor`？颜色的正路是怎么设的？
7. `SetDrawingEffect` 塞进去的画刷，引用计数归谁管？设备丢失之后哪件事得重做？
8. `DrawText` 与 `DrawTextLayout` 各适合什么场景？反复重绘同一段字时差在哪？
9. `IDWriteTextRenderer` 的 `DrawGlyphRun` 递给咱们的东西，与上一篇哪个结构是同一件？
10. 字体回退正常工作靠哪两个前提？区域名在哪一步起作用？
11. 本篇出现的浮点数，从字号到 `GetMetrics` 的宽度，单位统一是什么？它与 D2D 的坐标是什么关系？

## 下一步

版式实体到手了，下一篇的主角正是本篇只认了脸熟的那一类查询：鼠标点的位置落在哪个字上，光标的落点在哪个坐标，选区的两头各是哪个 position。主角的名字叫[《测量、命中测试与光标》](22_text_metrics.md)，`HitTestPoint` 与 `HitTestTextPosition` 的名字在书架排印章里露过脸，cluster 映射与显示顺序这些上一篇攒下的家当，到了那边也就真正派上用场了。您要是急着动手，拿本篇的 layout 配上书架的命中测试小节就能玩起来，讲全它们的活，咱们留给下一篇。
