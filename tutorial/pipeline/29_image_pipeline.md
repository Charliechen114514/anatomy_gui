---
title: 图像：解码、缓存与 mipmap
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 图像：解码、缓存与 mipmap

按[篇目索引](index.md)的登记，本篇在 Direct3D 那一段里排的是末一篇。上一篇[《D3D12 对照：显式化的代价与收益》](28_d3d12_contrast.md)收尾的时候把话递了过来：图片怎么解码、解码的结果怎么缓存、mip 链怎么生成，正事都押给了本篇。咱们再往前翻，[《资源与视图》](25_d3d11_resources.md)（下文简称它为资源篇）讲子资源与 mip 切片的时候也留过同一个约定，本篇把它一并兑现了。咱们的主线也只有一条：咱们跟着一张 PNG，从磁盘上的压缩字节一路走到屏幕上的像素，把旅途里的每一站都认全。

本篇要往回接的旧识不少，咱们把链接挂齐：D2D 那边的[《位图与离屏渲染》](10_d2d_bitmaps.md)（下文简称它为位图篇）走过多半条 WIC 的链，[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)（下文简称它为画刷篇）讲过的，是 alpha 与透明合成的底子。还有[《坐标、DIP 与 DPI》](03_dip_and_dpi.md)（咱们简称它为 DIP 篇），讲过多倍率下的资产挑档。往后接着的[《DWM：重定向表面与窗口合成》](30_dwm_composition.md)您接着读就是。更远的[《透明、阴影、模糊》](33_transparency_effects.md)，咱们把细节留到那边再展开。

事实的底本主要是书架（也就是 `tutorial/native_win32/` 目录里那套带完整示例的老教程）的[纹理与采样器章](../native_win32/39_ProgrammingGUI_Graphics_D3D11_TextureSampler.md)（下文简称它为纹理章）与[混合与 Alpha 章](../native_win32/42_ProgrammingGUI_Graphics_D3D11_BlendAlpha.md)（下文简称它为混合章），位图篇转述过的[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)咱们也再借一回。本篇的代码都是讲解用的骨架，笔者没有在本仓库里编译验证过，您真要上手，以手上的 SDK 与文档为准。正文里有一处官方文档的备注，笔者没能联网核对到原文，落在 mip 生成的示例一节，您读到时留意一下。

## 解码链条：从容器到像素数组

位图篇咱们走的是 D2D 的来路，四站念过一遍：`CoCreateInstance` 拿的是 WIC 的工厂，`CreateDecoderFromFilename` 开的是解码器，`GetFrame(0)` 取的是头一帧，`IWICFormatConverter` 把像素归一到统一的格式。当时咱们的目的只是把 `ID2D1Bitmap` 建出来，链条走到转换器就交了棒。本篇咱们把后半程也走完：转换器交出来的还不是一段随便可用的字节，咱们想把它送进 D3D11 的纹理，中间还有一次显式的拷贝。

咱们把文件到像素的三层分开认一认。`CreateDecoderFromFilename` 认的是容器格式：PNG、JPEG 干的活是把压缩过的字节流连同元数据打包进一个文件，容器自己的手里没有现成的像素。`GetFrame(0)` 取的帧才是解码出像素的那一层，位图篇说过 GIF 那样多帧的文件里，第 0 帧就是它的封面，GUI 里的静态图取它也就够了。转换这一步为什么躲不开，JPEG 那边有现成的理由：它解出来的往往是没带透明通道的 24 位 RGB，咱们不转换的话通道数就对不上，纹理章排障清单里颜色错乱、红蓝互换的毛病就是从跳过转换来的。

为什么要拷贝？咱们在位图篇里走到头靠的是 `CreateBitmapFromWicBitmap`，搬运的活被 D2D 收进了调用里。D3D11 那边没有这样的便车：纹理章的加载函数里，像素得咱们自己拿数组接。接的调用长在 `IWICBitmapSource` 的身上（帧与转换器都是它的派生接口），参数表里头一位是裁剪的矩形，传 `NULL` 的意思就是取整幅，往后三位咱们多看两眼（`cbStride`、`cbBufferSize`、`pbBuffer`），说的全是咱们自己那块数组的事。尺寸的来路也交代一下：`GetSize` 挂在帧的身上，纹理章在转换之前就调了它，报回来的是解码后的像素宽高，咱们的数组按它备。

纹理章的写法咱们原样搬：

```cpp
pFrame->GetSize(&width, &height);

UINT rowPitch  = width * 4;          // 4 bytes per pixel
UINT imageSize = rowPitch * height;
BYTE* pPixels  = new BYTE[imageSize];

pConverter->CopyPixels(NULL, rowPitch, imageSize, pPixels);
```

四字节一个像素的格式，行宽就是像素数乘上四得来的，`CopyPixels` 拿到咱们报的行距与总字节数，就把转换好的像素一行一行地填了进来。

## 行距：一行像素在内存里占多宽

stride 这个词咱们该正式认识了。它说的就是 `rowPitch` 那个数的含义：一行像素在内存里占的字节数。它有个更口语的别名 pitch。画刷篇建棋盘格画刷的时候，这个数只挂着英文注释里的名字 row pitch，位图篇回顾到那一段的时候，才给它定了行距这个叫法。一个像素 4 字节的格式，宽 64 的图一行就是 256 字节。这个数看着是乘法就能算出来的，不过它值得咱们单独立一个词，原因藏在行尾补的字节里。

补字节干什么？咱们得从行对齐讲起。不少格式与接口要求每行的字节数凑成某个数的倍数，BMP 文件格式要求的就是 4 字节的倍数：宽 5 个像素、每像素 3 字节的图，一行实打实占的是 15 个字节，咱们得补到 16 才算完，末尾补的那 1 个字节不装像素，只为让下一行从整齐的地址起步。这么干的动机是效率，CPU 按机器的字长取内存，行首落在了整齐的地址上，取数的活也就走得更顺了。24 位的 BMP 讲究这个，4 字节一个像素的格式算出来的行宽天然就是 4 的倍数，咱们才敢拿 width 乘 4 一算了事。

`CopyPixels` 的参数表把 stride 交给调用者来填，道理也在这里：数组是咱们自己的，一行想留多宽咱们说了算，转换器只管按咱们报的数一行一行地填。咱们报 width 乘 4，告诉它的就是行与行紧挨着，一个多余的字节也不留。往后咱们把这数组交给 `CreateTexture2D`，随行的 `D3D11_SUBRESOURCE_DATA` 里有个 `SysMemPitch` 字段，纹理章填的正是同一个数，两头的行距对上了，像素也就不会错位了。

## 像素格式：预乘落在哪一层约定

位图篇咱们选格式的时候，WIC 侧的目标是 `GUID_WICPixelFormat32bppPBGRA`，名字里那个 P 就是预乘（premultiplied）的缩写。预乘的意思位图篇讲过：进显存的像素，红绿蓝三个分量已经各自乘过了自己的透明度。画刷篇讲 alpha 的时候咱们已经知道，透明合成干的是按比例混合两层颜色的活，预乘的版本在合成时省掉了一次乘法，D3D11 的混合状态可以把 `SrcBlend` 直接写成 `D3D11_BLEND_ONE`，混合章对这套参数有完整的展开。它还写了预乘的另一桩收益：纹理缩放的时候，边缘像素是相邻像素的插值，直通 alpha 的插值会让边缘偏暗，预乘的不会。

对照着看 D3D11 那边的加载，纹理章的函数用的偏偏是不带 P 的 `GUID_WICPixelFormat32bppBGRA`。同一个书架给出了两条路与两种选法，咱们把两边的参数对着看。两章的样本咱们分开认：纹理章的示例着色器只做了采样，咱们在那一章里找不到一处配过混合状态的地方。混合章的示例配的才是混合参数，走的是直通 alpha 的标配，`SrcBlend` 写的是 `D3D11_BLEND_SRC_ALPHA`。把两章合起来看是笔者自己的拼法：BGRA 直通的数据与 SRC_ALPHA 的混合走的是同一套直通约定，配成一对是自洽的。混合章还写过配错的症状：混合参数按预乘配、数据却是直通的，您看到的画面会偏亮，反过来的配法就会偏暗。

所以预乘这件事，咱们要放对它所在的层。格式名说的是通道的排法与位宽，预乘与否说的是字节里装的数有没有乘过 alpha，两码事咱们别把它们搅在一起。D2D 把这层约定写成了位图属性里的 `alphaMode`（位图篇用过的 `D2D1_ALPHA_MODE_PREMULTIPLIED`），D3D11 的纹理描述结构里没有这一栏，咱们得自己负责把 WIC 转换的目标格式与混合参数配成一对。透明合成的整套机制排在后面的[《透明、阴影、模糊》](33_transparency_effects.md)一篇。

## 解码是贵活：别堵在 UI 线程上

咱们来算个数字。4096 乘 4096 的照片类贴图，光解出来的数组就有 64 MB（按 4 字节一个像素算的），解码连同解压缩的 CPU 活，做上几十毫秒是很平常的事。几十毫秒是什么概念？60 Hz 的屏一帧的预算才 16.7 毫秒上下，节拍是[《屏幕、帧与垂直同步》](02_screen_frame_vsync.md)讲过的。咱们把这活塞进 WM_PAINT 的路上，消息队列就堵住了，鼠标与拖动全都在等它了，卡顿就是从这儿来的。具体的耗时数笔者没有替您实测，量级是稳的。

路子也就顺出来了：解码的活挪到后台线程去干，UI 线程只收结果就成了。骨架咱们用最朴素的一套：工作线程领了文件路径就去跑 WIC 的链条，解完了把像素打包成结果结构，`PostMessage` 一条自定义消息（`WM_APP` 起的那段消息编号是留给程序自己用的）把结果递回 UI 线程，位图或纹理对象的创建留在 UI 线程上做。D2D 的对象咱们不让它跨线程串门，重活也就只落在解码上了，这样的分工最省心，咱们要的结果结构并不复杂：

```cpp
struct DecodedImage {
    std::vector<BYTE> pixels;   // converted bytes, tightly packed rows
    UINT width, height;
    UINT64 generation;         // which request this belongs to
};
```

线程这一头还有要跟您交代的事，咱们挨个说。头一件是 COM 的：WIC 的工厂从 `CoCreateInstance` 来，COM 的要求是每个用它的线程都各自报到，所以工作线程进门头一件事是 `CoInitializeEx`，工厂也就地建、就地收了。另一件是所有权的：`PostMessage` 递回的指针得是从堆上 new 出来的结果，您在 UI 线程里收到、取走、delete，咱们把两边的约定说死了，免得出了没人认领的泄漏。

取消怎么办？咱们翻一翻 WIC 的调用表，`CreateDecoderFromFilename` 与 `CopyPixels` 的参数里都没有留取消的口子，解码一旦开跑了就会跑到底。朴素的办法是不去打断它：咱们给每次加载请求发一个递增的代号（generation，咱们就叫它代次号），任务把代次号随结果带了回来，咱们在 UI 线程那头一对，对不上当前的就把结果直接丢弃：用户早就换了目标图，或者窗口已经关了，旧的结果连上屏的资格都没有。取消的活在这套做法里，实际干的就是把过期结果的销毁顶了。

咱们把工作线程那一侧的骨架也摆出来，让您有个下手的地方。消息的编号咱们取 `WM_APP` 加一，咱们叫它 `kWMImageReady`。函数进门收的是路径、代次号与窗口句柄，线程里的活只有解码与打包，收尾的一句 `PostMessage` 把堆上的结果递回去。

```cpp
// const UINT kWMImageReady = WM_APP + 1;

struct DecodeRequest {
    std::wstring path;
    UINT64 generation;
    HWND hwnd;
};

DWORD WINAPI DecodeWorker(LPVOID param)
{
    DecodeRequest req = std::move(*static_cast<DecodeRequest*>(param));
    delete static_cast<DecodeRequest*>(param);

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // run the WIC chain here: decoder, GetFrame, converter,
    // CopyPixels into the vector -- all off the UI thread
    DecodedImage* pResult = new DecodedImage{};
    // ... fill pixels / width / height from the converter ...
    pResult->generation = req.generation;

    PostMessage(req.hwnd, kWMImageReady, 0,
                reinterpret_cast<LPARAM>(pResult));
    CoUninitialize();
    return 0;
}
```

收消息的分支要干的活不多，咱们挨件看：取指针、核对代次号、按结果建位图。代次号对不上的结果，咱们 delete 掉就算收了工。窗口提前关了的情形也交代一下：目标窗口没了，`PostMessage` 也就发不出去了，堆上的结果就没人接了，真正的工程里咱们会让发起方在退出前把手里的线程等完，或者给结果挂上更长的生命周期，笔者在这里只把风险指给您看。

## 缓存：解码一次，反复去取

解码既然贵了，同一张图被用第二回的时候，咱们就没有理由再付一遍了。列表项的图标、表情面板的小图、滚动的封面墙，都是同几张图反复上屏的常客。缓存的路子也直白：咱们拿一个键把解码的产物存起来，再要的时候按键取就成。朴素版咱们拿文件路径当键，讲究一点的版本会把文件的大小与修改时间也拼进键里，磁盘上的图被换过了，键就跟着变了，旧的缓存自然作废。键的深浅，咱们按自己程序的毛病挑，图片从不更换的小工具，拿路径做键也就够用了。

存下来的东西也有讲究。位图篇讲过位图住在显存里的事，咱们缓存的要是解码好的位图或纹理对象，省下的就是解码加上传的两趟。缓存的要是 CPU 侧的像素数组，设备丢失的时候它活得住，不过每回上屏还得再建一次对象。GUI 的图量通常有限，咱们朴素地选头一种，设备一丢咱们就整包丢弃重建，位图篇的 `D2DERR_RECREATE_TARGET` 回路是现成的。

淘汰的规则，咱们用最常用的那套 LRU，它的全名是最近最少使用（least recently used）。教科书里的标准摆法是一条双链表配一张哈希表：取用一回就把条目提到了队头，预算装不下新条目的时候，咱们就从队尾一路丢起、丢到装得下为止。住在队尾的是最久没人碰的图，重建它的代价只是一次解码，咱们丢它也就不心疼了。

咱们把淘汰的循环写成伪码，配上字节的记法：

```cpp
// on every hit:
//   move the entry to the front of the list
// on every insert:
//   push the new entry to the front
//   budget += entry.bytes
//   while (budget > limit)
//       evict the back entry, budget -= its bytes
```

咱们要是缓存解码好的 D2D 位图，字节数按 `GetPixelSize` 还回来的像素宽高去乘就成（位图篇讲过它跟 `GetSize` 的分工，一个还的是像素，一个还的是 DIP），具体的算法下一段摊开。

预算的掂量，咱们按字节来算。每张 4 字节像素的图，占的字节数就是宽乘高再乘 4，挂上了 mip 链的话，整条的字节数再乘上三分之四。4096×4096 的图顶层数组 64 MB，整条链就算成了约 85 MB。得数为什么是它，咱们在后面讲 mipmap 的时候摊开算给您看。上限的数字给多少，咱们找不到普适的答案：控件图标类的小程序，几 MB 就宽裕了，图库类的程序给到几百 MB 也正常。超了丢谁，LRU 已经替咱们答了，咱们要做的只是把预算的数写下来，别让它变成一拍脑袋的玄学。

## 缩小为什么会闪：欠采样的毛病

资源篇讲 `MipLevels` 的时候转述过纹理章的警告：层的数目只填 1、纹理缩到很小的时候，画面上的像素就会闪起来。咱们现在把机理补上。咱们想象一张细条纹的图，比如远处砖墙的贴图，条纹的间距只有几个纹素（纹素就是纹理里的一个格子，资源篇讲过的单位）。它被缩到很小的尺寸上了屏之后，屏幕上的一个像素只从整幅纹理里采一个样点，条纹比样点的间隔还密，采样点落到的位置每帧又有些偏移，这一帧捞到的可能是亮纹、下一帧捞到的可能就是暗纹，闪烁也就来了。信号处理早就给它起了名字，咱们叫它欠采样（aliasing），咱们中文里也常译作走样。

咱们把双线性插值请来也救不了它。`LINEAR` 插的是采样点周围四个纹素的加权平均，可它的滤波窗口就这么大，比窗口还细的内容它滤不掉。

毛病咱们看得清：拿原图的细节密度，去供一个密度低得多的屏幕，中间缺了一道把细节提前收走的工序。

mipmap 就是补那道工序的东西。咱们给纹理预造一摞缩小的版本：顶层装的是原图，往上一层的长宽各减了半，再往上一层又减了半，一路减到 1×1 才收了顶，整摞就是资源篇说过的 mip 链。每一层是这么造的：把下一层的内容滤过一遍再缩，相邻的纹素平均掉了，细节也就被收走了，剩下的都是低一档的频率。等缩小的采样落到高层上，它面对的已经是一张细节密度合适的图，欠采样也就无从谈起了。预过滤这个词说的就是它：过滤的工序走在降采样的前面，塔是预过滤出来的塔。

存储的代价咱们顺手算清。每层的面积是上一层的四分之一，咱们把整条链加起来就是一份顶层、四分之一份、十六分之一份，咱们一路加下去，顶层以下各层合计是顶层的三分之一，再添上顶层的整份，算出来的就是三分之四。64 MB 的顶层，配整条链就是约 85 MB 的开销。多出来的三分之一，换的是全尺寸范围内的干净采样，多数的程序都换得起。

## mip 链的生成与采样的挑层

到了 D3D11 的地盘上，咱们怎么把塔造起来？资源篇讲子资源的时候把条件记全了大半：纹理描述里的 `MipLevels` 填成 0，运行时会替咱们数出完整的层数。咱们得在 `BindFlags` 上同时挂 `D3D11_BIND_SHADER_RESOURCE` 与 `D3D11_BIND_RENDER_TARGET`。还有 `MiscFlags` 里的 `D3D11_RESOURCE_MISC_GENERATE_MIPS` 也得带上。三样配齐了，立即上下文身上的 `GenerateMips` 收一个 SRV，整条链就补齐了。立即上下文自己的正事归[《D3D11 设备与立即上下文》](24_d3d11_device.md)，本篇咱们不展开。

生成的路数资源篇里给咱们提过：GPU 画的是缩小一层的内容，画的去向又是同一块纹理，所以它才需要渲染目标的绑定。

还差半句资源篇没提的：格式本身得是能当渲染目标用的那一类。官方的 GenerateMips 文档页写明了格式要求的来路，常用的 `B8G8R8A8_UNORM` 也在能用的名单里，咱们链上用的正是它。

咱们拿资源篇算过的 256×256 纹理再看一眼：咱们把 `MipLevels` 填成 0，D3D11 数出来的是 9 层，尺寸从 256、128、64 一路减半到了 1×1，子资源的编号从 0 排到 8，编号的公式资源篇给过，咱们不再抄一遍了。咱们真用 GenerateMips 生成的时候，填进纹理的只是顶层的像素，其余 8 层都是调用之后补出来的。

采样的那一头，硬件替咱们挑层。像素着色器里相邻像素的纹理坐标差得越大，说明的是纹理在屏幕上被压得越狠，硬件挑的层也就越高，挑多高咱们不用管，咱们要管的是采样器的 `Filter` 字段选哪个档。纹理章列过的枚举，名字都是 MIN、MAG、MIP 三段拼起来的：MIN、MAG 两段管纹理缩小、放大两个方向的层内采样，MIP 段管层间的挑层与插值。咱们按 mip 段读一遍：`D3D11_FILTER_MIN_MAG_MIP_POINT` 的三段全是点采样，mip 段不做层间的插值，层与层切换的时候画面就会跳。双线性对应的是 `D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT`，它做的是层内的插值、层间不插。三线性对应的是 `D3D11_FILTER_MIN_MAG_MIP_LINEAR`，层内插完了再把相邻两层的结果插一次，层与层的切换就磨平了。再往上还有各向异性的 `D3D11_FILTER_ANISOTROPIC`，它在斜面上的质量更好，开销也大了不少。GUI 的图咱们到三线性就够用了，各向异性是 3D 场景里的家什。

采样器怎么建，纹理章里有现成的成稿，咱们把 GUI 场景的档位填进去：

```cpp
D3D11_SAMPLER_DESC sampDesc = {};
sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
sampDesc.MinLOD = 0;
sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

pDevice->CreateSamplerState(&sampDesc, &pSampler);
```

寻址的三格咱们给的是 CLAMP：UV 超出范围就钳在边缘的颜色上，单张图片的采样要的就是它。纹理章的 WRAP 是平铺的档，位图要重复铺满一片区域的时候咱们再换它，判断的路数跟位图篇分画刷与 DrawBitmap 是同一个思路。

咱们再回头认一门亲戚。DIP 篇讲多倍率 DPI 的时候提过，位图资源要按屏幕的档位挑尺寸的版本，书架的[图标、光标、位图资源章](../native_win32/14_ProgrammingGUI_NativeWindows_IconCursorBitmap.md)里的一份 ICO 文件装的就是多个尺寸。离线备好一摞缩小的版本、用时挑合适的一份，这跟 mip 链的思路是一样的，mip 把那摞版本自动造进了纹理里，挑层的活也交给了硬件。区别在服务的对象上：DPI 的版本表照顾的是屏幕密度，mip 链照顾的是缩放幅度，两条路就在缩小采样的问题上碰了头。

## 示例：补全位图篇那条半程的链

机制讲完了，咱们把承诺过的骨架拼出来。位图篇里 WIC 的几站是当片段贴的，本篇咱们把它们装订成一个完整的函数：进门的参数是渲染目标、WIC 工厂与文件路径，出门交还的是一张 `ID2D1Bitmap`，每一步的 `HRESULT` 都查过，失败了就就地返回。WIC 的四步与 D2D 的收尾都跟位图篇、纹理章的成稿一致，装订本身是笔者落的笔，笔者没有编译验证过。

```cpp
HRESULT CreateBitmapFromPngFile(ID2D1RenderTarget* pRT, IWICImagingFactory* pWIC,
                                const wchar_t* path, ID2D1Bitmap** ppBitmap)
{
    IWICBitmapDecoder* pDecoder = nullptr;
    HRESULT hr = pWIC->CreateDecoderFromFilename(path, nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) {
        return hr;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);

    IWICFormatConverter* pConverter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = pWIC->CreateFormatConverter(&pConverter);
    }
    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0f,
            WICBitmapPaletteTypeMedianCut);
    }
    if (SUCCEEDED(hr)) {
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            0.0f, 0.0f);   // DPI 0: follow the WIC source's resolution
        hr = pRT->CreateBitmapFromWicBitmap(pConverter, props, ppBitmap);
    }

    if (pConverter != nullptr) { pConverter->Release(); }
    if (pFrame != nullptr) { pFrame->Release(); }
    pDecoder->Release();
    return hr;
}
```

转换器初始化的目标格式给的是带 P 的 PBGRA，与位图篇 D2D 路线的选法是一致的，预乘在转换的那一步就办好了，咱们不用自己动手乘。位图属性里的 DPI 给 0，位图篇引过官方的文档：用的是 WIC 来源自带的分辨率。错误处理走的是最朴素的逐级返回，真实项目里您多半还想给每一步配上日志与失败的退路，骨架就留给您扩了。

这个函数跑在哪个线程的问题，咱们按前文的路数摆：它整个落在 UI 线程上是能跑的，小图这么用是没有毛病的。大图的活您就把 WIC 的四步拆到工作线程，D2D 的收尾一步留在 UI 线程，用的还是前文那套代次号加 PostMessage。

## 示例：mip 就绪的 D3D11 纹理加载

纹理章的 `LoadTextureFromFile`，咱们动四处就能把 mip 链接上。头三处咱们落在描述结构上，就是下面的代码。第四处落在 SRV 的建法上，为什么躲不开，您到本节末段能看到交代：

```cpp
texDesc.MipLevels = 0;   // let D3D11 count the full chain
texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE
                  | D3D11_BIND_RENDER_TARGET;
texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

// ... CreateTexture2D with the level-0 pixels, create the SRV
//     (pass nullptr for the SRV desc so the view spans the
//      whole chain), then one call on the immediate context:
pContext->GenerateMips(pSRV);
```

初始数据的一处文档细节，笔者如实交代：书架纹理章的 mip 片段只改了描述的字段，随行的初始数据是没有的，第 0 层的像素怎么送进挂着 GENERATE_MIPS 的纹理，按笔者的理解，官方 CreateTexture2D 文档的备注容许只备第 0 层的初始数据，其余的层留给 `GenerateMips` 补。可笔者写这篇时网络不通，笔者没能翻开原文核对，您落地前务必自己翻一眼文档，别把它当成验证过的话。

SRV 的描述是第四处改动，咱们干脆整个省掉，创建视图时传 `nullptr` 的意思是圈住整块资源，资源篇造 RTV 的时候用过同一招，mip 链的每一层也就都在视图的圈里了。纹理章原函数的 SRV 描述把 `Texture2D.MipLevels` 写死成 1，照它的写法配 mip 链，圈出来的视图就只剩顶层了，咱们要么省掉描述，要么就把链长如实地填全。再看采样器的那一头，咱们把 `Filter` 换成 `D3D11_FILTER_MIN_MAG_MIP_LINEAR` 的三线性，挑层与层间插值的活就都上齐了。

## 您能动手的三处

1. 缩小的闪烁，您亲手见一回最有说服力。咱们拿纹理章的函数原样建一张细条纹的纹理（`MipLevels` 填 1），咱们把它缩到很小画进窗口里，拖一拖窗口的大小，摩尔纹就冒出来了。咱们再把本篇的四处改动加上，同一个场景咱们跑第二遍，闪的就换成稳的了。
2. 咱们把采样器的 `Filter` 在 POINT、MIN_MAG_LINEAR_MIP_POINT、MIN_MAG_MIP_LINEAR 三档之间切换着看同一张缩小的图，层与层切换的跳动在哪一档消失，您眼见为实。
3. 缓存与不缓存的对照：咱们拿同一个文件夹的几十张图画一个滚动的列表，头一版每回的显示都走解码，第二版按路径做了缓存。滚动起来的顺滑差别您看得见，任务管理器里 CPU 一栏的曲线也替咱们作证。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. WIC 的解码链条从工厂到 D2D 能收的对象要走哪几步？哪一步做的格式归一？
2. stride 说的是什么？为什么 `CopyPixels` 要调用者来填？BMP 的行对齐补的是什么字节？
3. 预乘 alpha 省掉了合成里的什么？混合参数按预乘配、数据却是直通的，画面会怎么？
4. 解码挪到后台线程后，位图对象为什么留在 UI 线程建？代次号取消的思路是什么？
5. LRU 的朴素淘汰规则怎么走？缓存一张 2048×2048、4 字节像素、带完整 mip 链的图，预算按大约多少 MB 算？
6. 纹理缩小为什么会闪？mip 链靠什么把这毛病治住了？
7. `GenerateMips` 对纹理描述的哪三个字段有要求？调用本身长在谁身上、收的是什么参数？
8. 采样器 `Filter` 枚举名字里的三段各管什么？三线性比双线性多做了什么？

## 下一步

图像从磁盘到采样的旅途到本篇就走完了，Direct3D 那一段也随之收了摊。下一篇[《DWM：重定向表面与窗口合成》](30_dwm_composition.md)您接着走就是。想马上动手的您也有现成的活：本篇 mip 就绪的加载函数，配上纹理章的全屏四边形与着色器，咱们把一张大图缩放着画出来，咱们在 Filter 三档各截一张图摆在一起，欠采样被治住前后的那点差别，就是您自己机器上的一手证据。
