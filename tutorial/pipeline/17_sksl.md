---
title: SkSL：运行时着色器
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# SkSL：运行时着色器

按[篇目索引](index.md)的登记，本篇在讲 Skia 的这批文章里排的是第五位。咱们往前数一数：[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)、shader 的深入、文本与 GPU 的后端，四篇的稿子都跟您见过面，[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)收尾的时候还提了本篇一句，说您到了这边就能自己写着色器的源码，画前两篇画不出来的效果。咱们再往后看，窗口的活留给下一篇[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)，Skia 的一段就在那边收尾。

本篇的底子是[《SkPaint 深入：shader 与混合》](14_skia_paint_deep.md)。paint 的 setShader、shader 管的源色、工厂交还的 sk_sp<SkShader>，这些咱们在那一篇里都认过了，本篇也就直接拿来用了。您要是想不起来 shader 管的是什么，咱们拿一句白话来复习：它回答的是“这笔绘制的颜料从哪儿来”。纯色、渐变与位图都是工厂给的现成答案，本篇咱们自己来写公式，把答案亲手算了出来。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够。本篇的示例全画在光栅画布上，[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)的家当不是本篇的前提，您没读过那一篇也照样跟得上，咱们提 GPU 的地方都只带一句。D2D 的 Effect 图咱们当对照面用，认过[《特效：Effect 图》](11_d2d_effects.md)当然更好，没读过它也不影响咱们的主线。

本篇以 2024 年前后的 Skia 主线为准，SkSL 的语法与 C++ 侧的接口这几年都在动，uniform 的传法更是换过了一整代，新旧写法并存的地方咱们当场注明，笔者拿不准的签名宁可少写细节，您以手上的头文件为准就好。

## 从工厂到公式

咱们拿 D2D 对个照，也就知道 Skia 的门槛矮在哪里了。[《特效：Effect 图》](11_d2d_effects.md)收尾的时候提过一笔：想给自己的像素公式找条正路，D2D 那边得实现 ID2D1EffectImpl、注册自定义的 Effect，属于进阶的活。而 Skia 这边省了这些仪式，咱们把公式写成一段字符串交上去，它交还的是一个能塞进 paint 的 shader，中间没有 COM 类型的登记，也没有额外的编译系统。

干活的入口住在 `SkRuntimeEffect` 的身上，静态的 `MakeForShader` 吃的是一段 SkSL 的源码字符串。编译通过的话，它交还给咱们 effect 对象，编译不过的话，SkSL 编译器的报错原话会递到咱们手里，咱们拿着报错去改字符串就是了，改完了再编一遍，一轮也就几秒钟的工夫。

```cpp
#include "include/effects/SkRuntimeEffect.h"

const char* src = R"SKSL(
uniform half4 gColor;
half4 main(float2 p) {
    return gColor;
}
)SKSL";

auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(src));
if (!effect) {
    // compile failed: err.errorText holds the compiler's message
    return;
}
```

骨架里 SkSL 的那几行咱们到下一节再单说，您看 C++ 的这一侧就好：源码装进了 SkString，交了出去，拿回来的就是一个 effect 加一份报错。拿回的 effect 还不是能直接画的 shader，咱们可以把它理解成“编译好的公式”，真到了绘制的时候，配上参数它才变成了 shader。

主线的某几个年份里，MakeForShader 交还的是一个叫 Result 的结构，effect 与报错的文本装在一起，咱们骨架里解构的就是它。更早的年代另有直接交还 sk_sp 的写法，报错走输出的参数。两个年代笔者都没法替您押哪一个，您翻一翻自己手上版本的 SkRuntimeEffect.h，认准了“交还什么、报错从哪儿读”两件事也就行了，用法本身是没有变的。

咱们把两边的自定义做法摆成一张表，差别也就更清楚了：

| 对照的点 | D2D 自定义 Effect | Skia runtime effect |
|:---|:---|:---|
| 咱们写什么 | ID2D1EffectImpl 加 HLSL | 一段 SkSL 的字符串 |
| 注册的负担 | 自定义 Effect 的登记 | 没有 |
| 编译的时机 | 跟着工程走 | 程序跑起来之后 |
| 参数的递法 | 属性口 SetValue | uniform 的位子 |
| 子节点的接法 | Effect 图连输入 | child shader 加 sample |
| 上手的门槛 | 进阶 | 几行起步 |

SkRuntimeEffect 家里其实还有两个兄弟入口：MakeForColorFilter 管的是颜色滤镜，MakeForBlender 管的是混合档，与咱们本篇的 shader 入口是一样的。咱们本篇只走 shader 的门，另外两扇等您用到的时候再开。

Skia 的源码树里，SkSL 的编译器住在 src/sksl 的目录下，咱们想看它怎么报错、怎么翻译，翻进去就能对着读了。

effect 还有一个省心的性质：编译一次、到处配的。同一个 effect 的对象，咱们配上不同的 uniform 就能造出好几个 shader，公式没有重编的份，参数各是各的。它的生命周期跟咱们在《SkPaint 深入》那篇里认过的 sk_sp 是同一套，引用计数交给了指针，咱们不用手工去管。

什么时候该亲手写公式？工厂给的渐变、位图、模糊能办到的事，咱们不必抢过来自己写，内建的实现是调过优的。runtime effect 的价值在工厂办不到的地方：形状按距离裁、颜色按坐标算、子 shader 的结果再加工，这些自己的公式说了算的活，才是它的主场。性能上的开销笔者没有量过数，咱们的公式里少写分支、多用内建的函数，是一般稳妥的路子。

源码字符串别散在绘制的代码里，咱们把它跟 MakeForShader 一起收在初始化的地方，程序起跑的时候编一次，编不过的话就在启动的时候炸出来，总比画到一半才发现错了体面。

## SkSL：Skia 自家的着色器语言

SkSL 是 Skia 自家的着色器语言。着色器的说法咱们在《SkPaint 深入》那篇里就见过，咱们拿白话再讲一遍：它是跑在绘制最里层、对每个像素各算一遍的小程序。SkSL 长得跟 GLSL（OpenGL 的着色器语言）是一个路数，会 GLSL 的您上手就能写，不会的也不要紧，本篇用到的也就几样东西，咱们一样一样地过。

它跟 GLSL 最要紧的差别在归宿。咱们写的这段 SkSL 不直接跑在任何硬件上，Skia 会把它编译成各后端的真着色器：D3D 的后端把它翻译成了 HLSL，OpenGL 的后端翻成了 GLSL，Vulkan 与 Metal 也各有各的译法。光栅后端也没有把咱们关在门外，CPU 的一侧同样备了执行它的路，本篇的示例画在光栅画布上，咱们走的就是它，您没有 GPU 也能把整篇跑通。

最小的一段长成下面的样子，咱们逐行来认：

```text
uniform half4 gColor;

half4 main(float2 p) {
    return gColor;
}
```

uniform 打头的行声明的是变量的空位，也就是 C++ 那边能填数的地方，gColor 是咱们自己起的名字。half 是着色器世界里的浮点类型，在支持的硬件上它是 16 位、算得更快的浮点，不支持的硬件上它就退回了普通的 float。half4 是四个 half 拼成的向量，这里装的是红绿蓝加透明度，取值的范围咱们按 0 到 1 来理解。

main 是每个像素都要进的一趟函数。p 是当前像素在画布坐标系里的位置，跟《SkPaint 深入》那篇里渐变轴住的是同一套坐标，它的返回值就是这一像素的颜色。咱们把这几样认全了，最小的 SkSL 也就会写了。

main 的签名咱们多看一眼，因为它正是迁移最勤的地段。2024 年前后的主线认的是 half4 main(float2 p)，坐标是从参数进来的。别的年代里坐标另有从全局变量读的路，自定义参数的传法也改过了一轮，咱们按参数进来的写法讲，您在资料里见到不一样的骨架，当成年代差来看就行了。

透明度有一处笔者不咬死的地方：main 返回的 half4 按什么样的语义参与合成，预乘的还是不预乘的，官方文档的措辞咱们没有逐字抠过。本篇示例里的颜色要么全透明、要么全不透明，正好绕开了这里，您拿半透明的颜色做实验时，以官网的 SkSL 页为准。

## 写起来跟 GLSL 差在哪

咱们头一回写 SkSL，带进来的多半是 GLSL 的习惯，有几处的习惯得改一改。头一处是版本声明：GLSL 开头的 #version 一类指令，SkSL 里是没有的，咱们写上它，编译器会不客气地报错。咱们把字符串交给 MakeForShader 就行，版本的事 Skia 替咱们管了。

第二处要改的是输出的写法。GLSL 的老教程里给 gl_FragColor 赋值的那套路子，runtime shader 里是没有这个出口的，颜色走的是 main 的返回值。习惯上咱们只要改一条：咱们不去想“赋值给谁”的事，咱们只管算出来什么就返回什么。

咱们再认两样顺手的语法。常量咱们拿 const 声明，自定义的函数照 C 的样子写就行，类型上还有个小讲究：half 与 float 混着算的时候，结果就提升成了 float，咱们想要 half 的快，就把整个表达式都写成 half 一路的。

```text
const half kDim = 0.5;

half4 darken(half4 c) {
    return half4(c.rgb * kDim, c.a);
}
```

分支与循环在 SkSL 里是能写的，不过老一代的 GPU 着色器对动态的循环有各式各样的限制，咱们写的时候按固定次数的循环打算最稳。这些讲究官方的 SkSL 页写得全，真写的时候回头查那一页。

SkSL 里能写 C 风格的注释，咱们示例里的英文注释就是这么写的，报错的行号按字符串里的行数走，改字符串的时候行号别对错了地方。

## 头一批报错长什么样

咱们头一回跑通的时候，最常卡住的是名字对不上：C++ 侧的 `uniform("name")` 调用（写法出自后文的 SkRuntimeShaderBuilder）与 SkSL 里的声明，两边差了一个字母，Builder 就找不到位子了。它递不递带名字的报错，要看版本与编译的配置：断言停住、静默不生效都是有可能的。所以咱们写的时候把两边的名字当场对齐，也就省下了事后猜的工夫。

第二类是 GLSL 的习惯带进来了：写了 #version、写了 gl_FragColor、把 main 的返回写成了 void，编译器都会在对应的行上报错。报错的行号按咱们字符串里的行数走，咱们对着行号回去看那一行，咱们一般一眼就找得到病根。

第三类是类型上的：half4 与 float4 混着赋值的场合、拿向量当标量用的场合，SkSL 的编译器管得比 C++ 还严，它会在编译的时候就把这些拦下来，报错的措辞跟 GLSL 的编译器是一个风格。咱们把它当成一位严格的审稿人就好，它拦下来的每一处，都是真藏着问题的写法。

还有一类错不在编译的时候，在咱们自己的眼睛里：公式算出的颜色不对，多半是坐标系没对齐、端点给反了一类的手误。咱们拿最简单的公式验起，比如纯色返回的 main，一层一层地往上叠，哪一层要是开始不对了，病根也就落在哪一层了。

## uniform：把 C++ 的数递进公式

公式里的空位，咱们得有法子把它从 C++ 那边填进来。effect 编译好了之后，它登记着每个 uniform 的信息：名字、在数据块里的偏移、字节数、类型的标志，都住在 `SkRuntimeEffect::Uniform` 的布局里，是不是矩阵也有单独的标志。清单从 effect 的身上读，入口的具体写法您以手上版本的 SkRuntimeEffect.h 为准。

填数的写法分了两代，咱们都认个脸。旧的一代里，咱们按偏移把字节搬进一块 SkData，再连同子 shader 的数组与 localMatrix 一起打包，交给 effect 的 makeShader。新的一代给了一个 SkRuntimeShaderBuilder，咱们按名字取 uniform 的位子直接赋值，矩阵也是一样的，把 SkM44 赋给对应的位子就行了，字节的搬运它替咱们干了。咱们按新的写法讲，老代码您认得出脸就好。

旧一代的样子咱们也摆一段，您在老代码里要认得它：

```cpp
// 旧一代的写法：手工把 uniform 的字节搬进 SkData
sk_sp<SkData> data = SkData::MakeUninitialized(effect->uniformSize());
// ...按 Uniform 的 offset 把 float4 一类的值写进 data->writable_data()...
sk_sp<SkShader> shader = effect->makeShader(
    std::move(data), /*children=*/nullptr, /*childCount=*/0,
    /*localMatrix=*/nullptr, /*isOpaque=*/false);
```

头一行造的是一块按 effect 尺寸来的裸内存，咱们照着 Uniform 布局里的偏移把数搬进去，递交的那一步走的还是 makeShader，连孩子带矩阵也一起打了包。两代写法干的是同一件事，Builder 赢在不用咱们数字节。您翻老项目的时候两副面孔都得认得，新代码咱们一律走 Builder。

```cpp
SkRuntimeShaderBuilder builder(effect);
builder.uniform("gColor") = SkColor4f{0.20f, 0.31f, 0.88f, 1.0f};
sk_sp<SkShader> shader = builder.makeShader();
paint.setShader(std::move(shader));
```

builder 的 makeShader 交还的，就是咱们在《SkPaint 深入》那篇里认过的 sk_sp<SkShader>。塞进 paint 的 setShader 之后，绘制的时候每个像素都会进一趟公式。咱们走到了这一步，runtime effect 造的 shader 与工厂造的渐变、位图 shader，在 paint 的眼里就没有差别了，《SkPaint 深入》那篇里认过的 SkShaders::Blend 在这边照样能用，咱们还能拿别的 shader 来当它的孩子，child 一节咱们就见得到。

uniform 能装的东西咱们也数一数：单浮点、两分量到四分量的向量、矩阵 SkM44，数组也是支持的，布局里的 count 一栏记的就是数组的长度。颜色咱们顺手用 SkColor4f 填就行，Builder 替咱们做类型的换算。名字要是打错了，它多半不递带名字的报错过来：调试版多半在断言上停了下来，发布版则更可能是静默地不生效，这正是咱们要防的一处。

改了 uniform 之后，咱们得再调一回 makeShader 才生效，shader 本身是不可变的，这一点跟《SkPaint 深入》那篇里认过的一样。咱们把 builder 留在手边，每帧改参数的场合就每帧造一个 shader，创建的代价笔者没有量过数，您要是嫌它勤快，把不动的参数收进初始化也就好了。

旧一代 effect->makeShader 的签名里还带着一个 isOpaque 的口子，上头贴的老代码里您也见得着它。公式要是保证返回不透明的颜色，咱们就把它报成 true，后端能省下透明合成的活，前提是公式真的做得到，报错了画面也就是错的。Builder 年代的 makeShader() 还收不收这个参数，笔者没有逐版核过，您以手上版本的头文件为准。

## child shader：公式里再接一层

SkSL 里还能声明一类特别的空位，类型写的是 shader，咱们叫它 child。咱们在公式里拿 sample 对它取值，取回的是子 shader 在指定坐标上的颜色。子 shader 可以是工厂造的位图或渐变，也可以是另一个 runtime effect 造出来的 shader，一层套一层地级联。《SkPaint 深入》那篇的 SkShaders::Blend 做合成，child 做的是取样，干的事不同，却都是组合的手段。

```text
uniform shader base;

half4 main(float2 p) {
    half4 c = sample(base, p * 2.0);
    return half4(c.rgb * 0.5, c.a);
}
```

两行公式干的活咱们读一遍：坐标乘 2 是把子画面的内容缩小一半地铺开，rgb 乘 0.5 做的是整体压暗。C++ 那边配孩子走的是 builder 的 child 位子，子 shader 的造法您在《SkPaint 深入》那篇里都学过：

```cpp
builder.child("base") = SkImageShader::Make(
    img, SkTileMode::kRepeat, SkTileMode::kRepeat,
    SkSamplingOptions(SkFilterMode::kNearest), nullptr);
```

sample 的写法有年代上的差，2022 年之后的主线把它换成了 base.eval 一类的成员写法，两者的意思是一样的，您两头都要见得着。本篇的示例特意沿用旧的 sample 拼法，为的是机制不变、老项目里的样子您也认得，您手上的版本要是只认其一，编译器的报错会告诉您的。

sample 还有个不带坐标的省事写法：咱们写 sample(base) 的时候，用的就是当前的 p，子 shader 取的就是同一个像素上的值。级联的场合多半要的是改一改坐标再去取，乘个数、挪个位置都是一行的事，咱们示例里乘 2 的写法就是最直的一例。

声明了的 child 要是没配齐，makeShader 的那一步就会出问题，具体是断言停下、崩溃还是采到了空 shader，笔者没有验过，咱们把它塞进 builder 也就好了。

## 从公式到像素的一程

公式写好了，咱们把一帧的活从头走一遍，看看咱们的 main 是在哪儿被叫起来的。示例一里咱们拿它填一个 360×240 的矩形，算下来是 86400 个像素的规模，绘制的活到了这一层，就是每个像素各进一趟咱们的公式，进去的时候带的 p 各不相同，出来的颜色也就各不相同。

咱们再想一层就知道它为什么天然适合 GPU：86400 个像素是互相不依赖的，谁等谁的问题压根就没有，数据并行的来路，咱们在[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)里讲过。咱们画在光栅画布上的时候，这 86400 趟是在 CPU 上跑的，Skia 替咱们把 SkSL 编译成它能执行的样子，画布换成 GPU 的，同样的公式就换成了 HLSL 去跑显卡。公式的代码一个字都不用改。

还有一件省心的事：公式只会被圈进形状里的像素叫到，圈外的像素根本不会进咱们的公式，drawRect 圈住的范围就是干活的范围，咱们画整幅的背景就填整幅的矩形，画个按钮就填按钮大小的一块，计算量是跟着形状的面积走的，跟咱们在[《失效、脏区与重绘》](05_invalidation_dirty_region.md)里认过的省法是一个思路。

动画的口子也是现成的：咱们每帧改一个 uniform，比如咱们把 center 沿着一条线挪，形状就跟着动起来了，咱们改的只是 uniform，公式本身是没有动的。真正的动画循环在实战篇，咱们这里把口子认下来就好。

## 示例一：不用工厂，手搓线性渐变

咱们来干一件有实战味道的事：咱们不调 SkGradientShader，亲手把线性渐变算了出来。思路咱们摆开了讲，[《SkPaint 深入：shader 与混合》](14_skia_paint_deep.md)那篇里的渐变轴是两个端点，每个像素的颜色由它在轴上的投影位置来决定，咱们把同一件事写成公式：

```text
uniform float2 p0;   // gradient axis start
uniform float2 p1;   // gradient axis end
uniform half4  c0;
uniform half4  c1;

half4 main(float2 p) {
    float2 d = p1 - p0;
    // project p onto the axis, normalize to [0, 1]
    float t = clamp(dot(p - p0, d) / dot(d, d), 0.0, 1.0);
    return mix(c0, c1, t);
}
```

t 的算法是向量投影：dot 量的是像素离起点沿轴走了多远，除以轴长度的平方，归一成 0 到 1 的比例。clamp 的两个边界就是 SkTileMode 里 kClamp 的手工版，轴外的像素拿端点的颜色垫住。您要是把 clamp 换成了 fract，同一份公式就变成了 kRepeat 的排法，tileMode 的每一档在咱们手里都有了写法，工厂藏在盒子里的机关，咱们现在自己摸得着了。

《SkPaint 深入》那篇里渐变轴的教训在这边原样成立：咱们的轴活在画布的坐标系里，它不会跟着被填充的形状走。您要是把矩形挪了位置，公式里的端点是不动的，渐变的起点也就留在了原地，解法跟那篇的同款，要么按形状的新位置重配 uniform，要么把坐标的换算写进公式里。

您要是好奇精度上的事：咱们示例里的坐标走的都是 float，像素的量级用 float 是绰绰有余的，half 拿来装颜色是正合适的，两头混着算的时候，SkSL 会按咱们前面讲过的提升路数办。

C++ 这边咱们来配参数、上画布。画布的取法沿用《SkPaint 深入》那篇垫过的光栅骨架，三件套的细节在[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)：

```cpp
sk_sp<SkSurface> surface = SkSurfaces::Raster(
    SkImageInfo::MakeN32Premul(360, 240));
SkCanvas* canvas = surface->getCanvas();

SkRuntimeShaderBuilder builder(effect);
builder.uniform("p0") = SkV2{20.0f, 20.0f};
builder.uniform("p1") = SkV2{340.0f, 220.0f};
builder.uniform("c0") = SkColor4f{0.17f, 0.23f, 0.42f, 1.0f};
builder.uniform("c1") = SkColor4f{0.95f, 0.62f, 0.20f, 1.0f};

SkPaint bg;
bg.setShader(builder.makeShader());
canvas->drawRect(SkRect::MakeWH(360, 240), bg);
```

预期的画面是一块从左上深蓝走到右下橙的斜向渐变。SkV2 是两个 float 的向量类型，跟 float2 是对得上的，矩阵一类的空位同理拿 SkM44 去填，Builder 都是认的。本节与下一节的代码是笔者按 2024 年前后的接口签名写的骨架，没有经过编译的验证，您接进真画布时以编译器的报错为准。

三个颜色的停靠咱们也顺手写了：《SkPaint 深入》那篇里 MakeLinear 的 pos 数组登记每个颜色的停靠位置，咱们手上没有数组，分段就是它的手工版。咱们加一个 c2 的空位，咱们把 t 分成两段，各做一次 mix 的调用：

```text
// three evenly spaced stops: c0 at 0, c1 at 0.5, c2 at 1
half4 grad3(float t) {
    return t < 0.5 ? mix(c0, c1, t * 2.0)
                   : mix(c1, c2, (t - 0.5) * 2.0);
}
```

两段 mix 的接缝在 0.5 处是对得齐的：左段的终点是纯 c1，右段的起点也是纯 c1，颜色也就不会有跳变了。您想要不等距的停靠，改的就是分界点的位置，等停靠点多了，分段的层数也就跟着多了，写法还是原来的那套。得到的 shader 咱们也能塞给任何绘制的调用，drawRect 与 drawCircle 都是一样的，画笔的调用走到哪儿，公式就算到了哪儿。

## 示例二：距离场画圆角矩形

第二个例子咱们画圆角矩形，用的路数叫距离场。signed distance field 直译过来是带符号的距离场，白话的意思是给每个像素记一个数：落在图形里头的记负数，落在外头的记正数，绝对值就是像素到图形边界的距离。咱们有了这个数，边缘的平滑、描边的宽窄咱们就都能算了。圆角矩形的一条经典公式出自 Inigo Quilez 的二维距离函数清单，咱们照他的原式来写：

```text
uniform float2 center;
uniform float2 halfSize;   // half width, half height
uniform float4 radius;     // per-corner radii, iq's ordering
uniform half4  inColor;

float sdRoundedBox(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

half4 main(float2 p) {
    float d = sdRoundedBox(p - center, halfSize, radius);
    // 2-pixel soft edge: d<0 inside the shape, d>0 outside
    float a = 1.0 - smoothstep(-1.0, 1.0, d);
    return inColor * half4(1.0, 1.0, 1.0, a);
}
```

公式咱们不逐项去推，咱们认下结构就好：q 算的是像素到矩形四条边的距离，圆角的半径把直角的判定收成了圆角，任何一个像素算出来的 d，说明的是它落在收圆角之后的形状里还是外。头两行的条件运算是在四个角里挑对应的半径，角落的排法以 Inigo 的原页为准。

半径的取值也有个上限，咱们想想几何就知道：圆角的半径一旦超过半宽与半高里较小的那个，公式算出的形状就开始出怪样子了。您实验的时候收着点给，或者干脆照数学的直觉，咱们拿 min 把半径跟半宽半高夹一下再往里传。

咱们把公式挪进 C++ 那边也一样能写，咱们为什么要选 SkSL？因为 paint 的体系是现成的：公式造出来的 shader 直接进 paint，drawRect 照旧地画，跟渐变、位图是站在同一条队列里的。咱们不用另开一条渲染的路，这是它比自绘循环省心的地方。

smoothstep 把过零的那一小段抹成了 2 像素宽的软边，抗锯齿就这么白送了。咱们不用再去开 paint 的抗锯齿开关，形状的边缘本来就是公式按距离算出来的渐变，每个像素的透明度该是多少，d 自己就说了算了。

描边咱们也不用另画一笔。拿 d 的绝对值减去线宽的一半，得到的又是同一个距离的问题，落在 0 附近的像素就是边框上的：

```text
    // stroke band around the edge, w is half the line width
    float band = abs(d) - w;
    float a = 1.0 - smoothstep(-1.0, 1.0, band);
```

到这儿改圆角的半径、改线宽都只剩改 uniform 一件事，绘制端咱们一笔 drawRect 都不用动。代价是咱们得把形状写成数学，好在圆角矩形有现成的公式，Inigo 的清单里还躺着一大排别的形状，胶囊、菱形与五角星一类的都有，您拿它们拼界面里的图形，比一笔一笔地画路径省事。

软阴影咱们也不用另写公式。咱们拿同一个距离场再算一回，把中心往右下挪了几个像素，smoothstep 的区间放宽到十几像素，出来的就是一块模糊的底，再拿它垫在原图形的后面，投影就成了。您把两层画在同一张画布上，底下的那层就是影子，上面的那层还是原图形，一笔公式管了两层：

```text
    // soft shadow: same SDF, shifted center, wider edge
    float ds = sdRoundedBox(p - center - float2(6.0, 8.0),
                            halfSize, radius);
    float sa = (1.0 - smoothstep(0.0, 16.0, ds)) * 0.45;
    half4 shadow = half4(0.0, 0.0, 0.0, sa);
```

影子那层的模糊宽窄就是 smoothstep 的区间宽度，咱们给的是 16 像素，偏移的 6 与 8 定的是影子挪窝的方向，数值都是您随手调的参数，一改一编译您就能看见。

## 同一门手艺在外面的样子

说一句 Flutter 的事。Flutter 的 fragment shader 特性，您在 .frag 文件里写着色器、拿 FragmentProgram 加载的那一套，底下用的就是 SkRuntimeEffect 的机制，.frag 里写的语言也就是 SkSL。咱们本篇学的语法到那边是通用的，安卓与 iOS 上的 Flutter 应用一样用它，这一门手艺学会了，三个平台的活咱们都能接。

Flutter 那边有一处跟咱们不同的讲究：它的 .frag 编译是发生在构建时的，是不走运行时的，咱们这边的 MakeForShader 是程序跑起来之后的事。说的虽然是同一门语言，两家各挑了各的编译时机。

## 动手清单

咱们动手的活一共有十三处，每一处要改的都不出几行，原料都是示例里现成的代码：

1. 咱们把示例一的 clamp 换成 fract，再换成一折一返的写法，kRepeat 与 kMirror 的手感您就亲手摸到了，《SkPaint 深入》那篇的 tileMode 实验咱们在这边用公式重做了一遍。
2. 咱们把渐变的两个端点挪成竖直的，跟《SkPaint 深入》那篇 MakeLinear 的示例画在同一张画布上，两副渐变对得上了，公式也就没有推错了。
3. 咱们故意把 p1 与 p0 设成同一个点，dot(d,d) 就变成了 0，除法除以了零。您看看 SkSL 的编译器拦不拦它，不拦的话，画出来的样子您亲手验一遍。
4. 咱们把圆角 radius 的四个分量全设成 0，公式就退化成了直角矩形，再设成一个特别大的数，形状就收成了胶囊，收放的极限您也就见着了。
5. 咱们给距离场的公式加一个 uniform float2 offset，把鼠标的位置往里填，圆角矩形就跟着指针走了，运行时改 uniform 的办法您也就跑通了。
6. 咱们拿 child shader 把示例一包进去，子节点用的是渐变，外层的公式按距离场的 alpha 把它裁成圆角矩形，填充的颜色是渐变、外形是圆角框，级联的效果一下就有了。
7. 咱们把示例二的 inColor 改成带透明度的颜色，再回头看透明度的语义跟咱们预想的对不对，篇里笔者不咬死的那处，您亲手就验上了。
8. 咱们拿 SkShaders::Blend 把示例一与示例二造的两个 shader 组一个，渐变当源、圆角当蒙版的思路，《SkPaint 深入》那篇里认过的组合手段跟本篇的公式就接上了。
9. 咱们把示例二 smoothstep 的软边从 2 像素放宽到 4 像素，再收窄到 1，边缘的软硬您就看全了，抗锯齿的宽窄原来就是一个参数的事。
10. 咱们故意把 C++ 侧的 uniform 名字写错一个字母，看看 Builder 那边是什么反应，断言停住还是静默不生效，您亲手验一遍。
11. 咱们拿示例二的公式把某个角的半径单独设大，四个角各不相同的圆角您就画出来了，界面上常见的不对称圆角就是这么来的。
12. 咱们把示例一的起点 p0 每帧挪一格，渐变也就跟着动起来了，动画的口子您也就跑了一遍。
13. 咱们去 Skia 的源码树里把 src/sksl 翻开，认一认报错信息是从哪个文件里印出来的，源码看一眼，编译器的脾气也就不神秘了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. MakeForShader 吃进去的是什么，编译通过与不过各交还什么？
2. 咱们写的 SkSL 跑到硬件上时，D3D 的后端把它翻译成了什么语言？
3. half4 main(float2 p) 里，p 与返回值各装着什么？
4. Builder 年代与 SkData 年代，uniform 各是怎么填进去的？
5. child shader 在 SkSL 里怎么声明，公式里怎么取它的值？
6. 示例一的 t 是怎么算出来的，clamp 对应《SkPaint 深入》那篇 tileMode 的哪个档？
7. 距离场里每个像素记的那个数，正负与绝对值各是什么意思？
8. smoothstep 在距离场的例子里干了什么活，抗锯齿是怎么来的？
9. Flutter 的 fragment shader 底下用的是哪套机制，.frag 里写的是什么语言？
10. runtime effect 造的 shader 与工厂造的 shader，在 paint 的眼里有差别吗？
11. isOpaque 的口子在什么前提下敢报 true，报错了会怎样？
12. 同一个 effect 配两套 uniform 造两个 shader，公式要重编吗？
13. GLSL 的 gl_FragColor 在 SkSL 的 runtime shader 里对应的是什么写法？
14. sample(base) 不带坐标参数时，取的是哪个坐标上的值？
15. 三停靠的分段写法，对应 MakeLinear 的哪个参数？
16. 画布的矩形挪了位置，公式里的渐变轴会跟着走吗？
17. 圆角半径的上限由什么决定，超了会怎样？
18. 软阴影那一层的模糊宽窄，是哪个数说了算的？
19. 名字对不上的报错，是哪两边的名字没对上？
20. 画在光栅画布与 GPU 画布上，同一份 SkSL 的公式要改吗？
21. MakeForColorFilter 与 MakeForBlender 各把公式用在哪儿的？
22. 公式算出的颜色不对的时候，咱们的排查办法从什么样的公式验起？
23. child 声明了没配，报错从哪一步冒出来？
24. Flutter 的 .frag 编译发生在什么时候，跟咱们这边的差别在哪？
25. 咱们的示例画在光栅画布上，公式是在哪一侧跑的？
26. 动画每一帧要改的东西，从哪个口子递进公式？
27. 距离场公式的出处是哪一位的清单，咱们照谁的原式写的？

## 下一步

公式咱们会写了，参数咱们也会填了，缺的就是一个真的窗口。窗口的活归下一篇[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)，您从那边把整套循环领走。

咱们再往后看，索引里讲 Direct3D 的那一段还有一篇《着色器与常量缓冲》，那边讲的是 D3D 原生的 HLSL 着色器与常量缓冲，跟咱们本篇的 runtime effect 一对照，您就能看清 Skia 替咱们包掉了多少事，两边的概念也就对得上号了。

到《实战：Win32 窗口跑 Skia 渲染循环》那边咱们建 D3D12 的设备与交换链，把[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)里认下的对象流一组一组地装起来，本篇的 runtime shader 也会跟着搬进 GPU 的画布。咱们的公式还是同一份，算它的从 CPU 的一个核，换成了显卡上的几千个核。
