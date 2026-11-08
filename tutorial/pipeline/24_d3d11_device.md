---
title: D3D11 设备与立即上下文
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# D3D11 设备与立即上下文

按[篇目索引](index.md)的登记，本篇在 Direct3D 这一段里排的是第二篇。上一篇[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)（下文简称管线篇）里，咱们把 Draw 之后流水线的五段走完了，可设备的来历、上下文的管辖，咱们当时全借了书架（也就是 `tutorial/native_win32/` 目录里那套带完整示例的老教程）的现成代码，本篇就把它的底细补上。排在本篇后面的[《资源与视图》](25_d3d11_resources.md)与[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)，咱们到讲它们的地方再细看。

主线咱们一句话就能摆开：D3D11 的世界里立着两根柱子，设备（也就是 `ID3D11Device`）管的是造东西，立即上下文（也就是 `ID3D11DeviceContext`）管的是用东西。咱们说的顶点缓冲、纹理、着色器、视图，全都是设备的造物，往流水线上绑资源、发出 `Draw` 调用的活，就归了上下文。书架给设备、上下文、交换链起了个三件套的并称，交换链的内容归[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)的正事，咱们本篇只管前两根柱子。

读者的设定照旧，您会 C++ 和 Win32 的基础用法就够，D3D11 的调用咱们从零讲起。事实的底本主要取自书架的[初始化与交换链章](../native_win32/37_ProgrammingGUI_Graphics_D3D11_InitSwapChain.md)，[《特效：Effect 图》](11_d2d_effects.md)（本系列的第 11 篇，下文咱们简称 11 篇）里露过面的那行 `D3D11CreateDevice` 是本篇的引子，官方文档的 [D3D11CreateDevice 页](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)咱们也对照着查过参数。

## 两根柱子的分工

咱们把书架开头的定性借来念一遍：`ID3D11Device` 是 GPU 的逻辑抽象，它负责的只有创建资源这一件事情。顶点缓冲的 `CreateBuffer`、输入布局的 `CreateInputLayout`、混合状态的 `CreateBlendState`，管线篇咱们都已经见过，着色器的 `CreateVertexShader` 与 `CreatePixelShader` 也归它管。还有一句要紧的原话：设备是线程安全的，创建方法咱们可以从多个线程一起调。

`ID3D11DeviceContext` 管的是另一头，命令的提交全归它。咱们设置顶点缓冲、挂上着色器、发出 `Draw` 调用，走的全是它的门。书架的叮嘱咱们原样记着：一个上下文对应的是一条命令队列，多个线程共用同一个上下文是不行的。

咱们把两根柱子的脾气拉成一张对照表，咱们讲着讲着就能回来对一眼：

| | `ID3D11Device` | `ID3D11DeviceContext` |
|---|---|---|
| 职责 | 创建资源与状态对象 | 绑定资源、提交绘制命令 |
| 线程 | 创建方法线程安全 | 一次只归一个线程 |
| 数量 | 一块卡建一个 | 立即上下文仅一个，延迟上下文可另开（供多线程录命令，后文细说） |
| 代表调用 | `CreateBuffer`、`CreateVertexShader` | `IASetVertexBuffers`、`Draw` |

咱们再把这些对象的一辈子画成一条时间线，谁在什么时候出场、什么时候退场，您扫一眼就对上了：

```text
启动
  D3D11CreateDevice ──→ 设备 + 立即上下文
  设备建资源（一遍）──→ 缓冲、纹理、着色器、视图
窗口变了尺寸
  设备按新尺寸重建资源（还是一遍）
每帧
  立即上下文 ──→ 绑定、Draw、Present
出事（驱动崩了或超时复位，TDR）
  GetDeviceRemovedReason ──→ 全部放掉，回到启动重来
```

咱们后文的讲解就顺着它走，前半段讲的是创建与协商，中段讲的是每帧的绑定与绘制，收尾讲的是出事之后的丢弃与重建。

## D3D11CreateDevice：把参数逐位看过去

咱们把 11 篇里浓缩的那次调用请回台前，展开成官方签名的全貌：

```cpp
HRESULT D3D11CreateDevice(
    IDXGIAdapter*            pAdapter,
    D3D_DRIVER_TYPE          DriverType,
    HMODULE                  Software,
    UINT                     Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT                     FeatureLevels,
    UINT                     SDKVersion,
    ID3D11Device**           ppDevice,
    D3D_FEATURE_LEVEL*       pFeatureLevel,
    ID3D11DeviceContext**    ppImmediateContext);
```

十个参数咱们挨个过。`pAdapter` 挑的是显卡：传 `nullptr` 的意思是用默认适配器，多数程序走的都是它。真要挑卡（笔记本上核显与独显并存的机器，咱们想让渲染压给独显），咱们就得走 DXGI 的枚举：用 `CreateDXGIFactory1` 建好工厂，咱们再把适配器一个个数出来，把选中的那块卡递给 `pAdapter`。这里有一条搭配的讲究，官方文档的原话写得很明白：`pAdapter` 传了具体的卡，咱们就必须把 `DriverType` 填成 `D3D_DRIVER_TYPE_UNKNOWN`，驱动类型的判断就交给了适配器本身。传 `nullptr` 配 `D3D_DRIVER_TYPE_HARDWARE` 的组合才是常见的合法款。

`DriverType` 说的是用谁跑。`D3D_DRIVER_TYPE_HARDWARE` 走的是真显卡，GPU 的全部速度都踩在这档上。`D3D_DRIVER_TYPE_WARP` 是咱们本篇的后备：CPU 模拟出来的软件光栅化器，速度上慢了一大截，功能倒是齐全，咱们留在回退一节细说。`D3D_DRIVER_TYPE_SOFTWARE` 是留给自家软件光栅化器的档，配它的 `Software` 参数得给出 DLL 的句柄，咱们平时用不上。`Software` 参数咱们传 `nullptr` 就好，它的用武之地也就剩 SOFTWARE 档了。

官方的 [D3D11 驱动类型文档](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_driver_type)还记着一类参考光栅化器（`D3D_DRIVER_TYPE_REFERENCE`）：跑得极慢、图的只是结果正确，如今的 SDK 已经不随附它了，咱们知道有过它就行。

`Flags` 里咱们挑两个说。`D3D11_CREATE_DEVICE_BGRA_SUPPORT` 是 D2D 互操作的硬要求，11 篇咱们引过书架的警告：缺了它的话，D2D 设备的创建就会失败，因为 D2D 的世界流通的是 BGRA 格式的缓冲区，而 D3D11 默认产的是 RGBA。`D3D11_CREATE_DEVICE_DEBUG` 是调试层的开关，开了之后 D3D 会替咱们多做一层检查，咱们把参数给错了，报错的信息也详细得多，代价是速度与额外的安装要求，咱们调试的时候开、发布的时候关。

再往后的参数管的是功能级别（feature level，D3D 给各代硬件划的能力档位）的协商。咱们递一个从高到低的数组进去，D3D 从头挑硬件撑得住的头一档，把协商的结果写进 `pFeatureLevel` 交还给咱们。数组里最高的 `D3D_FEATURE_LEVEL_11_0` 对应的是 DirectX 11 一代的能力，往下的 10_1、10_0、9_3、9_1 一路降级，老机器的卡也能接着跑。11 篇传的是 `nullptr` 配 0，D3D 就用它自家默认的清单，从 11_0 一路列到了 9_1，本篇咱们把清单写明，协商到了哪一档心里就有数。档位还决定了流水线能干什么，拿到的级别低于 11_0 的话，着色器的模型也就跟着退了。

还有一处小地方：咱们不想收协商结果的时候，把 `pFeatureLevel` 传成 `nullptr` 是被允许的，咱们示例里还是收下了，机器的真实能力摸一摸没坏处。

剩下的三个参数是一组的。`SDKVersion` 咱们写死 `D3D11_SDK_VERSION` 就好。结尾的两个输出指针，一个交还的是设备，另一个交还的是立即上下文，一次调用咱们把两根柱子都拿到了手。书架初始化章走的 `D3D11CreateDeviceAndSwapChain` 是另一条合并的路子，设备与交换链一次就造齐了，咱们 11 篇走的是分开的路，本篇沿用的也是它。

## 真要挑卡：DXGI 的枚举

多数程序用默认适配器就够了，可咱们总有用得上挑卡的一天：双显卡的笔记本想让独显扛渲染，或者咱们的演示想绕开一块出了问题的卡。枚举的活在 DXGI 那边（书架初始化章的 DXGI 一节也提醒过，多显示器、多 GPU 的场景里，跟硬件打交道的正是 DXGI 那一层），咱们把循环写出来给您看：

```cpp
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")

IDXGIFactory1* pFactory = nullptr;
CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);

IDXGIAdapter1* pAdapter = nullptr;
for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 desc{};
    pAdapter->GetDesc1(&desc);
    // desc.Description: the card name
    // desc.DedicatedVideoMemory: dedicated VRAM in bytes
    pAdapter->Release();
    pAdapter = nullptr;   // the next call overwrites it anyway
}
pFactory->Release();
```

循环的架子如上，咱们看两处要紧的：`EnumAdapters1` 交还的适配器按编号排，交完的那次返回 `DXGI_ERROR_NOT_FOUND`，循环就停了。`GetDesc1` 交出来的 `DXGI_ADAPTER_DESC1` 里，`Description` 装的是卡名，`DedicatedVideoMemory` 装的是专用显存的字节数，咱们按名册挑中一张，把它的指针传给 `D3D11CreateDevice` 的 `pAdapter`。搭配的讲究咱们在前文交代过：咱们要把 `DriverType` 换成 `D3D_DRIVER_TYPE_UNKNOWN`，选了卡，驱动类型就由卡自己说了算。

枚举的路子咱们点到这里为止，本篇的示例走默认适配器，您要试双显卡的机器，把循环接进去跑一遍也就好了。

## 设备管造，造出来的要省着用

设备身上的创建方法有一个共同的脾气：创建是贵的。驱动要做的校验、分配、登记一样都省不了，一次创建的成本远高于一次绑定的成本。所以咱们把创建挪出每帧的循环，资源建一遍就跨着帧复用了，窗口没了尺寸变了才重建。《特效：Effect 图》里咱们走的是每帧建、用完就释放的路子，Effect 节点是轻的，那点开销对单张图的示例无所谓。可 D3D11 的资源就沉得多了，咱们宁可建一次、用到程序收摊。

书架还有一条线程上的话值得咱们记下：设备的创建方法是线程安全的，咱们可以开几个工作线程各自建资源。管使用的立即上下文就不同了，同一时刻握着它的线程只能有一个。想多个线程一起录命令的活，D3D11 另辟了延迟上下文的道，咱们隔一段说它。

清理的顺序书架也给了清单：视图、交换链、上下文咱们挨个放掉，设备永远排在释放名单的末尾。设备身上的对象没放干净就放设备，析构的顺序会打架。

### 创建放哪儿

写进代码的时候，咱们把设备的创建与资源的创建包成一个函数，指针判空的守卫打头，建过的直接返回。管线篇那个三角形的顶点缓冲，照这个样子放进去就好了：

```cpp
static ID3D11Buffer* g_pVertexBuffer = nullptr;

HRESULT CreateDeviceResources()
{
    if (g_pDevice == nullptr) {           // the device comes first
        HRESULT hr = CreateD3DDevice();   // built in the example below
        if (FAILED(hr)) return hr;
    }
    if (g_pVertexBuffer != nullptr) {     // already built, reuse across frames
        return S_OK;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(triangleVertices);  // the pipeline article's array
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = triangleVertices;

    return g_pDevice->CreateBuffer(&bd, &init, &g_pVertexBuffer);
}
```

守卫的次序有讲究：设备得排在资源的前面，资源是挂在设备身上的，设备没了的时候，它们也就悬空了。判空的第二道守卫挡掉的，就是每帧重复创建的浪费，管线篇示例里咱们没有交代的创建去处，到这里就有了着落。

## 立即上下文：每设备一个的发令通道

立即上下文为什么冠着立即（immediate）的名？因为它发出的命令当场就往 GPU 送。每个设备配的立即上下文只有一个，创建设备的时候它跟着一起交到咱们手里，后面咱们想再拿，设备身上的 `GetImmediateContext` 随时能给。它管的事咱们按绘制的顺序捋一遍：管绑定的 `IASetVertexBuffers`、`VSSetShader`、`PSSetShader`，管绘制的 `Draw` 与 `DrawIndexed`，还有清屏的 `ClearRenderTargetView`，管线篇那串调用念的全是它的台词。

它还有几样每帧都可能碰面的活：更新常量缓冲的 `Map` 与 `UpdateSubresource`（常量缓冲的正事归[《着色器与常量缓冲》](26_d3d11_shaders.md)），还有把命令真正赶下去的 `Flush`。这里咱们只记它们的归属：动词类的调用，几乎都长在上下文的身上。

咱们设下去的顶点缓冲、挂上的着色器，全都算它的内部状态，下一条 `Draw` 读的就是上一条 Set 留下的现场。两条线程往同一个上下文里塞的命令要是交了叉，现场就乱套了，所以 API 干脆不做跨线程的保证。11 篇开头的全局家当清单里的 `g_pD3DContext` 就是立即上下文本尊，D2D 在它背后画的那些 Effect 命令，走的也是同一条通道，咱们看不见而已。

那想多线程干活怎么办？D3D11 给的路叫延迟上下文（deferred context）。咱们拿设备身上的 `CreateDeferredContext` 另开一个上下文，咱们在它身上录命令，录完的整册命令交还一个 `ID3D11CommandList`，执行的收尾归立即上下文：拿 `ExecuteCommandList` 一口气放完。录制是各录各的，几个线程就能并行地攒命令，执行的口子仍然只有立即上下文一个。各家驱动对它的支持是参差的，真要把多线程的渲染用进产品，驱动的表现咱们得实测，本线的示例用不上它，咱们知道它是条路就够了。

多线程录制的这套设计，到了 D3D12 里被更彻底的命令队列与命令列表顶了替，设备与上下文的边界也整个重画，咱们把它留到[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)再看，本篇咱们不展开。

## 设备移除：GPU 丢了之后

设备移除（device removed）说的是这样一种状态：GPU 那头出了事，设备整个作废了。起因咱们挑常见的两种说。一种是驱动崩了或驱动内部出错。另一种咱们倒觉得更有意思，TDR 的全称是超时检测与恢复：Windows 给 GPU 的命令交差设了时限，驱动过了时限没交差，系统就认定驱动挂死了，跟着把 GPU 复位了一遍。等复位完了，咱们手里的设备已经作废。时限的默认值，微软的 [TdrDelay 文档页](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/tdrdelay)（TdrDelay 是注册表里控制这个时限的项）记的是 2 秒，所以一段着色器要是把 GPU 拖住超过两秒，桌面一闪、程序里的设备就没了。

现象是从哪儿报出来的？多数时候咱们不用主动查，错误码会自己找上咱们的门：设备移除之后，返回 `HRESULT` 的调用会把 `DXGI_ERROR_DEVICE_REMOVED` 交还给咱们，设备侧的创建方法、`Map`，还有 `Present` 与 D2D 那边 `EndDraw` 的口子都在这一类。上下文身上返回 `void` 的调用（`Draw`、各样的 `Set`）没有码可交，它们只是静默地不干活，错误往往要从别处的口子才发现。咱们想问个究竟，设备身上的 `GetDeviceRemovedReason` 是专门的口子，它交还的码里 `DXGI_ERROR_DEVICE_HUNG`、`DXGI_ERROR_DEVICE_RESET`、`DXGI_ERROR_DRIVER_INTERNAL_ERROR` 各对应一类死因，咱们拿它记日志，区分是咱们自己的着色器拖死的 GPU，还是驱动自己闯的祸。

处置的路数，咱们其实早在 D2D 就练过了。[《工厂、设备与渲染目标》](06_d2d_targets.md)里渲染目标丢失的应对是丢弃重建，咱们一见到 `D2DERR_RECREATE_TARGET`，目标作废了，整条链也就得重来了。[《特效：Effect 图》](11_d2d_effects.md)里咱们还见过设备丢失在新路线上的面目：新路线上它常以 `EndDraw` 报出的 `DXGI_ERROR_DEVICE_REMOVED` 露面，丢弃的名单从目标一张扩到从 D3D 设备到交换链的整串。D3D11 这边是同一套思路换了个入口：错误码拿到了手，咱们就把设备、上下文、连同造出来的资源全部放掉，咱们从 `D3D11CreateDevice` 重新来一遍。设备移除恢复不了原状，重建是唯一的路，微软文档也把彻底重建写成了建议的做法。

## WARP：没显卡的保底

回头看 `DriverType` 一节欠下的说明。WARP（Windows Advanced Rasterization Platform）是 Windows 自带的软件光栅化器，咱们把 `DriverType` 填成 `D3D_DRIVER_TYPE_WARP`，CPU 就扮起了 GPU。速度自然是慢的：光栅化、着色这些活全压在 CPU 的身上，一帧的活够 CPU 忙一阵的。它的价值落在保底上：没独显的老机器、显卡驱动出了问题的环境、还有 CI 机房那些不带显示器的机器，程序照样能正常地跑，画面也照样出得了。咱们写教程的示例尤其受用，您手里什么机器都有，WARP 兜住了底线，示例的功能不至于因为一块卡缺席就演示不了。官方的 WARP 说明页记给它的功能级别从 9_1 一直到 11_1，咱们示例里协商的档位在 WARP 上同样谈得下来。

虚拟机里跑示例也算它的用武之地：Hyper-V 一类的环境里常常没有可用的硬件驱动，硬件档的调用多半直接失败，等 WARP 接了班，画面也就照样出得来了。所以回退在咱们的示例里是正路的一部分，咱们把它写成了保命的活。

回退的写法是连着试：硬件档要是失败了，咱们退 WARP 再试一次，两头都败了才算真败。写法咱们并进下面的示例。

## 示例：带错误处理与回退的创建

咱们把 11 篇那行浓缩的调用展开成完整的函数，错误判断全上、WARP 回退接上、协商出来的功能级别也存进全局变量。底下的代码是咱们按官方签名与书架示例的样式拼的骨架，咱们没有在本仓库里编译验证过，您真要跑的时候，以手上的 SDK 为准。

```cpp
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

static ID3D11Device*        g_pDevice      = nullptr;
static ID3D11DeviceContext* g_pContext     = nullptr;  // the immediate context
static D3D_FEATURE_LEVEL    g_featureLevel = D3D_FEATURE_LEVEL_11_0;

HRESULT CreateD3DDevice()
{
    // negotiate from the highest level the hardware can afford
    D3D_FEATURE_LEVEL requested[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_1,
    };

    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // D2D needs BGRA

    HRESULT hr = D3D11CreateDevice(
        nullptr,                   // default adapter
        D3D_DRIVER_TYPE_HARDWARE,  // the real GPU
        nullptr,                   // software rasterizer DLL, unused
        flags,
        requested, ARRAYSIZE(requested),
        D3D11_SDK_VERSION,
        &g_pDevice,
        &g_featureLevel,           // the level we actually got
        &g_pContext);              // the one and only immediate context

    if (FAILED(hr)) {
        // no usable hardware driver, fall back to WARP
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            requested, ARRAYSIZE(requested), D3D11_SDK_VERSION,
            &g_pDevice, &g_featureLevel, &g_pContext);
    }

    return hr;
}
```

函数的几点咱们过一遍。请求的档位数组按从高到低排，D3D 挑中了哪档，`g_featureLevel` 里躺的就是哪档，咱们可以用它判断机器的真实能力，拿到的档位低于 11_0 的话，咱们就把吃硬件的效果收起来。失败的分支里咱们换 WARP 再调一遍，参数是原样不动的，换的只有驱动类型，要是两头都失败了，咱们就把错误码原样交还给调用方。判 `FAILED` 的活儿照系列的老样子，调用方拿到不成功的 `hr` 自己决定怎么办。

有一处失败咱们要单独说：硬件档的失败未必是机器没卡，驱动版本太老、远程桌面会话里没有可用的 GPU，都会让头一次的调用失败。所以咱们别把 WARP 的回退当成小众的保险，您要是从远程桌面连过来，吃到的正是这层保底。

设备建好之后想验证 `g_pContext` 的身份，咱们拿 `GetImmediateContext` 再要一次，交还的指针跟创建时拿到的是同一个对象，`Release` 配平了引用计数就行。

设备移除的检查，咱们在绘制循环的末尾加一小段。`Present` 的返回值别扔，它失败的时候咱们问一句 `GetDeviceRemovedReason`，咱们把原因记下来，然后咱们把家当整个放掉，再做一次完整的重建。`g_pSwapChain` 的来历归交换链那篇的正事，这里咱们只看失败分支的处置：

```cpp
HRESULT hr = g_pSwapChain->Present(1, 0);
if (FAILED(hr)) {
    HRESULT reason = g_pDevice->GetDeviceRemovedReason();
    // log `reason`: HUNG / RESET / DRIVER_INTERNAL_ERROR tell why
    DiscardDeviceResources();  // release device, context, everything built on them
    CreateD3DDevice();         // then rebuild from scratch
}
```

`DiscardDeviceResources` 的写法咱们在 11 篇已经用过同款，咱们从设备到视图挨个判空释放，照着名单放就是了。

## 您可以动手的三处

示例是拼装的骨架，下面三处咱们改的都是一行、编的都是一遍：

1. 咱们把请求的档位数组删得只剩 `D3D_FEATURE_LEVEL_11_0` 一行，老机器上硬件档的调用会失败，WARP 分支把它接住。协商出来的档位，您在调试器里看一眼 `g_featureLevel` 就清楚了。
2. 咱们给 `flags` 添上 `D3D11_CREATE_DEVICE_DEBUG`（系统得装着调试层，Windows 的可选功能里勾 Graphics Tools），跑起来之后，调试输出里 D3D 报的检查消息就一条一条地冒出来了。
3. 咱们把 `DriverType` 直接改成 `D3D_DRIVER_TYPE_WARP` 交一次调用，任务管理器里盯着 CPU 的占用，软件光栅化的代价您就亲眼见到了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. 设备与上下文各管什么？创建顶点缓冲与发出 `Draw` 各走谁的门？
2. `pAdapter` 传具体适配器的时候，`DriverType` 必须填什么？传 `nullptr` 配 `D3D_DRIVER_TYPE_HARDWARE` 又是什么意思？
3. `D3D11_CREATE_DEVICE_BGRA_SUPPORT` 服务于哪条互操作路线？缺了它的后果是什么？
4. 功能级别的协商怎么进行？咱们从哪个输出参数读到协商的结果？
5. 立即上下文每个设备有几个？哪个接口能再把它拿回来？
6. 延迟上下文录完的命令装在什么对象里？执行归谁？
7. TDR 是怎么把设备弄没的？`GetDeviceRemovedReason` 能告诉咱们什么？
8. 设备移除之后的处置思路，跟 D2D 的哪个错误码是同一套？
9. `EnumAdapters1` 的循环靠哪个返回值停？`DXGI_ADAPTER_DESC1` 的哪两个字段常拿来挑卡？
10. 咱们说创建是贵的，示例里靠哪两道守卫挡住每帧的重复创建？
11. 硬件档的失败还有哪些常见的场合？WARP 的回退为什么算正路的一部分？

## 下一步

设备与上下文到手了，咱们手里还只有几个光秃秃的指针。设备造出来的家当长什么样、视图为什么是资源的必经包装，这些归下一篇[《资源与视图》](25_d3d11_resources.md)的正事。再往后的[《着色器与常量缓冲》](26_d3d11_shaders.md)您想接着读随时能点过去。想马上动手的您，也可以拿本篇的 `CreateD3DDevice` 替换 11 篇示例里那段创建代码，跑一遍 Effect 的投影示例，回退的路数您也就亲手摸过了。
