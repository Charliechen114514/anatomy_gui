---
title: D3D12 对照：显式化的代价与收益
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# D3D12 对照：显式化的代价与收益

按[篇目索引](index.md)的登记，本篇在 Direct3D 这一段里排的是第六篇，也是给 D3D11 收尾作对照的一篇。前面五篇咱们把链接挂齐：[《GPU 管线：从顶点到像素的各阶段》](23_gpu_pipeline.md)（下文简称管线篇）、[《D3D11 设备与立即上下文》](24_d3d11_device.md)（简称设备篇）、[《资源与视图》](25_d3d11_resources.md)（简称资源篇）、[《着色器与常量缓冲》](26_d3d11_shaders.md)（简称着色器篇），还有紧挨着的[《交换链与呈现（DXGI）》](27_dxgi_swapchain.md)（简称交换链篇）。排在本篇后面的[《图像：解码、缓存与 mipmap》](29_image_pipeline.md)，咱们到那边接着读。

设备篇与交换链篇给咱们留了两件旧事，本篇把两件一起兑现了。设备篇讲延迟上下文的时候说过：多线程录制的旧设计到了 D3D12 被更彻底的命令队列与命令列表顶了替，边界也整个重画了一遍，那句话咱们留到了本篇细看。交换链篇的收尾也说过，Present 一句就把页翻了的轻松，是 D3D11 替咱们扛下来的，D3D12 把这些一样一样地还给了程序，代价与收益的对照就归了本篇。读者设定照旧，您会 C++ 和 Win32 的基础用法就够，D3D12 的对象咱们从零认起。

事实的底本主要是书架（也就是 `tutorial/native_win32/` 目录里那套带完整示例的老教程）的 D3D12 几篇：[设计哲学章](../native_win32/43_ProgrammingGUI_Graphics_D3D12_Philosophy.md)、[命令队列章](../native_win32/44_ProgrammingGUI_Graphics_D3D12_CmdQueue.md)、[资源与堆章](../native_win32/45_ProgrammingGUI_Graphics_D3D12_ResourceHeap.md)、[描述符与根签名章](../native_win32/46_ProgrammingGUI_Graphics_D3D12_DescRootSig.md)，微软官方文档的 D3D12CreateDevice、CreateCommandQueue、ResourceBarrier 与 SetEventOnCompletion 几页，咱们也对照着查过。本篇的代码全部是讲解用的骨架，笔者没有在本仓库里编译验证过，您真要上手，以手上的 SDK 与文档为最后的依据。

## 动机：驱动替咱们管的事，一件件还回来

咱们在 D3D11 那五篇里过的日子，回头一看其实相当舒服。调度的活归了立即上下文，咱们递一句 Draw 进去，驱动当场就收了它。同一块纹理既当画布又当采样源的用法，运行时也自己替咱们插了屏障。Present 一句就翻了页，翻页的队列排多深、缓冲怎么轮换，咱们全都不用操心。舒服的代价藏在底下，书架设计哲学章把缘由摆开了：咱们每次提交一个 Draw，驱动都得检查资源的状态、插入必要的屏障、管理内存的驻留、处理隐式的同步，这些活全都落在了 CPU 端，而且对咱们的代码不可控。场景复杂到了一定程度，瓶颈就常常出在 CPU 来不及给 GPU 喂数据的身上，GPU 本身其实并没有忙不过来。

D3D12 的目标就是砍掉这层 CPU 侧的开销，它的路子也很直接：把驱动替咱们管的那些职责一件件还给程序。命令的录制与提交、资源的状态转换、内存的驻留、CPU 与 GPU 的同步，往后全由咱们亲手管。书架设计哲学章给过一个量级的说法，咱们原样转述：代码量翻了三五倍，收益是做对了的话，CPU 开销可以降到之前的十分之一甚至更低。咱们注明它只是书架的估计，咱们自己没有实测过，但方向是对的：显式化拿样板代码换驱动的卸载。

还有一样的收益落在多线程上。设备篇咱们见过 D3D11 的延迟上下文：录制是可以并行的，执行的口子仍然只有立即上下文一个，各家驱动的支持还参差。D3D12 把命令的录制与提交整个解耦了，CPU 在这儿录下一帧的命令表，GPU 在那头同时执行上一帧的命令，几条命令表也能由几个线程各录各的，录完的一摞一起交上去。这套并行是 D3D12 性能优势的根，代价则是样板代码的暴涨，咱们后面的章节一件一件数过去。

咱们把两代的差别并成一张表，讲着讲着您翻回来对一眼：

| 维度 | D3D11 | D3D12 |
|---|---|---|
| 命令提交 | 立即上下文调一句送一句 | 命令表录制，命令队列整摞提交 |
| 资源状态 | 运行时自动跟踪与转换 | 咱们手动 ResourceBarrier 报备 |
| 资源绑定 | 各式 Set 一句一句绑 | 描述符堆加根签名间接引用 |
| 管线状态 | 混合、光栅化逐项 Set | PSO 一次打包，整套切换 |
| 同步 | 驱动隐式插入等待 | 围栏显式打信号、显式等 |
| 多线程 | 延迟上下文，支持参差 | 命令表原生并行录制 |

平台的门槛咱们顺带交代。书架记的环境是 Windows 11 配 Windows SDK，而 D3D12 从 Windows 10 起就原生支持了。设备创建走的是 D3D12CreateDevice。功能级别（feature level）是标记 GPU 能力档位的编号，档位的高低就写在编号的数字里，咱们在 12 里能要的最低档从 11_0 起步，9_1 到 10_1 的老档位它一概不认。所以老机器上 11 能跑、12 起不来的情形是存在的，咱们拿虚拟机跑示例的时候，咱们心里都得有这根弦。

## 命令队列与命令表：从排队办事到自己录册子

咱们从命令的流转看起。D3D11 那头的立即上下文，它的脾气设备篇讲过：发出的命令当场就往 GPU 送，像咱们在柜台前排队办事，咱们把 Draw 递进去，驱动的收件、校验、转发都当场发生。D3D12 把柜台换成了三件套。命令队列 `ID3D12CommandQueue` 咱们自己开，它才是命令真正进 GPU 的通道。命令分配器 `ID3D12CommandAllocator` 管着一段录命令用的内存池，刚录下来的命令就都放在了这里。真正握在咱们手里的是命令表 `ID3D12GraphicsCommandList`，咱们把清屏、设管线状态、发 Draw 的活一笔一笔录在它身上。录完的命令表要调 Close 封册，再由队列的 ExecuteCommandLists 一摞交上去。

```cpp
D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;   // the 3D graphics kind
queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

ComPtr<ID3D12CommandQueue>        commandQueue;
ComPtr<ID3D12CommandAllocator>    commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;

device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                               IID_PPV_ARGS(&commandAllocator));
device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                          commandAllocator.Get(), nullptr,   // no initial PSO yet
                          IID_PPV_ARGS(&commandList));

// ... record commands on commandList ...
commandList->Close();

ID3D12CommandList* lists[] = { commandList.Get() };
commandQueue->ExecuteCommandLists(1, lists);
```

类型上咱们认一个就够：DIRECT 是直接交给图形队列执行的常用档。书架还报了 BUNDLE、COMPUTE 与 COPY 几位，命令包、计算与拷贝各有各的队列，咱们本篇用不上，您混个脸熟就好。命令表与分配器各有一个自己的 Reset，两样的脾气却差得很远，书架的命令队列章特意把它们分开讲。命令表的 Reset 只是把录制的接口拨回开头的轻量动作，咱们每帧开头调它一次就好。分配器的 Reset 是真的回收命令内存，硬性的前提是 GPU 已经执行完录在它身上的全部命令，不然的话调试层当场报错。怎么知道 GPU 干完了呢？这要等围栏一节的主角出场，名字咱们记在这儿。

设备篇欠下的那句话，咱们在这儿兑现：延迟上下文在 D3D12 里没有继承者，因为整家的设计都换了。命令表天生就是拿来做录制的，几个线程各拿一条各录各的，录完了的一摞一起交给 ExecuteCommandLists，官方的文档还提过驱动可能把一摞命令表合并着执行。设备与上下文一个管造、一个管用的两根柱子，在 12 里变成了设备造对象、队列收命令、命令表干活的三件套，边界确实整个重画了一遍。

还有一处容易落空的地方，书架的设计哲学章提醒过，咱们原样转述：在 D3D12 里建链的时候，CreateSwapChainForHwnd 的头一个参数收的是 `ID3D12CommandQueue`。从 11 过来的手上习惯很容易把设备递进去，吃回来的就是 DXGI_ERROR_INVALID_CALL。交换链要跟咱们开的命令队列直接挂钩，翻页的活才排得进咱们自己的队列。

## 根签名与描述符堆：着色器参数的申报单

D3D11 那边绑资源的办法，咱们在资源篇与着色器篇过的全是各式 Set：PSSetShaderResources 塞的是 SRV，VSSetConstantBuffers 塞的是常量缓冲，一句对一个槽的写法。D3D12 把这套换成了两层的结构。底下的那层是描述符堆（descriptor heap）：描述符就是 11 里视图的对应物，资源的解读方式与内存的位置，都被写成了一小块固定大小的说明。CBV、SRV、UAV 共住的是一类堆，采样器、RTV 与 DSV 各有各自的堆。UAV 的全名是无序访问视图，着色器里咱们既能拿它读也能拿它写，RTV 与 DSV 的全名分别是渲染目标视图与深度模具视图，资源篇讲视图的时候咱们都见过。堆里的位置用句柄来指，句柄怎么挪咱们得问设备：GetDescriptorHandleIncrementSize 报的就是一个描述符占的字节数，这个数是随 GPU 与描述符类型变的，咱们不能拿 sizeof 去猜。

堆还有一道可见性的开关。带 D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE 的堆才暴露给着色器直接访问，CBV/SRV/UAV 的堆通常要带上它。RTV 与 DSV 只在绑管线的时候用得到，不带也就无所谓了。着色器可见的堆占着 GPU 里专门的地址空间，硬件对同时绑定的个数是有限制的，咱们实际用起来全程序开一到两个通常也就够了。

再往上的那层是根签名（root signature），咱们把它读成着色器参数的申报单：着色器要用的槽有几个、每个槽又从哪儿取，全都写进了单子里。单子上的参数分三档，书架的描述符与根签名章给过一页性能对照，咱们借来排开看：

- 描述符表：最省根上的地方，一张表只占一个 DWORD，指向堆里一段连续的位置，咱们能把一批资源一次绑齐，代价是多出来的一次间接寻址。
- 根描述符：直接写资源的 GPU 虚拟地址，省了查表的功夫，占两个 DWORD，可惜只收缓冲类的资源，纹理挂不上去。
- 根常量：最直接的一档，32 位的值内嵌在根签名里，着色器拿它的速度跟拿寄存器一样快。

整张单子的上限是 64 个 DWORD，三档的搭配怎么排，就是咱们在 12 里做绑定设计的头一道题。

咱们把最常见的一张单子写成骨架，一个 CBV 的描述符表，配的是 HLSL 里的 register(b0)：

```cpp
D3D12_DESCRIPTOR_RANGE cbvRange = {};
cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
cbvRange.NumDescriptors      = 1;
cbvRange.BaseShaderRegister  = 0;             // register(b0) in HLSL
cbvRange.OffsetInDescriptorsFromTableStart =
    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;    // right after the previous range

D3D12_ROOT_PARAMETER rootParam = {};
rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootParam.DescriptorTable.NumDescriptorRanges = 1;
rootParam.DescriptorTable.pDescriptorRanges   = &cbvRange;
rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 1;
rootSigDesc.pParameters   = &rootParam;
// a static sampler could be embedded here, no heap space needed

ComPtr<ID3DBlob> serialized, error;
D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                            &serialized, &error);
device->CreateRootSignature(0, serialized->GetBufferPointer(),
                            serialized->GetBufferSize(),
                            IID_PPV_ARGS(&rootSignature));
```

描述结构填好了还不能直接用，咱们得拿 D3D12SerializeRootSignature 把它序列化成二进制，再造出 CreateRootSignature 的对象。书架的描述符与根签名章提过一个大家常栽的跟头：范围重叠、寄存器编号写错一类的问题都到序列化的这一步才暴露，错误信息走的是 error blob，咱们把它打印出来才看得到。着色器那头也有对等的写法，HLSL 里的 RootSignature 属性能把同一张单子嵌进着色器的源码，两边写的布局必须对得上，不然绑定的行为就没有保证了。

每帧用起来的时候是三步：咱们拿 SetGraphicsRootSignature 把申报单递上去，再用 SetDescriptorHeaps 把描述符的堆挂上，末一步用的是 SetGraphicsRootDescriptorTable，把表指到堆里的位置。真正的硬约束只有一条：根签名得设好了，描述符表才指得了，它要指的堆也得挂好，这两步的次序咱们倒不用讲究。CBV 那里还有一条硬性的要求值得跟 11 对照：着色器篇讲过 D3D11 的常量缓冲字节数得对齐 16，D3D12 的 CBV 把门槛提到了 256，缓冲的大小与视图的 SizeInBytes 都得是 256 的倍数，差一点创建就把咱们拒了。

## PSO：整套状态打包成一个对象

管线状态对象管的是 11 里咱们逐项 Set 的那堆现场，它的全名是 pipeline state object，咱们简称 PSO。设备篇的创建清单里有 CreateBlendState 一类的调用，11 的世界里混合状态、光栅化状态、深度状态各有各的对象，绑上了上下文的它们随 Set 随换，驱动在每次 Draw 前都得把当前的一堆现场汇齐了核一遍。12 的路子是一次打包：D3D12_GRAPHICS_PIPELINE_STATE_DESC 把两段着色器、混合、光栅化、深度模具、输入布局、图元拓扑类型、渲染目标的格式全填进一份描述，CreateGraphicsPipelineState 造出来的是一个不可变的对象，绘制的时候 SetPipelineState 一调，整套的现场一次换齐。

```cpp
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = rootSignature.Get();
psoDesc.VS             = vsBytecode;   // D3D12_SHADER_BYTECODE from compiling
psoDesc.PS             = psBytecode;
psoDesc.BlendState         = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
psoDesc.RasterizerState    = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
psoDesc.DepthStencilState  = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
psoDesc.InputLayout    = { inputElements, ARRAYSIZE(inputElements) };
psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
psoDesc.NumRenderTargets = 1;
psoDesc.RTVFormats[0]  = DXGI_FORMAT_B8G8R8A8_UNORM;
psoDesc.SampleDesc.Count = 1;

device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
```

打包成不可变的对象，咱们图的是什么？驱动在创建 PSO 的时候就能把整套状态编译成硬件要的样子，用的时候一次就能切换，逐项验证的成本没有了。11 的驱动也做状态缓存，可它不知道咱们接下来要 Set 的是哪几项，猜的活只能边收边干。12 的 PSO 把可能性收死成了一份，猜测的负担也就卸下了。命令表的创建签名里第 4 个参数 pInitialState 收的就是初始 PSO，咱们在前文的示例里传了 nullptr，真要起手就录绘制命令的时候，递过去的就该是它。辅助类型 CD3DX12 开头的那几位来自 d3dx12.h，书架专门叮嘱过它是不随 Windows SDK 走的，咱们记得这桩就行。

## 资源屏障：状态转换要自己报备

资源篇讲 mipmap 自动生成的时候，咱们见过一块纹理身兼两职的样子，渲染目标的活与采样源的活一肩挑。D3D11 里咱们挂两面 BindFlags 就完事了，运行时在背后替咱们插了屏障，咱们连资源还有状态这件事都不必知道。D3D12 把状态的登记交到了咱们手里：每块资源在任一时刻只处在一个状态里，用途要换的时候，咱们就得在命令表里拿 ResourceBarrier 显式报备。

最常见的两道屏障就长在交换链的后台缓冲上。帧的开头，缓冲刚从呈现的状态里回来，咱们报备它转成渲染目标，画完了再报备它转回呈现，一进一出的两道，咱们每帧都得录。上传顶点数据的路里还有一条链：数据拷进默认堆的时候缓冲是拷贝的目标，拷完就转成顶点缓冲可用的状态。少了一道的话，GPU 就拿旧的状态解读资源，读出来的东西就说不准了。

```cpp
CD3DX12_RESOURCE_BARRIER toRT = CD3DX12_RESOURCE_BARRIER::Transition(
    renderTargets[frameIndex].Get(),
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_RENDER_TARGET);
commandList->ResourceBarrier(1, &toRT);

// ... clear and draw ...

CD3DX12_RESOURCE_BARRIER toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
    renderTargets[frameIndex].Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PRESENT);
commandList->ResourceBarrier(1, &toPresent);
```

忘记报备的后果，书架的资源与堆章写得很重，咱们原样转述：调试层开着的话它会报错提醒，禁用了调试层，结果就不确定了，出错的画面、闪烁的画面都可能出现，甚至会在某些硬件上直接丢了设备。所以在图形调试工具里抓状态问题的时候，ResourceBarrier 的缺失是常客，咱们写 12 的代码，状态转换要当正事一样地逐处落笔，咱们不能凭感觉省。

## 围栏：CPU 等 GPU 的信号牌

D3D11 的同步咱们基本没操过心：可能打架的调用凑到一起的时候，运行时自己插了等待。D3D12 里的提交是异步的，ExecuteCommandLists 把命令表交上去就返回了，CPU 接着去干别的活，GPU 在后面慢慢地执行。可分配器的 Reset、上传缓冲（CPU 往 GPU 送数据用的中转缓冲）的释放，这些回收的活都有一个前提：GPU 已经干完了。围栏（fence）就是干这个用的信号牌，咱们拿设备身上的 CreateFence 造一个，队列在干完活的地方调 Signal 打上一个递增的值，CPU 这边拿 GetCompletedValue 查它的进度，进度没到的时候，咱们就用 SetEventOnCompletion 挂一个事件再等。

```cpp
ComPtr<ID3D12Fence> fence;
device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

void WaitForGPU(UINT64 target)
{
    if (fence->GetCompletedValue() < target) {   // fast path: already done
        fence->SetEventOnCompletion(target, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

// after ExecuteCommandLists:
currentFenceValue++;
commandQueue->Signal(fence.Get(), currentFenceValue);  // GPU bumps it here
```

真正的用法不是每帧末尾的傻等，那样的话 CPU 与 GPU 又串成了一条线，书架的命令队列章把正确的姿势摆得很清楚。咱们给每帧配一套独立的命令分配器，让 CPU 超前录好几帧的命令，轮到复用某套分配器的时候，只等几帧前对应的那道围栏。GPU 够快的话，等的那道早就过了，CPU 就一刻都不用停了。超前的帧数就是帧资源的套数，交换链篇讲过的排队深度与输入延迟的权衡，在 12 这儿变成了咱们自己填的数字。书架还画过一条并行的时间线，咱们借来摆一眼：

```text
CPU:  [录帧0] [录帧1] [等帧0的围栏] [录帧2] ...
GPU:          [执行帧0]              [执行帧1] ...
                  ↑Signal(1)              ↑Signal(2)
```

围栏自己也有一桩常发的故障，书架命令队列章的排障节写过：程序会卡死在等待上，多半是咱们忘了调 Signal，围栏的值没人去打，CPU 就永远等下去了。咱们照着链路把两头查一遍：一头查的是 Signal 调没调，另一头是每帧记的围栏值与 Signal 用的值对不对得上，核完问题一般就现形了。

围栏还牵着一桩书架资源与堆章点过的隐蔽错误，咱们记下来：CopyBufferRegion 只是录了一条命令，真正的拷贝要等 GPU 执行了才发生。上传缓冲要是函数里的局部变量，函数一返回 COM 指针就没了，GPU 执行到拷贝的时候，读到的就是已经放掉的东西。正路是把上传缓冲放进帧资源的行列，等围栏确认了 GPU 干完，咱们才能动它们，一个都不许咱们提前放。

## 画一个三角形：两代各要几步

概念都认齐了，咱们拿管线篇那个三角形当靶子，按通行的步骤划分把两边的步骤摆开数一遍。行数咱们不报，各家示例的长短差得远，报了就成了伪精确，量级看步骤就够了。

D3D11 一边的清单，咱们在前面五篇里其实全走过了：

1. 建设备与立即上下文，设备篇的 D3D11CreateDevice。
2. 建交换链，取后台缓冲造 RTV，交换链篇的正事。
3. CreateBuffer 带一份初始数据，把顶点一把送上去，管线篇走过的路。
4. 编译两段着色器，配输入布局，着色器篇的装配。
5. 每帧清屏、绑目标、Draw、Present。

D3D12 一边的清单，咱们照同样的口径摆开：

1. 建设备，D3D12CreateDevice，功能级别从 11_0 起步。
2. 建命令队列。
3. 建交换链，头一个参数挂的是队列。
4. 建 RTV 描述符堆，按增量句柄给每块后台缓冲各造一个描述符。
5. 建命令分配器与命令表。
6. 填根签名描述，序列化，造根签名对象。
7. 填 PSO 描述，连着色器与输入布局一起打包。
8. 建上传堆缓冲，Map 进去 memcpy 顶点。
9. 建默认堆缓冲，录拷贝命令，再录一道转状态的屏障。
10. 提交命令表，Signal，等围栏，上传缓冲才能放。
11. 每帧开头等几帧前那道围栏，Reset 分配器与命令表。
12. 录 PRESENT 与 RENDER_TARGET 之间的两道屏障，中间夹着清屏与 Draw，还要设根签名、挂描述符堆、指描述符表。
13. Close，提交，Signal，Present。

步骤摆开了，多出来的地方一目了然：三角形本身的步骤一步没多，管线篇画的五段图在 12 里原样适用，多出来的全是基础设施，咱们前面认过的队列、分配器、描述符堆、根签名、PSO、屏障、围栏，全在多出来的名单上。书架那句代码量翻三五倍的说法，咱们数完步骤也就信了大半。这些基础设施不是白背的，它们换来的是驱动不再替咱们核状态、插屏障的那些活，和几条命令表并行录制的路子，只是对画一个三角形的活来说，代价是收不回来的，收益也用不上了。

## GUI 用哪一代

GUI 的一帧都画些什么？清一块背景、铺几块矩形、把文本的贴图搬上去的活，Draw 的次数两只手数得过来，D3D11 驱动替咱们扛的那点开销，搁在咱们这个量级上根本不疼。咱们前五篇攒下的设备、资源、视图、着色器、交换链这些家伙，对 GUI 的活全是够用的，交换链篇里咱们也说过，消息驱动的节奏配默认的闸门就够了，闸门说的就是队列排满之后 Present 就地阻塞的那道刹车，Present 的同步间隔设 1 只是同一道闸的另一面。所以本线的示例与建议都站在 11 这边，12 并不是咱们一个窗口程序该挑的正路。

D3D12 的显式化换的是引擎级的控制权：几千次 Draw 的场景、多线程的录制、内存驻留的排班，重活抢时间的引擎要的就是这些。真要混着用的话也有一条路，书架的[互操作章](../native_win32/47_ProgrammingGUI_Graphics_D3D12_D3D11Interop.md)讲的是 D3D11On12 互操作层，拿 12 的设备包一层 11 的上下文出来，Direct2D（微软的二维绘图接口）画出来的 UI 就能叠在 12 的画面上，咱们认个脸就好，本篇就不展开了。

那咱们为什么还要认 12？图的就是看穿驱动这件事。把屏障、围栏、描述符、PSO 这几样认过了之后，咱们回头看 D3D11 的调用，每次 Set、每句 Present 背后驱动替咱们干了哪件活，心里都有数了。往后调试 11 的程序遇到怪现象，这片底细就是咱们的参照系。本线把 12 当解剖课的标本，不当日常的工具。

## 您可以动手的三处

下面的三处咱们改起来都不重，书架的示例代码是现成的底子：

1. 咱们把书架设计哲学章的骨架跑起来，调试层开着，然后把建链的头一个参数换成设备指针再试一次，DXGI_ERROR_INVALID_CALL 当场就把两条路的差别教给咱们了。
2. 咱们把清屏示例里 PRESENT 转 RENDER_TARGET 的那道屏障注释掉再跑，把调试层报错的原文抄下来，往后咱们在工具里再见到它，一眼就认得出来路。
3. 咱们把帧资源的套数从 2 改成 3，分配器、围栏值、描述符堆的分区都得跟着动，改完数一遍哪些地方被牵动了，多帧并行的约束就长在手上了。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得回去再看一遍。

1. D3D11 里驱动替咱们做的四样活是什么？D3D12 分别把它们还给了谁？
2. 命令分配器与命令表各管什么？两个 Reset 的差别与前提各是什么？
3. 延迟上下文的旧设计在 12 里被什么顶了替？执行的口子从哪个对象换成了哪个对象？
4. 建交换链时的头一个参数，11 与 12 各收什么？传错了报什么码？
5. 根签名的三档参数各占多少 DWORD？哪一档挂不了纹理？
6. D3D11 与 D3D12 的常量缓冲对齐要求各是多少字节？
7. PSO 打包了哪些东西？不可变给驱动省掉了什么？
8. 一帧里后台缓冲的两道屏障各在什么位置、转向哪个状态？
9. Signal、GetCompletedValue、SetEventOnCompletion 各长在谁身上、各干什么？
10. 上传缓冲为什么不能是局部变量？它的生命周期由谁确认？

## 下一步

Direct3D 这一段的对照到此收尾。本篇把 12 的家什认了一圈，咱们回到 11 的正路接着走：下一篇的[《图像：解码、缓存与 mipmap》](29_image_pipeline.md)，会把资源篇欠下的 mipmap 正事接上，图片怎么解码、解码的结果怎么缓存、mip 链怎么生成，您到那边逐件看。想马上动手的您也有现成的活：把书架命令队列章里那个清屏的动画跑起来，拿交换链篇介绍过的 PresentMon 录十秒，帧资源的套数 2 与 3 各录一遍，对照着看 MsBetweenPresents 的分布。这个示例由约 16 毫秒的定时器驱动，CPU 与 GPU 的活都极轻，咱们多半会看到，两套配置的分布都还是定时器的节拍。多帧并行的收益要等 CPU 录命令接近满载的时候才露头，您真想见着它的话，就往命令表里多录一些绘制的命令，把 CPU 压住了再对照两档套数，效果就更实在了。
