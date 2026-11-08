---
title: Skia 的文本：SkFont 与 TextBlob
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# Skia 的文本：SkFont 与 TextBlob

按[篇目索引](index.md)的登记，本篇在 Skia 段里排的是第三位。前一篇[《SkPaint 深入：shader 与混合》](14_skia_paint_deep.md)已经跟您见过面了，参数袋 SkPaint 管的“怎么画”咱们都在那边认过了。更前面的[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)讲的是三件套的分工与来历，后一篇轮到了[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)，它管的是 Skia 在 GPU 一侧的进出。

前一篇咱们把 paint 的柜子翻到了底层，画形状的家当已经齐了，可界面的大头是字。画一串字比画一个矩形多出什么，正是本篇要拆的主线，咱们把多出来的东西摆在明处：“字体哪来、字号多大、抗锯齿几档”，清单的后半是“怎么量宽、怎么截断”。矩形要的只是四个数，字却要咱们把排版的零件一件件握在手里。[第一篇《从状态到像素》](01_status_to_pixels.md)里 `DrawTextW` 一个调用就包办的活，Skia 全还给了咱们。

Skia 的文本 API 是这几年迁移得最凶的地段，咱们把基准交代清楚。本篇以 2024 年前后的 Skia 主线为准：字体的参数住在 `SkFont` 的身上，画的入口是 `drawString` 与 `drawTextBlob`。您在老教程里会看到另一套长相，字体参数全挂在 `SkPaint` 的身上，`setTypeface` 跟 `setTextSize` 都是它的成员，画的入口叫 `drawText`。那是旧世界的东西，后文咱们专门给它留了一小节，您认得出脸就行。具体哪个版本把老接口删掉的，笔者没有去翻提交记录，咱们按新旧两套名字并给的路数讲。

## drawString：最短的一条路

咱们从最短的调用看起，别的参数暂且都不碰：

```cpp
canvas->drawString("你好，Skia", 20.0f, 60.0f, font, paint);
```

签名咱们逐个地认，它是字符串加起点的 x 与 y，再加一只管字的 `SkFont` 与一只管颜料的 `SkPaint`。内部其实就是 `SkFont` 加 `SkPaint` 的组合，分工跟前一篇的参数袋是接得上的：`SkFont` 说的是用哪只字体、多大的字号，`SkPaint` 说的是拿什么颜料。它的另一个重载收的是 `SkString`，咱们两个版本都躲不开 paint，没有省参数的版本。真正要当心的是那个 `y`，它给的是基线的位置。基线说的是字母站立的一条看不见的线，a、b、c 的脚都踩在它的上面，g、y 的尾巴垂到线的下面。您要是按顶部去理解它，画出来的字会沉下去半截，咱们到测量的一节把 ascent 与 descent 认完，您就知道沉掉的正是那半截。

咱们在[第三篇《坐标、DIP 与 DPI》](03_dip_and_dpi.md)里认过的 DPI 教训，在 Skia 的文本上同样适用。`SkFont` 的字号活在画布的坐标系里，画布要是按物理像素建的，高 DPI 的屏幕上 24 的字就显得小了。咱们要么把字号乘上缩放系数再去造 `font`，要么在画布上铺一层缩放的坐标变换，两条路咱们选一条就行。第二条路是本篇新开的口子，做法咱们留给实战篇，缩放系数的来路还是第三篇认过的那套换算。

咱们拿 GDI（Win32 自带的老图形接口）对照着看，差别一眼就出来了。[第一篇《从状态到像素》](01_status_to_pixels.md)里的 `DrawTextW` 收的是一块矩形加几个标志位，`DT_LEFT` 一类的标志位一排摆着，往矩形里怎么排、竖着怎么居中、放不下怎么补省略号，全由它替咱们料理。`drawString` 这边只给基线上的一个点，往哪儿排、行高多少、放不下怎么办，全是咱们自己的活。Skia 把排版的零件拆散了交到咱们手上，本篇剩下的篇幅，做的就是把这些零件一件件认回来的活。

## SkFont：字体、字号与抗锯齿的档位

`SkFont` 里最大的角色是 `SkTypeface`，咱们用白话讲：一个 `SkTypeface` 就是一款字体的“脸”，对应着系统里一份具体的字体数据，微软雅黑有它自己的脸，宋体的长相又是另一副。造它的工厂是 `SkTypeface::MakeFromName`，头一个参数填的是家族名，第二个填的是 `SkFontStyle`。`SkFontStyle` 把字重、宽窄、倾斜打包成了一组参数，常用的 `Normal` 与 `Bold` 都是现成的值。工厂还回来的包在 `sk_sp` 里，就是前一篇 shader、image 用过的带引用计数的智能指针，怎么持有怎么放，咱们用起来跟前一篇是同一套路数。

脸拿到了手，咱们就能配 `SkFont` 了：

```cpp
sk_sp<SkTypeface> face = SkTypeface::MakeFromName(
    "Microsoft YaHei", SkFontStyle::Normal());
SkFont font(std::move(face), 24.0f);
```

构造函数收的是脸加字号，两样一次就配齐了。顺带交代一处笔者拿不准的地方：`SkFont` 这边有没有 `MakeFromName` 一类的快捷工厂，笔者没有把握，咱们走稳当的路子，工厂造的是脸，构造函数配的是参数，两步咱们都摆在明面上，您翻到别的写法时对得上就好。字号咱们按画布坐标的像素一级理解，跟前一篇线宽的口径是一致的，`24.0f` 画出来就是 24 像素上下的个头。

裸构造的 `SkFont` 也是合法的，缺省的脸与字号由平台替咱们挑，正式代码里咱们还是显式给全。

字号还能单独地改，改字号的入口是 `setSize`，改一个数也就行了。这里有一处跟前一篇相反的脾气值得记下：`SkFont` 是可以原地改参数的活对象，而咱们前一篇认过的 `SkShader`、`SkImage` 都是创建之后就不许动的，一边是 paint 的参数袋，一边是一次性的成品，管理的脾气也就各是各的。

文字的抗锯齿在 `SkFont` 这边叫 `edging`，开的入口是 `setEdging`，咱们能挑的档位有三格。`kAlias` 是关掉抗锯齿的档，字形的边缘一格一格地硬切，小字号下咱们看得出锯齿。`kAntiAlias` 走的是灰度抗锯齿，边缘的像素拿灰阶做过渡，锯齿就化成了柔和的毛边。再进一档的是 `kSubpixelAntiAlias`，它把 LCD 屏幕里红绿蓝子像素的排列也用上，横向的精度比整像素更细，代价是字形边缘可能泛出淡淡的彩边。有一处新旧世界的分界咱们顺路认下：老世界里文字的抗锯齿跟 `paint` 的开关走，新世界里它归 `font` 的 `edging` 管，`paint` 的 `setAntiAlias` 管的是形状与线条的那一摊。

`setHinting` 咱们一句带过。hinting 说的是把字形往像素网格上贴的微调技巧，小字号下它换得来更清楚的笔画，代价是字形的样子被拉扯了。档位的枚举叫 `SkFontHinting`，从 `kNoHinting` 一路排到了 `kFull`，默认取哪一档笔者不去背书，您做小字号的界面真较真时再逐档对比着看。示例里咱们不动它。

## 字体从哪儿来：家族名、文件与内存

`MakeFromName` 挑的是系统里装好的字体，家族名的字符串就是您在字体面板里看到的名字。家族找不到的时候回退成谁，不同平台的字体管理器各有各的路数，笔者没有逐平台验证过，您要跨平台发布的话值得亲手确认一遍。

自带字体的路子咱们也常见：游戏、编辑器一类的应用常把字体文件打进安装包，用户的机器上装没装它都不受影响。Skia 给的入口是 `SkTypeface::MakeFromFile`，收的是文件路径，再带一个字体集合的索引，单独的 ttf 文件传 0 也行：

```cpp
sk_sp<SkTypeface> face = SkTypeface::MakeFromFile(
    "assets/fonts/MyFont-Regular.ttf");
```

字节流已经在内存里的场合，走的是 `SkTypeface::MakeFromData`，它收的是 `sk_sp<SkData>`，网络下载来的、从加密包里解出来的字体都从这儿进。这一对工厂的签名笔者同样没有跑过验证，您以手上 Skia 版本的官方头文件为准。`SkTypeface` 的工厂里还有 `MakeDefault` 一档，它还回来的是显式取来的平台缺省脸。裸构造的 `SkFont` 没有显式给脸，绘制的时候才回落到平台的缺省脸上，画出来的效果是一样的，您想显式拿缺省的脸时走 `MakeDefault`。不管咱们走哪个入口，造好的 typeface 都存成成员变量用到底就好，创建的代价笔者没量过，可同款字体建一次用到老的路子，总归是不会错的。

## glyph：Skia 吃的其实是字形编号

咱们停下来看一件事：`drawString` 收进去的是字符串，Skia 内部画的时候吃的是字形编号。真正被画出来的东西叫字形（glyph），也就是字体里存着的一个个图形。一个字符在文字的世界里只是个编号（编号怎么来的，咱们留到[《字符、码点与字形》](19_chars_and_glyphs.md)一篇再补），落到屏幕上的那一笔一画才是字形，每款字体给同一个字符配的字形也都不一样。所以字体内部有自家的一套编号，咱们叫它 glyph ID，绘制管线认的就是它。

咱们把字符编号换成字形编号，靠的是字体文件里自带的一张映射表，表的名字叫 `cmap`，您把它读成“字符码查字形号”的表就好。`drawString` 与后文的 `measureText` 收到字符串，内部就是拿 `typeface` 的 `cmap` 逐个查号，查出来的 glyph ID 序列才是管线的口粮。

一对一查表够用吗？中文这边大体是够用的，字与形基本是一对一的，查出来的序列照画就行。可文字系统的世界比这大：阿拉伯文的字母连写要看前后邻居换字形，天城文的元音符号要附着在辅音上，英文排印里的 fi 两个字母可以合成一个连字，相邻字母的间距还有 kerning 的微调。把这些做对的过程叫整形（shaping），Skia 的核心不管整形，它住在单独的 `SkShaper` 模块里，常用的实现走的是开源整形引擎 HarfBuzz，具体挑哪套后端要看您拿到的构建是怎么配的。从右往左的语言与从左往右的文字混排（BiDi）同样也是整形那边的活。按[篇目索引](index.md)的登记，本系列给整形留了[《整形：连字、kerning、BiDi》](20_shaping.md)专篇，咱们到那边再展开。本篇的示例全是中文，一对一的查表就够，咱们才敢走 `drawString` 的近路。

查表这件事还留着一个实际的后果：查表是逐字符的活，同一行的字画一遍就得查一遍。您想想滚动着的列表，每一帧都在重画同样的几十行字，查表的活也就一遍一遍地白做了。Skia 给的省法，就是把查好的结果打包存下来，这正是 `SkTextBlob` 登场的位置。

## SkTextBlob：把一段文字打包存好

造 blob 的工厂是 `SkTextBlob::MakeFromText`，咱们看一眼它的样子：

```cpp
sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(
    text, std::strlen(text), font, SkTextEncoding::kUTF8);
```

头两个参数收的是字符串加字节数，您当心数的是字节：咱们传的字符串是 UTF-8 编码的，一个汉字占的是 3 个字节，跟字符的个数不是一回事，数错单位是这里的常见失误。末位的 `SkTextEncoding` 说的是字符串按哪套编码读，编码的档位从 `kUTF8` 一路排到 `kUTF32`，另有 `kGlyphID` 的一格，它是直接喂字形编号的口子，整形器算出来的结果正好从这儿进 Skia，咱们到整形那一篇再见到它。

咱们写 Win32 的工程，手里的字符串多半是 wchar_t 的 UTF-16，[第一篇《从状态到像素》](01_status_to_pixels.md)里 `GetWindowTextW` 取回来的就是它。喂给 Skia 前其实不必转码，`SkTextEncoding::kUTF16` 就是给宽字符准备的口子，字节数按 wchar 元素的个数乘 2 算：

```cpp
std::wstring wide = L"这是用来演示测量与截断的一行中文";
SkScalar width = font.measureText(
    wide.data(), wide.size() * sizeof(wchar_t),
    SkTextEncoding::kUTF16);
```

两套编码混着用的话，您就得记牢每次传的是哪一套，字节数错了或者编码标错了，查出来的字形就是乱的，错了还不报错，非得画到屏幕上咱们才发现。咱们示例统一走的是 UTF-8 的窄字符串，为的就是路子单纯些。顺路一个差别您心里有数就好：GDI 的世界整个建在宽字符上，`DrawTextW` 吃的就是 UTF-16，Skia 的世界建在字节流上，编码是自报的，两边的口味不同，互转的事咱们接手时留个神。

工厂还回来的 `sk_sp<SkTextBlob>` 是不可变的对象：创建的那一刻，用的哪只 `font`、查出的字形、各自的位置，全都定死了，之后谁也改不动它的内容。blob 把创建时用的 `typeface` 一起拿住了，您不用担心 `font` 没了 blob 会悬空，引用链的事它自己会管。画的入口是 `canvas->drawTextBlob`，您给它换 `x` 与 `y`，同一份 blob 就画到了别处。样式仍然归 `paint` 管：文字的笔画在 Skia 眼里是一串带位置的掩模，`paint` 的颜色、混合档、shader 照常当颜料使，前一篇的渐变拿来做一行彩色过渡的字，是可以直接办到的。

咱们把建与画的两步摆在一起看：

```cpp
// Built once when the list item's text is set.
sk_sp<SkTextBlob> item = SkTextBlob::MakeFromText(
    kLabel, std::strlen(kLabel), font, SkTextEncoding::kUTF8);

// Called every frame with the current scroll offset.
canvas->drawTextBlob(item, x, baselineY, paint);
```

建 blob 与画 blob 的时机是错开的，注释里排的就是咱们滚动列表的用法：文案定下来的那一刻建一次，之后每一帧咱们只管挪坐标。

复用在滚动的场景里最见好处。长文本的滚动每帧挪一小段，文字的内容一个字都没变，变的只是坐标 `x`，咱们把 blob 建一次存起来，每帧咱们只调 `drawTextBlob`，查表与定位的活一次都不用重做。不可变还附送一条：同一份 blob 被多处共享是安全的，引用计数的事由 `sk_sp` 看着，跟您熟悉的智能指针是同一套用法。

另有一处讲究咱们补在末尾：`MakeFromText` 打包的是单一 `font` 的一段字，一行的文字里要是混着两种字体或字号，咱们就得请 `SkTextBlobBuilder` 来分段拼装，那是进阶的家当，本篇咱们不展开，您真做混排时再去认它。咱们把 Direct2D（D2D，Win32 的另一套图形 API）也放一边：那边画字走的是 `DirectWrite` 的排版对象，管它的是[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)的专篇，咱们本篇不碰。

## 测量：measureText 与 SkFontMetrics

量宽度是排版的头一件活，入口就在 `font` 的身上。咱们调用 `font.measureText(text, byteLength, SkTextEncoding::kUTF8)`，它还给咱们一个 `SkScalar`，说的是从起点画完这串字，笔往前走了多远，行话里叫的是 `advance width`。测量跟绘制走的是同一只 `font` 的同一套查表，量出来的宽跟画出来的宽是一致的，量宽的入口摆在 `font` 身上，道理说的就是它。它还有一个带 `SkPaint` 的重载，字带描边一类的效果时，咱们量宽时得把效果算进去才准，示例的效果是素色的，三参的简化版就够用。

`measureText` 还有一个能顺手填包围盒的重载，咱们传一个 `SkRect*` 进去，画出来的字实际占了哪块地，包围盒它顺手就填好了，背景框与命中测试用的都是它。

光有宽度还排不了行，竖向的零件在 `SkFontMetrics` 里。咱们把 `font.getMetrics(&fm)` 一调用，结构体里就填好了几样数：`fAscent` 记的是基线以上的高度，数是负的，负号指着往上的方向，`fDescent` 记的是基线以下垂下去的深度，数是正的，`fLeading` 是两行之间建议再垫的空隙，名字来自铅字排版时代垫在行间的那根铅条。一行字的行高，常用的算法就是 descent 减 ascent 再加 leading，下一行的基线就在上一行基线往下挪这么多个像素，咱们的示例就按它摆第二行。

leading 加不加倒是个口味的题目：工具栏一类的紧凑界面，咱们常取 descent 减 ascent 就收，正文阅读的排版才把 leading 垫上。Skia 不替咱们定口味，它只是把三个数给全了。

结构体里的另一对是 `fTop` 与 `fBottom`，它们记的是整款字体字形的实际最高与最低，跟 ascent、descent 的版式口径不完全是一回事，咱们示例用不到，您做紧凑排版时会遇到它们。

GDI 那边的这套零件同样一件不缺，`GetTextMetrics` 还回来的 `tmAscent`、`tmDescent`、`tmExternalLeading` 一一对得上号，差别在谁替咱们干活。[第一篇《从状态到像素》](01_status_to_pixels.md)的 `DrawTextW` 拿矩形加标志位把排版的活全包了，`DT_VCENTER` 一个标志就是咱们刚才做的那道基线加法，`DT_END_ELLIPSIS` 一个标志就是咱们接下来要手工做的截断。Skia 把这些活全还给了咱们，多写的不过是几行加法与截断，换到手的是行高压紧还是放宽都由咱们自己定。

## drawText：旧世界的退役接口

老接口咱们认一眼就收。旧世界的入口叫 `canvas->drawText(text, byteLength, x, y, paint)`，字体的全部参数住在 `paint` 的身上，`setTypeface`、`setTextSize`、`setTextScaleX` 一家子都是那个年代的写法。2018 年前后 Skia 引入了 `SkFont`，随后把文本参数从 paint 身上搬了过去，`drawText` 标了废弃，其后几年从主线上删掉了，接班的正是咱们本篇的 `drawString` 与 `drawTextBlob`。您翻搜来的老代码与老教程，满篇都是 `paint.setTypeface` 的长相，认得出它是旧世界的东西就好，新代码里咱们别再跟着写。前一篇里造渐变的工厂从 paint 身上挪了出去，咱们文本这边挪得更远，连画字的入口都换成了 `drawString` 与 `drawTextBlob`。

## 示例：画一行中文，量宽，超了就截断

咱们把家当拼成一段完整的骨架。画布的来路沿用[前一篇](14_skia_paint_deep.md)垫过的最小骨架：

```cpp
sk_sp<SkSurface> surface = SkSurfaces::Raster(
    SkImageInfo::MakeN32Premul(360, 240));
SkCanvas* canvas = surface->getCanvas();
DrawTextScene(canvas);
```

本节的代码是笔者按 2024 年前后主线的接口签名写的骨架，没有经过编译的验证，您接进项目时以编译器的报错为准。示例用的画布是普通的光栅画布，行为的依据以 CPU 一侧的实现为准。

咱们从字体与竖向的摆位开始：

```cpp
void DrawTextScene(SkCanvas* canvas) {
    canvas->clear(SK_ColorDKGRAY);

    // Pick an actual typeface instead of the platform default.
    SkFont font(SkTypeface::MakeFromName(
                    "Microsoft YaHei", SkFontStyle::Normal()),
                24.0f);
    font.setEdging(SkFont::kAntiAlias);
```

字体的四行咱们前面都认过了，摆进函数里就是一副标准的长相。接着是文案常量与竖向的摆位：

```cpp
    const char* text = "这是用来演示测量与截断的一行中文";
    const SkScalar kLeft = 20.0f;
    const SkScalar kAvail = 360.0f - 2.0f * kLeft;

    // Vertical placement: put the line box top at y = 40.
    // fAscent is negative, so the baseline lands below the top.
    SkFontMetrics fm;
    font.getMetrics(&fm);
    SkScalar baseline = 40.0f - fm.fAscent;
```

前半段咱们扫一眼就过：脸配的是微软雅黑的家族名，字号定的是 24，edging 挑的是灰度抗锯齿的档。基线的那一行值得咱们停一下：咱们想让行的顶边落在 40 上，`fAscent` 记的是负数，40 减去了它，基线就落到了顶边往下 ascent 那么深的位置，字往上伸的部分正好顶在 40。您刚才担心的“y 是不是顶部”，到这里就换成了一笔明确的加法。

接着咱们量宽、截断、落笔：

```cpp
    SkScalar width = font.measureText(
        text, std::strlen(text), SkTextEncoding::kUTF8);

    std::string line(text);
    if (width > kAvail) {
        SkScalar ellW = font.measureText("…", 3, SkTextEncoding::kUTF8);
        while (line.size() > 3 &&
               font.measureText(line.data(), line.size(),
                                SkTextEncoding::kUTF8) > kAvail - ellW) {
            line.resize(line.size() - 3);  // one CJK char = 3 bytes
        }
        line += "…";
    }

    SkPaint ink;
    ink.setColor(SK_ColorWHITE);
    canvas->drawString(line.c_str(), kLeft, baseline, font, ink);
```

截断的活咱们自己干，没有现成的标志位可以指望。思路跟 `DrawTextW` 的 `DT_END_ELLIPSIS` 是同一件事的两种做法：量出省略号的宽，咱们从尾巴上每次砍一个字，砍到塞得下了为止，再把省略号接了上去。本例的可用宽度放得下一个字加省略号，塞得下的前提在例子的数值下成立。真遇上极窄的场合，循环的下限是给咱们剩一个字，您要是连一个字加省略号都放不下，它就带着仍超宽的结果退出来，这个保底咱们心里有数就好。一次砍 3 个字节是咱们的简化，它成立的前提是内容纯中文、每字恰好 3 字节，混进英文或 emoji 就不成立了，通用的写法得按 UTF-8 的首字节认出每个字符的边界再砍，咱们把通用版留进动手清单。

接尾的省略号咱们用的是 U+2026 的“…”。它在 UTF-8 里占的恰好是 3 个字节，宽度跟一个汉字是一样的，中文排版的标准省略号就是它，拿三个 ASCII 句点凑的做法不算数。`measureText` 里传的清一色是字节数，`std::strlen` 数出来的也是字节，单位上正好是咬合的。

末段咱们把 blob 用上，再画一条给您核对测量用的横线：

```cpp
    // Ruler line: its length IS the measured advance width.
    SkScalar drawnW = font.measureText(
        line.data(), line.size(), SkTextEncoding::kUTF8);
    SkPaint ruler;
    ruler.setColor(0xFFE0B050);
    ruler.setStyle(SkPaint::kStroke_Style);
    ruler.setStrokeWidth(1.0f);
    canvas->drawLine(kLeft, baseline + fm.fDescent + 4.0f,
                     kLeft + drawnW, baseline + fm.fDescent + 4.0f, ruler);

    // Package the final line once; a scrolling scene would keep
    // this blob and only move x/y each frame.
    sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(
        line.data(), line.size(), font, SkTextEncoding::kUTF8);
    SkScalar rowHeight = fm.fDescent - fm.fAscent + fm.fLeading;
    SkPaint blobInk;
    blobInk.setColor(0xFF9FC2FF);
    canvas->drawTextBlob(blob, kLeft, baseline + rowHeight, blobInk);
}
```

咱们把骨架的预期画面交代清楚：深灰的底上画着两行字。第一行是白色的中文，文案超出了可用宽度，被截断后尾巴上接了省略号，字底下横着一条等长的橙色核对线，第二行则是同一段文案的浅蓝色复画。这些画面是咱们按代码推出来的预期，真跑出来的截图笔者手里没有，您接进真画布时对一眼就知道推没推对。

第二行画的就是截好的那句：把它打包成 `blob`，`drawTextBlob` 把它画到按行高挪下来的第二条基线上。真到了滚动的列表里，blob 是建一次存着的，每帧换的只是坐标，示例里咱们建完当场画一次，路数走的是同一条。横在字底下的橙色横线是给您核对用的：它的长度就是 `measureText` 报回来的宽，跟上一行字的右边缘对得齐。

## 动手清单

动手的活一共有八处，每一处咱们改的都不出几行，原料都是示例里现成的代码：

1. `setEdging` 的三档值得轮流换一遍。24 号之下差别含蓄，您把字号降到 12 再比，`kAlias` 的锯齿与 `kSubpixelAntiAlias` 的细滑就分出来了。
2. 咱们把家族名换成别的字体，再故意写一个系统里没有的名字，看看缺字体时平台给咱们顶上的是什么长相。
3. 给 paint 装上前一篇的渐变 shader 再画 `drawTextBlob`，一行字的笔画吃到了渐变，“掩模当颜料容器”的说法就有了画面。
4. 截断循环的 3 字节步长可以改成按 UTF-8 首字节认边界：首字节落在 0xE0 到 0xEF 之间的是 3 字节字，0xC0 到 0xDF 是 2 字节，0xF0 起头的是 4 字节，混排的字符串砍起来才不会砍出半个字。
5. 把示例的文案换成一串英文长句再跑截断，3 字节的步长会当场砍出半个字，乱码就是简化付出的代价，您亲眼看一遍才记得牢。
6. blob 值得搬到绘制循环的外面建一次，每帧只调 `drawTextBlob` 挪它的坐标，省下的就是反复查表的功夫。您拿它做一行来回滚动的字幕，复用的路数就走通了。
7. 咱们按 `fTop` 与 `fBottom` 的值画两条辅助线出来，跟 ascent 与 descent 的位置比一比，一款字体的版式边界与字形实际边界差了多少，摆在画布上就清楚了。
8. `kUTF16` 的宽字符版本也接进示例试一遍，wchar 的文案量宽与画字各走一遍，两套编码量出来的宽度是一样的，那就说明接对了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `drawString` 的 `y` 参数指的是哪条线的位置，按顶部理解会看到什么？
2. `kAntiAlias` 与 `kSubpixelAntiAlias` 的差别落在哪个方向上，代价是什么？
3. `SkTypeface::MakeFromName` 的两个参数各说了什么？
4. 字符串进了 Skia 之后，经哪张表换成了什么编号？
5. 整形（shaping）替咱们处理哪一类的事，Skia 里管它的是谁，底下用的是什么引擎？
6. `MakeFromText` 的 `byteLength` 数的是什么单位，`kGlyphID` 是留给谁的口子？
7. `SkTextBlob` 的不可变，替共享与复用省了什么心？
8. 一行字的行高，用 `fAscent`、`fDescent`、`fLeading` 怎么拼出来？
9. 旧接口 `drawText` 的字体参数住在谁的身上，接班的是哪两个入口？
10. `DT_END_ELLIPSIS` 在本篇示例里对应的是哪一段手工活？
11. `SkTypeface::MakeFromFile` 与 `MakeFromData` 各自适合什么场合？
12. Win32 工程里 wchar_t 的字符串，走哪个编码值喂给 `measureText` 才对路？
13. `SkTextBlob` 与创建时用的 typeface，谁把谁拿住了？

## 下一步

文本的家当咱们认到了 blob 的这一层，Skia 那边咱们还欠着两块。下一篇[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)会把咱们领到硬件加速的门口，本篇的画布一直待在 CPU 一侧，`font` 与 `blob` 到了 GPU 那边照样认得。文本在后头还有单独的一段，咱们把它交给[《字符、码点与字形》](19_chars_and_glyphs.md)与[《整形：连字、kerning、BiDi》](20_shaping.md)两篇，glyph 一节里欠下的展开，字符编号的来历、整形器怎么把字符序列变成正确的字形序列，到那边咱们一寸一寸补上。再往后的[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)里，咱们这副量宽截断的骨架就有了真正的窗口可以住。
