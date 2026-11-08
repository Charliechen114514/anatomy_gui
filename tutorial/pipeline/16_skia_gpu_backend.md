---
title: GPU 后端：DirectContext 与后端目标
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# GPU 后端：DirectContext 与后端目标

按[篇目索引](index.md)的登记，本篇在 Skia 段里排的是第四位。咱们往前数一数，[《Skia 的文本：SkFont 与 TextBlob》](15_skia_text.md)是紧挨着的前一篇，再前面的[《SkPaint 深入：shader 与混合》](14_skia_paint_deep.md)，与段首的[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)，想必您也都读过了。咱们再往后看，还剩讲公式的[《SkSL：运行时着色器》](17_sksl.md)，和收尾的[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)，都在等着咱们。

本篇立的是概念与对象流。跑得起来的窗口，还有交换链（Windows 上把画好的画面轮流送上屏幕的一组缓冲，Direct3D 那边的机制），按索引的登记排在《实战：Win32 窗口跑 Skia 渲染循环》那边，咱们这边把搬进 GPU 的每个对象认清楚，到实战篇组装的时候您才知道每样零件是从哪儿来的。

前两篇认的 shader 与 font 都待在 CPU 的画布上，本篇干的活等于给它们搬家，搬完之后您手里的绘制知识一件不废，多出来的只是底下那层 GPU 的门道。

本篇的 API 以 2024 年前后的主线为准。Skia 的 GPU 接口是这几年搬家搬得最凶的地段之一，旧名字咱们并给，您翻老教程时对得上脸就好。拿不准的具体签名，笔者宁可少写细节也不编，涉疑的地方咱们当场标出来。

## 为什么要把画布搬上 GPU

到咱们动笔写本篇为止，Skia 段的两篇示例用的都是 `SkSurfaces::Raster` 造的光栅画布，也就是 Skia 自家的 CPU 光栅化器：每一笔绘制到了最后，都要在 CPU 里对像素逐个地算，画布背后是一块普通的内存。算得动是算得动的，可一幅 1080p 的画面是两百万像素，抗锯齿、混合、渐变插值全是逐像素的活，CPU 的核再强，也是一排核排着队地干。

GPU 的脾气正好对上这摊活。逐像素的算法有个天生的性质：每个像素都不依赖它的邻居，咱们不用担心谁等谁，这正是数据并行最标准的形状，GPU 上的几千个核同时开了工，干的也就是同一件事。咱们在[《工厂、设备与渲染目标》](06_d2d_targets.md)里见过同款的分野：GDI 的每条线都由 CPU 逐个像素去算，而 GPU 闲在一边，Direct2D 却把活借道 Direct3D 交给了 GPU。Skia 搬上 GPU 走的是同方向的路，差别在于它把底下那层做成了可换的：同一套 Canvas 的 API，可以落在 Direct3D、OpenGL、Vulkan、Metal 里的任何一副上，本篇的后半咱们把这几位逐个认脸。

Chrome 的界面绘制跑在 Skia 上，安卓的系统界面也是它的用户，Flutter 的渲染后端长期也是 Skia，这几年正在往自研的 Impeller 上迁。这几家的共同点是画面都在 GPU 上算。您读完本篇再回头看[第一篇《从状态到像素》](01_status_to_pixels.md)里 `DrawTextW` 走的 CPU 老路，就能掂出 Skia 把光栅化搬进 GPU 是图什么了。

量级上再感受一下：1080p 的画面是约两百万像素，界面的动画跑满 60 帧的话，每秒就是一亿两千万像素的计算量，而 CPU 这边还有消息循环、布局与业务逻辑的活要干。[《失效、脏区与重绘》](05_invalidation_dirty_region.md)那篇的老办法在这里也帮不上忙：局部失效省下的是重画的面积，省不下的是单个像素的算法本身，抗锯齿与混合该算的还是得算，真正能替咱们分忧的只有并行化这一招。

## DirectContext：通往 GPU 的入口对象

搬家的头一件家当是 `GrDirectContext`。咱们用白话讲，它代表的是“一块 GPU 上的 Skia”。绘制用的纹理、着色器、缓存归它管，命令的记录与下发也归它管。角色上对位的是 D2D 那边的设备。它的名字是一部搬家史，咱们按年代排：老教程的年代它叫 `GrContext`，后来它改名成了 `GrDirectContext`，`GrContext` 留下来当了弃用的别名。改名的准确年份笔者没有去翻提交记录，两个名字指的都是同一位，这一点是稳的，咱们新代码按新名字写。

它的创建不靠构造函数，靠的是工厂入口。2024 年前后的主线里，工厂就是 `GrDirectContext` 自带的静态成员：D3D 的入口是 `MakeDirect3D`，GL 的入口是 `MakeGL`，Vulkan 与 Metal 各有各的入口，名字也是同款的。更晚的版本把这些入口收进了 `GrDirectContexts` 工厂命名空间，入口的名字没换。这里有一处咱们得说清：Skia 的 D3D 后端建在 Direct3D 12 上，而不是 11。咱们系列的后半有 Direct3D 段，[《D3D11 设备与立即上下文》](24_d3d11_device.md)与[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)都在那边排着，眼下咱们只要认得一点：Skia 吃的是 12 那一代。

`MakeDirect3D` 收的是一份 `GrD3DBackendContext` 一类的结构（准确的名字以头文件为准，它还有收显式指针的重载），里面装的是咱们备好的 D3D12 家当：`ID3D12Device` 的指针、命令队列、适配器这些。所以设备与队列咱们得在宿主那边造好，工厂只是把它们接进 Skia 的门。工厂还回来的对象装在 `sk_sp` 里，引用计数的老朋友了，用法跟前两篇的 shader、typeface 是同一套。

`GrDirectContext` 还管着一层跟咱们已经沾边的东西，也就是 GPU 侧的资源缓存。[《Skia 的文本》](15_skia_text.md)里查好的字形，到了 GPU 后端就住在缓存里的一张图集纹理上，同款的字画第二遍就不用重新上传了。缓存的上限、字节数，`GrDirectContext` 身上都有查询与设置的入口，咱们记个脸熟，咱们真做性能调优时再回去翻。

使用的边界咱们也记一条：一只 `GrDirectContext` 同一时刻只该有一个线程在用，绘制、flush 与 submit 走的都是同一个线程，这是官方文档口径的约束。多线程地喂一只有 GPU 资源的 context，是 Ganesh 的高级话题，Ganesh 是 Skia 现在这代 GPU 框架的名字，本篇后文咱们还会正式认识它。咱们按单线程的路数讲，好在 Win32 的消息循环本来就是单线程的脾气。

咱们还会想到一件事，就是同一只 context 的名下能不能挂多只 surface？答案是可以的，也很常见的，主窗口挂的是一只，离屏的中间画面再挂一只，字形图集一类的缓存也就大家共享了。关张的顺序倒过来走：surface 们全都析构了，咱们再放手 context，最后才轮到宿主的设备，咱们逆着来就是给析构函数出难题。

## 对象怎么流起来：三步走完

咱们把整条对象流摆出来，稍后咱们再逐个展开。宿主的图形 API 那边，咱们已经有设备与纹理（D3D 那边就是 `ID3D12Device` 与一块能当渲染目标的 `ID3D12Resource`，交换链的后备缓冲就是现成的一块），头一步是把它包成 Skia 认识的 `BackendRenderTarget`。第二步咱们拿 `GrDirectContext` 配着它造出 `SkSurface`。走第三步的时候，`getCanvas` 还回来的画布跟前两篇用的是同一副脸，画完了咱们调 `flush` 与 `submit` 收尾。三步都走完了，画面就从 CPU 内存搬进了 GPU 的纹理里。

三步里登场的主角，咱们排一份名单，后文各位的展开都对着它找：

- `GrDirectContext`：GPU 那边的管家，工厂 `GrDirectContext::MakeDirect3D` 造，一程一只
- `GrD3DBackendContext`：装 D3D12 设备与队列的搬运箱，咱们在宿主那边备好
- `GrBackendRenderTarget`：宿主纹理的描述符，宽高、样本数加 API 专属信息
- `SkSurface`：Skia 侧的画布，`SkSurfaces::RenderTarget` 或 `WrapBackendRenderTarget` 造
- `SkCanvas`：咱们的老朋友，`getCanvas` 还回来，API 与 CPU 时代一字不差

名单看着挺长的，真正的新面孔其实集中在 context 一侧：`SkSurface` 与 `SkCanvas` 这两样在 CPU 时代咱们就造过同款，Skia 只是给它们换了个住处。

## BackendRenderTarget：把 D3D 的纹理包给 Skia 看

头一步的主角是 `GrBackendRenderTarget`。它其实是一只描述符，记着的是宽、高、样本数，再加一份各 API 自己的资源信息。D3D 那边给它配的资源信息类型叫 `GrD3DTextureResourceInfo`，包着的是 `ID3D12Resource` 的指针，捎带着格式与资源状态的一栏，具体的字段咱们不逐个抄，您以头文件为准。资源状态的一栏对的是 D3D12 的资源状态机，当渲染目标用的纹理填的是渲染目标的档，您真填错档，驱动的调试层会毫不客气地报给咱们看。Skia 画图的时候真正写入的还是那块 `ID3D12Resource`，描述符干的只是介绍信的活，咱们用它告诉 Skia 画往哪儿去。

`BackendRenderTarget` 是不持有那块纹理的，纹理的生死咱们宿主自己管，这一点咱们到讲生命周期的段落还会回来细说。它的兄弟叫 `BackendTexture`，包的是“当素材读的纹理”一类的对象，咱们本篇用不上，您认个脸就好。

描述符里的样本数一栏是给 MSAA 用的。MSAA 的全称是多重采样抗锯齿，它的做法是让每个像素多采几个样再合成出结果，换来的边缘也就更平滑，代价花的是显存与带宽。咱们示例不碰它，咱们传缺省的一就好。

## SkSurfaces：RenderTarget 与 WrapBackendRenderTarget

造 `SkSurface` 是第二步的活，咱们要从 `SkSurfaces` 工厂里找入口，旧世界里它住在 `SkSurface` 的静态成员上，老教程里的 `SkSurface::MakeRenderTarget` 与 `SkSurface::MakeFromBackendRenderTarget` 就是这一对的前世。两条路咱们分开讲，因为它们适合的场合不同。

头一条路的名字叫 `SkSurfaces::RenderTarget`，咱们请 Skia 自己在 GPU 上开一块纹理当画布，宿主那边也就不用提前备纹理了。离屏渲染一类的中间画面，用的都是它。您看它的参数：头一个是 `GrDirectContext`，第二个是 `skgpu::Budgeted::kYes`（预算内分配的意思，说的是这块纹理算在 Skia 的缓存预算里），再往后的参数是一份 `SkImageInfo`，跟前两篇造光栅画布时用的是同一个类型，长相上是熟悉的。

第二条路的名字是 `SkSurfaces::WrapBackendRenderTarget`，咱们把自己的后端目标包成画布，交换链的后备缓冲走的就是它。它的参数里有两样值得停下来认，头一样要看的是 origin，也就是纹理的 y 轴朝向。D3D、Metal、Vulkan 的纹理都是 y 向下的，咱们报 `kTopLeft_GrSurfaceOrigin`，OpenGL 有个 y 向上的老脾气，GL 上常报的是 `kBottomLeft_GrSurfaceOrigin`，报错了画面就是上下颠倒的。

另一样要看的是 colorType，它得跟纹理的真实格式对上。D3D 交换链的后备缓冲常见是 BGRA8，咱们就得报 `kBGRA_8888_SkColorType`，报成 RGBA 的话红蓝两个分量就换了位，画出来的颜色是错的，而且错得很好认。

这一对工厂的参数形状这几年一直在动，colorType 与 colorSpace 后来有并成一份 `SkColorInfo` 的版本。咱们按分开的年代写，您手上的头文件要是收的是打包款，包一份就是了。

两条路造出来的都是 `sk_sp<SkSurface>`，这一步走完了，GPU 后端跟 CPU 后端在咱们眼里的差别就消失了：`getCanvas` 还回来的还是 `SkCanvas`，前两篇认过的 paint、font、blob 拿过来照用，咱们一笔都不用改写。

`SkImageInfo` 的尺寸咱们按物理像素填，呼应[《坐标、DIP 与 DPI》](03_dip_and_dpi.md)里讲过的换算：GPU 的纹理只有物理像素的概念，DIP 的换算在咱们自己的代码里做，[《Skia 的文本》](15_skia_text.md)里留的缩放口子，到实战篇咱们就从这儿接。末位的 surfaceProps 管的是画布的零碎偏好：字体的子像素抗锯齿按哪种像素排列做，正是它的辖区，咱们不动它就吃默认值，您做文本密集的界面时值得回来细看。

## 收尾：flush 与 submit

画布咱们照常用，变化全落在了收尾上。咱们在光栅画布上画完，像素已经在内存里了，画完也就得到了。GPU 的画布却不是这样：`drawRect` 一类的调用只是被记了下来，命令都攒在了队列里，咱们不催，GPU 是不会动手的。催的口子有两个，都在 `GrDirectContext` 的身上。咱们把 surface 递给 `flush`，它把这块画布上攒的命令记进后端 API 的命令缓冲里，到了这一步命令还没有真正发给 GPU。`submit` 干的才是递送的活：此前所有 flush 攒下的东西，到了 submit 这一步才真正交给 GPU 去执行。咱们传 `GrSyncCpu::kYes` 进去，submit 就会等 GPU 把活干完了再返回，缺省的 `kNo` 是不等，命令发出去咱们就接着走。`SkSurface` 自己还给了 `flushAndSubmit` 的便捷口子，一步把两件事都办了。

咱们拿[《工厂、设备与渲染目标》](06_d2d_targets.md)里的 `EndDraw` 对个位：D2D 把提交与呈现合在 `EndDraw` 一步里了，Skia 的步子分得更细，flush、submit、present 各是各的，present（把交换链的后备缓冲送上屏幕）归宿主的图形 API 管，不在 Skia 的手里。分开的代价是多两行调用，换来的好处是咱们能自己挑时机：动画循环里咱们可以早一步把命令发出去，咱们也能腾出手干点别的，回头再等它的结果。什么时候等、等多久，咱们到实战篇的渲染循环里再细算。

咱们再认一个常见的搭档：`makeImageSnapshot`，它把画布当下的样子拍成一张 `SkImage`，离屏画布的成果要交给别的画布当素材时，走的就是它。GPU 画布上拍出来的结果就住在纹理里，它跟普通 SkImage 的用法是一样的，drawImage 也照常地收。

咱们要是只 flush 不 submit 会怎样？命令还停在命令缓冲里，GPU 到这时还没收到要执行的活，画面的更新也就没有着落。实战篇里咱们拿真窗口验一遍这一层。

## 两种画布的对照

咱们把两代画布摆在一起对照，您一眼能看出搬进 GPU 之后多了哪些家当：

| 对照的点 | 光栅画布（前两篇） | GPU 画布（本篇） |
|:---|:---|:---|
| 造画布的入口 | `SkSurfaces::Raster` 配 `SkImageInfo` | `SkSurfaces::RenderTarget` 或 `WrapBackendRenderTarget`，都少不了 context |
| 像素住在哪儿 | 咱们内存里的一块 | GPU 的纹理里 |
| 画布之外要备的对象 | 一个都不用 | 宿主的设备与队列，加 context |
| origin 与 colorType | 没有这回事 | 要跟纹理的真实属性对上 |
| 样本数一栏 | 用不上 | MSAA 的场合才填 |
| 画完之后的动作 | 画完也就得到了 | `flush` 加 `submit`，呈现另走交换链 |
| 画布上的绘制 API | 同一套 `SkCanvas` | 同一套 `SkCanvas` |
| 设备出事的时候 | 没有这个概念 | `abandonContext` 之后整套重建 |

表里最值得咱们留意的是头一行与末两行：造画布的入口换了，绘制的 API 是没换的，设备出事的处置也换成了新的功课。

## 生命周期：surface 与后端纹理谁管谁

纹理的归属，两条路的路数并不相同，咱们分开交代。走 `SkSurfaces::RenderTarget` 的那一路，纹理是 Skia 自己造的，主人就是它：surface 的引用计数归了零，底下的纹理也就跟着收走了，咱们什么都不用管。走 `WrapBackendRenderTarget` 的那一路却是反过来的，纹理是咱们的，Skia 借的只是使用权，所有权是不转移的，咱们得保证 surface 活着的时候纹理不被拆走。那什么时候释放才稳当？`Wrap` 的参数里留了 releaseProc 与 releaseContext 两个口子，咱们挂一个回调进去，Skia 用完了那块纹理，回调也就响了，响了之后再释放才是稳的。这些是官方文档口径的规则，具体实现细节笔者没有逐版验证过，您真做释放顺序的排序时，以手上版本头文件的注释为准，笔者的建议只有一条：surface 得走在纹理的前面，纹理得走在设备的前面。逆着拆的话，拆到一半可能正好赶上 Skia 还在用那块纹理的时刻。

## 设备丢失：GPU 世界的旧功课

咱们在[《工厂、设备与渲染目标》](06_d2d_targets.md)里交过设备丢失的功课，`D2DERR_RECREATE_TARGET` 触发的场景都是日常的：驱动崩溃之后的恢复，外接显示器的热插拔，还有远程桌面的断开。GPU 后端的 Skia 住在同一片地上，同样的风浪也就一样会拍过来，D3D 那边的表现是设备被移除了，错误码一类的细节咱们到实战篇的代码里再见。

Skia 给的口子叫 `abandonContext`，咱们用白话讲它的意思：底下的设备已经没了，您手里的 GPU 资源不用再正经释放了，直接放弃就行了。正经的释放（`releaseResourcesAndAbandonContext`）是设备还活着、咱们只是要收摊的时候用的，两个口子的分工以头文件的注释为准，笔者的理解如上。调用之后的 context 是废的，咱们能走的路只有一条：重建宿主的设备，另造一只新的 context，再把 surface 与画笔的一整套从头建一遍。

对照着 D2D 的回路，咱们能看出姿态的差别：D2D 那边工厂不用动、只重建目标一侧的家当，Skia 这边的 context 本身就绑在设备上，设备没了它也跟着作废，重建的摊子也就更大。好在咱们本篇立的是概念，真实的设备丢失处理代码，实战篇的渲染循环里咱们给它留了位置。

## 后端家族：BackendAPI 与身后的 graphite

后端的家伙不止 D3D 一位，Skia 把家族成员收进了枚举里。Ganesh 这边用的枚举叫 `GrBackendApi`，成员咱们最熟的四张脸是 `kOpenGL`、`kVulkan`、`kMetal` 与 `kDirect3D`，另有测试用的 `kMock` 与表示不支持的 `kUnsupported`。还有一枚 `skgpu::BackendApi` 是给新后端 graphite 用的，那边才有 `kDawn` 的位置，Dawn 是 WebGPU 的实现层，走浏览器那套新图形接口的路，那枚枚举里反而没有 D3D 与 GL 的座次。同一份后端名单正是 Chrome 与 Flutter 跨平台的底子：同一套 Canvas 的 API，Windows 上落的是 D3D，macOS 与 iOS 交给了 Metal，Linux 与安卓挑的则是 GL 或 Vulkan，画图的代码一行都不用改。咱们系列挑 D3D 当主线讲，是因为咱们挑了 Win32 当宿主，别的后端您需要时，换的是工厂与资源信息那两层，中间的 surface 与 canvas 原样认您。

还有一层名字咱们得交代，因为它就藏在类的命名空间里：`skgpu::ganesh`。ganesh 是 Skia 现在这代 GPU 框架的名字，咱们本篇认的一切都住在它的屋檐下。Skia 同时还在造一副新的后端，新家的名字叫 graphite，目标是把命令与资源的管理做得更现代，迁移的事还在路上，官方的材料也明说了它是进行时。2024 年前后的主线默认仍是 ganesh，哪个产品切到了 graphite、切到哪一步了，笔者不押注，咱们如实记一笔：您将来在别处看见 graphite 的名字，知道它是接班的候选人就好，本篇与接下来的实战篇讲的都是 ganesh。

咱们为什么现在还要学 ganesh？因为它是当下默认的那一副，您今天链接到的 Skia，GPU 走的就是它。graphite 真成了默认的那天，surface 与 canvas 的上层用法也不会一夜作废，变的更多是底下管理的细节。

## 光栅后端还留着做什么

读完本篇您可能会想，光栅后端是不是就该退役了？其实不是的。离屏的小图、工具链里的离线渲染、单元测试的确定性输出，都是 CPU 后端的主场：没有 GPU 的环境里它能跑，输出的像素也不随驱动与显卡的差别漂移。

咱们在两种后端之间挑的路子也简单：画布造出来之前咱们想清楚用途，要上屏幕的走 GPU，要确定性或离线的走光栅，两边的 canvas 代码本来就是同一套。

本篇的示例为什么用 `RenderTarget` 那路？因为它是不挑宿主的：咱们没有窗口、也没有交换链，光有一只 context 咱们就能把 GPU 的对象流走通。

## 示例：让 Skia 自己在 GPU 上开一块画布

本节的代码是笔者按 2024 年前后主线的接口写的骨架，它没有经过编译的验证，D3D 设备与队列的来路（怎么建设备、怎么造交换链）是实战篇的主菜，咱们这里用注释占着位：

```cpp
// d3dBackend bundles the ID3D12Device, command queue and friends
// we created on the host side. Building them is the hands-on Win32 article's
// job. The context is created once and kept for the app's lifetime
// (per device); multiple surfaces may hang off it.
sk_sp<GrDirectContext> context =
    GrDirectContext::MakeDirect3D(d3dBackend);

sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
    context.get(), skgpu::Budgeted::kYes,
    SkImageInfo::MakeN32Premul(360, 240));
SkCanvas* canvas = surface->getCanvas();
```

`MakeDirect3D` 接的是咱们备好的 D3D12 家当，`RenderTarget` 请 Skia 自己在 GPU 上开了块 360×240 的纹理，`getCanvas` 把画布递到咱们手上。画的部分您会觉着眼熟，因为本来就是前两篇的家当：

```cpp
void DrawGpuScene(SkCanvas* canvas) {
    canvas->clear(SK_ColorDKGRAY);

    SkPaint ink;
    ink.setAntiAlias(true);
    ink.setColor(SK_ColorWHITE);
    canvas->drawRect(SkRect::MakeXYWH(20, 20, 320, 200), ink);

    // Text rides along for free - the glyph atlas is a GPU-side
    // cache owned by the same DirectContext, so the second frame
    // with the same glyphs uploads nothing.
    SkFont font(SkTypeface::MakeFromName(
                    "Microsoft YaHei", SkFontStyle::Normal()),
                24.0f);
    font.setEdging(SkFont::kAntiAlias);
    canvas->drawString("GPU 上的家当照常用", 20.0f, 60.0f, font, ink);
}
```

字的一笔咱们专门拎出来说：font 的四行跟[《Skia 的文本》](15_skia_text.md)里一字不差，画字的家当一件不改，底下的画布换成了 GPU。字形头一回上屏的时候要进图集，第二帧起就是缓存里的现货。

画完了咱们就收尾，把攒下的命令真正交给 GPU：

```cpp
context->flush(surface.get());
context->submit(GrSyncCpu::kYes);
```

两行收尾的调用咱们值得多看一眼：flush 与 submit 是分开的两步，示例里咱们紧接着调，所以看起来像一步，实战篇里咱们会把它们摆到循环里各自的位置上。

骨架的预期画面是这样：深灰的底上是一块白色的矩形，矩形底下压着一行微软雅黑的白色中文字。真跑出来的样子笔者手里没有截图，您接进真画布时对一眼就知道推没推对。

这一段跑完了，白色的矩形与那行字就落在 GPU 的纹理里了，窗口上是看不见它的，送它上屏幕的活要走交换链，那是实战篇的活。包交换链的那一路，骨架的样子也给您过目，参数的排布以您手上的头文件为准：

```cpp
// backBuffer is the swap chain's ID3D12Resource, typically BGRA8.
// The info struct carries the resource pointer plus format and
// D3D12 resource state; the header is the source of truth here.
GrBackendRenderTarget target(
    width, height,
    GrD3DTextureResourceInfo(/* backBuffer, format, state... */));

// Origin: D3D textures are top-left. colorType must match the
// texture's real format - BGRA8 for most swap chains.
sk_sp<SkSurface> swap = SkSurfaces::WrapBackendRenderTarget(
    context.get(), target,
    kTopLeft_GrSurfaceOrigin,
    kBGRA_8888_SkColorType,
    /* colorSpace */ nullptr,
    /* surfaceProps */ nullptr,
    /* releaseProc */ nullptr,
    /* releaseContext */ nullptr);
```

之后的画法与收尾跟上面那段一模一样，canvas 咱们照常要，flush 也照常地调，present 再由咱们拿着交换链去调。两条路的差别只到 surface 造出来为止。Wrap 那一路咱们要备的清单，咱们也摆在明处：交换链（含它的 back buffer）、D3D12 的设备与命令队列、包出来的 BackendRenderTarget，三样的顺序就是创建的顺序。实战篇的代码就是照着同一份清单走的，您到时候对号就好。

## 动手清单

咱们动手的活一共有十一处，每一处要改的都不出几行，原料都是本篇与头文件里现成的东西：

1. 您把手头 Skia 的 `GrBackendApi` 枚举从头文件里翻出来，把成员跟本篇的名单对一遍，看看有没有咱们没提到的面孔。
2. 咱们把示例里的 `GrSyncCpu::kYes` 换成 `kNo`，在 flush 前后各打一个时间戳，CPU 侧等待的差别您就亲眼看到了。
3. `Wrap` 那条路上的 `releaseProc` 咱们挂一个打日志的回调进去，观察它在什么时机会响，回调不响就释放纹理会发生什么，您有调试器的话单步看一眼。
4. 咱们把 colorType 故意报成 `kRGBA_8888_SkColorType`，BGRA 的纹理被当成 RGBA 去写，画出来的红蓝是换位的，颜色错得有多整齐，您看一眼就忘不掉。
5. `GrD3DTextureResourceInfo` 与 backend context 结构的字段，咱们翻着头文件数一数，Skia 要咱们备齐几样 D3D12 的家当，实战篇开建的时候清单就在心里了。
6. 咱们把造 surface 的尺寸换成您窗口客户区的大小，画满一屏再 flush 收尾，您开任务管理器看看 CPU 与 GPU 各自的占用，直观的感受也就有了。
7. OpenGL 后端的入口咱们也去头文件里认一眼，`MakeGL` 收的是一份函数指针表一类的家当，跟 D3D 那路要备的东西对个照。
8. `GrDirectContext` 身上资源缓存的查询入口咱们打一行日志，画满一屏字再查一遍，字形图集吃了多少也就有数了。
9. 咱们把示例的 `SkImageInfo` 换成显式给 `kBGRA_8888_SkColorType` 的版本再造一次，RenderTarget 路上格式跟的是您传的 `SkImageInfo`，Wrap 路上要迁就的则是宿主纹理的真实格式，这层差别您就有体感了。
10. 咱们把 `RenderTarget` 造的画布用 `makeImageSnapshot` 拍一张，再拿 `readPixels` 把像素读回 CPU 存成文件，GPU 的活干没干成，您拿眼睛就能验收。
11. 咱们给 Wrap 那一路的 origin 故意报成 `kBottomLeft_GrSurfaceOrigin`，上下颠倒的画面就是给咱们核对方向用的。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `GrContext` 与 `GrDirectContext` 是什么关系？
2. Skia 的 D3D 后端建在 Direct3D 的哪一代上？
3. `BackendRenderTarget` 是纹理本身吗？它不接管的是什么的归属？
4. `SkSurfaces::RenderTarget` 与 `WrapBackendRenderTarget` 各自适合什么场合，交换链走的是哪条？
5. D3D 的纹理咱们报 origin 时报哪个值，报错了画面会怎样？
6. colorType 报错成 RGBA 的时候，画面的颜色会怎么错？
7. `flush` 与 `submit` 各干什么，`GrSyncCpu::kYes` 改变的是哪一件事？
8. present 这一步归谁管，为什么 Skia 不替咱们做？
9. Wrap 那条路上，纹理什么时候释放才稳当，靠什么知道时机？
10. `kDawn` 对应的是哪套图形接口，它住在哪一枚枚举里？
11. ganesh 与 graphite 各是什么，2024 年前后的主线默认是哪一副？
12. 同一只 `GrDirectContext` 的名下能挂几只 surface？收摊的时候谁走在了前面？
13. 字体的子像素抗锯齿偏好，住在哪个参数的辖区里？
14. 一只 `GrDirectContext` 能被两个线程同时用吗？
15. 设备丢了之后，旧 context 还能接着用吗？咱们该调的口子叫什么？
16. `makeImageSnapshot` 在 GPU 画布上拍出来的那一份，住在什么地方？
17. 光栅后端在今天还适合什么样的场合？
18. 同一段绘制代码，从光栅画布搬到 GPU 画布，要改的是哪几处调用？
19. `BackendRenderTarget` 里的样本数一栏，是留给什么技术用的？
20. `SkImageInfo` 的尺寸按什么单位填？DIP 的换算归谁管？

## 下一步

对象咱们认完了，跑起来的活在实战篇。按[篇目索引](index.md)的登记，下一篇登记的是《SkSL：运行时着色器》，前两篇里咱们只把 shader 当过颜料，到那边您能自己写着色器的源码，画前两篇画不出来的效果。

真正把这副对象流跑起来的活，[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)管的就是它：建 D3D12 的设备与交换链，包出咱们自己的 surface，把 flush、submit、present 串成完整的一帧，咱们在本篇留的所有注释占位，到那边咱们一处一处填上真代码。CPU 侧的对照面，您可以回到段首的[《Surface、Canvas、Paint 三件套》](13_skia_surface_canvas_paint.md)，把缺的那块补上。
