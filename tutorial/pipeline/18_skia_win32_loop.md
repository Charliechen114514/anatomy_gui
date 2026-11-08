---
title: 实战：Win32 窗口跑 Skia 渲染循环
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 实战：Win32 窗口跑 Skia 渲染循环

按[篇目索引](index.md)的登记，本篇在 Skia 段里排的是末位，收尾的活归咱们。咱们往前数一数，[《Skia 的文本：SkFont 与 TextBlob》](15_skia_text.md)、[《GPU 后端：DirectContext 与后端目标》](16_skia_gpu_backend.md)与[《SkSL：运行时着色器》](17_sksl.md)都跟您见过面了，16 篇收尾时咱们还留了话，说设备、交换链与完整的循环都归本篇交代。

段后咱们要进的是文本段的[《字符、码点与字形》](19_chars_and_glyphs.md)，它的稿子已经在等您。

D3D 那边咱们还要引用几篇：[《D3D11 设备与立即上下文》](24_d3d11_device.md)、[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)、[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)与[《动画与帧调度》](34_animation_frames.md)，正文的链接咱们到用得着的地方给。

本篇干的活，是把前面认过的零件拼成一台会转的机器：窗口是个 Win32 的窗口，每一帧都由 Skia 的 GPU 后端画出来，再经交换链翻上了屏幕。窗口与消息循环的底子来自主线第一篇的[裸窗口 demo](../gui/01_naked_win32.md)，主线指的是咱们在 `tutorial/gui/` 目录里走过的那套入门系列。设备的家当按 16 篇认下的那串创建顺序来装，咱们从设备、队列一路建到画布。动画的内容咱们也不新造，15 篇那行字加上 17 篇的着色器，正好一样顶一样地用。

代码的来路咱们摆在开头：本篇的代码是骨架，笔者没有把它编译运行过哪怕一段。它的依据分三层，Win32 的部分照着仓库里验证过的裸窗口 demo 的样子写，D3D12 与 DXGI 的调用照书架（`tutorial/native_win32/` 目录下那套带完整示例代码的老教程）的 D3D12 章节与 SDK 文档拼装，Skia 的签名沿用 16、17 两篇交代过的口径。

您把它接进项目时，以编译器与手上的头文件为准。API 的年代也交代一句：Skia 一侧以 2024 年前后的主干版本为准，新旧名字咱们并给，Windows 一侧以 D3D12 与 DXGI 的现行文档为准。

## 从阻塞的循环改成每圈必画

窗口是怎么立起来的，主线第一篇里咱们一步一步走过，注册窗口类、创建窗口、消息循环三件事的代码也在那边验证过，本篇就不重讲了，咱们只挑要紧的两处改。

头一处改的是背景刷：demo 注册窗口类时给 `hbrBackground` 填的是系统颜色，咱们的窗口类骨架里把它填成 `nullptr`，因为擦背景的活由每一帧的整幅绘制接管了。demo 里还得在 `WM_ERASEBKGND` 里返回 1 拦住系统的擦除，咱们这边连消息都不用接，渲染目标全程待在 GPU 的纹理里，GDI 的背景刷根本没机会进场。

真正要动的是消息循环。demo 里的 `GetMessageW` 是阻塞的，队列里没有消息它就睡着了，这正合[《屏幕、帧与垂直同步》](02_screen_frame_vsync.md)里讲过的那套脾气：窗口程序的绘制由消息驱动，用户不碰它就一帧都不出了。可咱们的动画每圈都要交一帧，睡着的循环给不了节奏，所以咱们把循环换成 `PeekMessageW` 的样子：

```cpp
// The animation loop: drain pending messages, then draw one frame.
// GetMessageW sleeps when the queue is empty; PeekMessageW returns
// immediately, so the loop keeps its own beat and Present paces it.
MSG msg{};
bool running = true;
while (running) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            running = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!IsIconic(g_hwnd)) {
        RenderFrame();
    }
}
```

内圈把积压的消息清空，外圈的活是每圈画上一帧，空转的节奏由 Present 拦着，Present 的参数咱们到讲帧顺序的段落里再细说。`IsIconic` 的判断把最小化的窗口摘出去，省得它在任务栏里还满速地空画。窗口最小化了客户区就是零尺寸，画了也没有去处，这个判断跟 demo 里 `WM_SIZE` 判 `SIZE_MINIMIZED` 是同一个道理的两副写法。

## 设备与队列：Skia 只认 D3D12

建什么样的设备，咱们得听 Skia 的。16 篇里咱们交过底：Skia 的 D3D 后端建在 Direct3D 12 上，`MakeDirect3D` 收的家当是 `ID3D12Device` 的指针、命令队列与适配器，D3D11 的设备在它的门里派不上用场。咱们系列里讲 D3D 排的是 D3D11，那边的展开归[《D3D11 设备与立即上下文》](24_d3d11_device.md)，本篇咱们一步跨到 D3D12，照书架的[ D3D12 开篇一章](../native_win32/43_ProgrammingGUI_Graphics_D3D12_Philosophy.md)搬最小的一套：

```cpp
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

ComPtr<ID3D12Device> g_device;
HRESULT hr = D3D12CreateDevice(
    nullptr,                    // default adapter
    D3D_FEATURE_LEVEL_11_0,     // minimum feature level
    IID_PPV_ARGS(&g_device));

D3D12_COMMAND_QUEUE_DESC queueDesc{};
queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
ComPtr<ID3D12CommandQueue> g_queue;
g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_queue));
```

`D3D12CreateDevice` 的头一个参数传 `nullptr`，咱们的意思是挑默认适配器，功能级别咱们填的 11_0，这是书架那一章原样用的组合，咱们原样沿用。命令队列咱们也只要一条 `DIRECT` 类型的：Skia 在它内部自己管命令的记录与下发，咱们备上的队列就是它的出口，交换链的呈现也挂在它身上。D3D12 里队列、分配器、命令列表各是各的对象，咱们不写自己的绘制命令，这些咱们就都不用碰了，您想看清这套显式模型的全貌，《D3D12 对照：显式化的代价与收益》那边再展开。

## 交换链：把翻页的权利拿到手

交换链管的是画面怎么轮流上屏，咱们在 02 篇里认过它的机制：几块缓冲轮流当起了后台，画好的那块翻到前面去，翻页对准刷新的节拍。创建它的调用是 `IDXGIFactory2` 的 `CreateSwapChainForHwnd`，有一处从 D3D11 带过来的习惯要当心：头一个参数在 D3D12 这边认的是命令队列，您要是把设备传了过去，调用也就失败了，书架的那一章还专门提醒过这里的门道。

```cpp
ComPtr<IDXGIFactory4> factory;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

DXGI_SWAP_CHAIN_DESC1 scd{};
scd.Width        = 0;    // 0: match the window's client area
scd.Height       = 0;
scd.Format       = DXGI_FORMAT_B8G8R8A8_UNORM;
scd.SampleDesc.Count = 1;
scd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
scd.BufferCount  = 2;    // double buffered
scd.Scaling      = DXGI_SCALING_NONE;
scd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
scd.AlphaMode    = DXGI_ALPHA_MODE_UNSPECIFIED;

ComPtr<IDXGISwapChain1> g_swapChain;
factory->CreateSwapChainForHwnd(
    g_queue.Get(),   // the command queue, NOT the device
    g_hwnd, &scd, nullptr, nullptr, &g_swapChain);
```

参数咱们挑要紧的说。`BufferCount` 咱们填 2，为的是双缓冲：两块缓冲里前头的那块在显示，后面的那块挨着画。`SwapEffect` 挑的 `FLIP_DISCARD` 是翻转模型，D3D12 推荐的档，翻页像翻书一样地换指针，DWM 合成窗口的时候拿它更省事。翻过去了之后后备缓冲的内容是不确定的，咱们每帧整幅重画，旧内容丢了也不可惜。

`Format` 挑 BGRA8 是有讲究的。书架那一章的示例填的是 RGBA8，两条路其实都合法，咱们挑 BGRA8，是因为 16 篇里包后备缓冲时报的 colorType 是 `kBGRA_8888_SkColorType`，交换链的格式与报给 Skia 的格式必须对上，咱们从头到尾只用一套名字，也就少了一个能错的位置。翻转模型各参数的讲究，往后的《交换链与呈现（DXGI）》另有一篇细讲。

交换链到了手之后，咱们再认一个每帧都打交道的调用：`GetCurrentBackBufferIndex` 还回来的是此刻轮到哪块缓冲挨画，双缓冲下它报的数就在 0 与 1 之间轮着变。咱们按它取缓冲、包给 Skia 画、画完交给 Present 翻上去。

## 把后备缓冲包成 Skia 的画布

包装的活是 16 篇那串创建顺序的落地，咱们把那边的名单再过一遍：`GrD3DBackendContext` 装的是设备与队列，交给 `MakeDirect3D` 换回的才是 `GrDirectContext`。后备缓冲的 `ID3D12Resource` 塞进 `GrD3DTextureResourceInfo`，配上了宽高就做成 `GrBackendRenderTarget`，`SkSurfaces::WrapBackendRenderTarget` 再把目标包成了 `SkSurface`。咱们的代码按同一个顺序走：

```cpp
// Field names are your Skia header's business: the struct bundles the
// device, the queue, the adapter and a few more slots from the host.
GrD3DBackendContext backend{};
backend.fDevice = g_device.Get();
backend.fQueue  = g_queue.Get();

sk_sp<GrDirectContext> g_context = GrDirectContexts::MakeDirect3D(backend);
// Older trees keep the same entry as a static member:
//   GrDirectContext::MakeDirect3D(backend);

sk_sp<SkSurface> WrapBackBuffer(int w, int h) {
    const UINT i = g_swapChain->GetCurrentBackBufferIndex();
    ComPtr<ID3D12Resource> back;
    g_swapChain->GetBuffer(i, IID_PPV_ARGS(&back));

    // The info struct carries the resource pointer plus format and
    // D3D12 resource state; the header is the source of truth here.
    GrBackendRenderTarget target(
        w, h,
        GrD3DTextureResourceInfo(/* back buffer, format, state... */));

    return SkSurfaces::WrapBackendRenderTarget(
        g_context.get(), target,
        kTopLeft_GrSurfaceOrigin,    // D3D textures are top-left
        kBGRA_8888_SkColorType,      // must match the swap chain format
        /* colorSpace */ nullptr,
        /* surfaceProps */ nullptr,
        /* releaseProc */ nullptr,
        /* releaseContext */ nullptr);
}
```

16 篇里告诫过的两处咱们再对一遍：origin 咱们报 `kTopLeft_GrSurfaceOrigin`，D3D 的纹理 y 向下，报反了的话，画出来的画面就是上下颠倒的。colorType 咱们报 `kBGRA_8888_SkColorType`，跟交换链的 BGRA8 对上，报成 RGBA 的话红蓝两个分量换位，颜色错得反而很好认。

还有生命周期的一处讲究：Wrap 这一路纹理是咱们宿主的，surface 得走在缓冲的前面放手。`releaseProc` 是 Skia 用完纹理的回执口子，骨架里咱们把它留了空，您接真项目时值得挂一个打日志的回调，看看它在什么时机响。

## 一帧的完整顺序

家当齐了，咱们把一帧从头走到尾。时间的来路是 `steady_clock`，它给的是单调的时间，咱们在代码里折成秒，动画拿它当节拍是最稳的。画布咱们不缓存，每帧按当前的缓冲现包。包的开销在描述符那一层，描述符是 D3D12 里指认资源的小结构，16 篇把它比作资源的介绍信，这一层的开销小到咱们不必为它做缓存，翻页轮换的缓冲靠它天然对得上号。

```cpp
static auto g_start = std::chrono::steady_clock::now();

void RenderFrame() {
    const float t = std::chrono::duration<float>(
                        std::chrono::steady_clock::now() - g_start)
                        .count();

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0 || !g_context) {
        return;
    }

    sk_sp<SkSurface> surface = WrapBackBuffer(w, h);
    if (!surface) {
        return;
    }

    const UINT  dpi  = GetDpiForWindow(g_hwnd);
    const float dpiScale = dpi / 96.0f;   // the 03 rule, in float form

    SkCanvas* canvas = surface->getCanvas();
    canvas->save();
    canvas->scale(dpiScale, dpiScale);    // draw in DIPs from here on
    DrawScene(canvas, t, w / dpiScale, h / dpiScale);
    canvas->restore();

    g_context->flush(surface.get());
    g_context->submit(GrSyncCpu::kNo);
    g_swapChain->Present(1, 0);
}
```

头一段干的是准备。客户区的尺寸按物理像素取，DPI 按[《坐标、DIP 与 DPI》](03_dip_and_dpi.md)的换算折成 `dpiScale`，铺在画布的坐标变换上，从这儿往后咱们写的坐标全是 DIP。15 篇里留的那个口子就在这里兑现：字号写在 DIP 的世界里，24 的字在任何缩放的屏上看起来一样大，`DrawScene` 收到的逻辑尺寸也免去了缩放的牵挂。

`DrawScene` 的内容咱们不新造。背景交给 17 篇的 runtime effect，一段两色过渡的 SkSL，时间咱们从 uniform 递进去，两端的颜色随着时间摆了起来。字用的就是 15 篇那行微软雅黑。17 篇里咱们交代过 builder 留在手边、每帧改 uniform 再 `makeShader` 的路数，正好在这儿用上了：

```cpp
// Compiled once at startup via SkRuntimeEffect::MakeForShader
// (the SkSL chapter):
//   uniform float gTime;
//   half4 main(float2 p) {
//       float t = 0.5 + 0.5 * sin(gTime);
//       half4 a = half4(0.17, 0.23, 0.42, 1.0);   // deep blue
//       half4 b = half4(0.95, 0.62, 0.20, 1.0);   // orange
//       return mix(a, b, half(t));
//   }
extern SkRuntimeShaderBuilder g_bgBuilder;   // built from that effect
extern SkFont g_font;                        // Microsoft YaHei, 24 (the text chapter)
extern SkPaint g_ink;

void DrawScene(SkCanvas* canvas, float t, float w, float h) {
    canvas->clear(SK_ColorDKGRAY);

    g_bgBuilder.uniform("gTime") = t;
    SkPaint bg;
    bg.setShader(g_bgBuilder.makeShader());
    canvas->drawRect(SkRect::MakeWH(w, h), bg);

    canvas->drawString("Skia 在 D3D12 的交换链上转起来了",
                       20.0f, 60.0f, g_font, g_ink);
}
```

收尾的三行咱们在 16 篇里逐个认过，摆进循环里咱们再看一遍分工：`flush` 把画布上攒的命令记进命令缓冲，`submit` 才真正把它们交给了队列，`Present` 把画好的缓冲翻上去。

`submit` 咱们传 `kNo`，CPU 递出了命令就接着走，咱们不等 GPU 把活干完，节流的活交给 `Present` 的 SyncInterval 1，它在刷新的节拍前拦住咱们，循环也就跟着屏幕的钟走了。您要是给 submit 传 `kYes`，CPU 就会停下来等 GPU 干完了再走，画面的节奏倒是不变，多出来的是每帧一段干等的 CPU 时间，两档的差别值不值得省，量了数才算数。

SyncInterval 的两档咱们照书架的口径记：1 是等刷新的档，0 是不等刷新尽快呈现的档。02 篇里咱们还认过一层：窗口模式的内容经 DWM 合成上屏，撕裂在合成的路上其实是看不到的，所以 SyncInterval 1 在窗口里的意义落在节奏与功耗上，防止咱们把画好的帧堆成排。动画与帧调度的更多讲究，咱们留到《动画与帧调度》那篇再展开。

## 缩放：ResizeBuffers 与重建

咱们把窗口一缩，后备缓冲的尺寸就跟不上客户区了，咱们得让交换链重开缓冲。书架的[ D3D11 初始化一章](../native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md)给过严格的顺序：攥着旧缓冲的东西都得放掉，`ResizeBuffers` 得等这些都放完了才能调，取缓冲与视图的重建排在后面。咱们每帧现包的选法在这里兑现了它的便利：surface 与后备缓冲的引用都活在帧的函数里，出了函数就放手了，等到 `WM_SIZE` 到达的时候，谁的手里也不攥着旧缓冲，分支里剩下的活也就只有 ResizeBuffers 自己：

```cpp
case WM_SIZE: {
    if (wParam == SIZE_MINIMIZED) {
        return 0;   // client area is 0x0 while minimized (gui/01)
    }
    // Order per bookshelf ch.37: release holders of the old buffers
    // first, then resize, then re-fetch. Our per-frame wrap already
    // satisfies the first step; a cached surface would need reset()
    // here, before ResizeBuffers.
    g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                               DXGI_FORMAT_UNKNOWN, 0);
    return 0;
}
```

宽高咱们传新值，格式传 `DXGI_FORMAT_UNKNOWN` 表示沿用创建时的旧格式，这些是 SDK 文档的口径。您要是改走缓存的路数，把每个缓冲包一只 surface 存起来省每帧包装的活，您就得在调 ResizeBuffers 之前把缓存 reset 干净，顺序上一步也都省不了，便利与代价咱们摆开了讲。最小化的那一下客户区是零尺寸，demo 里判 `SIZE_MINIMIZED` 的写法咱们沿用同一个判断。

## DPI 变化：03 篇的另一半

咱们把窗口一缩，跟来的消息是 `WM_SIZE`。您再把显示器一换、缩放比一变，跟来的就是 `WM_DPICHANGED`。03 篇里咱们把链路走过一遍：manifest 里声明了 PerMonitorV2，系统的拉伸就停了，代价是 `WM_DPICHANGED` 到达时活归咱们。demo 的接法是照建议矩形（系统随 `WM_DPICHANGED` 在 lParam 里附带的推荐窗口位置）调一次 `SetWindowPos`，那段代码在主线第一篇里是验证过的，本篇一字不改地沿用。窗口跟着变大的一圈之后，跟来的 `WM_SIZE` 会走上面 ResizeBuffers 的分支，新的物理尺寸与新的 DPI 都到位了，下一帧里自然都对上了。

字号咱们不用单独操心：坐标变换铺在了画布上，字号写在 DIP 的世界里，窗口大了一圈，字也就跟着大了一圈，03 篇里按钮写死常数缩水的教训，在 DIP 的路上压根就不存在。要说咱们自己接的活，其实只剩 scale 的更新，而咱们每帧都现问一遍 `GetDpiForWindow`，连这点活也省了。03 篇里咱们告诫过缓存 DPI 的风险，每帧现问是最笨也最稳的写法，问一次的代价咱们付得起。

## 设备丢了：16 篇留的位置

16 篇里咱们留过话，说设备丢失的处理代码在实战篇里见。套路在 16 篇里已经讲清楚了：D3D 这边设备被移除的时候，调用报回来的是 `DXGI_ERROR_DEVICE_REMOVED` 一类的错误，Skia 那边咱们调 `abandonContext` 放弃整套 GPU 资源，然后咱们把设备、队列、交换链、context 从头建一遍。咱们每帧现包的选法又帮了一次忙：不缓存的 surface 不用清理，重建的清单上只有那四样。骨架里的接法是把判断挂在 Present 的返回值上：

```cpp
const HRESULT hr = g_swapChain->Present(1, 0);
if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
    // 16's promise, kept: abandon the dead context - no orderly
    // release is possible once the device is gone - then rebuild the
    // device, queue, swap chain and context from scratch.
    g_context->abandonContext();
    g_context.reset();
    RebuildDeviceStack();   // the four objects above, in creation order
}
```

16 篇里咱们留过话，说错误码一类的细节到实战篇的代码里见。这一层笔者确实没有亲历，哪些调用会报、报出来的样子是什么，不敢替您描述，16 篇许的这个诺只能改道兑现。真项目里 submit 与 GetBuffer 的返回值也该一并查，等您读到 D3D 段讲 D3D12 的[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)，咱们再回真实的窗口上验这一层。

## 整套循环就是您项目的对照

咱们把整个循环的环节竖着排开，每个对象的来路对回它自己的篇目：

| 环节 | 对象与调用 | 来路 |
|:---|:---|:---|
| 窗口与消息 | `WNDCLASSEXW` 到 `PeekMessageW` 循环 | 主线第一篇的底子，改成每圈必画 |
| GPU 设备与出口 | `ID3D12Device` 与 DIRECT 队列 | `D3D12CreateDevice` 加 `CreateCommandQueue`，书架 D3D12 章 |
| 翻页上屏 | `IDXGISwapChain1`，双缓冲加 `FLIP_DISCARD` | `CreateSwapChainForHwnd`，头一个参数传队列 |
| Skia 的门 | `GrDirectContext` | `GrD3DBackendContext` 交 `MakeDirect3D`，16 篇 |
| 画布 | `SkSurface` 包后备缓冲 | `WrapBackendRenderTarget`，origin 与 colorType 对上 |
| 每帧的收尾 | flush、submit、Present | 命令的记录、递送与翻页各是各的，16 篇 |
| 尺寸与 DPI | `ResizeBuffers` 与 `GetDpiForWindow` | `WM_SIZE` 与 `WM_DPICHANGED` 各接各的，03 篇 |
| 动画的内容 | SkSL 背景加一行中文字 | 17 篇的 effect 加 15 篇的 font |

咱们拼起来的整套循环，就是您自己项目里 Skia 用法的完整对照。窗口那一头您换成任何一个 HWND，`DrawScene` 里的动画内容您也换掉，剩下的环节咱们一个字都不用动。任何一个用 GPU 后端往 Win32 窗口里画动画的 Skia 应用，走的都是这套循环换了内容的样子。

## 动手清单

咱们动手的活一共有十处，每一处要改的都不出几行，原料都是本篇与 15、16、17 三篇现成的代码：

1. SyncInterval 的 0 与 1 各跑一分钟，任务管理器里看 GPU 占用与功耗的差别。02 篇里讲过两座钟，一座是 CPU 出帧的节奏，一座是屏幕刷新的节奏，这一下您就亲手量出了数。
2. submit 的 `kYes` 与 `kNo` 各跑一轮，在 flush 前后打时间戳，CPU 干等的差别您就看到了。
3. 咱们把 submit 那一行注释掉直接 Present，看看屏幕上是什么样子。16 篇里留下的那个问题，这一处就是您补验的地方。
4. colorType 故意报成 `kRGBA_8888_SkColorType`，红蓝换位的画面错得有多整齐，您看一眼就忘不掉。
5. origin 故意报成 `kBottomLeft_GrSurfaceOrigin`，上下颠倒的画面正好拿来核对方向。
6. 您把窗口在两块缩放不同的显示器之间拖，断点停在 `WM_DPICHANGED` 与 `WM_SIZE` 上，两条消息的到达顺序就看了一遍。
7. 17 篇的圆角矩形距离场搬到 DrawScene 里当按钮，指针坐标从 `WM_MOUSEMOVE` 填进 uniform，交互的口子您就接上了。
8. 每帧现包的写法改成缓存两只 surface，每个缓冲一只，缩放分支里补上 reset 的顺序，便利与代价您两头都摸到了。
9. Wrap 那一路的 releaseProc 挂一个打日志的回调，看它在帧与帧之间的什么时机响，16 篇的生命周期一说就有了实感。
10. `drawString` 换成 15 篇的 `drawTextBlob`，文案不变的动画里，字形位置每帧重新查找排布的活省了没有，您拿两版对照着看。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 本篇建设备用的是 D3D12 的入口，依据出自哪一篇的哪一处交代？
2. `CreateSwapChainForHwnd` 的头一个参数为什么是队列，传设备会怎样？
3. `FLIP_DISCARD` 翻页之后后备缓冲的内容不确定，咱们的循环为什么不吃亏？
4. 交换链的 Format 挑 BGRA8，跟 16 篇的哪一条讲究连着？
5. origin 与 colorType 各报什么，报反了各是什么画面？
6. flush、submit、Present 各干什么，谁的活里没有别人？
7. submit 传 `kNo` 配 SyncInterval 1，节流的活最后交给谁了？
8. `GetCurrentBackBufferIndex` 还回来的是什么，双缓冲下它怎么变？
9. 每帧现包 surface 的写法，让 `WM_SIZE` 的分支省了什么？
10. 走缓存复用的路数时，ResizeBuffers 之前必须做什么？
11. DPI 的换算铺在哪一层，字号为什么不用单独换算？
12. 设备被移除时咱们调的口子叫什么，为什么不能正经释放？
13. 咱们的动画内容由哪两篇的家当拼成，各出了什么？
14. `WM_DPICHANGED` 里咱们沿用 demo 的什么做法，后面的链路谁接手？

## 下一步

Skia 段到这里就收齐了。下一篇咱们进文本段，[《字符、码点与字形》](19_chars_and_glyphs.md)接的正是 15 篇里咱们一笔带过的字符编号，到那边一寸一寸地展开。

本篇拼起来的整套循环，您也可以拿它当 D3D 段的预习：到了[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)，Present 的参数、翻转模式的几副面孔、全屏的切换咱们到那边细看。[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)则会把咱们本篇只用了三行的 D3D12 家当，一件一件地摊开讲清楚。
