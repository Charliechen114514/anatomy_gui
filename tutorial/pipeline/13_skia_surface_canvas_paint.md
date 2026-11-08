---
title: Surface、Canvas、Paint 三件套
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# Surface、Canvas、Paint 三件套

上一篇[《与 GDI 互操作》](12_d2d_interop.md)收尾的时候咱们预告过，下一段要换一套全新的 2D 库。Direct2D 的家当咱们从[工厂、设备与渲染目标](06_d2d_targets.md)一路认到了互操作，整整认了七篇，可它再顺手也是微软一家的东西，出了 Windows 就没有它了。本篇咱们跨出门去认 Skia：它是谁、谁在用它，以及它的对象模型三件套，也就是 SkSurface、SkCanvas、SkPaint 的分工。您会看出一件有意思的事，这三件套跟 D2D 的目标、画布、画刷几乎是面对面的座位对调，认亲的活不重，咱们慢慢来。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够，渲染零基础的您也照样跟得上。本篇对照的底子是[《工厂、设备与渲染目标》](06_d2d_targets.md)那篇的对象模型，外加[开篇](01_status_to_pixels.md)立过的那个问题：怎么把咱们这一侧的表面填得又快又好。

## Skia 是什么，谁在用

咱们把主角的身世交代清楚。Skia 是一个 C++ 写的二维绘图库，Google 是它的主导维护方，用的是 BSD 开源许可。咱们在 Windows、macOS、Linux、Android、iOS 上都能编它，离题的移植细节咱们不讲，认它一个跨平台的名分就好。它跟窗口系统是解耦的，也不认哪家 GPU 的门牌，咱们给它一块普通内存，它就把像素都算进了内存，咱们给它一块 GPU 表面，活就交给了显卡。本篇咱们只走内存的这一边，GPU 的部分留给[后篇](16_skia_gpu_backend.md)。

谁在用它？Chrome 浏览器的页面与界面绘制走的是 Skia，Flutter 框架的全部画面跑在 Skia 上，Android 的系统绘制里也到处是它。三家都在自己的官方文档里写过自己用 Skia，咱们认的是这些公开的说法，没有挨个翻三家的源码去核实，您要是较真，从各家的工程文档入手能查到同样的表述。

它跟 D2D 最大的不同在分发方式上。D2D 是 Windows 的系统组件，操作系统替咱们备好了，咱们链接 `d2d1.lib` 就能用上它。Skia 却是一套纯源码的库，没有随系统分发的运行库，咱们得把它编出来、接进自己的工程。编 Skia 有官方的 gn 与 ninja 工具链，包管理器里也有现成的包，具体的构建步骤本篇不展开，您按 Skia 仓库自带的构建说明或包管理器的版本走就好，本篇只把代码按头文件的真名写全。

还有一件名词上的事，咱们放在开头说定。Skia 不发带版本号的安装包，只有一路向前的主线源码，API 的名字这几年挪过几回家。本篇以 2024 年前后的主线为基准，旧名字长什么样，等代码露脸的时候咱们对着认。

## SkSurface：像素住在哪儿

开篇咱们说过，整条渲染管线讲到底回答的都是同一个问题：怎么把咱们这一侧的表面填得又快又好，头一个要落定的就是画在哪。GDI 的答案是 HDC，D2D 的答案是渲染目标，Skia 的答案叫 surface，中文的名字就叫表面，白话说就是一块能画的像素容器，装它的两条路数咱们在本篇开头已经交代过了。咱们本篇造的是头一种，它有个专门的名字叫 raster，中文的名字叫光栅，光栅化说的就是把“一个圆”这样的形状描述，算成一块一块真实像素的过程。

咱们要造一块 CPU 的表面，要交给 Skia 的信息有两样：多大，什么样的格式。您看代码：

```cpp
#include "include/core/SkSurface.h"

SkImageInfo info = SkImageInfo::Make(
    400, 300,                       // width, height in pixels
    kN32_SkColorType,               // 32-bit color, byte order follows the platform
    kPremul_SkAlphaType);           // alpha is premultiplied into the channels

sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
```

`SkImageInfo` 打包的就是那两样事。宽和高的部分好说，量的单位是物理像素。`kN32_SkColorType` 的意思是每个像素 32 位，字节序是跟着平台走的，在 Windows 上就是 BGRA 的排法，跟咱们在互操作篇认过的 `DXGI_FORMAT_B8G8R8A8_UNORM` 是同一种内存布局。`kPremul_SkAlphaType` 声明透明度按预乘的方式存，预乘 alpha 的白话互操作篇讲过：颜色分量各自乘上透明度之后再存进内存，合成的时候算得快。Skia 的表面默认就认它，咱们照着写就好。

`SkSurfaces::Raster` 交回来的是一个 `sk_sp<SkSurface>`。sk_sp 是 Skia 自家带引用计数的智能指针，用法跟咱们熟悉的 `std::shared_ptr` 是一个手感，析构的时候引用减一，减到零的时候对象自己销毁。它的更多脾气下一篇还会专门说，本篇您只要带上一件事：拿到了 sk_sp，咱们就不用再手工管释放，D2D 那边 `Release` 的功课，在 Skia 这边交给了指针自己。

旧名字长什么样，咱们对着代码认。老代码里同样的活是这样写的：

```cpp
// pre-migration spelling, still found in older code and docs
sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(400, 300);
```

它收的是宽和高，内部替您拼一个 N32 预乘的 `SkImageInfo`，语义跟新写法是一样的，只是工厂函数的住处搬了家：从类身上的静态方法，搬进了同名的命名空间。咱们读老资料的时候把名字对上号就行，写新代码咱们用 `SkSurfaces::Raster`。

## SkCanvas：所有 draw 调用挂在它身上

表面有了，往上面落笔的家伙叫 `SkCanvas`，咱们叫它画布。它不用咱们创建，每个 surface 出生的时候就自带一块画布，咱们伸手去要：

```cpp
SkCanvas* canvas = surface->getCanvas();
canvas->clear(SK_ColorWHITE);
```

`getCanvas` 交回来的是裸指针，咱们不用对它计引用，因为画布的寿命归 surface 管，surface 活着的时候它就活着。上面那句 `clear` 咱们已经用上了：把整块画布刷成白色，对位的就是 D2D 里 `Clear` 的那一步。

绘制的调用全是 `draw` 开头的，它们统统都挂在画布的身上。咱们把最常用的四个摆出来认脸：

```cpp
canvas->drawRect(SkRect::MakeXYWH(40, 40, 180, 120), paint);  // rectangle
canvas->drawCircle(300, 100, 56, paint);                      // circle: cx, cy, radius
canvas->drawPath(path, paint);                                // arbitrary shapes
canvas->drawPaint(paint);                                     // no shape: paint everything
```

头三个咱们都好懂，画的是矩形、圆，外加任意的路径，坐标用的全是浮点。`drawPath` 收的 `SkPath` 是 Skia 的路径对象，跟咱们在[几何篇](08_d2d_geometry.md)认过的 `ID2D1PathGeometry` 对得上号，形状是靠 `moveTo`、`lineTo`、`close` 拼出来的，本篇示例里咱们会亲手拼一条。第四个 `drawPaint` 值得咱们单独说一句：它连形状的概念都没有，拿 paint 把整块画布（准确说是裁剪区里的画布）整个涂一遍，相当于咱们拿样式当滚刷刷了整面墙。D2D 那边没有直接的对位，最接近的姿态是咱们拿纯色 `Clear` 刷整面，不过 Clear 只认一种颜色，滚刷里装了渐变它就刷不出来了。

您要是拿它跟 D2D 对照，会发现座位换了一处。D2D 的绘制调用挂在渲染目标身上，`FillRectangle` 是目标自己的方法。Skia 把画布从表面里分了出来：surface 管像素住在哪儿，canvas 管怎么画，分工更细了一层。不过分了也不散，画布还是 surface 出厂自带的那一块，两个对象本来是一体的，只是接口上分了两个名字。

## SkPaint：样式塞在参数袋里

每个 draw 调用的末位都收一个 `SkPaint`，它就是本篇的第三件套。您把它想成一个参数袋：颜色、抗锯齿开关、描边还是填充、线宽，画的时候要用的样式全装在袋子里，画的时候跟着传过去，画完了也不留状态。咱们从最朴素的一笔看起：

```cpp
SkPaint paint;
paint.setAntiAlias(true);       // AA is OFF by default in Skia
paint.setColor(0xFF2D7DFA);     // ARGB packed into one 32-bit integer
canvas->drawRect(SkRect::MakeXYWH(40, 40, 180, 120), paint);
```

`setColor` 收的 `SkColor` 是把 ARGB 四个 8 位分量打包成的一个 32 位整数，`0xFF` 打头的头两位就是完全不透明的 alpha，`SK_ColorWHITE` 一类常量是它的预定义值，咱们用起来跟 GDI 的 `COLORREF` 是一个手感，只是通道的排布不同。讲究的写法还有 `setColor4f`，收的是四个浮点分量，语义对位 D2D 的 `D2D1_COLOR_F`，那一套咱们下一篇展开。

抗锯齿那一行请您多看一眼。D2D 的绘制默认就是抗锯齿的，Skia 却把默认设成了关，咱们想要画面平滑，咱们就得自己把开关拨上来，`setAntiAlias(true)`。您要是忘了拨，圆的边缘会现出锯齿，文末《您可以动手的三处》的第 1 处就是看它。

描边与填充的切换也在 paint 身上，咱们看同一支笔换两副装束：

```cpp
paint.setColor(0xFFE8590C);
paint.setStyle(SkPaint::kStroke_Style);   // outline instead of fill
paint.setStrokeWidth(6.0f);               // stroke width in geometry units
canvas->drawCircle(300, 100, 56, paint);

paint.setStyle(SkPaint::kFill_Style);     // back to fill
```

`setStyle` 切换的是描边（kStroke）与填充（kFill），`setStrokeWidth` 管描边的粗细，量的是几何单位，画布不带变换的时候就是像素，画布缩放起来它也就跟着变了。D2D 那边描边走的是 `DrawRectangle` 一类的独立方法，画刷管填充、独立的方法管描边。Skia 把它们合进了同一个 draw 调用，画成什么样由 paint 说了算，这正是咱们开头说的座位对调：D2D 把样式塞进了画刷对象，Skia 把样式塞进了 paint 参数袋，画布只管落笔的位置。

## save 与 restore：矩阵和裁剪一起入栈

画布身上会跨调用留下来的有两样，就是当前的矩阵和裁剪区。`translate`、`rotate`、`scale` 改的是矩阵，画布的坐标系跟着挪。`clipRect`、`clipPath` 改的是裁剪区，裁剪区之外的笔画一律落空。第六篇末尾咱们数过 D2D 同款的现象：`SetTransform` 设下去会一直压在后面的绘制身上，直到您再设一次。Skia 的画布也是这样的脾气，但它多给了一件 D2D 没有的家什：一个栈。

```cpp
canvas->save();                                       // push matrix + clip
canvas->translate(200.0f, 0.0f);                      // move the origin
canvas->clipRect(SkRect::MakeXYWH(0, 0, 180, 300));   // confine drawing
// ... draw a group of shapes in local coordinates ...
canvas->restore();                                    // pop back
```

`save` 把当前的矩阵和裁剪区整份压进画布内部的栈里，`restore` 再把它整份地弹回来。中间咱们随便挪原点、随便裁。等 restore 执行完了，一切就都回到了 save 那一刻。画成组的图形时咱们就靠它：一组部件画完咱们 restore 回全局原点，下一组从干净的地方起笔，咱们不用自己算逆变换。

咱们对照 D2D 看，差别就落在栈的有无上。D2D 的 `SetTransform` 是个槽位，是没有栈的，[变换篇](09_d2d_transforms.md)里咱们只能靠显式复位过日子：画完一段就把矩阵摆回单位阵，或者每帧开头把要用的矩阵显式设一遍。裁剪倒是有 `PushAxisAlignedClip` 与 `PopAxisAlignedClip` 一对，可那是只管轴对齐矩形的专用通道，跟矩阵各管各的。

Skia 把两样塞进了同一个栈，咱们一次 save 下去，全套都管了。

GDI 手里其实也有一对 `SaveDC` 与 `RestoreDC`，思路是同款的，咱们本篇没用到它们，您知道有这门亲戚就好。

栈还有个不出错的保底用法：函数进门 save、出门前 restore，函数内部怎么折腾坐标系，外面的调用者都感觉不到。您写自绘控件的代码时会感谢它的。栈上还能压更重的东西，`saveLayer` 会把一块离屏的图层也压进去，混好了再合回来。这门手艺咱们这套教程暂不展开，您知道有这回事就好。

## CPU 渲染的最小回路

三件套认齐了，咱们把它们串成一条能跑的路：咱们造好一块表面，画完了想画的几笔，把画好的像素取出来，最后写进文件里就完事了。CPU 的表面上有个好消息：每个 draw 调用都当场落进了像素内存，攒命令的事在咱们这头是不存在的。您可能在网上见过 `surface->flush()` 的写法，那是 GPU 表面的事。这门 API 有三段历史：旧主线用它把攒下的命令冲给显卡，中间一度改成了 `flushAndSubmit`，近年的主线把两个名字都从 `SkSurface` 身上移走了，刷新的活归了 GPU 上下文的管理者 `DirectContext`，细节咱们留到 GPU 后端篇再说。CPU 的表面上本来就没有攒命令这一说，咱们取像素走的是另一对入口。

取像素的入口有两个。`peekPixels` 交回来的是一个 `SkPixmap`，它不做像素的拷贝，只是表面内存的一个直通视图，您顺着它读到的就是本尊，所以快。但窗户只在表面活着的时候有效。`readPixels` 则是把像素拷进您给的缓冲，换来的是多一次搬运，好处是拷贝出来的数据想留多久就留多久。咱们存 PNG 用 peekPixels 就够，反正咱们紧接着就要编码。

轮到编码 PNG 了，新主线的入口是 `SkPngEncoder::Encode`，收的是一个输出流、一份像素、一份选项。老代码里您会见到 `SkImageEncoder` 的旧入口，更老的还有 `SkImage::encodeToData`，都是同一个活的前几任房客，咱们按新名字写。输出落到文件的活由 `SkFILEWStream` 干，它是 Skia 自带的文件流。下面咱们把整条路真刀真枪走一遍。

咱们顺路把 `SkPixmap` 认半张脸。它打包的是像素的地址、每行多少字节的 rowBytes，再加上一份 `SkImageInfo`（宽、高和像素的格式都装在里面）。rowBytes 值得您留意，一行像素的尾巴上可能有对齐用的填充，下一行的起点不见得就是宽度乘四排下来的位置。所以您要手动遍历像素的时候，行进的步子得取 rowBytes，您直接拿宽度乘 4 去跳行，跳到一半的时候就可能踩进填充里。咱们把编码交给 SkPngEncoder 就好，它收的就是同一个 SkPixmap，rowBytes 它自己会认的。

## 动手：画一张图存进 PNG

咱们写一个完整的 main，画完了四样东西，末尾咱们把图存进文件。头一段是材料的准备：

```cpp
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"

int main() {
    SkImageInfo info = SkImageInfo::Make(
        400, 300, kN32_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) {
        return 1;   // allocation failed
    }
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
```

中段是绘制的部分，咱们把前面认过的家伙全用上：填充的矩形、描边的圆、一条拼出来的路径，最后再加一组 save 与 restore 包着的半透明块，看一眼栈的实际效果：

```cpp
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(0xFF2D7DFA);
    canvas->drawRect(SkRect::MakeXYWH(40, 40, 180, 120), paint);

    paint.setColor(0xFFE8590C);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(6.0f);
    canvas->drawCircle(300, 100, 56, paint);

    SkPath path;
    path.moveTo(60, 260);
    path.lineTo(200, 220);
    path.lineTo(340, 250);
    path.close();
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(0xFF1B2A66);
    canvas->drawPath(path, paint);

    canvas->save();
    canvas->clipRect(SkRect::MakeXYWH(200, 0, 200, 300));
    paint.setColor(0x803E5C9A);   // 50% translucent blue
    canvas->drawRect(SkRect::MakeXYWH(180, 60, 160, 200), paint);
    canvas->restore();
```

半透明那块是故意越界的：咱们把一块宽 160 的矩形从 x=180 画到 x=340，裁剪区的左边界却压在 x=200。所以成品图上，左边一条 20 像素宽的窄边被裁剪区拦下了，可见的部分从 200 开到了 340。裁剪的效果这样看得最直白。

咱们在末段干的是取像素、编码、写文件的收尾：

```cpp
    SkPixmap pixmap;
    if (!surface->peekPixels(&pixmap)) {
        return 1;
    }

    SkFILEWStream out("skia_first.png");
    SkPngEncoder::Options options;   // defaults are fine here
    if (!SkPngEncoder::Encode(&out, pixmap, options)) {
        return 1;
    }
    out.flush();   // push the buffer down to disk before return
    return 0;
}
```

`peekPixels` 失败基本只会发生在拿不到直接视图的表面身上，在 raster 的表面上不会出这样的事，判一下返回值是咱们的习惯。`SkFILEWStream` 的析构会关文件，flush 是咱们主动把缓冲写下去，咱们稳一手。您跑完这一段，工程目录里躺着的 `skia_first.png` 就是咱们第一张 Skia 出品的图，四样笔画都在上面了。

构建的活咱们如实地在明处交代：示例按 2024 年前后主线的头文件路径写全，把它接进 CMake 工程、编出 Skia 库本身的步骤，咱们本篇没有铺开，您按 Skia 仓库的构建说明走，实战篇也会默认库已经备好了。

## 对着 D2D 再看一遍

咱们把两边的座位摆成一张表，您回头翻的时候对着看：

| 对照的点 | Direct2D（第六篇起） | Skia（本篇） |
|:---|:---|:---|
| 画布载体 | 渲染目标 `ID2D1HwndRenderTarget` | `SkSurface` |
| 创建入口 | 工厂 `CreateHwndRenderTarget` | 静态工厂 `SkSurfaces::Raster` |
| 落笔的家伙 | 目标自己，`FillRectangle` 挂在目标上 | `SkCanvas`，`getCanvas` 从表面取 |
| 样式载体 | 画刷 `ID2D1Brush`，独立 COM 对象 | `SkPaint` 参数袋，随调用传 |
| 抗锯齿 | 默认开 | 默认关，`setAntiAlias` 自己拨 |
| 变换 | `SetTransform` 槽位，无栈 | 矩阵与裁剪同一个栈，`save`/`restore` |
| 一轮的收尾 | `EndDraw` 提交并呈现 | raster 上画完即落内存，`peekPixels` 取 |
| 生命周期 | COM 引用计数，`Release` | `sk_sp` 引用计数，析构自管 |
| 分发方式 | 系统组件，随 Windows 就位 | 源码库，自己编自己接 |

表里最值得您多停一眼的是样式的那一行。D2D 把样式做成了独立对象，好处是画刷能攒起来反复地用，代价是又多了一类要管生死的东西。Skia 把样式拍扁成了参数袋，画刷这个层次整个没有了。咱们想画渐变的时候，paint 里得装进一个叫 shader 的对象，家什又回来了。

设备丢失的功课咱们在第六篇也交过：EndDraw 报回了 `D2DERR_RECREATE_TARGET`，咱们就走丢弃重建的回路。本篇的 raster 表面上，咱们这门功课暂时不用交，因为咱们的像素就在内存里，没有 GPU 的设备可丢。等咱们走到 GPU 后端那一篇，设备的事会回来，到时候咱们再交一次作业。

## 您可以动手的三处

1. 咱们把 `setAntiAlias(true)` 那一行注释掉再编。描边圆的边缘立刻现出锯齿，默认值是关的，您亲眼看过一遍也就不用再背了。
2. 咱们把示例里的 `canvas->restore()` 注释掉，再在它后面补一笔想画到左边的 `drawRect`。栈没弹回来，裁剪区还压在 x=200 那道界上，新笔画照样被拦在界的右边，越不过去，画面替您作证。
3. 咱们把 `peekPixels` 换成 `readPixels`：自己开一块 `std::vector<uint8_t>` 的缓冲，把 `SkImageInfo` 和缓冲递给它。拷出来的像素跟窗户里看到的是同一份数据，您顺手也就验证了搬运的入口。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来。

1. `SkSurfaces::Raster` 和 `SkSurface::MakeRasterN32Premul` 是什么关系？本篇以哪个年代的主线为基准？
2. `kN32_SkColorType` 配 `kPremul_SkAlphaType`，在 Windows 上对应的内存布局是什么样？哪一篇认过它？
3. 画布从哪来？它的引用计数归谁管？
4. D2D 的画刷在 Skia 里对位成了什么？描边和填充的切换在哪句话上？
5. `save` 压进栈的是哪两样东西？D2D 那边对应的能力长什么样？
6. raster 表面上画完，像素怎么取出来？`peekPixels` 和 `readPixels` 差在哪？
7. 本篇的 PNG 编码用的哪个入口？它的两任旧名字是什么？
8. `SkPixmap` 里的 rowBytes 是干什么的？咱们手动遍历像素的时候，步子该怎么取？
9. raster 表面上为什么不用担心设备丢失？D2D 那边对应的功课是什么？
10. `surface->flush()` 在 raster 表面上为什么没什么活可干？它和 `flushAndSubmit` 在近年的主线里落了个什么下场？

## 下一步

三件套的脸咱们算认熟了，paint 却还停在纯色的最浅一层。下一篇[《SkPaint 深入：shader 与混合》](14_skia_paint_deep.md)咱们就把参数袋倒开来看：颜色底下垫着的 shader 体系，源色与画面的混合方式，还有描边的几处细节。渐变、纹理怎么装进 paint，两家的画刷怎么对上号，都在那边接着讲了。GPU 的路线咱们也不急，咱们等文本篇过了之后，再看[GPU 后端](16_skia_gpu_backend.md)里 Skia 跑在显卡上的样子。
