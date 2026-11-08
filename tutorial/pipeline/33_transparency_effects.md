---
title: 透明、阴影、模糊
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 透明、阴影、模糊

[上一篇《图层与裁剪》](32_layers_clipping.md)收尾的时候，咱们把话头交到了本篇手里：`opacityBrush` 只调了透明度的图案，透明与混合的整套数学没有展开，垫在圆角照片卡底下那团影子也只当了回配角。本篇就把这套数学和那团影子一起接住了，咱们的主线很短：算清像素怎么叠，再看特效怎么来。合成的算式咱们从头算一遍，D2D 的效果路线接着[《特效：Effect 图》](11_d2d_effects.md)往下补，系统的玻璃材质与逐像素透明的老路（分层窗口）各占一节，末了把半透明卡片加投影的骨架拼出来。

事实的底本也交代一句。本篇的数学与接口行为，咱们对着微软的公开文档写：Composite 的档位清单与公式、颜色矩阵的排法、ClearType 的退回条文、系统材质的枚举，都照文档的原文落。整篇的代码是讲解用的骨架，咱们没有在仓库里编译过，您拿去用之前请对照 SDK 的签名。读者的设定照旧，您会 C++ 和 Win32 的基础用法就够，设备的接线借[《特效：Effect 图》](11_d2d_effects.md)，上一篇的家当咱们直接拿来用。

## over：透明合成的算式

[《DWM：重定向表面与窗口合成》](30_dwm_composition.md)讲叠合的时候，咱们见过 over 公式的一句话版本：结果等于上层的像素，加上下层像素乘以透明度的补。咱们把它请回正位，从头算清它的每一项。白话的版本，[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)（07 篇）给过：alpha 管的是不透明度，1.0 的时候全遮，0.0 的时候全透。咱们把上下两层各叫源与背景，源的不透明度记作 α，单个通道的合成就是：

```text
结果 = 源 × α + 背景 × (1 − α)
```

咱们拿数字走一遍：源给全白的 (1、1、1)，α 给的是 0.6，背景是深蓝的 (0.1、0.2、0.4)，三个通道算下来的结果分别是 0.64、0.68 与 0.76，屏幕上是一块掺了四成深蓝的浅灰蓝。07 篇画过 0.35 alpha 的高光线，[《位图与离屏渲染》](10_d2d_bitmaps.md)给 `DrawBitmap` 的 opacity 打过折，用的都是同一条式子，落的位置不同罢了。

咱们别忘了 alpha 通道自己，它跟着也合了一次。结果的 α 等于源的 α 加上底的 α 乘 (1 − 源的 α)，两片各 0.5 的半透明叠起来，叠出来的结果却是 0.75。07 篇数过颜色与画刷的乘法链，这边是叠放时的另一条链，两条规则各管各的关口，咱们别把它们混成一件事。

## 预乘：把乘法提前放进像素

预乘 alpha（premultiplied alpha）这个词咱们已经打过两回照面：10 篇把像素归一到 PBGRA 的时候提过，30 篇讲 DWM 手里的表面时又提过，两处都留了话没展开，兑现的活儿归本篇。咱们再往前追一笔，07 篇造棋盘格的时候选了 `D2D1_ALPHA_MODE_IGNORE`，因为内容完全不需要透明的参与，alpha 模式那道选题其实从那时就摆在了咱们面前。

咱们拿 60% 不透明的纯红看两种记法。原样存的记法叫 straight alpha，存的是本色，四个分量写的是 (1.0、0、0、0.6)。预乘之后的四个分量变成 (0.6、0、0、0.6)，RGB 与 α 是一起缩的，α 本身的数值两种记法是一样的。

预乘的第一样好处是合成变便宜。源的那次乘法已经做进像素里了，咱们合成的时候 over 式变成：

```text
结果 = 源′ + 背景′ × (1 − α)
```

源的那一项从乘了再算，变成了直接的加法。GPU 的混合单元每个像素都要过一遍它，咱们省下的是一次乘法，攒起来就是整屏的乘法。11 篇认过的 Composite 效果，微软文档给它的公式写的正是预乘的形态（S 是源、D 是背景），两个值都当预乘的用。

第二样的好处是边缘不发黑，咱们把机制推一遍。带透明通道的图片，内容边缘外的那圈像素常常是 (0、0、0、0)，颜色分量是黑的、α 是零。GPU 采样要做双线性插值（拿邻近像素加权平均的采样方式，10 篇 LINEAR 档说的就是它），插值在 straight 的世界里会把黑色掺进边缘的 RGB，α 却只淡了一半，叠上去边缘就罩了一圈深色。预乘之后 α 为零的像素 RGB 本来就是零，插值与合成怎么排顺序，结果都是一致的，深晕就没了。咱们直读一张带透明通道的 PNG，圆角边上那圈暗边的病根多半在这里，10 篇 WIC（Windows 自带的图片解码组件，负责把 PNG 解成像素）归一到 PBGRA 的那一步，等于提前把药吃上了。

还有一处文档写明的事，咱们记下就省得自己动手乘：`D2D1_COLOR_F` 的颜色永远是 straight 口径，`D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.6f)` 的四个分量按本色给，落笔的时候 Direct2D 自己按目标格式换算，咱们写画刷的颜色不用管预乘。

alpha 模式的三个取值，咱们在这里一并收拢。`IGNORE` 的意思是不管 alpha，07 篇的棋盘格是现成的例子，`PREMULTIPLIED` 是 D2D 内部位的默认偏好，B8G8R8A8 的目标文档里只认 PREMULTIPLIED 与 IGNORE 两档，`STRAIGHT` 只出现在 A8 单通道那一类的格式里。选格式的判断其实也简单：要透明的目标一律给 `PREMULTIPLIED`，完全不透明的图用 `IGNORE` 省事。

## Composite：十三档叠法

11 篇认 Composite 的时候咱们只用了默认的 `D2D1_COMPOSITE_MODE_SOURCE_OVER`，剩下的十二档，本篇的清单一并补上。头一件事还是输入的次序，两张输入的文档写法是：索引 0 当的是 destination（底层），索引 1 当的是 source（顶层），指定更多张的时候按输入的顺序依次叠。换档的属性是 `D2D1_COMPOSITE_PROP_MODE`，十三档咱们列全，公式咱们照文档原样转录，用的全部是预乘的口径，S 是源、D 是背景、SA 与 DA 是各自的 α、O 是输出的结果：

| 档位 | 公式 | 输出范围 | 白话 |
|:---|:---|:---|:---|
| `SOURCE_OVER` | O = S + (1 − SA) × D | 并集 | 默认档，源按透明度盖上去 |
| `DESTINATION_OVER` | O = (1 − DA) × S + D | 并集 | 反过来，背景盖住源 |
| `SOURCE_IN` | O = DA × S | 交集 | 只留背景不透明处的源 |
| `DESTINATION_IN` | O = SA × D | 交集 | 反向的镂空，留源范围内的背景 |
| `SOURCE_OUT` | O = (1 − DA) × S | 源区域 | 只留背景范围之外的源 |
| `DESTINATION_OUT` | O = (1 − SA) × D | 背景区域 | 挖掉源的范围，留剩下的背景 |
| `SOURCE_ATOP` | O = DA × S + (1 − SA) × D | 背景区域 | 源贴进背景的范围内，范围外仍是背景 |
| `DESTINATION_ATOP` | O = (1 − DA) × S + SA × D | 源区域 | 反向的贴入 |
| `XOR` | O = (1 − DA) × S + (1 − SA) × D | 并集 | 交叠处两边都挖掉 |
| `PLUS` | O = S + D | 并集 | 相加变亮，发光叠加的路子 |
| `SOURCE_COPY` | O = S | 源区域 | 只留源，背景整个扔掉 |
| `BOUNDED_SOURCE_COPY` | O = S（源存在处） | 并集 | 源的范围之外保留背景 |
| `MASK_INVERT` | O = (1 − D) × S + (1 − SA) × D | 并集 | 拿源当反向的蒙版 |

咱们日常用得最勤的还是头一档，眼睛看到的半透明盖上去就是它。`MASK_INVERT` 这一档值得咱们多看一眼：它拿源当反向的蒙版，背景里源越实的地方透出来越少，做擦除类的效果时它比两档 IN 直观。换档的写法就是一句 `SetValue`：

```cpp
pComposite->SetValue(D2D1_COMPOSITE_PROP_MODE,
                     D2D1_COMPOSITE_MODE_MASK_INVERT);
```

整张表还有一处值得咱们停一停：输出范围的那一列。交并的范围直接决定下游节点的画面尺寸，11 篇讲过 Shadow 的输出会按模糊量外扩，Composite 的范围规则是同一类的尺寸约定，链在一起的效果图，尺寸对得上了才不裁内容。

## 颜色矩阵：给整张图调透明度

11 篇咱们拿颜色矩阵做过灰度化，矩阵的行是输入通道、列是输出通道。透明度走的还是第四行：输入的 α 乘一个系数送进输出的 α，其余的三行保持原样，整张图的透明度就统一打折了。咱们把骨架摆出来：

```cpp
// Scale alpha to 60%, colors untouched - skeleton, not compiled here.
// Rows are input channels (R, G, B, A), columns are outputs; the
// fifth row carries the per-channel offsets.
D2D1_MATRIX_5X4_F fade = D2D1::Matrix5x4F(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.6f,
    0.0f, 0.0f, 0.0f, 0.0f);
pMatrix->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, fade);
```

它跟 32 篇讲过的两个透明入口管的事不一样。图层的 opacity 与 `opacityBrush` 是绘制侧的参数，作用域就是这一段图层的绘制，`PopLayer` 一调、这段就收了工。颜色矩阵是像素级的加工，挂在 Effect 图的流水线里，位图进来什么样、出去什么样，全由矩阵说了算，串链的路子它也认，Shadow、Blur 都能接在它的后面。咱们要的是一层内容整体淡出，走的是图层的入口，要的是素材本身带着六成透明度进流水线，交给的就是矩阵。

## Mica 与 Acrylic：系统替咱们画的玻璃

前面几节的效果，全是咱们自己的绘制在算。Windows 11 上还有一批效果住在系统的合成器里，Mica 与 Acrylic 是里面最常见的两样材质（材质是微软设计文档里的词，指系统预备好的背景质感）。分工咱们一句讲清：材质本身的活计，取样、模糊、着色全都归系统的合成器来画，应用这边的活只是挑材质，挑选的动作就是一次 `DwmSetWindowAttribute` 调用。

属性的编号叫 `DWMWA_SYSTEMBACKDROP_TYPE`，文档标明的起点是 Windows 11 Build 22621，`pvAttribute` 指向的是一个 `DWM_SYSTEMBACKDROP_TYPE` 枚举值，枚举咱们照文档列全：

| 取值 | 对应的材质 |
|:---|:---|
| `DWMSBT_AUTO` | 默认值，DWM 自己决定，且只垫在默认标题栏后面，还可能干脆不画 |
| `DWMSBT_NONE` | 不垫任何系统材质 |
| `DWMSBT_MAINWINDOW` | Mica，长驻窗口的材质 |
| `DWMSBT_TRANSIENTWINDOW` | Desktop Acrylic，短暂弹出窗口的材质 |
| `DWMSBT_TABBEDWINDOW` | Mica 的深色变体 Mica Alt，配带标签页的标题栏 |

调用的骨架一行就够，返回值咱们按老习惯判一下再信任：

```cpp
// Pick the system backdrop for a plain Win32 window - skeleton,
// not compiled here. Requires Windows 11 build 22621.
DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                   &backdrop, sizeof(backdrop));
// Failure means the build is older than 22621 or the attribute is
// unsupported: fall back to a plain opaque window.
```

两种材质的脾气，笔者在设计文档里能查到明白话。Mica 是不透明的，掺的是用户的主题与桌面壁纸，文档特意写明了它只取样壁纸一次，省的是性能。Acrylic 走的是半透明的路子。

设计文档还有一条配套的建议：想让材质露出来的层，背景都得是透明的，客户区里咱们画得不透明的东西会把它盖住。30 篇的合成模型正好帮咱们把这句话读懂：材质垫在窗口内容的后面，咱们的像素在上、材质在底下。

效果住在了系统那侧，开销也就由 DWM 的进程承担，材质的模糊与取样都是它在花工夫，花的正是合成那一程的工夫。30 篇讲过的减法道理（层堆得越多、合成的活就越重，少一层就省一层的合成）在这里照样适用。咱们真要量它的开销，用的家什归[《性能观测：PIX、RenderDoc、ETW》](36_performance_tools.md)一篇去讲。

## 分层窗口：逐像素透明的老路

它的来历咱们在 30 篇里讲过：Windows 2000 引入的 `WS_EX_LAYERED`，在没有合成器的年代，它是普通窗口拿到逐像素透明的唯一通道。DWM 来了之后所有窗口都过上了离屏照片的日子，老路的特殊地位淡了，路本身倒是还在，文档也没有作废它的意思，Windows 8 起连子窗口也能挂这个样式位了。

两个入口各管一摊的活。`SetLayeredWindowAttributes` 是便宜的一档：`LWA_ALPHA` 给整窗一个统一的不透明度，`LWA_COLORKEY` 拿一种颜色当透明的标记（色键的思路就是拿颜色当透明的暗号），窗口的绘制代码一行不用改，系统在合成时替咱们打折。`UpdateLayeredWindow` 是全接管的一档：咱们把一张位图整个交给系统，位图每个像素的 α，就是窗口每个像素的透明度。

文档里的几条硬性要求，咱们照录。窗口挂上了 `WS_EX_LAYERED` 之后，两个入口一个都没调的时候，它在屏幕上是不显示的。文档还不许咱们混用两个入口：调过 `SetLayeredWindowAttributes` 之后，`UpdateLayeredWindow` 的调用会一直失败，咱们得把样式位摘下来再挂回去，才换得过去新的路。命中测试（判断点击落在谁身上的机制）跟着透明度走，α 为零的区域，鼠标点击直接穿到底下的窗口。

`UpdateLayeredWindow` 这边还有自己的要求，头一条就接上了 10 篇。源 DC 里装的是一块 32 位的 DIB（设备无关位图的缩写），像素必须是预乘的 BGRA，`BLENDFUNCTION` 结构 `AC_SRC_ALPHA` 标志的文档原话写的就是：红绿蓝必须乘过各自的 α 再交给它。10 篇离屏目标攒下的预乘像素，它的去处之一正是这里。还有一条脾气值得咱们记下：它更新的永远是整窗，局部的刷新它不做，文档给的建议是局部的活走传统 `WM_PAINT`，配 `SetLayeredWindowAttributes` 的路子。

老路的形状，参照[《DirectComposition：visual 树》](31_directcomposition.md)立过的模型最好讲清：一整张位图换一个窗口，内容变了就得整张重交，窗口的层只有一个。DComp 那边走的是 visual 树，层是树上的节点，挪一层的开销只算一层。老路的好处在直接，一张位图就是全部的心智负担，通知窗、悬浮球一类的小窗口，今天咱们拿它做的仍然是正路。

咱们要是连 `WS_EX_LAYERED` 都不想挂，普通窗口从 Windows 11 Build 26100 起也有一条逐像素透明的路：`DWMWA_REDIRECTIONBITMAP_ALPHA` 让重定向表面直接收预乘的 α，普通窗口加 D2D 直画的组合也能逐像素透，文档标明这个属性默认是关着的。

## 透明背景上的文字：ClearType 自己退回

往透明的目标上写字，有一处现象您多半见过：同样的一行字，画在不透明的窗口上，边缘是利落的，放大了看带一点彩边，画到透明的位图上，彩边没了，字也整体发灰发虚了。咱们没画错，换算法的是 D2D 自己。

彩边的来历叫 ClearType，是 Windows 的亚像素文字抗锯齿：LCD 的一个像素实际是红绿蓝三条竖排的子像素，ClearType 把它们当三格独立的亮度用，横向的精度直接翻三倍。同款咱们在 15 篇[《Skia 的文本：SkFont 与 TextBlob》](15_skia_text.md)里见过，`kSubpixelAntiAlias` 档说的就是它。它有个隐含的前提：算出来的子像素亮度要直接混进底下的颜色，底的色值得的就是最终上屏的色值。不透明的目标满足这个前提，透明的目标就满足不了，咱们画上去的每个像素将来还要跟桌面再叠一遍，子像素的配比没有稳定的底可混。

文档把行为写死了：渲染目标的 alpha 模式不是 `D2D1_ALPHA_MODE_IGNORE` 的时候，文字抗锯齿自动从 `D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE` 退回灰度的 `GRAYSCALE` 档，灰度只动 α 的数值，跟透明目标是相容的。咱们硬调 `SetTextAntialiasMode` 改回 ClearType 也调得动，文档的措辞是结果不可预期，不建议咱们这么干。文档给的正路有两条：要 ClearType 就给文字垫不透明的底，拿 `PushAxisAlignedClip` 把写字的地方圈出来，配一句 `Clear` 铺上不透明的颜色，或者拿 `DrawRectangle` 画一块不透明的底再写字。32 篇的层也过同样的关卡。那篇认 `layerOptions1` 的时候，对未知的文本模式只交代了一句名目，道理留到了这里：不带 `D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE` 标志的层，里面的文字同样退灰度。

实践里的取法也简单，透明卡片上的字咱们直接接受灰度，边缘的锐度拿字号与字重找补。真要 ClearType 的标题，就给它垫一块不透明的背景条，观感与开销怎么取舍，您按自己的界面定。

## 阴影的两种来路

标题里的阴影，咱们把两条来路分开记。一条是 Effect 自己算的：`CLSID_D2D1Shadow` 吃的是一张剪影位图，交出一张模糊过的影子图，11 篇拼过它的 Effect 图，32 篇圆角卡垫的就是它。影子的软硬由 `D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION` 管，颜色走的入口是 `D2D1_SHADOW_PROP_COLOR`，输出的外扩量 11 篇给过估法。它的特点是跟内容走：卡片挪到哪，影子就跟到了哪，系统对它一概是不知情的。

另一条来路归了系统。窗口边框四周的投影是 DWM 在合成时加的装饰，30 篇讲边框与阴影时点过它的管辖，它的投影跟着窗口的样式走：带标题栏的普通窗口自带一份，无边框的自绘窗口就没有了。所以分层窗口、无边框窗口的投影，多半得咱们自己拿 Shadow 效果补，示例里咱们正要这么干。模糊的另一半主角 GaussianBlur，咱们在 11 篇已经认全了它，系统材质里的模糊跟它是同一门数学，算的人换成了 DWM 而已。

## 动手骨架：透明窗口上的半透明卡片

咱们把本篇的家当拼起来：一个 `WS_EX_LAYERED` 的无边框窗口，一张半透明的圆角卡片，底下垫一团柔和的投影，上面写的一行字。整段代码是讲解用的骨架，咱们没有在仓库里编译过，您拿去跑之前请对照 SDK 的签名。

头一段咱们备窗口与位图。窗口的样式用 `WS_POPUP`，扩展样式带上的是 `WS_EX_LAYERED`，窗口的尺寸跟位图保持一致：

```cpp
// Skeleton for illustration - not compiled in this repo.
constexpr int kWidth  = 360;
constexpr int kHeight = 240;

HWND hwnd = CreateWindowExW(
    WS_EX_LAYERED, kClassName, L"Anatomy GUI · Layered",
    WS_POPUP, 120, 120, kWidth, kHeight,
    nullptr, nullptr, hInstance, nullptr);

// A 32-bpp top-down DIB. The byte order D2D writes (B, G, R, A)
// matches what UpdateLayeredWindow reads, premultiplied.
BITMAPINFO bmi{};
bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
bmi.bmiHeader.biWidth       = kWidth;
bmi.bmiHeader.biHeight      = -kHeight;   // negative: top-down rows
bmi.bmiHeader.biPlanes      = 1;
bmi.bmiHeader.biBitCount    = 32;
bmi.bmiHeader.biCompression = BI_RGB;

void*   dib_bits = nullptr;
HDC     mem_dc   = CreateCompatibleDC(nullptr);
HBITMAP dib      = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
                                    &dib_bits, nullptr, 0);
SelectObject(mem_dc, dib);
```

第二段咱们画内容。设备的接线全借[《特效：Effect 图》](11_d2d_effects.md)的链，只是交换链咱们用不上：画面走的不是窗口呈现，画进一块能当渲染目标的位图（`D2D1_BITMAP_OPTIONS_TARGET`）就收了工。剪影位图的造法直接借 32 篇 `CreateCardAssets` 的那一套、尺寸按本篇的卡片改，文字的 `IDWriteTextFormat` 走 21 篇[《DirectWrite：TextLayout 全解》](21_directwrite_layout.md)里新建文字格式的那套做法：

```cpp
// Segment 2: paint into a plain TARGET bitmap - not compiled here.
// Device chain from the effects tutorial; no swap chain this time.
ComPtr<ID2D1Bitmap1> canvas;
D2D1_BITMAP_PROPERTIES1 target_props = D2D1::BitmapProperties1(
    D2D1_BITMAP_OPTIONS_TARGET,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                      D2D1_ALPHA_MODE_PREMULTIPLIED));
g_pDC->CreateBitmap(D2D1::SizeU(kWidth, kHeight), nullptr, 0,
                    target_props, &canvas);
g_pDC->SetTarget(canvas.Get());

g_pDC->BeginDraw();
g_pDC->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));  // transparent start

// Soft drop shadow from the rounded silhouette (article 32).
// The card sits at (24,24); the blur-expanded shadow lands at
// card origin nudged +12 right and +16 down.
ComPtr<ID2D1Effect> shadow;
g_pDC->CreateEffect(CLSID_D2D1Shadow, &shadow);
shadow->SetInput(0, g_pSilhouette.Get());
shadow->SetValue(D2D1_SHADOW_PROP_COLOR,
                 D2D1::Vector4F(0.0f, 0.0f, 0.0f, 0.45f));
shadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, 8.0f);
g_pDC->DrawImage(shadow.Get(), D2D1::Point2F(36.0f, 40.0f));
// shadow and its graph die with the smart pointer at scope end,
// article 11's per-frame create-and-release habit

// One brush, two jobs (article 07's SetColor habit):
// translucent card fill, then the text color.
ComPtr<ID2D1SolidColorBrush> brush;
g_pDC->CreateSolidColorBrush(
    D2D1::ColorF(0.16f, 0.19f, 0.26f, 0.88f), &brush);
g_pDC->FillRoundedRectangle(
    D2D1::RoundedRect(D2D1::RectF(24.0f, 24.0f,
                                  kWidth - 24.0f, kHeight - 24.0f),
                      16.0f, 16.0f),
    brush.Get());

brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
g_pDC->DrawTextW(L"透明卡片", 4, g_pTextFormat.Get(),
                 D2D1::RectF(48.0f, 48.0f,
                             kWidth - 48.0f, kHeight - 48.0f),
                 brush.Get());
g_pDC->EndDraw();
```

透明卡片、投影、灰度的字，全凭位图里的 α 一层布好，窗口的样子就是位图的样子。文字那边咱们一行抗锯齿的代码都没写，退回灰度是 D2D 自己办的事，前面退回一节的条文在骨架里正好对上号。

第三段咱们把像素搬出去、交给系统。TARGET 位图 CPU 读不了，文档写死了 `CPU_READ` 那一档的搭配：它的搭档只能是 `CANNOT_DRAW`，再搭别的标志都是不行的，进它的路只有 `CopyFromBitmap` 与 `CopyFromRenderTarget`。咱们照办，拿一块中转的位图再 `Map`：

```cpp
// Segment 3: stage, map, copy, hand over - not compiled here.
ComPtr<ID2D1Bitmap1> staging;
D2D1_BITMAP_PROPERTIES1 staging_props = D2D1::BitmapProperties1(
    D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                      D2D1_ALPHA_MODE_PREMULTIPLIED));
g_pDC->CreateBitmap(D2D1::SizeU(kWidth, kHeight), nullptr, 0,
                    staging_props, &staging);
staging->CopyFromBitmap(nullptr, canvas.Get(), nullptr);

// Row by row: Map's pitch is not guaranteed to be width * 4.
D2D1_MAPPED_RECT mapped{};
staging->Map(D2D1_MAP_OPTIONS_READ, &mapped);
for (UINT y = 0; y < static_cast<UINT>(kHeight); ++y) {
    memcpy(static_cast<BYTE*>(dib_bits) + y * kWidth * 4,
           mapped.bits + y * mapped.pitch, kWidth * 4);
}
staging->Unmap();

// Premultiplied BGRA on both sides: byte for byte, no conversion.
BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
POINT source{0, 0};
SIZE  layer{static_cast<LONG>(kWidth), static_cast<LONG>(kHeight)};
UpdateLayeredWindow(hwnd, nullptr, nullptr, &layer, mem_dc, &source,
                    0, &blend, ULW_ALPHA);
```

末尾的调用咱们把参数对一遍位：`hdcSrc` 是装着 DIB 的内存 DC，`pptSrc` 给位图的零点，`psize` 是窗口的新尺寸，`ULW_ALPHA` 配上 `AC_SRC_ALPHA` 与 `SourceConstantAlpha` 的 255，意思是逐像素的 α 全权做主。内容变了就重走一遍第三段，老路的整窗更新说的就是这个节奏。设备丢失的处置沿 11 篇的名单办，从 D3D 设备到 device context 的家当全部重建，位图也跟着重画了一遍。

## 您可以动手的三处

您要是把骨架补成了能跑的版本，下面三处都是改一行、编一遍、看一眼的事：

1. 咱们把 `SourceConstantAlpha` 从 255 改成 128，整窗的透明度再打一次对折，卡片、影子、字就一起变淡了。文档写明两层的次序：位图按这个常量做了预乘，再按逐像素的 α 混，0.88 的卡片最终算下来是 0.88 乘 128 再除 255。
2. 咱们把卡片的矩形挪小，让位图底边露出一段 α 全零的空白，咱们把鼠标点在那块空白上，点击会穿到底下的窗口。命中测试跟着 α 走的章法，手上一试就实了。
3. 咱们把卡片的填充从 0.88 改到 1.0，再把文字的字号加上两档，灰度抗锯齿下的字怎么靠字号字重找补锐度，眼睛会给您答案。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. over 算式逐通道怎么算？结果的 α 又怎么算？
2. 预乘把合成的哪一步省掉了？直读 PNG 的暗边，病根在存储还是在采样？
3. `D2D1_COLOR_F` 的颜色咱们按哪个口径填？换算由谁做？
4. Composite 的输入索引 0 与 1 各是什么位置？十三档里换档的属性叫什么？
5. 颜色矩阵调透明度动的是矩阵的哪一行？它与图层 opacity 的边界在哪？
6. `DWMWA_SYSTEMBACKDROP_TYPE` 从哪个版本起可用？`DWMSBT_AUTO` 与具体取值的差别是什么？
7. `SetLayeredWindowAttributes` 与 `UpdateLayeredWindow` 各管什么？混用会怎样？
8. 透明目标上的文字为什么自动退灰度？想要 ClearType 有哪两条正路？
9. 系统给窗口的投影归谁管？无边框窗口的投影从哪来？

## 下一步

透明度的另一头是动：淡入淡出挂的正是 31 篇认过的效果组 `SetOpacity`，它跟 visual 的 OffsetX 一样有接 `IDCompositionAnimation` 曲线的动画重载，挂上之后每一拍的插值归 DWM，咱们的进程一帧都不欠。曲线怎么挂、一帧的预算怎么花，再加上节拍器拿什么当，都是下一篇[《动画与帧调度》](34_animation_frames.md)的正题，咱们移步过去。本篇的透明窗口要是想整体淡入，老路的 `UpdateLayeredWindow` 这边没有曲线可挂，咱们得一拍一拍自己喂 α。
