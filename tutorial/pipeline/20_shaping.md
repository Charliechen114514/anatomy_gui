---
title: 整形：连字、kerning、BiDi
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 整形：连字、kerning、BiDi

按[篇目索引](index.md)的登记，本篇在文本段里排的是第二位。前一篇[《字符、码点与字形》](19_chars_and_glyphs.md)已经跟您见过面了，码点、字形与字素簇的地基都在那边打好了。排在后一篇的[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)，把整形的活收进了一个对象，咱们到那边再看它怎么收。段里的末一篇则归[《测量、命中测试与光标》](22_text_metrics.md)。

上一篇的结尾咱们留了话头：`cmap` 干的只是码点到字形的一对一查表，把一与多的错位全摆平的工序叫整形，本篇讲的正是它的里里外外。主线咱们用一句话摆开：整形收下的是码点序列加上一款字体，交出去的是画得出也排得对的 glyph 序列。连字怎么挑、字距怎么收、从右往左的文字怎么跟从左往右的混在一行，全都在这一道工序里落了定。上一篇咱们认过 `SkTextEncoding::kGlyphID` 那个口子，它等着的就是整形交出来的货。

## 整形在管线里干什么

咱们把输入的一头数一数。整形器要的远远不止一串码点：它还得知道用的字体是哪一款，因为连字与字距的规则全住在字体文件里。它还得知道文字的方向，还有它的书写系统，它的行话名字是 script，咱们说的拉丁、阿拉伯、汉字各占一个 script。同一个码点换了个 script，挑出来的字形可能就跟着换了。有的场合咱们还得告诉它语言，同一个 script 底下不同语言的排版习惯并不一样。进去的这一头缺了哪样，出来的字形就可能有出入。

输出的一头咱们也摆齐。整形器交回来的东西比一列光秃秃的 glyph 编号多出好几样，它是几样配套的家当：每个 glyph 的编号，每个 glyph 往前走多少的 advance，往上往下偏多少的 offset，还附着一份 cluster 的映射，它记的是哪个 glyph 对应源文本的哪一段。advance 您在[《Skia 的文本》](15_skia_text.md)的测量一节见过，offset 的用武之地在变音符号一类的附加标记上，它们不占自己的位置，得贴着底下的基字符挪。cluster 的映射听着抽象，咱们举个具体的：fi 连字把两个字符换成了一个 glyph，映射里记下的就是两个文本位置共享同一个 glyph，光标与选区的算法靠它才不会把连字数错。

## 字距的微调：kerning

咱们从最轻的一件事看起。大写的 A 与 V 挨在一起的时候，A 的右腿往左斜，V 的左肩也往左斜，两个斜角之间就空出了一块没人住的地方，排印的行话管这块空隙叫光学留白。字距微调（kerning）干的活就是把这块空隙收回来：相邻的字形要是约好了，排的时候就让后面那个往前面蹭一点，advance 加上一个负的修正量。您拿 To、Va、Av 这样的组合跟 TT、oo 摆在一起对比，收与不收的差别一眼就看得出来，衬线体的字形斜角更明显，差别也就更明显了。

那规则住在哪里？老 TrueType 的字体里有一张 `kern` 表，它其实就是一张“字形对到修正量”的查表，结构倒是简单，能表达的也只有配对这一路。到了 OpenType 的时代，这摊活搬进了 `GPOS` 表，它的全名是 Glyph Positioning Table，管的是字形定位：配对的字距、标记对基字符的贴靠、对整段上下文的定位规则，全住在了它的辖区里。两份家当咱们都可能在老字体里见到，新引擎的态度笔者没有逐家验证过，可大方向上认的是 `GPOS`，老 `kern` 表在不少新引擎里默认是不吃的，您手头真有只带 `kern` 表的老字体，值得您亲手测一遍再放心。

还有一处容易混的咱们顺路分清：kerning 改的只是 advance 的修正量，字形本身是一笔一画都不动的。咱们再看字形轮廓外圈自带的那圈白边，宽窄在字体设计的时候就定死了，kerning 动的并不是它，它调的是两个现成字形摆在一起时的间距。kerning 落地在定位的那一步，换字的那一步管不着它。

## 连字：GSUB 的换字活

连字（ligature）动到的是字形本身。衬线体里 f 与 i 相邻的时候，f 头上的弯钩会跟 i 的点打起架来，铸排时代的解法干脆利落：直接铸了一个 ﬁ 活字，把两个字母装进了一个形里。字体文件延续的就是这个办法：`GSUB` 表咱们可以想成一摞规则，写的都是“看到了这样的 glyph 序列，就换成那样的序列”。fi 的合成走的是名叫 `liga` 的特性开关，它属于可选的连字，排版的人可以决定开不开。

阿拉伯文的连写走的则是必需连字，它的特性名叫 `rlig`，排版的人说了不算：阿拉伯的字母在词里必须连着写，不开 `rlig` 交出来的就是一串互不相连的破字。连字之后两个文本位置就共享一个 glyph 了，上一节说的 cluster 映射在这里就派上了用场。连字的一位远亲是上下文形式，咱们接着看的还有它，因为两家住的是同一张表。

## 看邻居挑字形：上下文形式

阿拉伯文的每个字母都有几副长相：单独站着的是一副，站在词头、词中、词尾的又各是一副，行话给的叫法是独立形、首形、中形、尾形。`cmap` 查出来的默认形之外还有三副，归 `GSUB` 的上下文规则挑，对应的特性名是 `isol`、`init`、`medi`、`fina`，头一个 `isol` 管的就是咱们说的独立形。机制咱们用白话讲就是：整形器要看的就是前一个字符与后一个字符，判断当前字母在词里站的是什么位置，再按位置把默认形换成该用的那副形。天城文那一系的元音附标也是同一套机制在管：附标的形式与重排由 `GSUB` 的上下文规则挑，贴到基字符上方下方的位置偏移则归 `GPOS` 的定位特性管。

咱们写中文界面几乎碰不到上下文形式，汉字的字与形基本一对一，可咱们的文字只要走了整形器，这套逻辑引擎照样替咱们跑一遍。它也解释了上一篇的一句旧话：一款字体里 glyph 的总数通常比字符数多，多出来的那部分就是连字与各路变体的形。

## 两个方向的混排：BiDi

方向的事咱们从一行具体的字看起。希伯来文与阿拉伯文是从右往左写的，可一段从右往左的文字里混着阿拉伯数字与英文单词的时候，数字与英文自己还是从左往右读的，咱们眼里的下一个格子，跟内存里的下一个单元，也就分了家。存储与打字走的顺序叫逻辑顺序，屏幕上排列的顺序叫显示顺序，把逻辑顺序换算成显示顺序的算法就是 Unicode 的双向算法，它的标准编号是 UAX #9。这套算法按每个字符的方向类别算出嵌入层级，再按层级把各级文字的次序排出来，段落自己的方向由开头的强方向字符定，显式的控制符也可以插手。

双向排版在行话里的叫法是 BiDi，它是 bidirectional 的缩写。它跟整形是什么关系？方向得排在整形的前头：从右往左的语境里，圆括号得换成镜像的字形，方向本身就是挑字形的依据之一，咱们后面看 DirectWrite 的接口时，`isRightToLeft` 的参数会一路传到换字与定位的每一步。书架（也就是 `tutorial/native_win32/` 目录下那套带完整示例的老教程）的[DirectWrite 排印章](../native_win32/32_ProgrammingGUI_Graphics_DirectWrite_Typography.md)在 `DWRITE_TEXT_METRICS` 里露过一个 `maxBidiReorderingDepth` 字段，它记的就是一段文字里双向嵌套排到了多深，它算得上您调试时的好向导。

BiDi 还有一处值得您留心的麻烦：显示顺序换算完了之后，从左往右数第 n 个 glyph 的位置，跟逻辑上第 n 个字符的位置，换算完就对不上了，命中测试与光标的挪动都得回答一个问题，您问的到底是哪个顺序。那边的活，咱们就留给[《测量、命中测试与光标》](22_text_metrics.md)。

## 整形引擎是谁：HarfBuzz 与 DirectWrite

道理讲完了，干活的库咱们认一认。跨平台的世界里事实上的标准叫 HarfBuzz，它是开源的整形库，Chrome 与 Firefox 的浏览器用它，Qt 用的也是它，Android 的文本栈用的同样是它。它的输入输出模型咱们拿一段伪码摆开，接口名是 HarfBuzz 公开的 C API，流程是真实的，变量组织笔者做了压缩：

```c
// Pseudocode: the real API is a C API with error codes and
// object lifetimes, omitted here for clarity.
hb_buffer_t* buf = hb_buffer_create();
hb_buffer_add_utf16(buf, text, text_length, 0, text_length);

// Script, direction and language can be guessed from the text.
hb_buffer_guess_segment_properties(buf);

// The shaping call: font + buffer in, glyphs out. The buffer's
// code points are replaced in place by glyph IDs.
hb_shape(font, buf, nullptr, 0);

// After shaping, each hb_glyph_info_t carries:
//   codepoint -> the glyph ID (no longer a code point)
//   cluster   -> the source text position this glyph came from
// and each hb_glyph_position_t carries x_advance and offsets.
```

咱们看它进去与出来的两副模样：进去的 buffer 装的是码点，`hb_shape` 调用完了之后，同一块 buffer 里的东西就换成了 glyph 编号，cluster 字段把每个 glyph 都锚回了源文本。上一篇咱们数过家庭 emoji 的码点与单元，它就是 👨‍👩‍👧 那类用零宽连接符把几个家庭成员拼成一个的组合 emoji，整形之后它的成员们共享一个 cluster，道理跟前文连字的 cluster 是同一件。

Windows 这边 DirectWrite 走的却是自家的路，整形是它自己实现的，它的入口是 `IDWriteTextAnalyzer`，工厂的 `CreateTextAnalyzer` 造得出它。`IDWriteTextLayout` 把整形的活整个包了起来，书架的排印章里那些排版调用，底下走的都是它。咱们下一节就拿着 `IDWriteTextAnalyzer` 亲手走一遍，看看不借 `IDWriteTextLayout` 的手，整形露出来的是什么样子。

## 示例：拿 IDWriteTextAnalyzer 走一遍

咱们把目标定窄：不走完整的排版，只走分析文字、拿到一个 glyph run 的骨架。本节的代码是笔者按 `dwrite.h` 的声明回忆写下的骨架，没有经过编译的验证，参数的名字与次序您以手头的 SDK 头文件为准。流程咱们分成两步：头一步的活是分析，`AnalyzeScript` 与 `AnalyzeBidi` 是同一家的兄弟，靠回调把文字切成一段段的 run，每一段的内部 script 与方向一致。本篇的骨架只走 script 这一半，方向的活 `AnalyzeBidi` 您照同样的路数接。第二步的活是换算，`GetGlyphs` 换出 glyph 的序列，`GetGlyphPlacements` 把每个 glyph 的位置算出来。

分析这一步用的回调接口是 `IDWriteTextAnalysisSource`，咱们实现它，把待分析的文字递给分析器：

```cpp
// Skeleton, not compiled. Signatures quoted from memory of
// dwrite.h; check your SDK header before trusting them.
class TextSource : public IDWriteTextAnalysisSource {
public:
    // IUnknown: QueryInterface / AddRef / Release, omitted here.
    // GetTextBeforePosition exists too; omitted for brevity.

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(
            UINT32 position, const WCHAR** text,
            UINT32* length) override {
        // Hand out the whole string; DirectWrite calls this
        // to fetch the range starting at "position".
        *text = text_;
        *length = text_length_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetLocaleName(
            UINT32 position, UINT32* length,
            const WCHAR** locale) override {
        // The whole string is one locale. *length is how much
        // text this locale covers, not the locale name's length.
        *locale = L"zh-CN";
        *length = text_length_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(
            UINT32 position,
            IDWriteNumberSubstitution** subst) override {
        *subst = nullptr;  // no digit substitution in this demo
        return S_OK;
    }

private:
    const WCHAR* text_;
    UINT32 text_length_;
};
```

分析的结果由另一个回调接口收货，它的名字叫 `IDWriteTextAnalysisSink`，咱们同样实现它，把分析器报回来的每一段记进自己的列表：

```cpp
struct ScriptRun {
    UINT32 position;
    UINT32 length;
    DWRITE_SCRIPT_ANALYSIS analysis;  // script id + shapes
};

class AnalysisSink : public IDWriteTextAnalysisSink {
public:
    // IUnknown omitted as above.

    HRESULT STDMETHODCALLTYPE SetScriptAnalysis(
            UINT32 position, UINT32 length,
            const DWRITE_SCRIPT_ANALYSIS* analysis) override {
        runs_.push_back(ScriptRun{position, length, *analysis});
        return S_OK;
    }

    // SetBidiLevel, SetLineBreakpoints, SetNumberSubstitution:
    // part of the same interface, used by the sibling Analyze*
    // calls, omitted here.

private:
    std::vector<ScriptRun> runs_;
};
```

两边的回调都备齐了，分析的调用咱们就剩一行：

```cpp
// analyzer came from factory->CreateTextAnalyzer(&analyzer).
HRESULT hr = analyzer->AnalyzeScript(
    &source, 0, text_length, &sink);
```

跑完之后 `runs_` 里躺着的，就是把整段文字按 script 切开的段表。一句只含汉字与拉丁字母的文案，汉字与拉丁各归各的 run，具体几段咱们听分析器的，边界咱们也只管收。

换算的那一步是重头。`GetGlyphs` 拿一段 run 换出对应的 glyph 序列，它的参数表很长，咱们按职责念一遍：开头的参数是文字与长度，随后排的是 `IDWriteFontFace`，再往后排的是 `isSideways` 与 `isRightToLeft` 两个方向开关，跟着进来的还有 `scriptAnalysis`、区域名与数字替换，末尾等着的是一串输出缓冲：

```cpp
// One buffer set per run, sizes taken from dwrite.h docs.
std::vector<UINT16> cluster_map(text_length);
std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> text_props(text_length);

UINT32 glyph_count = 0;
analyzer->GetGlyphs(
    run_text, run_length, font_face,
    FALSE,                    // isSideways
    FALSE,                    // isRightToLeft (CJK run here)
    &run.analysis, L"zh-CN", nullptr,
    nullptr, nullptr, 0,      // typographic features: none
    0,                        // maxGlyphCount: see note below
    cluster_map.data(), text_props.data(),
    glyph_indices.data(), glyph_props.data(),
    &glyph_count);
```

这段骨架里有一处笔者故意留白的：`maxGlyphCount` 要传缓冲的容量，而 glyph 数的量级跟文字长度并不相等，连字的时候它会缩，一拆多的场合它又会涨。文档给的初始估算是文字长度的 1.5 倍再添 16，稳妥的路数是头一趟按这个估算开缓冲，返回 `E_NOT_SUFFICIENT_BUFFER`（它就是 `HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)`）的时候按需扩缓冲再投一遍。上面注释里写的 0 只是占位，咱们不拿它冒充能跑的代码。

咱们看 `cluster_map`，出来的就是前文念叨的 cluster 映射：每个文本位置记的都是一个数，指向它所属那簇起头的 glyph 下标。等 glyph 的序列到手之后，定位的调用接上来，kerning 的修正量就是在这里被算进 advance 的：

```cpp
std::vector<FLOAT> advances(glyph_count);
std::vector<DWRITE_GLYPH_OFFSET> offsets(glyph_count);

analyzer->GetGlyphPlacements(
    run_text, cluster_map.data(), text_props.data(),
    run_length, glyph_count, font_face, font_size,
    FALSE, FALSE,              // sideways / right-to-left
    &run.analysis, L"zh-CN",
    nullptr, nullptr, 0,       // features again: none
    glyph_indices.data(), advances.data(), offsets.data(),
    glyph_props.data());
```

等咱们走到这里，`DWRITE_GLYPH_RUN` 的几样原料就凑齐了，它要的就是 `fontFace`、`glyphIndices`、`advances`、`offsets` 与 `glyphCount` 等几样的组合。您要是把它递给 `ID2D1RenderTarget::DrawGlyphRun`，屏幕上就有字了。`DrawGlyphRun` 是本篇头一回见的接口，章法不新：它跟别的绘制调用一样，得夹在[第六篇《工厂、设备与渲染目标》](06_d2d_targets.md)讲过的 `BeginDraw` 与 `EndDraw` 的中间。您真要写工程代码的时候，请您掂量一句劝：上面这一整套回调加缓冲的活，`IDWriteTextLayout` 全替咱们包了，按登记它有《DirectWrite：TextLayout 全解》的专篇，您没有“一段 run 自己控制”的硬需求时，就别去接今天这副担子了。

## Skia 的处境：SkShaper 接上口子

Skia 那边咱们收线。咱们在[《Skia 的文本》](15_skia_text.md)里说过，整形不住在 Skia 的核心里，核心管的是画 glyph，整形住在单独的 `SkShaper` 模块里，底下常用的实现包是 HarfBuzz。今天咱们把整形器的输入输出认完了，那边留的口子就能接上：整形器交出来的是 glyph 序列与 cluster 映射，咱们把它喂给 `SkTextBlob` 一类的对象，`SkTextEncoding::kGlyphID` 的口子收的就是它，查表的活一次都不用重做。`SkShaper` 是构建时可选的模块，您拿到的 Skia 构建里配没配上 HarfBuzz 的后端，得看构建的配置，笔者不能替您的构建背书。

Windows 上还有一处交叉咱们提一句：Skia 在 Windows 的构建里常拿 DirectWrite 当字体的来源，字体怎么找、typeface 怎么造是 DirectWrite 那一摊，整形咱们仍交给 `SkShaper`，配的后端通常还是 HarfBuzz。所以不管整形是谁干的，交给 Skia 的都是 glyph 编号，`kGlyphID` 的口子都认。

## 动手清单

下面几处动手的活儿，原料都是本篇与书架里现成的东西，您改起来都不出几行：

1. 拿书架的排印章示例跑一段希伯来文或阿拉伯文的文案，看看 `DWRITE_TEXT_METRICS` 的 `maxBidiReorderingDepth` 报出来的深度，纯中文混英文的时候它报的又是什么，您对一边就有感觉了。
2. 在 `GetTextAtPosition` 的实现里打上断点，看看 DirectWrite 来取文字的位置与次数，分析器分几趟来取的，您亲眼看一遍比读十遍文档记得牢。
3. 把示例里的 run 换成一段纯英文，喂给一款带 fi 连字的衬线字体（Times 一类的老牌字体都有），数一数 glyph_count 与文字长度，连字生效的时候前者会小，您就亲眼见到一换多了。
4. 拿 `hb_shape` 的伪码对照本篇的 `GetGlyphs`，把两边的输入输出各列一列，cluster 映射与 `cluster_map` 的对应关系您就能自己讲出来了。
5. 故意把 `GetGlyphs` 的 `isRightToLeft` 传错，拿一段带括号的 RTL 文本试一遍，镜像括号的方向错了是什么样子，您看一遍就明白方向参数为什么要一路传到底。
6. 把 `GetGlyphs` 的容量故意给小，看看返回的 `HRESULT` 报的是什么，按需扩缓冲再投的重试路数您就亲手走了一遍。
7. 在 Skia 的工程里查一查您的构建带没带 `SkShaper`，找到它配的后端是谁，HarfBuzz 还是平台自家的库，构建配置里一眼就有答案。

## 本篇的术语速查

这些词咱们后面的篇目还要接着用，咱们把它们集中列一份，您忘了的时候回来查：

| 术语 | 一句话的意思 |
|:--|:--|
| 整形（shaping） | 把码点序列换成正确 glyph 序列并定位的工序 |
| kerning | 相邻字形的字距微调，A 与 V 相邻时收拢空隙 |
| `kern` 表 | 老 TrueType 的字距查表，新引擎常默认不吃 |
| `GPOS` | OpenType 的字形定位表，字距与标记贴靠归它管 |
| 连字（ligature） | 相邻字符合成一个 glyph，fi 合成 ﬁ |
| `GSUB` | OpenType 的字形替换表，连字与上下文形式归它管 |
| `liga` / `rlig` | 可选连字与必需连字的特性开关，阿拉伯连写靠 rlig |
| 上下文形式 | 看邻居挑字形，阿拉伯字母的独立、首、中、尾四副形 |
| BiDi | 双向文本，RTL 段里嵌着 LTR 的数字与英文 |
| 逻辑顺序 / 显示顺序 | 存储打字的顺序与屏幕排列的顺序，BiDi 让两者分家 |
| UAX #9 | Unicode 双向算法的标准编号 |
| cluster | glyph 与源文本段落的对应关系，光标与选区靠它 |
| advance / offset | glyph 往前走的量与上下偏移的量 |
| HarfBuzz | 开源整形库，Chrome、Firefox、Qt、Android 都用它 |
| `IDWriteTextAnalyzer` | DirectWrite 整形的入口接口 |
| `SkShaper` | Skia 的整形模块，常配 HarfBuzz 后端 |

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 整形器收下的输入除了码点序列还有哪几样，缺了方向会出什么错？
2. kerning 改的是字形本身还是 advance，它跟字形自带的留白差在哪？
3. 老 `kern` 表与 `GPOS` 各是什么年代的方案，新引擎默认吃哪一份？
4. fi 连字走的是哪个特性开关，它跟阿拉伯文连写的 `rlig` 差在哪？
5. 阿拉伯字母的四副形各自叫什么，挑形的依据是什么？
6. 一款字体的 glyph 数为什么通常比字符数多，多出来的是什么？
7. 逻辑顺序与显示顺序说的各是什么，BiDi 让什么对不上了？
8. 段落的方向由什么定，圆括号在 RTL 语境里要过哪一道处理？
9. `hb_shape` 调用前后，buffer 里的东西各是什么？
10. cluster 映射记的是什么，fi 连字之后两个文本位置落在哪？
11. `IDWriteTextAnalyzer` 的分析步骤里，`AnalyzeScript` 切出来的 run 是按什么划界的？
12. `GetGlyphs` 与 `GetGlyphPlacements` 各交出什么，kerning 的修正在哪一步进去？
13. `maxGlyphCount` 为什么不能拿文字长度冒充，稳妥的投法是什么路数？
14. `SkShaper` 底下常用的引擎是谁，`kGlyphID` 的口子留给谁？

## 下一步

整形的活到这儿交割清楚了，按登记咱们的下一站是[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)，它把咱们今天亲手走过的分析、换算、定位全包进一个对象里，样式范围、字体回退与断行也归它一并地管，本篇的 `IDWriteTextAnalyzer` 只是它身后的一个车间。cluster 的映射与显示顺序，到了[《测量、命中测试与光标》](22_text_metrics.md)才真正派得上用场。咱们攒下的 glyph 序列已经有了去处，`kGlyphID` 的口子通着，`DWRITE_GLYPH_RUN` 的原料也齐了，接下来咱们该看的，是谁把它们排成一行行的版。
