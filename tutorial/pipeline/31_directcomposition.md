---
title: DirectComposition：visual 树
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# DirectComposition：visual 树

按[篇目索引](index.md)的登记，本篇排的是合成与系统这一段的第二篇。[上一篇](30_dwm_composition.md)收尾的时候，咱们把一个入口立在了那儿：`WS_EX_NOREDIRECTIONBITMAP` 拿掉了重定向表面之后，程序就改走 DirectComposition 的路子，自己维护一棵交给 DWM 的 visual 树。本篇的活就是把它接住，树是怎么搭的、节点带了哪些家当、改动是怎么上屏的、动画又凭什么一帧都不劳驾咱们，咱们挨个摊开来看。

动画的节奏与缓动的全套对照，[《动画与帧调度》](34_animation_frames.md)里有现成的，咱们本篇动画的部分就直接借它的说法。

事实的底本也交代一句：DirectComposition 的接口行为在微软的公开文档里写得明白，这一部分咱们照文档说。文档没写死的内部细节，咱们照旧在正文里标出存疑，您读的时候请把它们与能说死的部分分开称重。本篇的代码都是讲解用的骨架，咱们没有在仓库里编译过它，您拿去用之前请对照 SDK 的签名。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够。设备与交换链的概念，咱们借的都是现成的：D3D 的设备来自[《D3D11 设备与立即上下文》](24_d3d11_device.md)，交换链的概念咱们取自[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)，咱们本篇直接拿来用，D2D 那头接的是[《特效：Effect 图》](11_d2d_effects.md)搭好的设备链。DirectComposition 是 Windows 8 起才进系统的部件，您的目标得是 Windows 8 或更新，头文件用的是 dcomp.h，链接的库是 dcomp.lib。

术语的口径咱们也约一下：visual 在微软的中文文档里叫可视对象，咱们本篇沿用英文的原词，为的是跟接口名对得上。照片与摆位的说法还是上一篇的旧约，咱们直接接着用，您读到的时候不用重新对表。

## 内容一动就得整帧重画，根子在哪

上一篇的照片模型咱们再摆出来一次：窗口的画面存在重定向表面里，DWM 每帧拿各家的照片拼桌面，窗口摆在桌面的哪里，这件事归了 DWM。所以拖动窗口时不用重画，因为动的只是摆位。可是照片内部的东西要动，就完全是另一回事了，咱们拿[《动画与帧调度》](34_animation_frames.md)里的滑动面板当素材：一块宽 300 像素的面板要从窗口的左边滑进来，老路线上每一拍都得走完整的重画链，每一拍都得推进自己的状态，咱们得把失效报上去，咱们在 `WM_PAINT` 里把整帧再画一遍。面板滑了 300 毫秒，咱们的程序就跟着画了 300 毫秒，画下来的还多半是根本没变的背景。

重画路线的通病，咱们在主线开篇[《具象 · 脱掉封装，把窗口立起来》](../gui/01_naked_win32.md)里就亲手写过一回（主线指教程里造骨架的 GUI 线，本篇所在的渲染管线是并行的另一条）：状态改了，界面却是纹丝不动的，因为失效的登记谁也没做。失效驱动的那套里，画面的每一步都得咱们的代码亲自送，而在照片内部挪一块东西，代价是整帧的。

这里其实藏着一个不对等，咱们把它挑明：合成器明明有挪摆位的能力，拖动窗口就是现成的证明，而应用的手其实一直够不着它，内容的摆放和内容本身，被捆在了同一张照片里。DirectComposition 解的就是这个捆：应用把画好的内容作为一块独立的纹理交给合成器，摆放的位置、旋转的角度、透明的程度，全都变成了合成参数。参数改了之后，合成器就按新的参数拼下一帧，内容是一个像素都不用重画的。上一篇讲系统动画的时候说过，照片其实一帧都没换过，动的只是照片的摆法。这一次的分别是，改摆法的手换成了咱们自己的。

名分咱们也用一句白话立住：DirectComposition 的名字直译过来就是直接合成，它是应用侧伸进合成器的手，管的是咱们能往 DWM 的合成里放什么、怎么摆。画面的像素从哪儿来，还是 D2D、D3D 那些老画具的事，DirectComposition 自己是一笔都不画的。

## 三个对象撑起骨架

DirectComposition 的对象模型不算大，本篇用得上的对象，咱们认三个就够了：管创建与提交的 device，绑窗口的 target，还有当节点的 visual。

对象的数目为什么这么少？咱们想想 DirectComposition（下文咱们简称 DComp）管的是哪一段就明白了：它只管摆位与叠放，画图的复杂度全留在了 D2D、D3D 那一侧，所以它的模型里不需要画刷、路径那些东西，节点带几份摆位的参数就够了。

IDCompositionDevice 是咱们的总管。visual、变换、裁剪之类的对象，咱们都从它的手里创建，提交改动的 Commit 也走它的门。它的底下连着一个 DXGI 设备，与 D2D、D3D 共用的是同一套底层，您在[《D3D11 设备与立即上下文》](24_d3d11_device.md)里认识的设备概念，在这里是原样适用的。创建的入口是 DCompositionCreateDevice，头一个参数递进去的就是那个 DXGI 设备：

```cpp
// Skeleton for illustration - not compiled in this repo.
#include <dcomp.h>
#pragma comment(lib, "dcomp.lib")

ComPtr<IDXGIDevice>         dxgi_device;    // queried from the D3D11 device
ComPtr<IDCompositionDevice> dcomp_device;

DCompositionCreateDevice(dxgi_device.Get(), __uuidof(IDCompositionDevice),
                         reinterpret_cast<void**>(dcomp_device.GetAddressOf()));
```

IDCompositionTarget 管的是把树绑到窗口上。CreateTargetForHwnd 是它的来处，咱们递一个 HWND 进去，它还咱们一个 target。文档写得明白的要求，咱们照录：窗口本身顶层与子窗口都收，但它必须属于咱们自己的进程，跨进程去绑是要被拒的。同一个窗口的身上，咱们最多建得了两个 target，topmost 的占一个，非 topmost 的占另一个，咱们在同一层位上重复地建，下场就是报错了。还有那个 BOOL 型的 topmost 参数，它定的是咱们的树与窗口的子窗口谁上谁下：填 TRUE，咱们的树就盖在子窗口的上面，填 FALSE 的时候，子窗口就盖住咱们的树。至于窗口自己直接画进重定向表面的内容，咱们的树永远在它上面，这一层是没有得选的。

device 与 target 的数量关系咱们也认一下：一个 device 可以管多个窗口的 target，咱们把 Commit 一交，管的所有 target 是一起生效的。所以多窗口的程序里，咱们共用一个 device，各窗口再各建自己的 target，一批 Commit 就能把所有窗口的改动捆成一次。

IDCompositionVisual 是一个可视的节点，画面的内容、位置、变换这些属性都长在它的身上，咱们下一节逐个认。树的概念也落在它的身上：CreateVisual 造出来的单个 visual，咱们用 SetRoot 把它挂到 target 上当根，咱们再用 AddVisual 往节点身上挂子 visual，一棵树就搭起来了。三个对象串起来的骨架，长的是下面这个样子：

```cpp
// Skeleton for illustration - not compiled in this repo.
ComPtr<IDCompositionTarget> target;
ComPtr<IDCompositionVisual> visual;

dcomp_device->CreateTargetForHwnd(hwnd, /*topmost*/ TRUE, target.GetAddressOf());
dcomp_device->CreateVisual(visual.GetAddressOf());
target->SetRoot(visual.Get());    // this visual is now the tree root
```

您走到这里数一数就看得出来：咱们还一笔都没画，窗口上自然是什么都不会有的。visual 树描述的是摆位与叠放，像素的本身得靠 Content 装进来，它的家当咱们下一节挨个认。

## visual 的家当，逐个认

头一份的家当是 Content。SetContent 往节点里装的是咱们前面说的那块纹理。文档写明的内容来源里，本篇的正路走的是交换链，而且不走连窗口的路子，它用的是 CreateSwapChainForComposition，造出来的是合成交换链。它与 CreateSwapChainForHwnd 的分别，就在名字的后半段上。ForHwnd 的版本把交换链 Present 到窗口的重定向表面，而 ForComposition 的版本不连任何的窗口，交换链的前台缓冲，DWM 是直接把它当纹理用的，挂到哪个 visual 的身上、摆在哪里，全由咱们的树说了算。

D2D 是怎么往它里面画的，走的还是[《特效：Effect 图》](11_d2d_effects.md)里接好的老接线。咱们 GetBuffer 拿到后台缓冲的 DXGI 表面，用 CreateBitmapFromDxgiSurface 包成 D2D 的位图，咱们的 SetTarget 指过去之后，落笔就落进了交换链。

device 那里还有 CreateSurface 系列的接口，把 DirectComposition 自己管的表面交给咱们，本篇就不展开了，您混个脸熟就好。

合成交换链还附赠一份实在的东西，咱们单独说说：它的 AlphaMode 可以填 DXGI_ALPHA_MODE_PREMULTIPLIED，交换链的像素就带上了逐像素的透明。连 HWND 的交换链是做不到这件事的，[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)里也说过同样的限制。逐像素的 alpha 正是主线那边惦记的东西，圆角、阴影、异形的窗口，靠的全是它把边缘透出来，上一篇的结尾也点过它的实在。

OffsetX 与 OffsetY 是咱们最常打交道的两份家当。SetOffsetX 与 SetOffsetY 干的事情很直白，就是把节点要平移的距离写下来，单位用的是 DIP，[《坐标、DIP 与 DPI》](03_dip_and_dpi.md)立过的口径在这里原样适用，100% 缩放下 DIP 与像素的数值相等，高 DPI 缩放下两个数就分开了。它俩管的只是平移，旋转与缩放它俩是不带的，胜在语义的直白，本篇骨架动的就是它们。

Transform 管的层次要深一些。SetTransform 的重载有两个，一个收 D2D1_MATRIX_3X2_F 的矩阵，另一个收的是变换对象。矩阵版本一次性写死了整个仿射变换，文档的说法是它挂不了动画。变换对象的版本才是正路：CreateTranslateTransform 造平移的对象，CreateRotateTransform 造旋转的，CreateScaleTransform 造缩放的。对象的属性与 visual 的属性是同一套脾气，能改的也能挂动画，咱们在[《动画与帧调度》](34_animation_frames.md)的骨架里，挂的正是 translate 变换的 OffsetX。

内容被缩放的时候还有一份细则：采样的方式由 SetBitmapInterpolationMode 定，NEAREST 是取最近像素的，快而粗糙的，LINEAR 是拿邻居掺着算的，慢而顺滑的。咱们做放大的过渡时选 LINEAR，画面才不会一格一格的。

Clip 管的是裁剪。SetClip 收一个 D2D1_RECT_F 的矩形，节点连同它的子树，输出的画面被框在这个矩形里，框外的部分合成器就不要了。更讲究的圆角裁剪走的是 IDCompositionRectangleClip，那是[《图层与裁剪》](32_layers_clipping.md)的正题，本篇咱们只认矩形这一档。

Opacity 管的是透明度。数值不在 visual 自己的方法里，要走一层效果组：device 的 CreateEffectGroup 造出一个效果组，组上的 SetOpacity 收一个 0 到 1 的浮点，0 对应的是全透，1 对应的是全不透明，咱们再把组递给 visual 的 SetEffect，透明度就挂上了。SetEffect 的效果罩的是节点连同它的子树，所以父亲挂了透明度，孩子也被一并罩住了。中间的混合用的就是上一篇讲 OVER 时的那套数学，预乘 alpha 的口径咱们也已经领过了，这里就不重讲了。

这些家当还有一层树的加成。AddVisual 挂进来的子 visual，坐标系是跟着父节点走的：父亲平移了 100 DIP，孩子也就跟着挪了 100 DIP。咱们摆界面的时候，把一组要一起动的东西挂到同一个父亲的身上，动画挂了父亲一个，整组就一起动了。

## 树的排布，跟子控件那套的对照

AddVisual 的排布咱们单独认一下，因为它跟 Win32 子控件那套有像的地方，也有不像的地方。AddVisual 的三个参数，文档的说明写得很细：头一个是要挂的子 visual，第二个是 BOOL 型的 insertAbove，它决定的是新节点落在参考节点的上面还是下面，第三个是当参考用的节点。参考节点不填的时候，新节点挂的位置相对的就是其余全部兄弟的前面或后面。文档对这一情形的方向对应另有一段明确的写法，跟直觉对应的未必是同一边，您用的时候请以它为准。兄弟节点之间的次序，就是它们叠放的顺序，后来的节点是能插进中间去的，所以排布的活，咱们全在一次调用里就说清楚了。

咱们把两套排布摆成一张对照表，您往后从子控件思维迁过来的时候拿它对号：

| | HWND 子控件 | 子 visual |
|:--|:--|:--|
| 挂靠的来处 | CreateWindowExW 挂到父窗口 | AddVisual 挂到父 visual |
| 上下顺序 | z 序，SetWindowPos 改 | 兄弟的次序，AddVisual 的参数定 |
| 坐标 | 客户区坐标，各管各的 | 父坐标系，父亲的变换累积到孩子 |
| 消息 | 各有自己的窗口过程 | 没有窗口，也没有消息 |
| 内容 | 系统控件自己画 | Content 装什么就是什么 |
| 动画时谁重画 | 控件自己的窗口过程 | 没人重画，DWM 按参数拼 |

咱们看得最要紧的是消息的那一行，因为 visual 是没有窗口身份的，它没有自己的 HWND，也没有自己的窗口过程，点击落在谁的身上、焦点归了谁，这些事树自己是不知道的。所以交互的活还得咱们自己做：命中测试拿窗口收到的鼠标坐标，跟自己记录的摆位去比，谁的区域被点中了，谁就是咱们的目标。上一篇讲缩略图的时候咱们提醒过同一句话，摆位的权利和交互的权利是两码事，DComp 也是一样的。

## Commit：一批改动，一次上屏

DirectComposition 的属性改动，有一个统一的脾气：调用的那一刻是什么都没发生的。SetOffsetX 改了值，SetContent 换了图，这些咱们以为的改动，在 Commit 之前都只是登记在 DirectComposition 自己的批里，屏幕上的一个像素都不会动。真正的提交是 IDCompositionDevice 上的 Commit，一次调用就是全部的手续，批里的登记就都生效了，DWM 在随后的合成帧里用上了它们。

批的边界咱们拿一个具体的场景掂一掂。界面上的两块 visual 要一起换位，咱们的代码是两次 SetOffsetX 加一次 Commit。咱们要是没有批，头一处改动已经上了屏、第二处还在路上，用户就会在某一帧里看到两块各走了一半的状态。有了批之后，两处的改动就在同一帧里一起生效了，用户看不到只走了一半的状态。文档给这个性质起的名字是原子性，一次 Commit 就是一个不可分的单元。

这个设计还附带一份清闲：静止的界面是不欠 DWM 任何东西的。Commit 之后树就是静的，DWM 每帧拿同样的参数拼同样的画面，应用与合成器之间没有每帧的通信。咱们对比一下重画路线就能掂出分量：动画一起来的时候，重画路线的每一拍都得走失效重画的全链，而 DComp 的树只要 Commit 一次曲线，后面每一拍都是 DWM 自己的事了。想等提交真正落定的话，device 的身上还有一位 WaitForCommitCompletion，名字就把它的活说完了，咱们本篇用不上它。

## 走偏的样子：改了属性，屏幕不动

咱们把初学最常见的一次哑场写在这儿，您以后碰上了能对上号：代码里 SetOffsetX 改了值，窗口的画面却纹丝不动，翻来覆去查 D2D 的绘制，其实毛病不在那儿。多半是 Commit 忘了交，改动还是留在批里的，DWM 拿到的仍是旧的参数，屏幕自然是不动的。

反过来的哑场也有：Commit 交了，画面还是出不来的，这一场的病根，咱们在本文末尾“您可以自己动手看的三处”的第二处里会写到，内容是压根没进前台缓冲的。两场哑场的分工正好把本篇的两条线分开：内容的流转走 Present，参数的流转走 Commit，哪一条断了，屏幕上都是空的。

## 动画：属性的另一种赋值

直接改属性的路子咱们已经会了：咱们拿 SetOffsetX 换个值，Commit 一下的工夫，画面就跳到了新的位置。跳变是没有中间过程的，拿它做动画的话，咱们就得自己一拍一拍地喂值，那是重画路线的活法。DirectComposition 给的另一条路，是把值随时间怎么走的整条曲线交出去。

家什用的就是 IDCompositionAnimation。咱们用 device 的 CreateAnimation 把它造出来，再往它的里面描述分段：AddCubic 加一段三次曲线的，线性段走的也是它，把二次与三次项都填零、只留一次项就是了，AddSinusoidal 加正弦的，曲线参数的细则您查文档就好，本篇的骨架只保证形状对。曲线造好了，把它递给属性的动画重载：SetOffsetX 收定值的那个重载咱们已经用过，它的兄弟收的正是 IDCompositionAnimation，挂上了之后，属性在某时刻的值就由曲线说了算。[《动画与帧调度》](34_animation_frames.md)把整条路走全了，还跟重画版摆了一张对照表，表里写得比咱们这儿全，您移步那篇看全貌就好。那篇还立过一条泛化：动画不只是 visual 的待遇，变换对象的属性与不透明度这些标量，普遍都有同样的动画重载，透明度的重载则挂在效果组的 SetOpacity 上。

什么时候该挂曲线，什么时候咱们自己喂值，这个选择咱们也交代一下。曲线适合从头到尾都定得下来的动画，滑进滑出、淡入淡出都是它的地盘。而跟随鼠标的拖拽就不一样了，鼠标的位置是一件一件的事件，下一个位置在 Commit 的时候咱们还不知道，所以拖拽的正路是每一趟事件里 SetOffsetX 定值加 Commit，画面跟手就是它了。两种活法的帧节奏，[《动画与帧调度》](34_animation_frames.md)里都有现成的讨论，咱们这里就不重复了。

曲线的时间从哪儿起算，文档的口径咱们也交代：Commit 被处理的那一刻，就是曲线时间轴的零点，各段起点的偏移都是相对它说的，而且偏移只填得下零和正数。所以咱们想让动画晚半秒出发，把头一段的偏移填 0.5 就是了，定时器是一个都不用的。

WinRT 的那边还有一位近亲，它的名字是 Windows.UI.Composition，思想是同源的，关键帧和缓动函数都包得挺省事的，UWP 的应用里见得多。咱们写桌面 Win32 的，DirectComposition 就是咱们的本路，混个脸熟也就够了。

## 树交给 DWM 之后的日子

树搭好了，Commit 也交了，它在机器里过的是什么日子？上一篇咱们看过 DWM 每帧的活，收集、算遮挡、叠合、送出的四步。咱们这棵树注册进 DWM 之后，就是收集那一步里多出来的一位供货方：咱们这棵树的输出，也就是每棵子树按变换和透明度算完的一块画面，被 DWM 当成了窗口画面的一部分，与桌面别家的照片一起拼进整帧。应用侧唯一的通信就是 Commit，参数不变的帧里，咱们的进程完全不用醒。

挂树的路线咱们认两条，差别就落在那个样式位的身上。不带 `WS_EX_NOREDIRECTIONBITMAP` 的普通窗口，重定向的表面还在，topmost 定的是咱们的树与子窗口谁上谁下，老用法是一点也没破坏的，窗口的客户区还是归 GDI、D2D 随便画，咱们的树只管叠加的那一层，而且它永远叠在直接画的内容之上。等咱们带上这个样式位之后，重定向表面就没有了，窗口的全部画面都出自咱们的树。上一篇预告过的代价，视觉树、样式、无障碍得咱们自己接，全落在这里了，本篇再补一笔它没点到的：背景得咱们自己来铺，树上没内容的地方透明度是零，透出来的就是底下别家的画面。本篇的骨架走的是全接管的路线，窗口干脆用无边框的，省得标题栏的绘制纠缠进来。

全接管的路线能做出几样经典的用法，咱们认其中的三样。头一样就是本篇骨架做的：内容画一次，位移动画是独立于内容的，面板、抽屉、通知条的滑进滑出全算这类。第二样是闪屏层：启动的时候拿一张静态图铺满 visual，主窗口的初始化在底下慢慢走，图是静态的，重画的活一点也不占，等层撤掉了，窗口接上的正是自己的内容。第三样是视频叠加：解码的输出画进合成交换链，咱们以 visual 把它叠在界面别层之上，解码线程刷它的内容，UI 线程动它的摆位，两条线各干各的活，谁也不欠谁的帧。您在系统里看到的窗口最小化动画、开始菜单的进出场，背后动的也是同一套合成参数。系统内部具体走的是哪条通道，微软其实没有公布，咱们就不替它下判断了。

DWM 把 visual 树合成进整帧的具体调度，树与重定向表面拼合的顺序细节，微软同样是没有公布的，咱们把模型层面的说法当通识标在这里：树是窗口画面的一位供货方，每帧的合成都少不了它，应用只在 Commit 的时候交一次参数。

开销的事咱们也顺着上一篇的思路提一句：动画虽然不占应用的 CPU，合成的活却是实打实的，树挂深了、层叠多了，DWM 每帧要拼的东西就越多。所以 visual 也不是越多越好，能一张画面解决的事，咱们就不必拆成十层，上一篇讲过的减法道理，在咱们自己的树上照用。

路线的选择咱们给一句实在的建议：普通的工具类界面，用普通窗口加重定向表面就够了，树只挂真正要动的那几层。内容全是自绘、又要异形又要逐像素透明的场景，才值得上全接管的路线。

## 动手骨架：画一次，挂上去，让它自己滑

骨架的活咱们分四段摆，一段一段地往下走。设备那段的创建代码在[《特效：Effect 图》](11_d2d_effects.md)里全程走过，咱们这里压着走。整篇的代码都没有编译验证过，它管的是把形状画对。

头一段的活咱们来备设备。D3D 的设备带着 BGRA 的支持标志建起来，再用 QueryInterface 问出 DXGI 的设备，D2D 的工厂、设备、上下文按老接线建齐：

```cpp
// Segment 1: the device chain from the effects tutorial - not compiled here.
ComPtr<ID3D11Device>       d3d_device;
ComPtr<ID2D1DeviceContext> d2d_dc;    // factory / D2D device elided
// D3D11CreateDevice(..., D3D11_CREATE_DEVICE_BGRA_SUPPORT, ...)
// d3d_device->QueryInterface(IID_IDXGIDevice, &dxgi_device)
// D2D1CreateFactory -> CreateDevice(dxgi_device) -> CreateDeviceContext
```

第二段咱们造的是合成交换链，咱们再让 D2D 把面板画进去。窗口是拿 `WS_EX_NOREDIRECTIONBITMAP` 创出来的无边框窗口，它的画面从此就全归咱们的树了：

```cpp
// Segment 2: a composition swap chain, painted once - not compiled here.
DXGI_SWAP_CHAIN_DESC1 sc{};
sc.Width            = 300;                    // the panel, not the window
sc.Height           = 200;
sc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
sc.SampleDesc.Count = 1;
sc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
sc.BufferCount      = 2;
sc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
sc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;   // per-pixel alpha

ComPtr<IDXGIFactory2>    dxgi_factory;
CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&dxgi_factory);
ComPtr<IDXGISwapChain1>  swap_chain;
dxgi_factory->CreateSwapChainForComposition(
    d3d_device.Get(), &sc, nullptr, swap_chain.GetAddressOf());

// Paint once via the effects-tutorial wiring:
//   GetBuffer -> CreateBitmapFromDxgiSurface -> d2d_dc->SetTarget
//   -> FillRoundedRectangle ... -> EndDraw
// then swap_chain->Present(1, 0);
```

交换链的宽高，咱们是跟着面板走的，是用不着跟窗口一样大的，因为咱们的内容就是这一块面板。AlphaMode 给了 PREMULTIPLIED，面板圆角外的地方透明度是零，透出来的就是底下的桌面。画完之后咱们 Present 一次，内容进了前台缓冲，从这一刻起 D2D 就可以收工了，后面的整个动画期，它是一次都不用再画的。

还有一处坐标的细节咱们点一下：根 visual 的坐标系，原点落的就是目标窗口客户区的左上角，骨架里 OffsetY 的 120，说的就是离客户区顶边 120 DIP。子 visual 的原点是跟着父节点走的，咱们前面认过了，两层叠了起来，排布心里就有数了。

SwapEffect 填的 FLIP_SEQUENTIAL，是[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)讲过的翻转家族，合成交换链与连窗口的交换链用的是同一套翻转模型，《交换链与呈现（DXGI）》里的现成讲法咱们直接搬过来用。

第三段咱们搭的是树，三样的对象一次建齐，咱们把 Content 装上交换链：

```cpp
// Segment 3: device, target, visual - not compiled here.
ComPtr<IDCompositionDevice> dcomp_device;
DCompositionCreateDevice(dxgi_device.Get(), __uuidof(IDCompositionDevice),
                         reinterpret_cast<void**>(dcomp_device.GetAddressOf()));

ComPtr<IDCompositionTarget> target;
dcomp_device->CreateTargetForHwnd(hwnd, /*topmost*/ TRUE, target.GetAddressOf());

ComPtr<IDCompositionVisual> visual;
dcomp_device->CreateVisual(visual.GetAddressOf());
visual->SetContent(swap_chain.Get());
visual->SetOffsetY(120.0f);            // vertical placement, fixed
target->SetRoot(visual.Get());
```

第四段咱们来放动画。曲线的值是绝对的，起点要写进曲线自己的常数项：从左外侧的 -300 走到落位的 0，走的就是 300 DIP。基值只管动画挂上之前与取消之后的位置，咱们把 Commit 一交，DWM 就接管了后面每一拍的插值：

```cpp
// Segment 4: hand the curve to the compositor - not compiled here.
ComPtr<IDCompositionAnimation> slide_in;
dcomp_device->CreateAnimation(slide_in.GetAddressOf());

// One cubic segment, linear term only: x(t) = -300 + 1000*t (DIP).
// It covers the 0.3 s slide from -300 to 0, then End holds it at 0.
slide_in->AddCubic(0.0, -300.0f, 300.0f / 0.3f, 0.0f, 0.0f);
slide_in->End(0.3f, 0.0f);

visual->SetOffsetX(-300.0f);           // base value: used before/after the animation
visual->SetOffsetX(slide_in.Get());    // the curve takes over from here
dcomp_device->Commit();
```

从 Commit 落定到动画走完的这段时间，咱们没有一次重画，没有一条 `WM_PAINT` 的消息，也没有一次 Present 的调用，动画独立于内容的样子，就这样落到了咱们手上。曲线走到 0.3 秒之后属性就停在终点值上了，能停得安稳靠的是 End 收的尾，没有 End 的话，最后一段会按自己的斜率一直走下去，面板就滑出画面了。想让它退出去的话，咱们再造一条反向的曲线挂上去就好。咱们想中途停掉动画的话，路子还是咱们熟悉的赋值老路：咱们给属性再赋一个定值再重新交一次 Commit，属性的最终解释权就回到了咱们手里，具体的替换细则，请您以文档为准。

您可能注意到《动画与帧调度》的骨架挂的是 translate 变换，本篇挂的是 visual 自己的 OffsetX，两条路都是合法的：标量属性成对的设计，visual 的 OffsetX 也好，变换对象的 OffsetX 也罢，都在它的照顾范围里。选哪一条看您顺手，咱们把位移动画挂 visual 上，把组合的变换挂对象上，慢慢也就成了咱们的习惯。

## 您可以自己动手看的三处

您要是把骨架补成了能跑的版本，头一处值得看的就是动画那两行的对照。把挂曲线的那行注释掉，只留定值的 SetOffsetX 加 Commit，您看到的会是一次跳变的画面，咱们再把曲线放回去，看到的就换成了滑动。两版之间咱们一行重画代码都没写，差别全在合成器怎么消化这个属性。

另一处看的是 Present 的位置。您把第二段代码末尾的那次 Present 注释掉，画面就出不来了，树也是挂着的，Commit 也交了，可交换链的前台缓冲里还是空的，所以屏幕上什么都没有。这个观察能帮您把分工记牢：Present 管的是内容进缓冲，Commit 管的是参数进合成，两趟班车是各走各的道。

第三处咱们请 PresentMon 出场。您开着它录一段动画，事件流里呈现最勤快的就是 DWM 的合成，而咱们自己的进程在动画期里是安静的，一头是忙的，一头是闲的，动画归了合成器这句话就有了读数。PresentMon 的用法[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)里介绍过，咱们这里就不重贴了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来了，答不上的那一段，就值得您回去再看一遍。

1. 面板滑动在老路线上每一拍要走哪些活？DComp 路线把这些活归给了谁？
2. IDCompositionDevice、IDCompositionTarget、IDCompositionVisual 各管什么？CreateTargetForHwnd 对窗口有什么要求？
3. visual 的 Content 能装什么？合成交换链与连窗口的交换链，分别在哪一步分道？
4. 属性改动从调用到上屏，中间隔着哪一步？一次 Commit 的原子性指什么？
5. 动画挂上之后，每一帧的插值由谁完成？应用还欠不欠每一帧的活？
6. topmost 参数与 `WS_EX_NOREDIRECTIONBITMAP` 各自决定什么？两条挂树的路线各自保留了什么？
7. 合成交换链的 AlphaMode 填 PREMULTIPLIED，换来了什么能力？
8. 子 visual 与 HWND 子控件，排布的顺序和坐标各自怎么算？visual 收得到消息吗？
9. 动画期间应用省下的开销，合成的开销谁来付？层的多少跟开销是什么关系？
10. 曲线各段起点的偏移相对的是哪个时刻？想让动画晚半秒出发，改的是哪里？

## 下一篇

下一篇的题目是[《图层与裁剪》](32_layers_clipping.md)。它接的是本篇留下的话头：Clip 咱们只认了矩形一档，圆角裁剪是它的正题，不透明度叠出的效果也归它对照着展开。Content 的更多来源，本篇只带了一句名目，等哪篇真用得上合成器侧的表面，咱们再把它接全。
