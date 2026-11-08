---
title: 交换链与呈现（DXGI）
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 交换链与呈现（DXGI）

按[篇目索引](index.md)的登记，本篇在 Direct3D 这一批文章里排的是第五位。前面四篇咱们把链接直接挂过去：[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)与[《D3D11 设备与立即上下文》](24_d3d11_device.md)（下文咱们简称设备篇），剩下的[《资源与视图》](25_d3d11_resources.md)与[《着色器与常量缓冲》](26_d3d11_shaders.md)也都在了，下文咱们分别简称它们为资源篇与着色器篇。紧挨着的下一篇是[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)。

本篇还背着两处欠着的债。[《屏幕、帧与垂直同步》](02_screen_frame_vsync.md)（下文咱们简称 02 篇）的结尾留了四笔：PresentMon 这个观测工具怎么拿怎么跑，独占全屏的真假怎么判，Present 的 SyncInterval 逐值的含义，还有 `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` 这个长名字的旗标换到了什么。每一笔咱们今天都兑现。[《实战：Win32 窗口跑 Skia 渲染循环》](18_skia_win32_loop.md)（下文咱们简称 18 篇）的收尾也留了话，说 Present 的参数、翻转模式的几副面孔、全屏的切换到本篇细看。兑现的路上，02 篇讲过的翻页、消隐期与排队的帧，全都会在 API 的面上露脸。

事实的底本有两侧。一侧是书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[D3D11 初始化与交换链章](../native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md)，另一侧是微软的官方文档，Present、ResizeBuffers 与 GetFrameLatencyWaitableObject 这几页咱们逐字对过，原文的链接挂在文中相应的位置，您想核对随时能点过去。本篇的代码全部是讲解用的骨架，笔者没有在本仓库里编译验证过，您真要上手的时候，以手上的 SDK 为最后的依据。

## DXGI：Direct3D 底下的那层地基

DXGI 的位置，咱们沿[《特效：Effect 图》](11_d2d_effects.md)（下文简称 11 篇）借书架打过的比方说：它站在 Direct3D 与显卡驱动的中间当一座桥，资源的呈现是它的正业。它的全名是 DirectX Graphics Infrastructure，拿白话讲就是 DirectX 的图形基础设施，D3D10、D3D11 与 D3D12 三代共用的都是它。咱们在设备篇已经从它手里过了一件家当，枚举适配器的 `EnumAdapters1`，本篇要过的第二件才是它的主业，交换链连同全屏的切换，都记在 DXGI 的名下。

书架的 DXGI 一节也给过一份职责清单，咱们借来对一遍：枚举 GPU 适配器、取显示器输出、管理交换链、处理全屏与窗口模式的切换，四样活全在 DXGI 的名下。咱们把这四样跟本篇的对上，本篇的主角是第三样，第四样的收尾留到全屏一节。

分工咱们一句话摆平：D3D 管的是画，画完了的东西怎么出门、跟屏幕的节拍怎么对齐，都是 DXGI 的活。创建的路线也有两条。11 篇走的是分开的路，设备由 `D3D11CreateDevice` 交到咱们手里，工厂再把交换链另建了一份。书架初始化章走的是合并的路，`D3D11CreateDeviceAndSwapChain` 一次调用就把设备与交换链都造齐了，配的是老一代的描述结构 `DXGI_SWAP_CHAIN_DESC`。本篇沿用的是 11 篇分开的路，描述结构换成新一代的 `DXGI_SWAP_CHAIN_DESC1`，两条路的差别，咱们在建链的段落里带着对照。

## 建链：CreateSwapChainForHwnd 与它的描述结构

咱们把 11 篇那次建链的代码请回来。当时它只是 D2D 路线的地基，字段的含义咱们没有逐个展开，现在咱们把它补齐：

```cpp
#include <dxgi1_2.h>   // IDXGIFactory2, CreateSwapChainForHwnd
#pragma comment(lib, "dxgi.lib")

RECT rc;
GetClientRect(hwnd, &rc);

DXGI_SWAP_CHAIN_DESC1 sc{};
sc.Width            = rc.right - rc.left;
sc.Height           = rc.bottom - rc.top;
sc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;  // the format D2D wants
sc.SampleDesc.Count = 1;
sc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
sc.BufferCount      = 2;
sc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
sc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;

IDXGIFactory2* factory = nullptr;
CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&factory);
HRESULT hr = factory->CreateSwapChainForHwnd(
    g_pD3DDevice, hwnd, &sc, nullptr, nullptr, &g_pSwapChain);
factory->Release();
```

按[官方的 CreateSwapChainForHwnd 文档页](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)，头一个参数收的是 `IUnknown*`，咱们把 D3D 设备直接递进去就行，接着的参数是窗口句柄，末参数交还的是 `IDXGISwapChain1*`。11 篇交代过一处与书架原文的出入：书架把指针声明成了 `IDXGISwapChain*`，按签名的型号是编译不过的，细节本篇就不重念了。剩下的两个是可选参数，一个收的是全屏专用的描述结构，一个限定的是内容只显示到哪个输出设备，起家咱们都传 `nullptr`，用的是窗口模式。

描述结构里的字段，咱们挑要紧的逐个认。`Width` 与 `Height` 定的是后台缓冲的初始尺寸，量的是窗口客户区。`Format` 填的 `DXGI_FORMAT_B8G8R8A8_UNORM` 是老熟人了，资源篇教过咱们把这类名字读成说明书，Windows 的 GUI 世界流通的是 BGRA。`SampleDesc.Count` 填 1 是翻转模型的硬约束，链上开不了多重采样，抗锯齿的活得走离屏渲染的路，本篇就不展开了。`BufferUsage` 要填的是 `DXGI_USAGE_RENDER_TARGET_OUTPUT`，“当画布”的用途被写在了门牌上，思路跟资源篇的 BindFlags 是一路的。

`BufferCount` 是 02 篇缓冲概念的落点。填 2 得到的就是双缓冲，填 3 对上的就是 02 篇讲的三重缓冲，挂屏幕的、候翻页的、握在程序手里接着画的各占一块，正是 02 篇说的那幅“手不停工”的图景。翻转模型对块数的要求是至少 2 块，块数往上最多还有 16 块的余地。多加一块缓冲的话，显存上就多付了一份，排队的容量也深了一格，02 篇说的“缓冲越多的画面越顺，玩家看到的世界就越滞后”，落到这里就成了具体的数字。`SwapEffect` 是本篇的主角之一，咱们单开一节去讲。`AlphaMode` 填的是 `DXGI_ALPHA_MODE_IGNORE`，它的意思是画面不透明，想要带透明通道的窗口画面得走 `CreateSwapChainForComposition`，那是[《DirectComposition：visual 树》](31_directcomposition.md)一篇的正事，它讲的是怎么把这样的画面挂进合成器的 visual 树。

### 后台缓冲怎么变成画布

着色器篇的绘制代码里用过 `renderTargetView`，它包着的那块后台缓冲的来历，咱们当时欠下了，现在咱们还上。还的就是 `GetBuffer`：

```cpp
ID3D11Texture2D* back = nullptr;
g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
g_pDevice->CreateRenderTargetView(back, nullptr, &g_pRenderTargetView);
back->Release();  // GetBuffer AddRefs the buffer; pair it with Release
```

`GetBuffer` 的头一个参数是缓冲编号。blt 老模型上 0 号就是当前的后台缓冲，咱们取它最稳。翻转家族的缓冲每翻一页就轮换一格，想按号取的话，18 篇用过的 `GetCurrentBackBufferIndex` 报的就是此刻轮到哪块。引用计数的事书架提醒过：`GetBuffer` 每次调用都会把缓冲的引用加一，用完的咱们必须 `Release` 配平，不然那块缓冲就永远等不到释放了。

翻转模型还有一条写在 [Present 文档备注里](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)的脾气，着色器篇咱们没有点破：一次成功的翻页之后，后台缓冲 0 就从管线上被解绑了，`OMSetRenderTargets` 设过的现场就没了，除非呈现的时候带上 `DXGI_PRESENT_DO_NOT_SEQUENCE`。所以每帧动笔之前，咱们都得把渲染目标重新绑一遍，少了这一步的话，Draw 画的就不是画布了，画面的去向就说不准了。

## SwapEffect：两家人两种交接

02 篇讲双缓冲的时候摆过两种交接的方案，一边是整块复制的方案，一边是换指针的方案。`DXGI_SWAP_EFFECT` 的四个常用值，正好按两种方案分成了两家人，02 篇结尾预告过的枚举名单，咱们现在逐个兑现。

blt 家族交接靠的是复制，成员的两位是 `DXGI_SWAP_EFFECT_DISCARD` 与 `DXGI_SWAP_EFFECT_SEQUENTIAL`。Present 之后的那一趟，画面是靠一次复制交到下一站的，血脉就是咱们在[《具象 · 脱掉封装，把窗口立起来》](../gui/01_naked_win32.md)（主线 gui 目录的第一篇，那个消息驱动的裸窗口 demo，下文咱们简称 01 篇）里用过的 `BitBlt` 整块贴图。两兄弟的差别只有一处：DISCARD 复制完之后后台内容就作废了，SEQUENTIAL 复制完之后内容是留着的。它们的处境咱们也清楚：DISCARD 是 D3D9 时代的默认，D3D10 与 D3D11 把它沿用了下来，而微软的翻转模型文档把 FLIP 系列列为新代码的推荐。其实老模型在今天的系统上还跑得动，可窗口模式下它的交接要多出一道复制，脏区一类的进阶能力也没有它们的份，咱们新写的代码，就没有再开它们的理由了。

老模型如今剩的还有一块正经用场，就是与 GDI 混合的画面。ResizeBuffers 的文档提过一笔，带 GDI 兼容旗标的链上还开着 DC 的话，咱们得赶在改尺寸之前把 `ReleaseDC` 调了，下文 PresentMon 一节的判读表里，`Composed: Copy with GPU GDI` 与 `Composed: Copy with CPU GDI` 两条说的就是它的样子。D2D 与 GDI 的互操作咱们在[《与 GDI 互操作》](12_d2d_interop.md)里走的是另一条路，本篇就不展开了。

flip 家族咱们接着认，它交接靠的是换指针，成员的两位是 `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` 与 `DXGI_SWAP_EFFECT_FLIP_DISCARD`，走的正是 02 篇讲的翻页：缓冲的位置原地不动，输出的指向一换就生效了。翻转模型的窗口画面还与 DWM 共享同一批表面，交到合成器的那道复制也省下了。两兄弟的差别同样只有一处：FLIP_SEQUENTIAL 在翻页之后保住后台缓冲的旧内容，程序可以只重画变化过的那部分。FLIP_DISCARD 翻页之后后台的内容未定义，每帧都得整帧地重画。

11 篇当时原样填进去的值就是 FLIP_DISCARD。GUI 程序的每帧本来就要整帧重画，保内容的能力咱们用不上，名字读通了之后，咱们填它凭的就是自己的判断，而不是示例代码的惯性。四张面孔咱们并成一张表，您往后翻回来查：

| 值 | 交接方式 | 翻页后的后台内容 | 处境 |
|:---|:---|:---|:---|
| `DXGI_SWAP_EFFECT_DISCARD` | 复制（blt） | 作废 | D3D9 时代的默认，新代码不推荐 |
| `DXGI_SWAP_EFFECT_SEQUENTIAL` | 复制（blt） | 保留 | 同上 |
| `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` | 换指针 | 保留，可只重画变化过的区域 | 翻转模型 |
| `DXGI_SWAP_EFFECT_FLIP_DISCARD` | 换指针 | 未定义，需整帧重画 | 翻转模型，推荐值 |

## Present 的 SyncInterval：逐值数过去

咱们在 11 篇与 18 篇里都写过 `Present(1, 0)`，现在咱们把两个参数摊开来看。头一个参数 SyncInterval 管的是翻页跟消隐期怎么对齐，另一个参数 Flags 管的是杂项，咱们放到下一节讲。SyncInterval 的语义随 SwapEffect 分家，[官方的 Present 文档](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)是分开写的，咱们照着念。blt 模型下的 0 表示立即呈现、不同步，1 到 4 表示的是等到第 N 个垂直消隐期再呈现。翻转模型下的 0 要更凶一点：它取消的是当前帧剩余的展示时间，队列里要是已经排了更新的帧，这一帧就直接丢掉了，1 到 4 表示的是至少等 N 个消隐期再翻。

Present 还有一个带脏区参数的兄弟 `Present1`，从 Direct3D 11.1 起才有的，滚动条一类局部更新的场景用它省带宽，GUI 的日常咱们用不上，您认个脸就行。

| SyncInterval | 行为 | 60 Hz 屏幕上的帧率顶 |
|:---:|:---|:---|
| 0 | 不等班车，立刻提交 | 跟着程序走，多交的被合成器合并 |
| 1 | 每个消隐期翻一页 | 60 |
| 2 | 每 2 个消隐期翻一页 | 30 |
| 3 / 4 | 每 N 个消隐期翻一页 | 20 / 15 |

咱们逐值解读。0 对应的是 02 篇“关掉垂直同步手感变快”的档位，提交完了就走，队也跟着排浅了。可窗口模式里屏幕那一头仍按刷新的节拍取样，多交的帧会被合成器合并掉，等于白画的就是这些，所以帧率的数字可以冲得很高，画面的更新却还是原来的节拍。其实撕裂也不会因为 0 就冒出来，想真见到它的话得动 Flags，下一节咱们接着见。1 是常规的档位，02 篇讲的班车定点发车就是它的脾气：错过一班就得整个周期地等下一班，帧率从 60 一脚踩到 30 的台阶，源头就在它的身上。2 到 4 是主动挑低档的档位，视频播放器一类的省电场景拿它把帧率的顶压到内容自身的节奏。代价是每一帧在队里候场的时间乘了 N，输入延迟跟着也就涨了。02 篇的那句“排在前面的帧越多，屏幕一幅一幅放过去的时间就越长，您这一敲才轮得到上屏”，说的正是这个方向。

Present 文档的备注里还给了一个队列的例子，咱们原样转述。队列里从旧到新排着的是五帧，它们的 SyncInterval 依次是 3、0、0、1、0。运行时让头一帧只展示一个消隐期就提前收了场，因为后面跟着的 0 要插队，中间两帧带着 0 的被整个丢弃，1 的那帧展示一个消隐期，末尾 0 的那帧一直展示到下一次提交。0 的“插队”脾气在文档的例子里看得清清楚楚，其实它排队排得并不安静，是要把前面那帧的展期拦腰砍断的。02 篇的三重缓冲图景，在 API 面上凑齐就是两个数字的事：`BufferCount` 填的是 3，SyncInterval 用的是 1，屏幕挂一块、一块候着、手里画一块的图就活了。

调用的位置，咱们也顺着说一句。11 篇的窗口程序里，Present 站在 OnPaint 的末尾，跟着画的节奏走，消息不来的时候它也就不动。18 篇的循环里，Present 站在每圈的末尾，节奏是循环自己带的。写法各有各的样子，SyncInterval 的含义却是同一个：翻页跟消隐期的对齐方式，不看您把它放在哪儿。至于动画的重画怎么按节拍安排，那是[《动画与帧调度》](34_animation_frames.md)的正事，您到那边读。

## Present 的 Flags：TEST、ALLOW_TEARING 与返回值

Flags 里咱们认三个值，外加返回值的老朋友。`DXGI_PRESENT_TEST` 不真的呈现，它问的是一句“现在的帧要是交了，用户看得见吗”。它跟另一个返回值是配着用的：窗口被整个挡住或者最小化的时候，Present 报回来的就是 `DXGI_STATUS_OCCLUDED`，咱们拿到它就知道自己转入了后台，之后的每帧都拿 `Present(0, DXGI_PRESENT_TEST)` 探一下，探回来的还是遮挡的话，这一帧的画活就全省了。文档还给了个来历：它在 D3D9 里的同族角色是 `TestCooperativeLevel`。

`DXGI_PRESENT_ALLOW_TEARING` 补的是自己代码里复现撕裂的路子，02 篇文末给的是游戏侧的办法，找一款允许设置垂直同步的游戏关掉跑一段。它要的是三件套，缺了哪一样都不行：建链的时候带上 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`，用 `IDXGIFactory5` 的 `CheckFeatureSupport` 查 `DXGI_FEATURE_PRESENT_ALLOW_TEARING` 确认机器支持，呈现的时候调 `Present(0, DXGI_PRESENT_ALLOW_TEARING)`。它配的只是 SyncInterval 0，也是 02 篇认过的可变刷新率显示器（VRR）的必修项。`DXGI_PRESENT_RESTART` 咱们一句带过：帧的节奏断过之后，比如窗口刚从遮挡的状态里回来，带上它等于请链把节奏重排了一遍。

返回值的部分还有设备篇的老朋友。Present 失败报 `DXGI_ERROR_DEVICE_REMOVED` 一类错误的时候，咱们就去问 `GetDeviceRemovedReason`，把家当整个放掉、从头重建的处置，设备篇给过完整的代码，本篇就不重贴了。

## 排队的上限与等待对象：把闸门拿到手里

Present 的队列里还有一道咱们看不见的闸门：DXGI 允许程序在前头排的帧，默认的上限是 3 个，`SetMaximumFrameLatency` 的文档记着这个默认值。排满之后 Present 调用本身就地阻塞，等于拿它当了节流阀。02 篇把输入延迟的机制讲清过：排在前面的帧越多，您的按键就要等越多帧之后才上得了屏，3 帧的默认容量，图的是程序的产量，牺牲的却是手感。

`DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` 这个长名字的旗标，换的就是这道闸门的手柄。建链的时候在 `sc.Flags` 里带上它，队列深度的刹车就从 Present 的内部挪到了咱们自己的手里。用法的接口都挂在 `IDXGISwapChain2` 的名下（头文件是 dxgi1_3.h，从 Windows 8.1 起才有的），咱们分两步走：

```cpp
// created with DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT in sc.Flags
IDXGISwapChain2* sc2 = nullptr;
g_pSwapChain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2);
sc2->SetMaximumFrameLatency(1);   // queue at most 1 frame ahead

HANDLE frameLatency = sc2->GetFrameLatencyWaitableObject();
// CloseHandle(frameLatency) before the swap chain is destroyed
```

每帧的循环里，咱们在动笔之前等它：

```cpp
while (running) {
    WaitForSingleObjectEx(frameLatency, INFINITE, FALSE);  // wait BEFORE drawing
    process_input();
    render_frame();
    g_pSwapChain->Present(1, 0);
}
```

[GetFrameLatencyWaitableObject 的文档页](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)把前两条要求写得很死：每一帧开始任何绘制之前，咱们都得在句柄上等一次，头一帧也是要等的。句柄用完的时候还要 `CloseHandle`，出处是微软自家的延迟示例，文档页里咱们是找不到的。输入延迟的下降，咱们把从按键到上屏的这段路串一遍就明白了：排队上限压到 1 之后，程序前头候着的最多就剩一帧了，“排队等班车”的那一段被砍薄了一层，按键到发光的路就短了。代价也是明摆着的：排队的缓冲没了余量，哪一帧的活要是拖长了，空档就直接露到帧率上了。竞技游戏宁可要跟手的快，视频播放器多半宁可要画面的顺，怎么选的权衡，还是 02 篇的那句话，看您的产品要什么。

18 篇的循环咱们回头看一眼，它带的链上没有这个旗标。18 篇自己从 SyncInterval 的角度说它，节流的活交给 `Present` 的 SyncInterval 1，落到队列这边就是 Present 排满之后的就地阻塞，两句话说的是同一道闸的两面，也就是本节开头说的那道默认闸门。两种刹车咱们都认过了，写 GUI 程序的时候，拿消息驱动的节奏配默认的闸门也就够了，想抠手感的动画循环，再考虑把闸门接到自己的手里。

## 窗口尺寸一变：ResizeBuffers 怎么调

后台缓冲的尺寸是建链的时候定下的，窗口自己变了尺寸，缓冲的尺寸却不跟着变。不调 `ResizeBuffers` 的话，翻上去的画面会被拉伸去填新窗口，糊不糊您一眼就看得出来。调用它有一条硬性的要求，[官方的 ResizeBuffers 文档](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)写得明白，咱们必须放干净对后台缓冲的全部引用。直接的引用是咱们手里 `GetBuffer` 拿到还没放的指针。间接的引用是 RTV、深度视图这些还攥着缓冲的对象，视图还绑在上下文上的话也算数。拿不准的话，咱们就调一句 `ClearState`，把管线的绑定整个清一遍。书架的排障清单可以作证：ResizeBuffers 报回来的码是 `DXGI_ERROR_INVALID_CALL`，十有八九是哪个引用没放干净的缘故。11 篇 D2D 路线里 `WM_SIZE` 的那句 `SetTarget(nullptr)`，干的就是同一件事，把还引用着后台缓冲的画布摘下来。

参数怎么传的问题，文档对每一处的说法都有记录，咱们把它读成省事的一套：块数的参数 `BufferCount` 传 0，块数保持的仍是原样，宽和高传 0 的时候，DXGI 自己会去取窗口客户区的新尺寸，格式的参数传 `DXGI_FORMAT_UNKNOWN`，格式保持的也是原样。翻转模型还压了一条下限下来，块数低于 2 是不行的。

```cpp
case WM_SIZE:
    if (wParam == SIZE_MINIMIZED || g_pSwapChain == nullptr) {
        return 0;  // zero-size client: nothing to resize into
    }
    g_pContext->OMSetRenderTargets(0, nullptr, nullptr);  // unbind the RTV
    SafeRelease(&g_pRenderTargetView);                    // indirect ref gone
    // a depth/stencil view would be released here as well
    g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

    // The buffers have rotated by now: fetch whichever page is
    // current, not a hard-coded index.
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(g_pSwapChain->GetCurrentBackBufferIndex(),
                            __uuidof(ID3D11Texture2D), (void**)&back);
    g_pDevice->CreateRenderTargetView(back, nullptr, &g_pRenderTargetView);
    back->Release();

    D3D11_VIEWPORT vp{0, 0, static_cast<FLOAT>(LOWORD(lParam)),
                      static_cast<FLOAT>(HIWORD(lParam)), 0.0f, 1.0f};
    g_pContext->RSSetViewports(1, &vp);
    g_pContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
```

调完之后要补的事都并进了上面的代码：`GetBuffer` 的指针重取一遍，RTV 照着资源篇给过的四步重建，深度缓冲与视口是那四步之外本篇再补的两样，末了咱们再登记一次重画。`SafeRelease` 是前面文章用过的释放帮手，替咱们放掉引用并把指针置空，代码里就不用再手工 `Release` 了。文档还留了一条容易漏的：Windows 8 之后的翻转模型，进出全屏的时候也都得 `ResizeBuffers` 一遍，不然的话，下一次的 Present 调用会直接失败，本篇的全屏一节咱们还会碰到它。

守卫里那句 `SIZE_MINIMIZED` 的判断，跟 01 篇裸 demo 里判最小化是同一个道理的两副写法，窗口最小化的时候客户区是零尺寸，`ResizeBuffers` 传 0 让 DXGI 自己取尺寸的路数，在零尺寸的窗口上取回来的也是零，所以咱们干脆跳过。

## PresentMon：把呈现的链路照亮

02 篇预告过的观测工具，咱们在本篇兑现。PresentMon 是 Intel 开源的呈现链路观测工具，仓库在 GitHub 的 [GameTechDev/PresentMon](https://github.com/GameTechDev/PresentMon) 名下，用的许可证是 MIT。它的 README 里还列了一串在它之上搭起来的工具，微软 PIX 的 System Monitor 也在名单里，它跟 PresentMon 的对照，咱们到[《性能观测：PIX、RenderDoc、ETW》](36_performance_tools.md)里再比。它凭什么看得见呈现呢？Windows 里有一套内核级的事件跟踪设施 ETW（Event Tracing for Windows），D3D 与 DXGI 的运行时干活的时候会往外发事件，PresentMon 订阅的正是这些事件，把每一次 Present 从提交到上屏的时间线拼了出来。

怎么拿、怎么跑的事，咱们接着交代。仓库的 Releases 页挂着现成的可执行文件，仓库里还带了一个图形界面的 Capture Application。控制台的版本要提着权跑：ETW 会话不是随便哪个进程都能开的，README 的排障节写明，用户不在 Performance Log Users 组里的话，吃到的就是 access denied，拿管理员的身份跑是最省事的。命令长的是下面的样子：

```text
PresentMon-2.3.1-x64.exe --process_name app.exe --timed 10 --output_file trace.csv
```

三个开关各干各的活：`--process_name` 干的是按进程名过滤的活，`--timed` 限定的是录多少秒，`--output_file` 干的是把结果写进 CSV 的活（逗号分隔的表格文本，是 Excel 和脚本都能接着处理的格式）。CSV 的每一行对应一次 Present，咱们挑与本篇直接相关的几列认脸，名字按 README 的登记：`SwapChainAddress` 让咱们认得出一个进程里的多条交换链，`SyncInterval` 是程序递给 Present 的值，文档也注明了驱动可能改它，`PresentFlags` 原样记录的是 Flags，`MsBetweenPresents` 是两次 Present 之间隔的毫秒数，它的倒数就是帧率的数字，`MsUntilDisplayed` 是从提交到真正上屏隔的毫秒数，算得上延迟观测的头号指标，`MsClickToPhotonLatency` 量的是从一次鼠标点击到画面发光的路程。字段是随版本有增减的，您跑的时候，以随仓库的 README 为准。

README 里还有两个开关值得咱们记下：`--hotkey` 干的是用组合键遥控开始与停止的活，想录游戏现场的您用得上它，`--etl_file` 让咱们分析的是录好的 ETW 跟踪文件，咱们不守着活进程也能出 CSV。另外有一列 `PresentRuntime` 记录的是程序用的哪套图形 API，取值随程序用的 API 而变，OpenGL 与 Vulkan 的程序常报 Other。咱们在 02 篇认过的 Present、队列与翻页，其实不挑图形 API，PresentMon 的观测也就跟着不挑。

咱们拿到 CSV 之后，照两步读：头一步看的是 PresentMode，确认帧走的是哪条路，第二步再看 MsBetweenPresents 与 MsUntilDisplayed 的分布，节奏与延迟就都有了着落。

最值钱的一列是 `PresentMode`，它判定的是每一帧实际走了哪条路。README 的判读表咱们原样搬过来，咱们的读法加注在右边：

| PresentMode | 读法 |
|:---|:---|
| `Composed: Flip` | 窗口模式的翻转链，画面经 DWM 合成 |
| `Hardware Composed: Independent Flip` | 翻转链拿到了硬件叠加平面（多平面叠加），直通扫描输出 |
| `Hardware: Independent Flip` | 程序没拿到屏幕所有权，但翻的页直接换屏幕画面 |
| `Hardware: Legacy Flip` / `Hardware: Legacy Copy to front buffer` | 真独占：程序接管了屏幕 |
| `Composed: Copy with GPU GDI` / `Composed: Copy with CPU GDI` | blt 老模型的复制交接 |

PresentMode 的判读表是下一节判定真假独占的尺子，也是咱们照自己代码的镜子：11 篇里建过的翻转链跑起来，PresentMode 该停的地方是 `Composed: Flip`。笔者没有替您跑过，它实际停在了哪儿，您录上十秒，自己就看到了。“窗口模式下您不容易看到撕裂”，其实 02 篇自己就答过：可能被撕的那块内存，窗口程序的手是根本碰不到的。判读表给咱们的是它在 API 面上的对应，Composed 开头的路径，根本没有直写屏幕的机会。

## 全屏：真假独占与 independent flip

咱们进出全屏的口是 `SetFullscreenState`，头一个参数给的是 `TRUE` 或者 `FALSE`，查询的口是 `GetFullscreenState`。麻烦的是它报的布尔值，能说明的只有程序在不在全屏态，真独占与全屏优化的区分，它是不分的。

全屏优化的机制，咱们一句话交代：Windows 10 之后的系统里，程序请求全屏的时候，系统默认是不把屏幕真的交出去的，画面走的仍是合成的体系，但系统会给这样的窗口安排直通的翻页路径，显示的模式不动，alt-tab 的切换也用不着重新协商。独占时代的大部分收益留了下来，切换的代价却被拿掉了，老玩家记忆里的“独占全屏 alt-tab 卡一下”在新系统上消失了，原因就在它的身上。

判定靠谁？靠的是上一节的判读表。`GetFullscreenState` 报了 TRUE 之后，咱们开 PresentMon 看 PresentMode：以 `Hardware: Legacy` 开头的那些值，才是程序真正接管屏幕的老式独占。翻转链拿到硬件叠加平面的 `Hardware Composed: Independent Flip`，还有没拿到所有权的 `Hardware: Independent Flip`，都是全屏优化给的直通，屏幕的所有权还在系统的手里。落在 `Composed: Flip` 上的话，画面就是仍老老实实经 DWM 合成的。02 篇把判定的具体办法留给了本篇，兑现的就是这几句。

书架初始化章还给过一条实践的建议：建链的时候照窗口模式建，真要全屏的时候再用 `SetFullscreenState` 去切，比建链时直接要全屏的路子稳当，咱们照着办就好。

independent flip 到这里也该收官了。02 篇讲撕裂的时候留了个尾巴：无边框铺满屏幕的游戏画面，为什么一般不撕？机制现在咱们凑齐了。翻转模型加全屏或铺满的窗口，驱动在条件合适的时候会让 DWM 让出道来，程序翻的页就直接连到了扫描输出上，中间不再过合成的那一环，在 PresentMon 的判读表里，Independent Flip 一档的来历就是它。可它依然是不撕的，因为翻页的时机还是对着消隐期走的，扫描线从头到尾读的都是一整块缓冲，02 篇立的“整遍扫描期间读的那块内存没动过”的条件依然守着。撕裂想重见天日的话，还是得靠上一节的 ALLOW_TEARING 三件套，那是另一条合法的路。至于什么条件才算得上合适，02 篇说的是由驱动决定，文档没有给死的清单，咱们判定靠的还是事后看 PresentMode。

真想要老式独占怎么办？咱们有两条路。一条在系统的兼容性设置里：exe 的属性里关掉全屏优化，系统就不再替程序做主了。另一条跟建链的旗标有关：`DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH` 管的是切换全屏时允不允许真的去改显示模式，带上它的，模式切换的老路才通。前文埋过的要求，咱们在这儿再念一遍：Windows 8 之后的翻转模型，进出全屏的时候也都得 `ResizeBuffers` 一遍，不然的话，下一次的 Present 调用会直接失败。

## 您可以动手的三处

下面的三处咱们改起来都不重，每处换回来的都是数据：

1. 咱们拿 PresentMon 盯 SyncInterval。靶子用 18 篇那个跑循环的示例，把 Present 的 SyncInterval 从 1 改成 0、再改成 2，各录十秒的 CSV，对照着看 SyncInterval 列跟上了没有，MsBetweenPresents 的分布动了没有。01 篇那个消息驱动的裸 demo 拿来录也有意思：没有输入的时候它一帧都不出，CSV 里几乎是空的，两种程序钟的脾气，数据自己会说话。
2. 咱们判定一台机器的全屏。给自己的示例加一个键，按下去 `SetFullscreenState(TRUE)`，用 PresentMon 看 PresentMode 从 `Composed: Flip` 挪到了哪一档，再去 exe 的兼容性设置里关掉全屏优化重跑一遍，两档对照着看，真假独占您就亲手判过一回。
3. 咱们验证 ResizeBuffers 的失败。把 `WM_SIZE` 里 `SafeRelease` 那一行注释掉再缩放窗口，ResizeBuffers 报回来的码跟书架说的一样吗？亲手放掉引用再跑通一遍，要求就长在手上了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得回去再看一遍。

1. DXGI 与 D3D11 怎么分工？交换链为什么长在 DXGI 名下？
2. FLIP_DISCARD 与 FLIP_SEQUENTIAL 的差别落在哪？blt 家族靠什么交接，处境如何？
3. 翻转模型下 SyncInterval=0 的官方语义是什么？“插队”插的是谁的队？
4. 60 Hz 屏幕上 SyncInterval=2，帧率的顶是多少？输入延迟朝哪个方向变？
5. `DXGI_PRESENT_TEST` 配哪个返回值用？省掉的是什么活？
6. 想复现撕裂，三件套是哪三件？
7. ResizeBuffers 之前必须放干净什么？宽高传 0 是什么意思？
8. `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` 把哪个闸门交到了咱们手里？等句柄的动作放在每帧的哪一步？
9. `GetFullscreenState` 报了 TRUE，怎么判定是真独占还是全屏优化？independent flip 为什么直通了还依然不撕？
10. PresentMon 的判读表里，`Composed: Flip` 与 `Hardware Composed: Independent Flip` 各走的是哪条路？
11. 全屏优化把独占的哪些收益留下了，又把哪些代价拿掉了？
12. 书架的 `D3D11CreateDeviceAndSwapChain` 建出来的链，交接走的是哪种路子？
13. `DXGI_PRESENT_RESTART` 用在什么样的场合？

## 下一步

本篇的 Present 一句调用就把页翻了，这样的轻松是 D3D11 替咱们扛下来的，队列、同步、缓冲的轮换全藏在驱动的手里。D3D12 把这些一样一样还给了程序，翻页背后的队列与同步对象也得自己管，代价与收益的完整对照，咱们留给下一篇[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)，那篇会给咱们把这些逐项摊开对照。想马上动手的您也有现成的活：拿书架里 `D3D11CreateDeviceAndSwapChain` 的老路子建一条 DISCARD 链，画一个清屏的动画，咱们用 PresentMon 录上十秒，再换成 FLIP_DISCARD 的链对比一遍，两家人交接方式的差别落在哪一档 PresentMode，您看一眼 CSV 就见分晓。
