---
title: 图层与裁剪
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 图层与裁剪

[上一篇《DirectComposition：visual 树》](31_directcomposition.md)收尾的时候，把 Clip 的话头留给了本篇，它只认了矩形一档，圆角裁剪咱们放到这边展开，透明度叠出的效果也留给这边对照。往前追溯还有更早的一笔，[《特效：Effect 图》](11_d2d_effects.md)收尾的时候说过，“PushLayer 与 PopLayer 管的是裁剪与透明度，跟 Effect 是两套分工不同的工具”，本线的后面还排着《图层与裁剪》一篇，您现在读到的正是它。本篇的主线很直：把画面的一部分圈起来单独处理，手里的库给了咱们两档手段，便宜的裁剪与贵的图层，咱们把分界认清楚，把接口的脾气摸透，末了拼一张圆角图片卡。

底本与代码的交代也放在前面。图层的事实底本是书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)后半场，`PushLayer` 的用法、配平的警告都出自它，接口行为的细则咱们对着微软的公开文档核。整篇的代码都是讲解用的骨架，咱们没有在仓库里编译过，您拿去用之前请对照 SDK 的签名。读者的设定照旧，您会 C++ 和 Win32 的基础用法就够，D2D 的对象模型借[《工厂、设备与渲染目标》](06_d2d_targets.md)，设备的接线借[《特效：Effect 图》](11_d2d_effects.md)，两篇搭好的骨架本篇不重贴。

## 裁剪与图层，分界在哪

咱们把两档手段的活分开摆。裁剪（clip）管的是画的东西不许出界，它是纯几何的约束，出了界的像素直接砍掉，落笔的调用本身一个都没有变。图层（layer）管的活要重得多，Push 了进去之后，咱们画的所有东西被收进一块屏幕看不见的离屏表面，处理完了 Pop 回来，整块地合进主画面。

价钱差在路的长度上。裁剪的便宜来自路短：每一笔落下去的时候，界外的像素当场丢弃、中间不落任何表面。图层的贵来自路长，一次 Push 就可能换来一块新分配的表面，Pop 的时候还要把整块内容混合回主画面，一收一发都是实打实的开销。官方的性能建议顺着成本给了分工，矩形够用的场合，咱们别劳驾图层。

另有一对名字咱们得防混。系列里出现过的“重定向表面”，说的是 DWM 替窗口存画面用的那块存储。本篇说的离屏表面，是咱们自己的绘制在 Push 与 Pop 之间住的那块临时画布。它们的名字长得像，来历其实是两回事，您读到的时候分开记。

## PushAxisAlignedClip：矩形一档，便宜

咱们要认的头一个裁剪入口是挂在渲染目标身上的 `PushAxisAlignedClip`。它收的参数有两样，一样是要裁的 `D2D1_RECT_F` 矩形，另一样是管抗锯齿的 `D2D1_ANTIALIAS_MODE`。配套的弹出叫 `PopAxisAlignedClip`，它俩得像括号一样成对地进退。嵌套的时候后进的弹出，栈的章法咱们到 Skia 那边还会再见到一回。[《坐标变换》](09_d2d_transforms.md)还领过它跟变换的一段纠葛：世界变换设着的时候，咱们传进去的矩形会被一并变过去，实际裁的是变换后区域的轴对齐包围盒。本篇的骨架不带变换，咱们只管它轴对齐的本份。

它办不了的活，也就是本篇后半场的由来：矩形的界再板正，圆角、椭圆、任意路径的形状它都表达不了，整段的透明度它更是沾不上边。这些活咱们全交给了图层。官方文档对分工的措辞写得很直白，咱们照录意思：只要轴对齐的矩形够用，咱们就用 `PushAxisAlignedClip`，图层留给非它不可的事。

GDI 的老住户在这时能帮咱们对个坐标。`PushAxisAlignedClip` 与 `PopAxisAlignedClip` 的分工，跟书架里 `SelectClipRgn` 设界、`SelectClipRgn(hdc, NULL)` 退界的路数是一个思路，两家的细节咱们留到 GDI 的段落再展开，您带着这个对照往下读就行。

## PushLayer：把一段绘制收进离屏表面

`PushLayer` 推的是一块层，调用之后咱们的每一笔都落在层上，直到 `PopLayer` 把整块的层合回主画面。层住的是一块临时的离屏表面，屏幕上没有它的位置，它是咱们与主画面之间的一段缓冲。书架对配平的警告写得很重：Push 与 Pop 走的是栈的章法，Push 了没 Pop、多 Pop 了一下，运行时也不给一个友好的报错，渲染结果是错乱的，严重的时候甚至把程序带崩。所以咱们的骨架里，Push 与 Pop 之间一根提前返回的分支都不放，配平的纪律靠结构保住，咱们不指望记性。

层怎么裁、怎么调透明，全由 `D2D1_LAYER_PARAMETERS` 说了算。在 device context（Effect 篇创建链用的 `ID2D1DeviceContext`）的路线上，咱们用的一代结构体叫 `D2D1_LAYER_PARAMETERS1`，配套的 `D2D1::LayerParameters1()` 帮咱们把默认值铺好。字段咱们挨个认，认完回头看圆角卡的骨架就顺了。

`contentBounds` 是矩形的界，界外的内容被裁掉。官方文档对它的措辞很谨慎，界外的内容只是不保证被裁掉，性能建议也配套地跟了一句，矩形界的活交给 `PushAxisAlignedClip` 办。咱们把它读成“能裁，但矩形裁剪别劳驾它”。

`geometricMask` 管的是任意形状。它收的是任何一个 `ID2D1Geometry`，咱们手里有的圆角矩形、椭圆、路径几何全能当蒙版，层里的绘制被裁进几何的内部，几何外的笔全部落空。配套的 `maskAntialiasMode` 管蒙版边缘的抗锯齿，`maskTransform` 还能给蒙版挂上一个单独的变换，蒙版的位置跟内容的坐标对不上的时候，咱们挪蒙版就好，内容的重画一笔都用不着。

透明的两份家当是 `opacity` 与 `opacityBrush`。前者是一个 0 到 1 的浮点，整层统一的透明度。后者收的是一支画刷，画刷调图案的门道咱们到后文《opacityBrush：透明度的伏笔兑现》一节再看，名字咱们在这里认下就行。结构体里还剩了一个 `layerOptions1`，管的是两件事：层内容要不要从背景初始化、未知的文本模式怎么处理，骨架里咱们给默认值不动它，真做图文混排的时候再查文档。老一代结构体上的 `layerOptions` 到了 1 代换成了它，咱们认得名字对得上号就行。

`PushLayer` 的第二个参数，收的是 `ID2D1Layer*`。书架给的现成用法是传 `nullptr`：device context 的路线上系统的运行库自管分配，改进是随着 D2D 1.1 来的，1.0 的老路线上咱们必须 `CreateLayer` 建好对象再递进去。官方的性能文档里另有一份与它互补的建议：反复 Push 同样大小的层时，咱们把 `ID2D1Layer` 建一次，每回递进去的都是同一个对象，省的正是 Push 一次就可能新分一块表面的开销。单张卡片推一两层的示例，传 `nullptr` 也没什么压力，两种给法咱们按场合挑。

咱们排界面的时候，层的资格只给裁剪办不了的活。整段的透明是一件，按图案调的透明是一件，给一组绘制挂的同一个蒙版又是一件。一个画面里层的数目值得咱们数一数，道理跟 Effect 篇数节点是一样的，每一层都按一块新表面、一次合成的开销做预算。

## opacityBrush：透明度的伏笔兑现

[《特效：Effect 图》](11_d2d_effects.md)收尾的时候留过一句话，`PushLayer` 与 `PopLayer` 管的裁剪与透明度里，裁剪咱们已经认过了，透明度的兑现就在 `opacityBrush` 的身上。机制咱们用一段白话讲：画刷铺在层的上方，合回去的时候，画刷每个像素的 alpha 乘到层内容同位置的 alpha 上，颜色分量是整个不出场的。想要边缘淡出的效果，咱们就拿径向渐变当画刷，中心不透明而边缘全透，中间平滑地过渡，暗角与聚光灯的观感都是它的活。

画刷的家当全是[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)的老朋友，咱们把径向淡出的骨架摆出来：

```cpp
// Radial fade via opacityBrush - skeleton, not compiled in this repo.
// The stops vary alpha only: an opacity brush contributes its alpha,
// its color channels are ignored.
D2D1_GRADIENT_STOP stops[2] = {
    {0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)},   // center: opaque
    {1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f)},   // rim: transparent
};
ID2D1GradientStopCollection* stops_col = nullptr;
g_pDC->CreateGradientStopCollection(
    stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &stops_col);

D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radial{};
radial.center   = D2D1::Point2F(240.0f, 190.0f);
radial.radiusX  = 190.0f;
radial.radiusY  = 190.0f;

ID2D1RadialGradientBrush* fade_brush = nullptr;
g_pDC->CreateRadialGradientBrush(radial, stops_col, &fade_brush);

D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1();
params.opacityBrush = fade_brush;
g_pDC->PushLayer(params, nullptr);
// ... draw the group here: each pixel survives by the brush's alpha ...
g_pDC->PopLayer();

fade_brush->Release();
stops_col->Release();
```

两站的 stop 只动了 alpha，颜色在两头都是白的。圆心与半径咱们按要淡出的那组内容的大小填，`D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES` 的字段就是画刷篇那排老面孔。层的参数咱们只填了 `opacityBrush`，界与蒙版走的都是默认值，Push 与 Pop 之间画什么都行，出来的成片统一地按画刷的 alpha 渐隐。均匀的透明度用 `opacity` 字段就够了，画刷是为图案准备的，两个入口咱们按需求分。

## Skia：裁剪与层，同一副栈

[第 13 篇《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)立过 Skia 的栈：`save` 把矩阵和裁剪区都压进了栈里，`restore` 再整份地弹回来。裁剪的入口是 `clipRect` 与 `clipPath`，咱们拿它跟 `PushAxisAlignedClip` 对着看，身份是一样的，纯几何的约束，不落任何中间的表面，便宜的理由同源。`clipPath` 认的又是 13 篇拼过的 `SkPath`，所以任意形状的裁剪在 Skia 那边根本不用劳驾层。

层的入口是 `saveLayer`，13 篇只带了一句“栈上还能压更重的东西”，咱们在本篇把它接了上来。它压进栈的是一块离屏表面：之后的绘制落在层上，`restore` 的时候按参数袋里给的 alpha 混回去。均匀的透明度塞在 `SkPaint` 的 alpha 里递给 `saveLayer`，对位 D2D 的 `opacity` 字段，两个库在层的身上倒坐到了同一个座位上。

形状这件事上两家的接口是反着走的。D2D 的任意形状裁剪要走层，蒙版是层参数里的一等公民。Skia 的任意形状裁剪走的是裁剪本身，`clipPath` 一个也就够了，层也就不必出场了。判据倒是只有一条：问一句用不用离屏的表面。几何的裁剪便宜，离屏的层贵。咱们跨库写代码的时候问这一句，比背哪家的接口名都管用。

## GDI：区域当家的裁剪

GDI 面板上的裁剪入口是 `SelectClipRgn`，它把一个区域设成 DC 的裁剪区。这里的 DC 是 GDI 的设备描述表，跟 D2D 的 device context 是两回事，缩写重了名，东西各是各的。区域（region）咱们在[《失效、脏区与重绘》](05_invalidation_dirty_region.md)里认识过：Win32 里描述任意画面范围的通用说法，矩形、椭圆、多边形、拼起来的组合形状，咱们要的它都装得下，而更新区域本身就是区域。所以 GDI 这边任意形状的裁剪不缺零件，区域的形状能力是现成的，缺的只是层那样的加工能力。

书架 [GDI Region 一章](../native_win32/24_ProgrammingGUI_Graphics_GDI_Region.md)的两处细节咱们照录。`SelectClipRgn` 用的是区域的副本，调用完了原来的 HRGN 还归咱们管，释放走的还是老一套 `DeleteObject`，这与所有权整个转移走的 `SetWindowRgn` 是两码事。清除裁剪的办法是 `SelectClipRgn(hdc, NULL)`，收尾的顺序书架也给了样板，咱们把裁剪退干净，再删手里的区域对象。

层在 GDI 的世界里没有对位。半透明一类的活在 GDI 面板上走的是 `AlphaBlend` 一类的像素搬运，书架的[AlphaBlend 一章](../native_win32/25_ProgrammingGUI_Graphics_GDI_AlphaBlend.md)教过咱们怎么调透明度看叠加。GDI 的年代里，透明靠的是搬运，裁剪靠的是区域，把两件事打包进一块表面的层，GDI 是没有往那个方向走的。

## 四家的对照

咱们把四家的入口摆成一张表，您往后翻的时候对着查。表里的 DComp 是 DirectComposition 的缩写，上一篇咱们已经认识它了。效果组也是上一篇的家当，透明度那行咱们借它用一用，正文 DComp 的段落再细说。

| 对照的点 | GDI | Direct2D | Skia | DComp |
|:---|:---|:---|:---|:---|
| 矩形裁剪 | `SelectClipRgn` 配矩形区域 | `PushAxisAlignedClip` | `clipRect` | `SetClip` 收 `D2D1_RECT_F` |
| 任意形状 | 区域自己拼形状 | `geometricMask` 走层 | `clipPath` 走裁剪 | `IDCompositionRectangleClip` 的圆角档 |
| 整段透明 | `AlphaBlend` 搬运 | `opacity` 与 `opacityBrush` | `saveLayer` 配 paint 的 alpha | 效果组的 `SetOpacity` |
| 还原的办法 | 传 `NULL` 清除 | `PopLayer` 与 `PopAxisAlignedClip` | `restore` 弹栈 | 属性进批，`Commit` 生效 |

任意形状那一行就是现成的例子：四家的答案都贴着自家管线的位置，谁的手里现成有什么，答案就从谁的手里出。您以后跨库对照的时候，问一句机制落在管线的哪一段、接口名跟着段位查，表也就不用背了。

## DComp 的 Clip：裁剪搬到合成器那侧

[上一篇](31_directcomposition.md)咱们只认了 Clip 的矩形一档，`SetClip` 收的是一个 `D2D1_RECT_F`，节点连同子树的输出被框在矩形里，框外的部分合成器就不要了。圆角档走的是 `IDCompositionRectangleClip`：device 的 `CreateRectangleClip` 把对象造出来，`SetLeft`、`SetTop`、`SetRight`、`SetBottom` 定矩形的界，圆角的半径由 `SetTopLeftRadiusX` 与它的兄弟们管，左上、右上、左下、右下各自的一对半径都有对应的入口。挂上去的还是 `SetClip`，另一个重载收的就是裁剪对象。

还有一处身份上的差别。D2D、Skia、GDI 的裁剪全发生在咱们进程的绘制里，画多画少是咱们自己的开销。DComp 的 Clip 是合成参数，跟上一篇领过的位移属性 OffsetX 享受的是同一个待遇，改动进了批，`Commit` 交了之后才生效，生效之后 DWM 就按新的界拼帧了，内容是一个像素都不用重画的。内容的重画与合成的参数，两条路的开销算法完全不同，[《动画与帧调度》](34_animation_frames.md)把两条路的对照摆得更全。

用法上的分工也就清楚了。内容本身要变的，走绘制侧的裁剪，D2D 的层与裁剪都在咱们自己的帧里。内容不动、可见的范围要动的，咱们就挂到合成的那一侧。比如让一块静止的画面的可见窗口跟着曲线挪，曲线挂上了之后，插值是 DWM 的事，咱们自己的进程连醒都不用醒。

透明度在合成的那一侧也有一份家当，它就是上一篇领过的效果组 `SetOpacity`，它跟本篇的 `opacityBrush` 是两个层面：效果组调的是合成参数，`opacityBrush` 调的是绘制内容自己的 alpha。至于 Content 的更多来源，上一篇只带了 `CreateSurface` 系接口的一句名目，等哪篇真用得上合成器侧的表面，咱们再把它接全，本篇的骨架走的都是交换链，咱们正好轮不上用它。

## 裁剪与失效：白画的不进像素

在 GDI 的面板上，裁剪与失效的联动是系统替咱们做好的。[《失效、脏区与重绘》](05_invalidation_dirty_region.md)里咱们见过原话：`BeginPaint` 交出的 HDC，裁剪区被设成了更新区域，画到脏区外面的部分自动被裁掉，画了也是白画。白画的笔不会写进像素，咱们把失效报得越准，白画的范围也就越小。

咱们自己叠的裁剪也搭同一班车。书架[文字章](../native_win32/21_ProgrammingGUI_NativeWindows_GDI_Text.md)里给过的手法是 `IntersectClipRect`，它把一个矩形与当前的裁剪区求交，交出来的界只会更小，落在脏区外的笔照旧落空。所以 GDI 的绘制路径里，失效、裁剪、落笔三样是咬在一起的，咱们报失效的精度，就是绘制省下来的精度。

这套现成的联动在 D2D 的路线上没有。咱们从[《工厂、设备与渲染目标》](06_d2d_targets.md)起走的就是整帧重画，失效在那边只当了个触发器，登记销掉的办法是画完整帧后调一句 `ValidateRect`，把重画的请求清掉。想把脏区的好处接回来，咱们得自己补一步，拿[《失效、脏区与重绘》](05_invalidation_dirty_region.md)里用过的 `GetUpdateRect` 把脏区的包围盒查出来，把盒子递给 `PushAxisAlignedClip` 的动作要赶在销登记的前面，界内的画完一 Pop，失效登记随后也就销掉了。失效报得准、裁剪收得紧两件事凑到了一起，《失效、脏区与重绘》的功课与本篇的裁剪在 D2D 这边就合上了流。

合成侧的裁剪连绘制都不用，跟失效的联动也就谈不上，内容的重画一次也没有发生，动的只是 DWM 拼帧的方式。GDI 的裁剪长在系统发的 DC 上，D2D 的裁剪是咱们自己 Push 的，DComp 的裁剪根本不在绘制的路径里。

## 动手骨架：圆角图片卡

咱们把本篇的家当拼成一张圆角的图片卡：照片要裁进圆角的矩形，底下垫一团柔和的阴影。设备与接线的部分全借现成的：创建链走 Effect 篇的 device context 路线，照片解码用 Effect 篇包好的 `CreateContentBitmap`，窗口消息的那几条也照它办。本篇新增的家当只有两样：圆角几何与剪影的位图。整段代码是没有编译验证过的，管的是把形状画对，您拿去跑之前请对照 SDK 的签名。

一次性的家当咱们只建一回。骨架里 `CreateCardAssets` 是每帧都被调到的，开头的守卫让它只在头一回真正动手，之后的调用都是空跑一趟。圆角几何交给工厂的 `CreateRoundedRectangleGeometry`，[《几何与路径》](08_d2d_geometry.md)认过的创建路线原样适用。剪影位图走[《位图与离屏渲染》](10_d2d_bitmaps.md)的离屏目标：开一块跟卡片同尺寸的 `CreateCompatibleRenderTarget`，往上面画好一个不透明的黑色圆角矩形，再用 `GetBitmap` 把它取了出来，阴影要的原料就备齐了。您要是直接把照片喂给 Shadow 效果，照片的方角会原样出现在影子里，所以圆的原料得咱们自己画。

```cpp
// One-time card assets - skeleton, not compiled in this repo.
// The device chain (g_pDC, g_pSwapChain, g_pFactory) follows the
// effects tutorial verbatim, so does CreateContentBitmap.
static constexpr float kCardLeft   = 60.0f;
static constexpr float kCardTop    = 60.0f;
static constexpr float kCardRight  = 420.0f;
static constexpr float kCardBottom = 320.0f;
static constexpr float kCardRadius = 16.0f;

static ID2D1RoundedRectangleGeometry* g_pCardClip   = nullptr;
static ID2D1Bitmap*                   g_pSilhouette = nullptr;

static HRESULT CreateCardAssets()
{
    // The silhouette is the LAST asset built: when it is non-null,
    // every earlier one already exists. Guarding on it alone keeps a
    // half-built state from skipping the rebuild (and leaking the
    // geometry on the next pass).
    if (g_pSilhouette != nullptr) {
        return S_OK;   // already built: no per-frame rebuild, no leak
    }

    const D2D1_ROUNDED_RECT card = D2D1::RoundedRect(
        D2D1::RectF(kCardLeft, kCardTop, kCardRight, kCardBottom),
        kCardRadius, kCardRadius);
    HRESULT hr = g_pFactory->CreateRoundedRectangleGeometry(card, &g_pCardClip);
    if (FAILED(hr)) return hr;

    // A black rounded rect on a transparent off-screen target. The shadow
    // effect will blur this silhouette; feeding it the photo directly
    // would put the photo's square corners into the shadow.
    ID2D1BitmapRenderTarget* off = nullptr;
    hr = g_pDC->CreateCompatibleRenderTarget(
        D2D1::SizeF(kCardRight - kCardLeft, kCardBottom - kCardTop), &off);
    if (FAILED(hr)) return hr;

    ID2D1SolidColorBrush* black = nullptr;
    off->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &black);
    off->BeginDraw();
    off->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    // Local coordinates: the silhouette spans the off-screen target 1:1,
    // same radii as the clip geometry above.
    off->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.0f, 0.0f,
                                      kCardRight - kCardLeft,
                                      kCardBottom - kCardTop),
                          kCardRadius, kCardRadius),
        black);
    hr = off->EndDraw();
    if (SUCCEEDED(hr)) hr = off->GetBitmap(&g_pSilhouette);

    black->Release();
    off->Release();
    return hr;
}
```

每帧的活咱们分两半。上半场干的是影子，Effect 图就是 Effect 篇的原班，Shadow 吃的是剪影，咱们只留它一个节点，Effect 篇里那步平移变换省下了，落点直接给在 `DrawImage` 的坐标里。下半场轮到的是照片，层的参数里填上圆角几何当蒙版，照片拿 `DrawBitmap` 画了进去，Pop 出来就齐了。

```cpp
// Per-frame painting - skeleton, not compiled in this repo.
void OnPaintCard(HWND hwnd)
{
    CreateDeviceResources(hwnd);   // effects-tutorial wiring
    CreateContentBitmap();         // WIC chain from the same tutorial
    CreateCardAssets();            // guarded: returns early when built
    if (g_pDC == nullptr || g_pBitmap == nullptr || g_pSilhouette == nullptr) {
        return;
    }

    g_pDC->BeginDraw();
    g_pDC->Clear(D2D1::ColorF(0.10f, 0.12f, 0.18f, 1.0f));

    // First half: the soft shadow. DrawImage places the effect output
    // (larger than the silhouette by the blur margin) at card + offset.
    ID2D1Effect* shadow = nullptr;
    g_pDC->CreateEffect(CLSID_D2D1Shadow, &shadow);
    shadow->SetInput(0, g_pSilhouette);
    shadow->SetValue(D2D1_SHADOW_PROP_COLOR,
                     D2D1::Vector4F(0.0f, 0.0f, 0.0f, 0.45f));
    shadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, 8.0f);
    g_pDC->DrawImage(shadow, D2D1::Point2F(kCardLeft + 12.0f, kCardTop + 16.0f));
    shadow->Release();

    // Second half: the photo clipped to the rounded card. No early return
    // between Push and Pop - the pair stays balanced by structure.
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1();
    params.contentBounds = D2D1::RectF(kCardLeft, kCardTop,
                                       kCardRight, kCardBottom);
    params.geometricMask = g_pCardClip;
    params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;

    g_pDC->PushLayer(params, nullptr);
    g_pDC->DrawBitmap(g_pBitmap, D2D1::RectF(kCardLeft, kCardTop,
                                             kCardRight, kCardBottom));
    g_pDC->PopLayer();

    HRESULT hr = g_pDC->EndDraw();
    if (FAILED(hr)) {
        DiscardDeviceResources();          // whole chain, as in the effects tutorial
        InvalidateRect(hwnd, nullptr, FALSE);
        return;                            // skip Present with the device gone
    }
    g_pSwapChain->Present(1, 0);
}
```

落点的那一行，模糊的输出比剪影大一圈，Effect 篇讲过外扩的量，所以影子的实际位置会比咱们给的坐标再缩进一点，您跑起来按眼睛微调落点就好，骨架把关系摆对也就够了。配平的纪律 PushLayer 一节讲过，这里的骨架照办，咱们在 Push 与 Pop 之间不放任何分支，早退全部排在 Push 的前面。您想给卡片再添一笔描边，Pop 完了拿 `DrawRoundedRectangle` 沿同一个圆角矩形走一笔就是，书架[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)里的毛玻璃卡片就是这么收的边，描边放在层的外面，蒙版就管不到它了。

## 您可以动手的三处

您要是把骨架补成了能跑的版本，下面的三处都不难改，每处都是改一行、编一遍、看一眼画面的事：

1. 把 `geometricMask` 换成椭圆几何，`CreateEllipseGeometry` 造一个，照片进椭圆框，头像的样式就出来了，`contentBounds` 您顺手按椭圆的包围盒收一收。
2. 把 `maskAntialiasMode` 换成 `D2D1_ANTIALIAS_MODE_ALIASED` 再编，圆角的边缘会现出锯齿，蒙版边缘的抗锯齿开关就有了直接的读数。
3. 给 `params.opacity` 填 0.5f，整张照片半透明，底下那团影子从照片里透出来，均匀透明度与画刷透明度的分工您一眼就看清了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 裁剪与图层的分界落在哪块表面上？什么样的活才有层的资格？
2. `PushAxisAlignedClip` 的两个参数各是什么？它与世界变换的纠葛是哪一篇领过的？
3. `D2D1_LAYER_PARAMETERS` 里管形状的字段是哪两个？管透明的又是哪两个？
4. `PushLayer` 的第二个参数在哪条路线上可以传 `nullptr`？复用的建议怎么给？
5. 层的代价由哪几笔开销组成？官方对只有矩形界的活怎么建议？
6. Skia 里对位 `PushLayer` 的是谁？任意形状的裁剪在 Skia 那边走哪条路？
7. GDI 的裁剪入口是谁？`SelectClipRgn` 对区域的所有权怎么算？
8. DComp 的圆角裁剪用什么对象？改了 Clip 之后内容要重画吗？
9. `BeginPaint` 交出的 DC，裁剪区是什么？D2D 路线上想把脏区的好处接回来，要补哪几步？

## 下一篇

下一篇的题目是[《透明、阴影、模糊》](33_transparency_effects.md)。它接本篇的班接得正顺：`opacityBrush` 只调了透明度的图案，透明与混合的整套数学、阴影与模糊的系统用法都归它展开，本篇垫底的那团影子就是它的现成素材。排得更后面的[《动画与帧调度》](34_animation_frames.md)里，合成参数挂曲线的章法正等着咱们。
