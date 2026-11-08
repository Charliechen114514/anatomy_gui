---
title: SkPaint 深入：shader 与混合
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# SkPaint 深入：shader 与混合

本篇的邻居，前一篇讲的是[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)，后一篇是[《Skia 的文本：SkFont 与 TextBlob》](15_skia_text.md)。前一篇管的是三件套的脸熟功夫：SkSurface 扮的是表面的角色，也就是您最终看到的画面，SkCanvas 是画笔的手，SkPaint 是装样式参数的参数袋。setColor 填颜色、抗锯齿的开关、描边与填充的切换，按排法您都在那一篇里读。本篇咱们把参数袋倒出来看深一层：颜色底下垫着的 shader 体系，源色与画面的混合方式，再带上描边的几处细节与两个滤镜的名字。

您手里的进度咱们也交代一下：本篇默认您读过[画刷篇](07_d2d_brushes.md)与[几何篇](08_d2d_geometry.md)，画刷、渐变的 stop、位图画刷、描边样式这些词，咱们直接拿来用，不再从头解释了。D2D 没讲过的概念，本篇在头一次出现的地方配白话。

Skia 的 API 这几年迁移得勤，咱们得把基准说清楚。本篇以 2024 年前后的 Skia 主线为准，渐变与图像的工厂函数住在 `SkGradientShader`、`SkImageShader` 这些工厂类身上，静态工厂方法返回的都是 `sk_sp` 智能指针。您在搜来的老代码里会看见另一套长相，比如图像 shader 走 `SkImage` 身上的 `makeShader` 入口、手工 `unref` 的裸指针，它们是同一个体系的前世。咱们按新名字讲。

## 颜色只是最简的 shader

咱们从 setColor 说起。`SkColor` 是把 ARGB 四个 8 位分量打包成的 32 位整数，`SK_ColorWHITE` 一类的常量就是它的预定义值，咱们用起来跟 GDI 的 `COLORREF` 是一个手感。讲究一点的写法是 `setColor4f`，它收的是四个浮点分量，还能捎带一个色彩空间的参数，语义跟咱们在[画刷篇](07_d2d_brushes.md)认过的 `D2D1_COLOR_F` 是对得上的。

咱们顺路把 paint 的出厂设置认一遍：默认的样式是填充，抗锯齿默认是关着的，混合档（也就是后文 `setBlendMode` 设的那个 `SkBlendMode` 参数）默认是 kSrcOver，shader 的位子默认是空。咱们示例里反复出现的 setAntiAlias(true)，补的就是第二项的缺省。

paint 身上还住着咱们没碰过的 `setShader`。您给它装一个 `SkShader`，绘制的源色就改由 shader 来算，paint 自己的颜色让了位。您把 shader 设回 nullptr，颜色才重新回到了岗位上。咱们用白话讲，shader 回答的问题是“这笔绘制的颜料从哪儿来”，纯色是它最简的答案，答案交给了 setColor 去代填。

判断眼前这笔用的是哪个，咱们有个直白的法子：看 shader 的位子是不是空。空的走颜色，非空的走 shader，没有第三种的状态。

咱们拿 D2D 对着看，结构上的差别就出来了。画刷篇里纯色、渐变、位图是三个各自独立的 COM 接口，创建的入口也各是各的。Skia 把它们统一成了一副骨架：shader 管源色，paint 管剩下的所有样式参数，纯色只是 shader 为空时的缺省行为。D2D 是一把把分开的刷子，Skia 则把一切都装进了参数袋。

有一处笔者不咬死的地方，咱们交代在明面上：shader 在场时，paint 颜色里的 alpha 还参不参与合成，官方文档的措辞咱们没有逐字抠过。动手清单的第一处就留给它，您改一个数就能亲眼看答案。

shader 的家族还能互相组合：`SkShaders::Blend` 收的是一个混合档加两个 shader，把两者的输出按混合档合成一个新的 shader，返回的还是 `sk_sp<SkShader>`。比如咱们把渐变乘上图像，出来的就是带着纹理的渐变，组合的原料都是咱们本篇认过的成员。

## sk_sp：shader 的生命周期

进门的新家当长的都是同一副模样：`sk_sp<SkShader>`。咱们把它当带引用计数的智能指针用就好，用法跟咱们熟悉的 `std::shared_ptr` 差不多，实现是 Skia 自家的一套。

生命周期的问题很实际：咱们把局部的 shader 传给 `setShader` 之后，函数返回、局部变量析构了，画笔还认不认它？认的。`setShader` 收走的是 sk_sp 的一份拷贝，引用计数跟着涨了一格，paint 手里的拷贝把对象稳稳拿住，局部变量的析构只是把计数降了一格而已。后文示例里的 `std::move` 只是把拷贝变成了移交，省掉一次计数的涨落，语义上是没有差别的。

反方向的误区也是真会遇到的：老代码里拿裸指针管理 shader、手工 unref 的写法您一定见得到，那是旧世界的过法。咱们的新代码一律走 sk_sp，引用计数的管理交给指针自己，出错的面就小了一大截。

咱们再补一句它的脾气：SkShader 自己也是不可变的对象，创建之后它的参数就不再变了，想要挪渐变的端点，咱们就得造一个新的。示例里每帧重造的写法省事是省事，创建的代价笔者没有量过数，您要是嫌它勤快，把不动的 shader 搬到循环的外面建一次也就好了。

## 线性渐变：MakeLinear

颜色从单色升级成按位置排好的一串，shader 就有了正经的用武之地。线性渐变的工厂函数是 `SkGradientShader::MakeLinear`，咱们看一段骨架：

```cpp
SkPoint pts[2] = { {0.0f, 0.0f}, {0.0f, 200.0f} };
SkColor colors[2] = { 0xFF3E5C9A, 0xFF1B2A66 };
sk_sp<SkShader> shader = SkGradientShader::MakeLinear(
    pts, colors, nullptr, 2, SkTileMode::kClamp);
paint.setShader(std::move(shader));
```

参数咱们逐个认。`pts` 是渐变轴的两个端点，对位 D2D 里的 startPoint 与 endPoint。`colors` 是颜色的数组，`count` 是它的个数。中间的 `pos` 咱们传了 nullptr，意思就是颜色均匀地分布，您要是给了数组，就是在逐个登记每个颜色的停靠位置，对位的是画刷篇里 stop 的 position 字段。末位的 `SkTileMode` 管的是渐变区间之外的地方怎么办，咱们下面单说。

`SkTileMode` 里的几个值咱们一起认：kClamp 的路数是拿端点的颜色垫住外侧，kRepeat 把整段渐变当壁纸反复地铺，kMirror 的折法是镜像着折回去，接缝处不会有颜色的跳变，kDecal 的脾气是区间外干脆透明。前三者跟画刷篇的 `D2D1_EXTEND_MODE` 对上了号，CLAMP、WRAP、MIRROR 一一对上了，kDecal 在 D2D 那边没有直接的对应，算是 Skia 多给的一档。

渐变轴也有 D2D 同款的误会，咱们把画刷篇的教训原样搬过来：轴活在画布的坐标系里，它不会自动跟着被填充的形状走。您要是指望轴自己相对于矩形从顶边渐到底边，画出来的会是一条窄带，带子以下全靠 kClamp 的端点色填满。解法也是同款的：绘制的时候拿形状当前的位置重新造 shader，或者按官方支持的做法用 localMatrix（shader 自带的坐标变换矩阵，能把渐变轴摆进形状自己的坐标系）把轴挪过去。本篇的示例选的是前一种，咱们图的就是直白。

`MakeLinear` 还有一个收 `SkColor4f` 数组的重载，多出来的是 `SkColorSpace` 的色彩管理，签名比整数版长了一截，语义却是同构的。咱们示例里用的是整数版，您讲究颜色精确过渡的时候再换它。

## 径向渐变：同构的表亲

径向渐变的工厂函数是 `SkGradientShader::MakeRadial`，您看它的参数，收的还是颜色数组、pos、count、tileMode 那几样，跟线性款是一样的。换掉的只是几何，头两个参数变成了圆心坐标与半径，颜色是从中心向圆周过渡的。画光晕与球面高光的场合，用的都是它，代码咱们不再展开。

同门还有一位锥形的表亲，老代码里的名字是 `SkGradientShader::MakeTwoPointConical`，描述里给的是两个圆心加两个半径，颜色是在两个圆之间过渡的，瓶底光环一类的效果靠它。笔者拿不准它在最新主线上的准确签名，新版里它的去处有迁移，咱们不展开，您在老代码里认得出脸就好。

## 图像当颜料：SkImageShader

您把图像直接当颜料使也行，搬的是像素本身，对位的正是画刷篇的位图画刷。Skia 这边的工厂函数是 `SkImageShader::Make`，咱们拿一个 8×8 的棋盘格当例子，素材的思路沿用画刷篇那段：

```cpp
SkBitmap bmp;
bmp.tryAllocPixels(SkImageInfo::Make(
    8, 8, kN32_SkColorType, kOpaque_SkAlphaType));
for (int y = 0; y < 8; ++y) {
    uint32_t* row = bmp.getAddr32(0, y);
    for (int x = 0; x < 8; ++x) {
        // same checkerboard as the D2D chapter: 4x4 blocks
        row[x] = (((x / 4) ^ (y / 4)) & 1) ? 0xFFDDDDDD : 0xFF666666;
    }
}
sk_sp<SkImage> img = SkImage::MakeFromBitmap(bmp);
```

`kN32_SkColorType` 说的是每像素 32 位的本机字节序格式，在 Windows 上就是 BGRA 的那一套，咱们填的 0xAARRGGBB 数值落进内存正好对得上。接着咱们把图像变成 shader：

```cpp
SkPaint tilePaint;
tilePaint.setShader(SkImageShader::Make(
    img, SkTileMode::kRepeat, SkTileMode::kRepeat,
    SkSamplingOptions(SkFilterMode::kNearest), nullptr));
canvas->drawRect(SkRect::MakeXYWH(20, 20, 300, 200), tilePaint);
```

SkImage 是不可变的对象，创建之后像素就不再变了，想换内容的话就得换一个新对象。它的好处跟不可变是一对的：同一份图像可以被多个 shader 安心地共享，还能常驻在 GPU 的那一边，咱们到 GPU 后端的那一篇再细说。

两个 tileMode 参数各管各的方向，对位的是位图画刷的 extendModeX 与 extendModeY，您在渐变那边已经认过平铺、镜像、垫边缘的脾气，这边跟那边是同源的。

采样是另一个参数管的事。`SkSamplingOptions` 管图像被缩放时的采样方式，对位位图画刷的 interpolationMode。`SkFilterMode::kNearest` 取最近的像素，放大后是锐利的色块。kLinear 换了路数，在邻近的像素之间做插值，放大后是柔和的过渡。棋盘格咱们照旧选 NEAREST，格子的边缘才是干脆的。

末位的 nullptr 是 localMatrix，图像怎么映射到画布的事由它说，咱们不动它就是一对一。

两个 tileMode 还能各配各的：您横向给 kMirror、纵向给 kRepeat，也是合法的搭配，镜像消掉了横向的接缝，纵向照旧地平铺。D2D 那边的 extendModeX 与 extendModeY 也是各填各的，自由度上两边打成了平手。

## SkBlendMode：源与目标怎么合

到目前为止咱们改的都是“画什么颜色”，还有一件事没碰过：画上去的颜色跟画面上已有的颜色怎么算。每一笔绘制都有源与目标的两方，源是这笔绘制想画上去的颜色，目标是画布上已经躺着的颜色，混合发生的地点就在这两方之间。

Skia 把混合方式做成了 paint 的一个参数：`setBlendMode`。默认的取值是 `SkBlendMode::kSrcOver`，源按自己的 alpha 盖在目标上面，alpha 不到 1 的地方目标透出来一部分。咱们在 D2D 里画了这么多篇，靠的就是同一条规则，只不过 D2D 没把它摆在参数表上让咱们挑。设备上下文 `ID2D1DeviceContext` 的身上有一个 `SetCompositeMode`，干的是同类的事，咱们本篇不展开它。

kSrcOver 的算法用白话讲：源的不透明度是 0.5 的时候，结果里源的颜色占一半，目标的颜色透上来一半，0.9 的时候源占九成。画刷篇讲过的 opacity 乘法链在这儿换了住址，乘法的本质咱们已经认过了，咱们不重复展开。

基础档的几个值用白话讲。kClear 您可以当成橡皮，它把结果整个清成了全透明，这笔扫过的区域等于被擦掉。kSrc 的眼里只有源，目标直接让了位。kDst 的眼里只有目标，这笔绘制等于白白画了一场。它们仨是合成运算里的老祖宗，Porter-Duff 一类说的就是这类按 alpha 加权的组合，名字您混个脸熟就好。kSrcOver 本身也是这个家族的成员。

家族里还有几位成员咱们顺路认个脸：kDstOver 的次序倒了过来，目标反过来盖在源的上面，kSrcIn 留下的是源与目标相交的那一块，kXor 把相交的部分挖掉。它们在 D2D 的 COMPOSITE_MODE 枚举里各有一位亲戚，对号的活儿留给您将来真用的时候再做。

混合档玩的是颜色数值的运算。kMultiply 做的是逐像素相乘，源的颜色是白的地方目标原样透出，源的颜色越暗就压得越黑。kPlus 换成了逐像素相加，叠出来的颜色越来越亮，光晕叠加用的就是它。咱们到示例里拿一个圆把乘法演示出来，白与灰的差别一眼见分晓。

blend 与 alpha 是两件各管各的事，咱们分开讲：alpha 管的是源自己有多透明，blend 管的是源与目标怎么结合，两件事在合成公式里各占各的位置。同一个半透明的源，kSrcOver 之下透出的是干净的底色，kMultiply 之下掺进来的却是暗化过的底色，透明度是没有变的，算法换了结果就跟着换了。

加法还有一条安分的限制：结果封顶在 1，加满了才停在纯白，不会回绕到黑的区域。咱们把两只亮圆叠在一起，您看交叠处，比单只的更亮，示例里那两笔半透明的圆远没到顶，真一路叠满的话，停下的地方才是纯白。

混合的设定挂在 paint 的身上，它意味着填充与描边共用的是同一个设定，您不需要像别的体系那样分两处配置。反过来提醒一句：忘了自己设过混合档、后面每一笔都带着它走，是新手常见的意外来源，咱们心里得记着它。把它设回 `SkBlendMode::kSrcOver` 就算是归了位。

基础档其实也不是玩具，kClear 在实际界面里的活儿是打洞：咱们在一个不透明的圆角矩形上，拿 kClear 的 paint 画一个小圆，圆内的像素被直接清成透明。洞里能不能看见画布底下的东西，得看您给画布带没带 alpha 通道，还得看宿主有没有拿它去跟窗口的内容合成。您把条件凑齐了，透出来的才是窗口的画面，条件不齐的话，看到的就是一片黑。D2D 那边做同样的事也有路子，走的是图层蒙版一类的办法，Skia 这边一笔就够用了。

## 描边的讲究：宽度、cap 与 join

描边与填充的切换按索引的登记划在前一篇，咱们往细节里走。线宽的入口是 `setStrokeWidth`，单位咱们按画布的坐标理解，也就是像素一级的量。Skia 画布自身没有内建 DIP 的概念，[第三篇](03_dip_and_dpi.md)讲过的那套 DPI 换算，在 Skia 这边得咱们自己在外围料理，坐标系怎么摆是宿主的事。

线宽还有一个容易忽略的脾气：描边的中心线贴着路径走，线宽向路径的两侧各分一半。咱们拿 10 的线宽去描一个 100×100 的矩形，外沿胀到了 105，内沿压到了 95，描出来的边框比几何本身大了一圈。这一课[几何篇](08_d2d_geometry.md)倒是真没正面讲过，咱们在这儿补上对照：D2D 的描边也是同款的行为，中心线也是贴着路径走的，两侧各分一半的路数两边是一样的。

线段两端与转角的样子，对位的正是您在[几何篇](08_d2d_geometry.md)认过的 StrokeStyle。cap 是线段两端收边的形状：`kButt_Cap` 是齐头切断的，`kRound_Cap` 是半圆的头，`kSquare_Cap` 是方形地往外扩一截，外扩的长度是线宽的一半。对位 D2D 的 FLAT、ROUND、SQUARE，一一对得上了。join 是两条线段相接处的形状：`kMiter_Join` 是尖的角，`kBevel_Join` 是斜切的角，`kRound_Join` 是圆的角，尖角太尖的时候会扎出去多远，它的分寸由 `setStrokeMiter` 把着，几何篇示例里给的 10.0f 常用值在这里照搬就好。

cap 与 join 各有出场的时机。cap 只在线段的开头与收尾起作用，路径闭上了，拐角处站着的就是 join。咱们描一个闭合的实线矩形，四条边上的端点一个也找不着，看到的转角全是 join 的作品，cap 在闭合的实线矩形上就使不上劲了。虚线在场时情况就变了：dash 把路径切成一段一段的开放子路径，每段的两端都算端点，cap 也就照常生效了。您回头看示例里圆角虚线框的那笔圆头，靠的就是这个机制。

结构上的对照值得咱们多看一眼。D2D 把这些参数收进一个工厂造的 `ID2D1StrokeStyle` 对象，它是设备无关的、建一次用到底。Skia 没有单独的样式对象，全都摆在 paint 的身上，在同一只参数袋里改参数就行了。代价与收益装在同一段话里：少了一层对象管理，多了份小心，同一只 paint 改完了描边参数再拿去画填充，您得记得把状态改回来。

另有一处便利咱们记下：`SkPaint::Style` 里除了 kFill 与 kStroke，还住着第三位的 `kStrokeAndFill`，一笔就同时干了填充与描边两件事。D2D 那边没有单调用直接对应的写法，得靠 fill 与 draw 两笔分开地走，算 Skia 给的便利。

## 虚线与路径效果

虚线咱们一句带过，按开篇排好的路线走。工厂函数的名字是 `SkDashPathEffect::Make`，收的是实段空段交替的数组、段数与相位：

```cpp
const SkScalar dashes[2] = { 6.0f, 4.0f };
paint.setPathEffect(
    SkDashPathEffect::Make(dashes, 2, 0.0f));
```

数组里的一个数记的是实段长、另一个数记的是空段长，它们交替着循环到路径的末尾，对位几何篇的 dashes 数组。末位的 phase 说的是排样从哪一步动笔，您挪一挪它，虚线的断口就沿着轮廓滑动，蚂蚁线动画靠的就是它。段数的要求是偶数，奇数份的数组它一概不接，这是函数给自己立的门槛。

path effect 其实是一个更大的家族，虚线只是其中的成员之一，别的路径效果咱们本篇不展开。家族里另一位常用的成员是 `SkCornerPathEffect::Make`，它拿指定的半径把路径的尖角抹圆，折线一类的线条想要柔和的拐角，一支它就办了。path effect 与描边参数是并排的两层，一个改的是路径的形状，一个改的是笔的粗细，两样同时在场也是各干各的。装 path effect 的入口是 `setPathEffect`，工厂 `SkDashPathEffect::Make` 返回的同样是 `sk_sp`，引用计数的事由智能指针管着，咱们不用手工 unref。

## 两个滤镜的柜子

参数袋的底层还有两个格子，咱们点到为止。颜色滤镜的入口是 `setColorFilter`，收的是 `SkColorFilter`，干的是对源色做整体变换的活，比如 `SkColorFilters::Blend` 拿一个颜色按指定的混合档把整笔绘制染一遍。遮罩滤镜的入口是 `setMaskFilter`，它的代表是 `SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle, sigma)`，它把描边与填充的边缘糊开一圈，sigma 的数值越大糊得越宽，阴影外围那圈软边靠的就是它。

它们在 D2D 那边的亲戚是 Effect 图，咱们在[《特效：Effect 图》](11_d2d_effects.md)里已经认过它。您眼下的收获是知道柜子里存了什么：真要做描边发光一类的效果时，您就知道往哪个格子里翻了。

blend 管的是源与目标怎么合成，滤镜管的是源自己的颜色变换，一个管对外的事，一个管对内的事。您拿一个染蓝的 ColorFilter 配上 kSrcOver，画出来的就是被染过的形状，两层各干各的活，谁也不抢谁的。

## 示例：渐变底、乘法圆与虚线框

咱们把新家当拼成一个完整的小段。画布的来路咱们留给后面的[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)，它排在 Skia 段的末尾，您在本篇还读不到它，这里就垫一段最小的取画布骨架，等您读到那边，再把它换成正规的渲染循环：

```cpp
sk_sp<SkSurface> surface = SkSurfaces::Raster(
    SkImageInfo::MakeN32Premul(360, 240));
SkCanvas* canvas = surface->getCanvas();
DrawScene(canvas);
```

DrawScene 就是下面的函数。本节的代码是笔者按 2024 年前后主线的接口签名写的骨架，没有经过编译的验证，您接进去时以编译器的报错为准。还有一句要交代的：本篇的示例没有碰 GPU 后端，画布就是普通的光栅画布，行为的依据以 CPU 一侧的实现为准。

```cpp
void DrawScene(SkCanvas* canvas) {
    canvas->clear(SK_ColorDKGRAY);

    // Vertical gradient backdrop. The axis lives in canvas
    // coordinates - it does NOT follow the filled shape.
    SkPoint pts[2] = { {0.0f, 0.0f}, {0.0f, 240.0f} };
    SkColor cols[3] = { 0xFF2B3A67, 0xFF3E5C9A, 0xFF22304F };
    SkPaint bg;
    bg.setShader(SkGradientShader::MakeLinear(
        pts, cols, nullptr, 3, SkTileMode::kClamp));
    canvas->drawRect(SkRect::MakeWH(360, 240), bg);
```

渐变铺底的部分咱们扫一眼就知道没意外：三个颜色均匀地分布，轴是竖直的，kClamp 守住 240 以下的地界。接着登场的是乘法圆，也就是本篇的主角之一：

```cpp
    // Multiply circle: a mid-gray source halves whatever
    // is underneath. White would be a no-op here (1 * x = x).
    SkPaint mul;
    mul.setAntiAlias(true);
    mul.setColor(0xFF808080);
    mul.setBlendMode(SkBlendMode::kMultiply);
    canvas->drawCircle(180, 120, 70, mul);
```

注释里那句就是乘法的机关：源的取值是白的话就等于没画，源的取值是中灰，底下的颜色被乘剩一半，圆里那块渐变看起来像蒙了层阴影。您把颜色换成黑，圆里就成了一块纯黑。最后是虚线描边的圆角框：

```cpp
    SkPaint frame;
    frame.setAntiAlias(true);
    frame.setStyle(SkPaint::kStroke_Style);
    frame.setStrokeWidth(2.0f);
    frame.setStrokeCap(SkPaint::kRound_Cap);
    frame.setStrokeJoin(SkPaint::kRound_Join);
    const SkScalar dashes[2] = { 6.0f, 4.0f };
    frame.setPathEffect(SkDashPathEffect::Make(dashes, 2, 0.0f));
    frame.setColor(SK_ColorWHITE);
    canvas->drawRoundRect(
        SkRect::MakeXYWH(60, 60, 240, 120), 10, 10, frame);

    // Additive glow: two overlapping circles with kPlus get
    // brighter where they intersect.
    SkPaint glow;
    glow.setAntiAlias(true);
    glow.setBlendMode(SkBlendMode::kPlus);
    glow.setColor(0x66224699);
    canvas->drawCircle(285, 205, 18, glow);
    canvas->drawCircle(305, 205, 18, glow);
}
```

圆角框的调用咱们也认一眼：`drawRoundRect` 收的是矩形加两个圆角半径，10 与 10 的意思是横向纵向各弯 10，跟 D2D 圆角矩形的 radiusX、radiusY 是同一个意思。圆角上的虚线会跟着轮廓拐弯，比画在直线上的样子更明显，dash 的排样在曲线上也是照常算的。

描边框的 paint 戴上了本篇的描边家当：stroke 的样式、2.0f 的线宽、圆的头与圆的角，外加一路的虚线。您看到的画面是深灰底上一片竖直的渐变，中间蹲着一个被乘暗的圆，圆的外面还套着白色的圆角虚线框，右下角的两只圆的交叠处比单只更亮，加法档的活儿就摆在眼前。几段代码各管各的 paint，谁也不蹭谁的状态，您要是忘了把混合档改回去、让描边参数残留着，那就是前面提醒过的状态串台，示例里就是这么防的。

## 动手清单

动手的清单一共有五处，每一处咱们改的都不出两三行，原料都是示例里现成的代码：

1. 咱们把 `mul.setColor(0xFF808080)` 改成 `mul.setColor4f({1.0f, 1.0f, 1.0f, 0.5f}, nullptr)`，看圆会不会变淡。这就是篇首欠下的那处：颜色在场时它的 alpha 有多大的发言权，您亲眼验一遍。
2. 咱们把渐变的轴缩短到 60 高，再把 `SkTileMode::kClamp` 依次换成 kRepeat、kMirror 与 kDecal，四副样子摆在一起，平铺模式的脾气就看全了，画刷篇的同款实验在这边能原样重做。
3. 咱们把圆的 blend 换成 `SkBlendMode::kPlus`、颜色换成深蓝，乘法的暗圈变成了加法的亮圈，两档混合一暗一亮，比任何解释都直白。
4. 咱们把虚线的 phase 从 0.0f 改成 5.0f，断口沿着圆角框滑了一段。排样的起点在哪儿，说的就是这个参数。
5. 咱们给示例加一笔 kClear 的小圆，画在乘法圆的圆心上，圆心那一小块被清成全透明。底下叠着什么就透出什么，光栅画布单看是一块黑，真合成到窗口上时，露出来的才是窗口的内容。打洞的效果，您亲眼看过一遍就会记得住。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. shader 为 nullptr 时，这笔绘制的源色从哪儿来？
2. `MakeLinear` 的 pos 数组传 nullptr 与传实数组的差别是什么？
3. kDecal 在 D2D 的 EXTEND_MODE 里为什么找不到对应？
4. kClear 与 kDst 各自画完之后，画面上留下的是什么？
5. cap 与 join 在 Skia 里住在谁的身上，跟 D2D 的 StrokeStyle 相比结构差在哪？
6. 虚线的 phase 挪动一步，画面上会看到什么变化？
7. 局部的 shader 变量析构之后，paint 里的 shader 为什么还活着？
8. 线宽 10 的描边画在 100×100 的矩形上，外沿到了哪里？
9. 同一个半透明的源，kSrcOver 与 kMultiply 各自透出来的底色差在哪？
10. `SkShaders::Blend` 组合出来的还是什么类型的东西？
11. 每帧重造渐变的 shader 与搬到循环外建一次，各自适合什么样的场合？
12. SkImage 与 SkShader 的不可变，各替咱们省了什么心？

## 下一步

参数袋咱们挖到这里，画形状、上渐变、铺图像、做混合的家当就齐了，可界面的大头是字，Skia 画字的入口咱们还一点没碰。按[篇目索引](index.md)的排法，下一篇讲的就是 SkFont 与 TextBlob，咱们到那边把字号、字形与整段文本的画法认起来。再往后您想亲手写 shader 的话，索引里登记着的《SkSL：运行时着色器》正等着，本篇的 shader 概念到那儿正好当地基。索引上排着的 GPU 后端一篇会把咱们领到 DirectContext（Skia 里代表 GPU 上下文、通往硬件加速的入口对象）的门口，本篇的画布一直在 CPU 一侧光栅化，跨过那一步咱们再认另一半。
