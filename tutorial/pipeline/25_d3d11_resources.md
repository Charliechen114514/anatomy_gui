---
title: 资源与视图
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 资源与视图

按[篇目索引](index.md)的登记，本篇在 Direct3D 这一段里排的是第三篇。上一篇[《D3D11 设备与立即上下文》](24_d3d11_device.md)（下文咱们简称设备篇）收尾的时候留过一句话：设备造出来的家当长什么样、视图为什么是资源的必经包装，归本篇的正事。往前的[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)咱们简称它为管线篇，往后的[《着色器与常量缓冲》](26_d3d11_shaders.md)咱们简称它为着色器篇。

再往后的[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)、[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)与[《图像：解码、缓存与 mipmap》](29_image_pipeline.md)（最后一篇下文咱们简称它为图像篇），咱们到讲它们的地方再碰头。

本篇的主线是：GPU 眼里的数据都叫资源，用的时候都要过一道视图。资源说的是存储，它回答的是字节放在哪、怎么排。视图说的是用途，它回答的是这块字节给管线的哪一站读、按什么样的格式来解读。咱们在本篇把资源这一层认全：缓冲与纹理两类身体，`ID3D11Resource` 这个共同的基类，还有把资源送上管线的四种视图。读者设定照旧，您会 C++ 和 Win32 的基础用法就够。

事实的底本主要取自书架（也就是 `tutorial/native_win32/` 目录里那套带完整示例的老教程）的[纹理与采样器章](../native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md)，下文咱们简称它为纹理篇，[顶点输入章](../native_win32/38_ProgrammingGUI_Graphics_D3D11_VertexInput.md)咱们也一并参考。USAGE 各档的约束，咱们对照过官方的 [D3D11_USAGE 文档页](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_usage)。本篇的代码都是讲解用的骨架，笔者没有把它们放进工程编译验证过，您真要上手跑的时候，以手上的 SDK 与文档为准。

## 资源是存储，视图是用途

咱们从设备篇借一句老话开场：设备管的是造东西。可它造出来的东西，长着几副不一样的身体。管线篇里那个三角形的顶点缓冲，它的接口叫 `ID3D11Buffer`，身子就是一块连续的字节，内部没有格子的划分。纹理篇里从图片文件造出来的贴图，接口的名字叫 `ID3D11Texture2D`，它是一张排好了行与列的格子网，每个格子里装的是一整个像素。咱们再往上数一层，缓冲与各式纹理还有一个共同的基类，它的名字叫 `ID3D11Resource`。咱们看继承关系，从 `ID3D11Buffer` 一路到 `ID3D11Texture1D`、`ID3D11Texture2D`、`ID3D11Texture3D` 的几位接口，它们全是它的派生类。在 D3D11 的世界里，GPU 侧的数据容器只认资源一个说法。

咱们翻设备篇里立即上下文的方法表就能看到，`UpdateSubresource`、`CopyResource` 这几个上下文身上的搬数据调用，参数收的类型全是 `ID3D11Resource*`。`Map` 与 `Unmap` 干脆就长在这个基类自己的身上。拷贝一块字节的时候，D3D11 压根不在乎它的身世是缓冲还是纹理，装东西的身份归装东西的，与它内部怎么排没有关系。资源这一层管的就一件事：把字节好好地存起来。

那视图是什么呢？视图自己也是一个 COM 对象，它是不存数据的，手里攥着的是资源指针，外加一份说明：这块资源按什么格式、圈哪一段、给谁用。咱们拿 D2D 那边的老经验对照着看。咱们在[《特效：Effect 图》](11_d2d_effects.md)（下文咱们简称它为特效篇）里就见过它的影子：DXGI 的后台缓冲区本身只是一个表面，D2D 想在它的上面画画，咱们就得拿 `CreateBitmapFromDxgiSurface` 再包一层位图出来。包出来的这层就是同一桩事在 D2D 世界里的样子，表面扮演的是存储，包出来的位图是用途。D3D11 把这层包装正式化了，造出来的东西就叫视图（view），创建的路全在设备身上：`CreateShaderResourceView`、`CreateRenderTargetView`、`CreateDepthStencilView`、`CreateUnorderedAccessView`，四位咱们后文逐个认脸。

为什么好好的资源不能再直接用，非要再包一道？咱们拿实在的场景回答。书架纹理篇讲 mipmap 自动生成的时候，咱们得给同一块纹理挂两道旗：`D3D11_BIND_SHADER_RESOURCE` 与 `D3D11_BIND_RENDER_TARGET`，生成的算法是 GPU 把缩小了一层的内容画出来，画的去向与采样的原料又是同一块纹理。同一块资源在同一时刻既是采样的原料、又是绘制的去向，两样的用途各要一个对象才能挂上管线。咱们要是直接拿资源本身往上送，一个指针就装不下两重的身份了。视图把用途从资源的身上剥了出来，一块存储想干的活要有几样，咱们就造几个视图，存储还是原来的那一块。

## 缓冲三兄弟：各喂各的站台

咱们来认缓冲这边的三兄弟。它们的数据结构是同一个 `ID3D11Buffer`，差的只有装的东西与去的地方，咱们把去处对准管线篇画过的五段图说。

顶点缓冲咱们最熟。三角形的三个顶点在 CPU 侧是一排 float，咱们拿 `CreateBuffer` 把它们搬上去，绘制的时刻咱们拿 `IASetVertexBuffers` 把它交给输入装配（IA 是 Input Assembler 的缩写，管的是把字节拼成顶点、连成三角形的那一段）。管线篇的示例走的就是它。

索引缓冲是替咱们省内存的。管线篇咱们带过一句：一个矩形由两个三角形拼成，对角线上的顶点存了两份。索引缓冲里装的是顶点的编号，IA 拿着编号回顶点缓冲里取它要的顶点，配套的调用是 `IASetIndexBuffer` 与 `DrawIndexed`。编号的格式常用 `DXGI_FORMAT_R16_UINT`，一个编号占的是两个字节、最大装到 65535，顶点数再往上咱们就换 `R32_UINT`。

常量缓冲是 CPU 往 GPU 递参数的通道，矩阵、时间、颜色这类每帧会变的数走的就是它。它不走 IA 的门，去的是着色器各段的寄存器槽（咱们系列里用到的就是 VS 与 PS 两站）：`register(b0)` 的说法咱们在着色器篇见过，C++ 侧的绑定走 `VSSetConstantBuffers` 与 `PSSetConstantBuffers`，其余段各有同名的调用。它还有两样自己的讲究。头一样是 `ByteWidth` 必须是 16 的倍数，来龙去脉着色器篇的常量缓冲一节铺开了，咱们不重讲。另一样是创建时 `D3D11_BIND_CONSTANT_BUFFER` 不能跟别的绑定旗标混用：挂了它，其余的旗一面也挂不上，混着挂的话创建调用直接把咱们拒了。

| 缓冲 | 装的东西 | 喂给哪一站 | 绑定的调用 |
|:---|:---|:---|:---|
| 顶点缓冲 | 顶点的原始字节 | IA 输入装配 | `IASetVertexBuffers` |
| 索引缓冲 | 顶点编号 | IA 输入装配 | `IASetIndexBuffer` 加 `DrawIndexed` |
| 常量缓冲 | 参数块 | VS 与 PS 的 b 槽 | `VSSetConstantBuffers`、`PSSetConstantBuffers` |

三兄弟的创建走的却是同一个 `CreateBuffer`，差别全落在描述结构的字段里。字段怎么填、旗标怎么选，咱们把纹理的描述结构认完以后一起说，因为它们用的是同一套档位。

还有一样值得咱们留意：缓冲自己是没带格式的。一块 `ID3D11Buffer` 在 GPU 的眼里只是若干字节，字节的切法由输入布局的说明书来给，咱们在管线篇见过它对 `POSITION` 与 `COLOR` 的安排。格式的事到了视图层还会回来，咱们后文的示例走的就是它。

## 纹理的描述结构：逐字段念

纹理的身子可不像缓冲那样是一根字节棍子，它的身子是一张排好格子的网，建网得用一份完整的说明书。咱们把纹理篇创建贴图的那段请回来，盯着 `D3D11_TEXTURE2D_DESC` 的字段一个个念：

```cpp
D3D11_TEXTURE2D_DESC texDesc = {};
texDesc.Width     = width;   // from the decoded image
texDesc.Height    = height;
texDesc.MipLevels = 1;       // one level, no mip chain for now
texDesc.ArraySize = 1;
texDesc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
texDesc.SampleDesc.Count = 1;  // no multisampling
texDesc.Usage     = D3D11_USAGE_DEFAULT;
texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
texDesc.CPUAccessFlags = 0;
texDesc.MiscFlags = 0;
```

`Width` 与 `Height` 说的是格子网的尺寸，单位用的是纹素（texel，也就是纹理里的一个格子），它量的可不是字节。`MipLevels` 说的是金字塔有几层：填 1 的意思是只有原图一层，填 0 的意思是由 D3D11 替咱们算出完整的一串，往上一层的宽与高都是减半的，一路减到了 1×1 才收。书架纹理篇特意警告过的摩尔纹问题就出在这个字段上：只给一层的话，纹理缩到很小的时候画面会闪，生成的讲究咱们留给子资源一节与后面的图像篇。`ArraySize` 说的是这样的纹理叠几层，纹理数组的时候它才不是 1。

`Format` 说的是每个格子里装什么，咱们把整个 DXGI_FORMAT 家族留到后文单独认。`SampleDesc` 管的是多重采样（multisample，让一个像素里落多个样点来抗锯齿的老技术），咱们教程一路用的都是 `Count` 为 1 的不采样档，您知道它管的是这个也就够了。

后四个字段就是缓冲那边也用的那一套通用档位了。`Usage` 说的是这块资源给谁读写、改得勤不勤，`CPUAccessFlags` 说的是 CPU 有没有门、有哪种门，`BindFlags` 说的是这块资源打算给管线的哪些站台用，`MiscFlags` 装的是杂项开关。这三样旗标才是本篇的正戏，咱们下两节展开。

创建的调用本身倒简单：咱们把描述结构与一份初始数据交给设备身上的 `CreateTexture2D`，纹理就立起来了。初始数据的 `D3D11_SUBRESOURCE_DATA` 里，`pSysMem` 指着 CPU 侧的像素数组，`SysMemPitch` 说的是一行占多少字节，纹理篇加载图片的函数里 `rowPitch = width * 4` 算的就是它。咱们留意一件事：缓冲的初始数据只给一个指针就够，纹理的还得加一行字节数，因为纹理是按行排的，驱动要知道的还有每行在哪断开。

咱们顺带把另外两位纹理兄弟也报个名。`ID3D11Texture1D` 是一根一维的行，渐变、查找表一类的数据常住在它那里。`ID3D11Texture3D` 是一坨三维的体数据，医学影像、体积雾一类的活归它。它们与 2D 的差别只在维度，描述结构与视图的套路是一样的，咱们本篇拿 2D 当代表讲，您在别处见到 1D、3D 的代码也就认得出同类。

## USAGE 四档与 CPU 访问的矩阵

Usage 字段咱们有四档可选，四档的约束归在下面的矩阵表里，写代码的时候回来对着查：

| 档位 | GPU 侧 | CPU 侧 | 典型的更新路子 | 典型的用途 |
|:---|:---|:---|:---|:---|
| DEFAULT | 读写 | 没有门 | `UpdateSubresource` 偶发地写 | 纹理、渲染目标 |
| IMMUTABLE | 只读 | 没有门 | 创建时给一份初始数据，之后不动 | 静态的顶点与贴图 |
| DYNAMIC | 读 | 写（一帧一写起） | `Map` 配 `D3D11_MAP_WRITE_DISCARD` | 每帧都变的常量缓冲 |
| STAGING | 不上管线 | 读和写都行 | `CopyResource` 拷进拷出 | 截图回读、数据搬运 |

咱们按行念几条要紧的约束。IMMUTABLE 的意思是创建之后永不更改，所以创建时必须给初始数据，不给的话 `CreateBuffer` 与 `CreateTexture2D` 会拒绝咱们，GPU 知道数据是不会变的，优化的手也就放得开。管线篇那个三角形用的正是它：顶点颜色是写死的，咱们造一次，就能一直把它用到程序收摊的那天。

DEFAULT 是最普通的一档，GPU 用它是随意的，CPU 却没有直接的门。咱们想改它的内容，走的路是 `UpdateSubresource`：上下文替咱们把一份数据交进驱动，拷贝的时机由驱动自己挑。它适合偶发的更新，几秒钟才换一次的配置数据用它正合适，每帧都更新的话它内部的那道拷贝就白交了，着色器篇对比 Map 的时候讲过同一件事的另一面。

DYNAMIC 是给每帧都要重写的数据准备的。它不许 CPU 与 GPU 的手同时摸一块内存，官方的建议是一次 `Map` 一份、每帧写一遍，着色器篇的常量缓冲一节咱们配过 `D3D11_MAP_WRITE_DISCARD`：旧内存 GPU 接着读，驱动换一块新的给咱们写，两头谁也不必等谁了。它还有一条不许：DYNAMIC 的资源是不能当渲染目标与深度模具用的，想边画边更新的活是轮不到它的。

STAGING 扮的是搬运工，它是不能直接绑给管线的，`BindFlags` 咱们也只能给它填 0，进出的路全靠拷贝：`CopyResource` 把 DEFAULT 资源拷给它，CPU 再拿 `Map` 读它的内容。咱们想截一张 GPU 画好的图存成文件，走的正是 DEFAULT 画、STAGING 搬、CPU 读的三段路。

CPUAccessFlags 与 Usage 是配着用的。DEFAULT 与 IMMUTABLE 的资源，咱们把这个字段填成 0 就对了。DYNAMIC 通常配的是写权限，STAGING 咱们按需要配读、写或者读写都开。配套的约束咱们记一条就够：CPU 的门与 GPU 的直接使用是不能全开的，咱们想两头都方便，D3D11 没有那样的档，替咱们管并发的心思全藏在矩阵表里。矩阵容许的组合之外，创建调用报回的就是 `E_INVALIDARG`，调试层（设备篇里用 `D3D11_CREATE_DEVICE_DEBUG` 旗子启用的那层诊断）开着的话，输出窗口里还有具体是哪个字段不配的说明。

咱们把矩阵念完，再记一个选档的思路。静态的数据咱们给 IMMUTABLE，每帧都变的给 DYNAMIC，偶发更新的给 DEFAULT 配 UpdateSubresource，要搬回 CPU 的另开一块 STAGING。咱们选错了通常也还能跑，跑慢了或者创建被拒了，咱们再回矩阵表对一眼也就有方向了。

## BindFlags：把用途写在门牌上

`BindFlags` 说的是这块资源给谁用，咱们把常见的几面旗认全：

- `D3D11_BIND_VERTEX_BUFFER`、`D3D11_BIND_INDEX_BUFFER`、`D3D11_BIND_CONSTANT_BUFFER`：缓冲三兄弟各自的门牌，咱们前文都见过了。
- `D3D11_BIND_SHADER_RESOURCE`：给着色器当只读的采样源，配上的是 SRV 视图。
- `D3D11_BIND_RENDER_TARGET`：当渲染目标的画布，配上的是 RTV 视图。
- `D3D11_BIND_DEPTH_STENCIL`：当深度模具的靶子，配上的是 DSV 视图。
- `D3D11_BIND_UNORDERED_ACCESS`：给着色器又读又写地用，配上的是 UAV 视图。

旗标咱们可以按位或着挂多面，前面 mipmap 的例子挂的就是 `SHADER_RESOURCE | RENDER_TARGET` 两面。挂了多面，每种用途各造各的视图，咱们想让一块资源身兼数职，走的就是这个路子。

旗标与视图是对着查的。咱们给一块资源造视图，创建视图的调用会核对其资源的 BindFlags 里有没有相应的旗：拿一块只挂了 `SHADER_RESOURCE` 的纹理去造 RTV，咱们收到的会是 `E_INVALIDARG`，调试层开着的话，咱们还能在输出窗口里读到原因。所以咱们把 BindFlags 理解成门牌：创建资源的时候咱们就把用途报备好，后面咱们造视图、绑管线，凭的全是门牌，报备了的才放行。

反过来的讲究咱们也记一条。书架纹理篇给自动生成 mipmap 的纹理列的要求，咱们原样记着：`MiscFlags` 得带 `D3D11_RESOURCE_MISC_GENERATE_MIPS`，`BindFlags` 咱们得让它同时含 `D3D11_BIND_SHADER_RESOURCE` 与 `D3D11_BIND_RENDER_TARGET`，`GenerateMips` 才肯干它的活。缺了任何一样，生成这一步是出不来的。咱们不急着展开 mipmap 的细节，记这个例子图的是说明旗标的分量：旗标是后续调用放行的凭据。

## 视图的四张脸

资源这一头咱们讲完了，咱们正式认视图的四种。它们是成对出现的，一种视图对应的是管线上的一个去处：

| 视图 | 接口 | 干什么 | 绑上去的调用 |
|:---|:---|:---|:---|
| 着色器资源视图（SRV） | `ID3D11ShaderResourceView` | 着色器只读地采样 | `PSSetShaderResources`、`VSSetShaderResources` |
| 渲染目标视图（RTV） | `ID3D11RenderTargetView` | 当画布，PS 交出的颜色写到这里 | `OMSetRenderTargets` |
| 深度模具视图（DSV） | `ID3D11DepthStencilView` | 存每个像素的深度与模板 | `OMSetRenderTargets` 的末参数 |
| 无序访问视图（UAV） | `ID3D11UnorderedAccessView` | 又读又写，读写通吃 | `CSSetUnorderedAccessViews` 等 |

咱们挨个把脸认清。SRV 是出场最多的一位，纹理篇加载图片的函数里，纹理造好之后紧跟着的那次 `CreateShaderResourceView` 造的就是它，着色器里 `Texture2D` 配 `register(t0)` 的采样读的就是它，t 槽的说法咱们在着色器篇的表格里见过。RTV 咱们其实天天在用：管线篇示例里的 `OMSetRenderTargets`，收的渲染目标就是视图，后台缓冲区要变成能画的画布，中间都得过它的门。特效篇里 D2D 的目标位图，背后的包装走的也是同一层的概念。DSV 在管线篇的深度测试那段露过面：`OMSetRenderTargets` 的末参数收的就是它，每帧开头的 `ClearDepthStencilView` 清的也是它，深度与模板两样值住的是同一张纹理。

视图攥着资源过日子：视图创建的时候，手里拿到的就是资源的指针，咱们放掉自己手里的资源指针也没事，所以只要视图还活着，D3D11 是不会真把资源销毁的。真正逼咱们换视图的是另一种情形：窗口的尺寸一变，交换链就按新的尺寸重建后台缓冲区，旧的 RTV 圈住的是已经退下去的旧缓冲，咱们得把它放掉，照新的尺寸再造一份。特效篇里 `CreateTargetBitmap` 开头那句 `SafeRelease` 干的正是这桩活，D2D 那边的丢弃重建，到了咱们 D3D11 这边就是资源与视图一起重来。设备移除的处置也是同一套，设备篇讲清理顺序时给过咱们一张释放清单：视图、交换链、上下文挨个放掉，设备则永远排在名单的末尾。咱们把 D3D11 侧重造 RTV 的样子写成四步：

```cpp
// the back buffer changed size, the old view is stale
SafeRelease(&g_pRenderTargetView);

ID3D11Texture2D* pBack = nullptr;
g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
g_pDevice->CreateRenderTargetView(pBack, nullptr, &g_pRenderTargetView);
pBack->Release();
```

造视图的第二个参数咱们传了 `nullptr`，意思是描述结构咱们不用给，视图圈住的是整块资源。窗口的尺寸一变就走它一遍，画布与视口也就跟上了新的窗口。

UAV 是四位里最年轻的一位，得等 `D3D_FEATURE_LEVEL_11_0` 的档位才有完整的待遇。它跟 SRV 的差别就在读写上：SRV 是只读的，同一轮绘制里着色器把它的内容读上百遍也没关系，可写的门是关死的。UAV 把写的口子也开了，同一个像素的着色器各写各的、写完还能被别的线程读到，次序不保证的地方它另有一套自己的约定。它的主战场是计算着色器（compute shader，一种不算像素、只做通用计算的着色器），咱们本线不往那边深入，您认下它读写通吃的身份也就够了。

视图的描述结构里还藏着一样咱们没说的本事：视图可以只圈资源的一段。SRV 的描述里 `MostDetailedMip` 与 `MipLevels` 说的是从第几层 mip 起、圈几层，RTV 的描述里 `MipSlice` 说的是当画布的是哪一层。咱们造视图的时候圈第 0 到 1 层，着色器采样的时候就只见这两层。这件事咱们得把子资源的编号讲清楚才说得明白，它正好是下一节的主角。

## 子资源与 mip 切片

一块 `MipLevels` 为 1、`ArraySize` 为 1 的纹理，它的身子就是一整块，咱们没什么可切的地方。可一旦 mip 链与纹理数组上了场，资源的内部就分了格子，咱们给每个格子都编了号，编号的单位就叫子资源（subresource）。

咱们拿具体数字算一遍。一张 256×256 的纹理，`MipLevels` 填 0 的话，D3D11 会替咱们算出 9 层，尺寸从 256、128、64 一路减半到了 1×1。`ArraySize` 为 1 的时候，这块资源里就分出了 9 个子资源，编号从 0 排到了 8，0 号是最精细的原图，8 号是那个 1×1 的小方格。编号的公式咱们原样记下，子资源的编号 = mip 层号 + 数组下标 × MipLevels，咱们按公式随时算得回来。数组叠了 4 层的话，第 2 层数组、第 3 层 mip 的编号，咱们按公式算出来就是 3 + 2 × 9 = 21。

子资源这个单位在 API 里的出镜率不低。`Map` 与 `UpdateSubresource` 的参数里都留了一格 Subresource，缓冲的身子只有一块，咱们填 0 就行，纹理多层的时候，填的就是要动的那一层。前文视图描述结构里的 `MipSlice` 与 `MostDetailedMip`，说的也是按层号去圈，圈的是哪几层，视图露出的就是哪几层。咱们把书架纹理篇那个摩尔纹的警告接上：`MipLevels` 只给 1 的话，缩小的采样只有原图可读，闪烁就来了。等 mip 链配齐了之后，采样器会按画面的远近挑层，三线性过滤再把挑中的相邻两层插值成一张。mip 怎么生成、图片解码之后怎么缓存，正事归后面的[《图像：解码、缓存与 mipmap》](29_image_pipeline.md)一篇。

纹理数组还有一样视图上的便利。SRV 的描述结构里把维度写成 `D3D11_SRV_DIMENSION_TEXTURE2DARRAY`，咱们用一个视图就能圈住整个数组，着色器那头多带一个下标也就能挑层了。咱们本篇用不到它，您往后在字体图集、粒子图集一类的代码里见到，认得它圈的是数组就好。

## DXGI_FORMAT：把名字读成说明书

前面几节里 `Format` 字段咱们一直含着没嚼，咱们在这里把它的全名认全。DXGI_FORMAT 的名字本身就是说明书，咱们拿真见过的几个把读法练熟。

`DXGI_FORMAT_R32G32B32_FLOAT` 咱们在管线篇的输入布局里用过：R、G、B 各占 32 位、类型是 float，连起来的就是三个浮点数，装的正好是一个 xyz 坐标。`DXGI_FORMAT_R32G32B32A32_FLOAT` 多的只是一个 A 通道，凑成的是四个 float，装颜色的 rgba 正好。读法咱们归纳成两段：字母串说的是通道与顺序，数字说的是各占几位，后缀说的是数值的类型。咱们倒着读也行，后缀定的是类型，数字定的是宽度，字母定的是排法，一份名字把三个问题全答了。

`DXGI_FORMAT_B8G8R8A8_UNORM` 是咱们的老熟人了。[《位图与离屏渲染》](10_d2d_bitmaps.md)（下文咱们简称它为位图篇）里 D2D 位图的属性用的是它，[《特效：Effect 图》](11_d2d_effects.md)里交换链的 `sc.Format` 填的也是它，当时咱们只把它当成 D2D 想要的格式复制了进来，现在咱们能把名字读通了：蓝、绿、红、透明四个通道各 8 位，凑成的是 32 位一个像素。UNORM 的意思是 unsigned normalized，无符号归一化：原始的字节是 0 到 255 的整数，读写的时候被归到了 0.0 到 1.0 的浮点区间，着色器拿到的就是浮点数，换算的活驱动替咱们包了。

咱们再报两个同场的：管线篇深度缓冲用的 `DXGI_FORMAT_D24_UNORM_S8_UINT`，24 位的深度归一化配上 8 位的整型模板。索引缓冲用的 `DXGI_FORMAT_R16_UINT`，装的是一个 16 位的无符号整数。

咱们这一路见过的格式都归在下面的表里，您往后翻回来查：

| 格式 | 排法与宽度 | 咱们在哪里见过 |
|:---|:---|:---|
| `DXGI_FORMAT_R32G32B32_FLOAT` | 三个 float | 管线篇的顶点位置 |
| `DXGI_FORMAT_R32G32B32A32_FLOAT` | 四个 float | 顶点颜色与本篇的示例 |
| `DXGI_FORMAT_B8G8R8A8_UNORM` | 蓝绿红透各 8 位 | 位图篇与特效篇的位图与交换链 |
| `DXGI_FORMAT_D24_UNORM_S8_UINT` | 24 位深度加 8 位模板 | 管线篇的深度缓冲 |
| `DXGI_FORMAT_R16_UINT` | 16 位无符号整数 | 本篇的索引编号 |

通道的排法还牵着一桩容易迷糊的事。`R8G8B8A8` 与 `B8G8R8A8` 的差别是字节落在内存里的次序，Windows 的 GUI 世界流通的偏是 BGRA，D2D 的位图与交换链的缓冲区用的都是它。咱们在位图篇还见过预乘 alpha 的说法，那是 alpha 通道参与合成的一个约定，跟这里的排法是两码事，您别把它们搅在一起。

家族里还有一位特别的，它的名字叫 TYPELESS（无类型）。`DXGI_FORMAT_R8G8B8A8_TYPELESS` 说的是四个通道各 8 位、但它暂时不承诺数值怎么解读，解读的活推给视图去定：同一个 TYPELESS 的纹理，咱们造一个 UNORM 的 SRV、再造一个 SNORM 的 SRV（有符号归一化，映射到的是 -1.0 到 1.0），一块存储上两种读法是并存的。这也回头补上了视图那层的另一桩用处：视图手里握着的格式，就是字节被解读成数的最后一道说明。咱们教程用不到 TYPELESS，您在别家代码里见到它的时候，知道它是在给多种解读留门也就够了。

## 示例：一个顶点缓冲的两张面孔

概念都齐了，咱们来做个示例，把两样身份合在同一块缓冲上：让它同时当顶点缓冲与着色器的数据表。这样的用法在真项目里其实不算稀奇，比如顶点动画、调色板一类的数据，CPU 侧咱们把它当数组排，GPU 那头咱们又按顶点喂它、按数据表读它，用的就是同一块缓冲挂两道旗、配两个视图的走法。数据咱们就用管线篇那个三角形的颜色，用的是三个 `XMFLOAT4`：

```cpp
// three vertex colors from the pipeline article's triangle
static const XMFLOAT4 kTriangleColors[3] = {
    XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),  // top, red
    XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),  // bottom right, green
    XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),  // bottom left, blue
};

D3D11_BUFFER_DESC bd = {};
bd.ByteWidth  = sizeof(kTriangleColors);   // 3 * 16 bytes
bd.Usage      = D3D11_USAGE_IMMUTABLE;     // static data, born with contents
bd.BindFlags  = D3D11_BIND_VERTEX_BUFFER
              | D3D11_BIND_SHADER_RESOURCE;  // two hats on one buffer

D3D11_SUBRESOURCE_DATA init = {};
init.pSysMem = kTriangleColors;

ID3D11Buffer* pColorBuffer = nullptr;
HRESULT hr = pDevice->CreateBuffer(&bd, &init, &pColorBuffer);
```

描述结构里咱们认得的几样都在：总字节数、IMMUTABLE 档，随行的初始数据按上面说的搭配喂了进去，咱们拿它跟顶点缓冲一比，差别只在 BindFlags 挂了两面。IMMUTABLE 配初始数据是必选的搭配，静态的数据生下来就带着内容，之后谁也不再动它了。

顶点缓冲的身份，绘制的时候咱们按老路子绑：`IASetVertexBuffers` 收它，stride 填的是 `sizeof(XMFLOAT4)`，算出来的是 16 个字节。着色器读表的身份，咱们给它造的是 SRV：

```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format        = DXGI_FORMAT_R32G32B32A32_FLOAT;  // one element = one float4
srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;      // read it as a table
srvDesc.Buffer.NumElements = 3;                          // three rows in the table

ID3D11ShaderResourceView* pColorSRV = nullptr;
hr = pDevice->CreateShaderResourceView(pColorBuffer, &srvDesc, &pColorSRV);
```

缓冲的 SRV 描述里没有 mip 那些格子，要紧的是两个字段。格式说的是一个元素怎么解读，咱们让一个元素对齐四个 float，与顶点结构里的 `XMFLOAT4` 正好同宽。`NumElements` 说的是表里有几行。咱们还有一样换算可以当场验：`ByteWidth` 给的是 48 个字节，`NumElements` 乘上每个元素的 16 字节正好也是 48，两头的数对得上，表也就读不错位了。着色器那头的读法咱们也摆一眼，HLSL 里的声明写成 `Buffer<float4>`、再挂上 t 槽，咱们拿 `Load(0)` 按编号取一行，跟 `Texture2D` 的采样是一个家族的写法：

```hlsl
Buffer<float4> vertexColors : register(t0);  // the SRV lands in slot t0

float4 PS_Main(VSOutput input) : SV_TARGET
{
    return vertexColors.Load(0);  // read row 0: the red one
}
```

两头咱们各对一遍。C++ 侧造 SRV 的时候，资源的 BindFlags 里挂着 `D3D11_BIND_SHADER_RESOURCE`，门牌对得上了，创建也就放行了。咱们要是忘了挂 `D3D11_BIND_SHADER_RESOURCE` 旗标，`CreateShaderResourceView` 报回的就是 `E_INVALIDARG`，调试层还会把缺旗标的话说全。HLSL 那侧的 t0 槽，绑定的时候咱们拿 `PSSetShaderResources` 把 `pColorSRV` 塞进 0 号槽就对上了，槽号的约定着色器篇讲过，咱们不重复。

咱们拼出来的一块缓冲、两个视图，就是本篇主线跑起来的样子。骨架本身笔者没有编译验证过，您真要跑它，设备的来路按设备篇的 `CreateD3DDevice` 走，着色器怎么编译、怎么挂上去，照着着色器篇的装配一节拼也就行了。

## 您可以动手的三处

示例是拼装的骨架，下面三处咱们改的都是一行、看的都是一遍：

1. 咱们把 `bd.BindFlags` 里的 `D3D11_BIND_SHADER_RESOURCE` 删掉再造 SRV，`CreateShaderResourceView` 当场报 `E_INVALIDARG`，调试层开着的话，输出窗口里连缺哪面旗都写得明白，门牌与视图的对应您就亲手验过了。
2. 咱们把 `srvDesc.Buffer.NumElements` 改成 4，视图要圈的 64 字节越出了缓冲的 48 字节，创建调用当场就把视图拒下、报回 `E_INVALIDARG`，越界的防线设在创建这一步，根本不等到读取的时候。
3. 咱们保留 `D3D11_USAGE_IMMUTABLE`、只把初始数据那两行去掉，IMMUTABLE 必配初始数据的约束就现身了：创建报错，错误码与提示都指向那对约束。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `ID3D11Resource` 是干什么的？哪几个接口从它派生？上下文身上哪些方法的参数收的就是这个基类？
2. 缓冲三兄弟各喂给管线的哪一站？常量缓冲的绑定旗标为什么不能跟别的混用？
3. `MipLevels` 填 0 与填 1 各是什么意思？填 1 的纹理缩小采样的时候会出什么症状？
4. USAGE 四档里，哪一档创建时必须给初始数据？哪一档不能当渲染目标？哪一档不直接绑管线、只靠拷贝搬运？
5. CPUAccessFlags 在 DEFAULT 与 IMMUTABLE 档必须填什么？DYNAMIC 通常配哪个值？
6. 自动生成 mipmap 对 BindFlags 与 MiscFlags 各有什么要求？
7. SRV、RTV、DSV、UAV 各对应什么用途？各走哪个绑定调用？哪一位是读写通吃的？
8. 子资源的编号公式是什么？256×256、9 层 mip、4 层数组的纹理，第 2 层数组第 3 层 mip 的编号是多少？
9. `DXGI_FORMAT_B8G8R8A8_UNORM` 的名字逐段怎么读？UNORM 把 0 到 255 的字节映射到什么区间？
10. 同一块缓冲想既当顶点缓冲又当着色器的表，创建的时候与造视图的时候各要多做什么？
11. USAGE 四档的选档思路是什么？静态、每帧变、偶发变、要回读的数据各走哪一档？

## 下一步

资源与视图的家底咱们盘完了，管线上的资源流转还差一块大件：画好的内容怎么送上窗口、怎么跟显示器的节拍对齐，那是交换链的正事。咱们到下一篇[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)里接着看。想马上动手的您，也有现成的路：着色器篇就是紧挨着的下一篇，咱们拿本篇的两张面孔缓冲替换它示例里的纯色顶点色，让 PS 从 `Load` 的手里取色，跑通了，本篇讲的存储与用途分开，您就亲手用上一回了。
