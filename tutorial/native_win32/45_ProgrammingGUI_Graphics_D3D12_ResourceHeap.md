# 通用GUI编程技术——图形渲染实战（四十五）——D3D12资源与堆管理：从上传到驻留

> 上回我们把 D3D12 的命令系统理了一遍——CommandAllocator 是命令内存池，CommandList 是命令录制接口，CommandQueue 是命令提交通道，Fence 负责同步。命令系统告诉我们"怎么指挥 GPU"，但指挥 GPU 做什么呢？操作数据。资源管理告诉我们"GPU 操作的数据在哪里、怎么从 CPU 传到 GPU"。这篇我们就来拆解 D3D12 的内存模型——三种堆类型、三种资源创建方式、以及资源屏障的正确使用。

## 前言：为什么 D3D12 的内存管理这么复杂

如果你还记得 D3D11 中的资源创建，那是一件相当简单的事情——调用 `CreateTexture2D` 或 `CreateBuffer`，传一个描述结构体，驱动帮你选择合适的内存位置，帮你管理驻留（Residency），帮你在需要的时候迁移数据。你根本不需要知道数据最终是放在系统内存还是显存里。

D3D12 把这些全部交给了开发者。你现在需要知道的是：GPU 有自己的专用内存（显存），CPU 有自己的内存（系统内存），两者之间的数据传输是异步的、有延迟的。你的任务是合理地安排数据在什么时候放在哪里，以及确保 GPU 在读取数据的时候数据确实还在那里。

这不是无意义的复杂化——了解底层细节意味着你可以做更激进的优化。比如把频繁更新的数据放在 Upload 堆中（CPU 可写），把只读的纹理放在 Default 堆中（GPU 本地，访问最快），把需要回读到 CPU 的数据（比如截图）放在 Readback 堆中。这种精细控制在 D3D11 中是不可能的。

## 环境说明

本篇延续前两篇 D3D12 文章的环境配置：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v143 工具集)
- **Windows SDK**: 10.0.26100 或更高版本
- **依赖**: `d3d12.h`、`dxgi1_4.h`、`d3dx12.h`（辅助头文件）
- **前置知识**: 文章 43（D3D12 设计哲学）、文章 44（命令列表与队列）

## 三种堆类型

D3D12 定义了三种预定义的堆类型，每种堆类型对应不同的 CPU-GPU 访问模式。理解它们各自的用途和限制是掌握 D3D12 内存管理的基础。

### DEFAULT 堆——GPU 的私人领地

`D3D12_HEAP_TYPE_DEFAULT` 是 GPU 本地内存（通常是显存）。CPU 不能直接访问这块内存——你不能用 `memcpy` 或指针来读写它。数据必须通过 GPU 的拷贝命令（`CopyBufferRegion`、`CopyTextureRegion` 等）从其他堆传输到 DEFAULT 堆。

DEFAULT 堆的特点是 GPU 访问速度最快。纹理、渲染目标、深度缓冲、以及那些一旦上传就不需要 CPU 端修改的顶点缓冲和索引缓冲，都应该放在 DEFAULT 堆中。对于游戏和图形应用来说，这是使用量最大的堆类型。

### UPLOAD 堆——CPU 写入的传送带

`D3D12_HEAP_TYPE_UPLOAD` 是一种 CPU 可写、GPU 可读的内存。你可以通过 `Map` 获取这块内存的 CPU 端指针，然后用 `memcpy` 写入数据，GPU 就能读到。它的主要用途是作为"上传中转站"——你先把数据写到 UPLOAD 堆，然后通过 GPU 拷贝命令把数据从 UPLOAD 堆复制到 DEFAULT 堆。

UPLOAD 堆的 GPU 访问速度比 DEFAULT 堆慢，因为它通常位于系统内存中（或者PCIe BAR空间），GPU 需要通过 PCIe 总线来访问。所以 UPLOAD 堆不适合存放 GPU 需要高频读取的数据——比如每帧被采样数百万次的纹理。但对于需要每帧从 CPU 更新的数据（比如 MVP 矩阵的常量缓冲区），UPLOAD 堆是最合适的选择。

### READBACK 堆——GPU 写回的回收站

`D3D12_HEAP_TYPE_READBACK` 是 CPU 可读、GPU 可写的内存。它的用途是把 GPU 端的数据回读到 CPU——比如截图（把渲染目标的内容读回来保存为图片）、GPU 计算结果（比如 GPU 端的碰撞检测结果）。READBACK 堆的使用频率远低于 DEFAULT 和 UPLOAD。

根据 [Microsoft Learn - Heap Types](https://learn.microsoft.com/en-us/windows/win32/direct3d12/heap-types) 的文档，三种堆类型的内存访问属性可以总结如下：

| 堆类型 | CPU 访问 | GPU 访问 | 典型用途 |
|--------|---------|---------|---------|
| DEFAULT | 不可 | 最快 | 纹理、渲染目标、静态顶点缓冲 |
| UPLOAD | 可写 | 较慢 | 常量缓冲、上传中转 |
| READBACK | 可读 | 可写 | 截图、回读计算结果 |

## 三种资源创建方式

D3D12 提供了三种创建资源的方式，复杂度递增但灵活性也递增。

### CreateCommittedResource——一步到位

这是最简单的方式，相当于"创建一个堆并在堆中放置一个资源"。一个函数调用就完成了堆的创建和资源的放置：

```cpp
// 创建一个上传堆中的常量缓冲区
ComPtr<ID3D12Resource> pConstantBuffer;

D3D12_HEAP_PROPERTIES heapProps = {};
heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
heapProps.CreationNodeMask = 0;
heapProps.VisibleNodeMask = 0;

D3D12_RESOURCE_DESC resourceDesc = {};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
resourceDesc.Width = 256;  // CBV 必须 256 字节对齐
resourceDesc.Height = 1;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

HRESULT hr = g_device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,  // UPLOAD 堆的初始状态
    nullptr,  // 清除值（仅用于渲染目标/深度缓冲）
    IID_PPV_ARGS(&pConstantBuffer)
);
```

`CreateCommittedResource` 的优点是简单——你不需要预先创建堆，一个调用搞定。缺点是每次调用都会创建一个独立的堆，堆之间可能有对齐填充导致的内存浪费。如果你需要创建大量小资源（比如几十个常量缓冲区），每个都 `CreateCommittedResource` 的话，对齐填充会浪费大量显存。

### CreatePlacedResource——手动管理堆空间

这种方式把堆的创建和资源的放置分开了——你先创建一个大的堆，然后在堆中的指定偏移位置放置资源：

```cpp
// 先创建一个大堆
D3D12_HEAP_DESC heapDesc = {};
heapDesc.SizeInBytes = 1024 * 1024;  // 1MB
heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
heapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
heapDesc.Flags = D3D12_HEAP_FLAG_NONE;

ComPtr<ID3D12Heap> pHeap;
g_device->CreateHeap(&heapDesc, IID_PPV_ARGS(&pHeap));

// 在堆的偏移 0 处放置顶点缓冲
D3D12_RESOURCE_DESC vbDesc = {};
vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
vbDesc.Width = sizeof(vertices);
// ... 其他字段

ComPtr<ID3D12Resource> pVertexBuffer;
g_device->CreatePlacedResource(
    pHeap.Get(),
    0,  // 偏移量
    &vbDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&pVertexBuffer)
);

// 在堆的另一个偏移处放置索引缓冲
// 需要计算正确的对齐偏移
UINT64 vbOffset = AlignUp(sizeof(vertices), 65536);  // 64KB 对齐

ComPtr<ID3D12Resource> pIndexBuffer;
g_device->CreatePlacedResource(
    pHeap.Get(),
    vbOffset,
    &ibDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&pIndexBuffer)
);
```

`CreatePlacedResource` 的优势是多个资源可以共享同一个堆，减少内存浪费和对齐填充。代价是你需要手动管理堆内空间的分配和回收，类似一个简单的内存分配器。

### CreateReservedResource——虚拟内存映射

这是最复杂的方式，对应 D3D12 的 Tiled Resources（平铺资源）。它创建一个虚拟地址空间，你可以按需提交物理页面。主要用于超大纹理的流式加载——比如一个 16384x16384 的地形纹理，大部分区域在当前帧是看不到的，不需要全部加载到显存中。

`CreateReservedResource` 在 GUI 编程中用得很少，我们就不展开了。

## 第一步——上传顶点数据的标准流程

理论讲够了，现在我们来实际操作。D3D12 中上传顶点数据到 GPU 的标准流程是：

1. 创建一个 UPLOAD 堆的中转缓冲区
2. `Map` 获取 CPU 端指针，`memcpy` 写入数据
3. 创建一个 DEFAULT 堆的目标缓冲区
4. 通过 GPU 拷贝命令把数据从 UPLOAD 复制到 DEFAULT
5. 插入资源屏障，将资源状态从 `COPY_DEST` 转换为 `VERTEX_AND_CONSTANT_BUFFER`

### 创建上传缓冲区

```cpp
// 顶点数据
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

Vertex triangleVertices[] =
{
    { XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(1, 0, 0, 1) },
    { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0, 1, 0, 1) },
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0, 0, 1, 1) },
};

const UINT vertexBufferSize = sizeof(triangleVertices);

// 创建 UPLOAD 堆的中转缓冲区
ComPtr<ID3D12Resource> pUploadBuffer;

D3D12_HEAP_PROPERTIES uploadHeapProps = {};
uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

D3D12_RESOURCE_DESC bufferDesc = {};
bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
bufferDesc.Width = vertexBufferSize;
bufferDesc.Height = 1;
bufferDesc.DepthOrArraySize = 1;
bufferDesc.MipLevels = 1;
bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
bufferDesc.SampleDesc.Count = 1;
bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

g_device->CreateCommittedResource(
    &uploadHeapProps,
    D3D12_HEAP_FLAG_NONE,
    &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&pUploadBuffer)
);
```

### Map/Memcpy/Unmap 写入数据

```cpp
// 获取CPU端指针
void* pData = nullptr;
pUploadBuffer->Map(0, nullptr, &pData);

// 拷贝顶点数据
memcpy(pData, triangleVertices, vertexBufferSize);

// 解除映射
pUploadBuffer->Unmap(0, nullptr);
```

`Map` 函数返回一个 CPU 可写的内存指针。对于 UPLOAD 堆，这个指针总是可用的——不需要等待 GPU。根据 [Microsoft Learn - ID3D12Resource::Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map) 的文档，UPLOAD 堆的资源始终处于可以被 CPU 访问的状态。

`Unmap` 调用是可选的——如果你频繁更新数据，可以保持 Map 状态而不 Unmap，GPU 仍然可以读取数据。但为了代码清晰，建议在不更新时 Unmap。

### 创建默认缓冲区并拷贝

```cpp
// 创建 DEFAULT 堆的目标缓冲区
ComPtr<ID3D12Resource> pVertexBuffer;

D3D12_HEAP_PROPERTIES defaultHeapProps = {};
defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

g_device->CreateCommittedResource(
    &defaultHeapProps,
    D3D12_HEAP_FLAG_NONE,
    &bufferDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,  // 初始状态：拷贝目标
    nullptr,
    IID_PPV_ARGS(&pVertexBuffer)
);
```

注意初始状态是 `D3D12_RESOURCE_STATE_COPY_DEST`——因为我们马上要从 UPLOAD 堆拷贝数据到这个缓冲区。

接下来在命令列表中录制拷贝命令：

```cpp
// 录制拷贝命令
pCommandList->CopyBufferRegion(
    pVertexBuffer.Get(), 0,       // 目标：偏移 0
    pUploadBuffer.Get(), 0,       // 源：偏移 0
    vertexBufferSize              // 大小
);

// 拷贝完成后，转换资源状态
CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    pVertexBuffer.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
);
pCommandList->ResourceBarrier(1, &barrier);
```

`CopyBufferRegion` 是一个 GPU 命令——它不是立即执行的，而是被录制到命令列表中，在提交给 GPU 后才执行。这意味着 CPU 端的 `CopyBufferRegion` 调用几乎是即时的，真正的数据传输发生在 GPU 端。

拷贝完成后，我们必须通过 `ResourceBarrier` 把资源状态从 `COPY_DEST` 转换为 `VERTEX_AND_CONSTANT_BUFFER`。这个状态转换告诉 GPU："这个缓冲区不再作为拷贝目标了，接下来要作为顶点缓冲使用"。GPU 在内部可能需要做一些缓存刷新或地址映射的调整，这些都在 Barrier 的掩护下自动完成。

### 设置顶点缓冲视图

资源状态转换完成后，我们就可以创建顶点缓冲视图（VBV）并在渲染时使用了：

```cpp
D3D12_VERTEX_BUFFER_VIEW vbv = {};
vbv.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
vbv.StrideInBytes = sizeof(Vertex);
vbv.SizeInBytes = vertexBufferSize;

// 渲染时
pCommandList->IASetVertexBuffers(0, 1, &vbv);
pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
pCommandList->DrawInstanced(3, 1, 0, 0);
```

## Resource Barrier 详解

资源屏障是 D3D12 中最重要的概念之一，也是初学者最容易忘记的地方。

### Transition Barrier——状态转换

最常见的屏障类型是 Transition Barrier，用于在资源的不同使用状态之间切换。一个缓冲区在某个时刻只能处于一种状态——你不能在它还是 `COPY_DEST` 的时候就把它当作 `VERTEX_BUFFER` 来用。

一些常见的状态转换场景包括：

- `COPY_DEST → VERTEX_AND_CONSTANT_BUFFER`：上传顶点/常量数据后
- `COPY_DEST → PIXEL_SHADER_RESOURCE`：上传纹理后
- `PRESENT → RENDER_TARGET`：开始渲染前
- `RENDER_TARGET → PRESENT`：渲染完成后
- `RENDER_TARGET → PIXEL_SHADER_RESOURCE`：后处理时把渲染结果作为输入

### UAV Barrier——无序访问同步

当你有两个连续的 Compute Shader dispatch 或像素着色器 UAV 写入操作，后一个需要等前一个完成时，使用 UAV Barrier。它不改变资源状态，只是告诉 GPU "等前一个 UAV 写入完成再开始下一个"。

### Aliasing Barrier——放置资源复用

当两个 `CreatePlacedResource` 创建的资源在同一堆的同一位置交替使用时，需要 Aliasing Barrier 来确保一个资源的写入对另一个资源可见。这种场景比较少见。

## ⚠️ 踩坑预警

**坑点一：忘记上传后转换资源状态**

如果你上传了顶点数据但忘了插入 `COPY_DEST → VERTEX_BUFFER` 的 Barrier，调试层会报一个错误，告诉你"资源处于错误的状态"。但如果你禁用了调试层，结果是不确定的——可能正常工作（因为某些 GPU 驱动比较宽容），可能在某些硬件上画面完全错误，也可能直接导致设备丢失。所以在上传数据后，**始终记得插入 Barrier**。

**坑点二：Upload Buffer 在命令执行期间被释放**

这是一个非常阴险的 bug。`CopyBufferRegion` 只是把拷贝命令录入了命令列表，真正的拷贝发生在 GPU 执行命令时。如果你在 `ExecuteCommandLists` 之前就把 Upload Buffer 的 COM 对象释放了（比如它是一个局部变量，函数返回后自动释放），GPU 执行时源数据已经不存在了——读取到的是无效数据。

解决方案是确保 Upload Buffer 的生命周期覆盖到 GPU 确认执行完毕之后。通常的做法是把 Upload Buffer 作为类成员存储，在下一帧开始时（通过 Fence 确认上一帧 GPU 已完成）才复用或释放它。

```cpp
// 正确做法：保留 Upload Buffer 直到 GPU 完成拷贝
struct FrameResource
{
    ComPtr<ID3D12Resource> uploadBuffer;  // 保留，不能提前释放
    // ... 其他帧资源
};
```

**坑点三：常量缓冲区的 256 字节对齐**

D3D12 的常量缓冲区视图（CBV）要求缓冲区大小必须是 256 字节的倍数。如果你的常量结构体只有 64 字节，你需要分配至少 256 字节的缓冲区空间。这不是建议，是硬性要求——违反会导致创建 CBV 时报错。

```cpp
// 正确的大小计算
constexpr UINT cbSize = (sizeof(MyConstants) + 255) & ~255;
```

**坑点四：堆类型和初始状态不匹配**

每种堆类型有允许的初始状态。UPLOAD 堆的初始状态只能是 `GENERIC_READ`，READBACK 堆的初始状态只能是 `COPY_DEST`。如果你给 UPLOAD 堆设了 `COPY_DEST` 的初始状态，创建会直接失败。

## 封装 GpuUploadBuffer 工具类

上传流程涉及的代码比较多，我们可以封装一个工具类来简化日常使用：

```cpp
class GpuUploadBuffer
{
public:
    GpuUploadBuffer() = default;
    ~GpuUploadBuffer() = default;

    // 上传数据到 DEFAULT 堆，返回目标资源
    ComPtr<ID3D12Resource> Upload(
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList* pCmdList,
        const void* pData,
        SIZE_T dataSize,
        D3D12_RESOURCE_STATES targetState)
    {
        // 创建上传缓冲区
        auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

        ComPtr<ID3D12Resource> pUpload;
        pDevice->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUpload)
        );

        // Map 并写入数据
        void* pMapped = nullptr;
        pUpload->Map(0, nullptr, &pMapped);
        memcpy(pMapped, pData, dataSize);
        pUpload->Unmap(0, nullptr);

        // 创建目标缓冲区
        auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        ComPtr<ID3D12Resource> pDest;
        pDevice->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pDest)
        );

        // 录制拷贝命令
        pCmdList->CopyBufferRegion(pDest.Get(), 0, pUpload.Get(), 0, dataSize);

        // 转换资源状态
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pDest.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            targetState
        );
        pCmdList->ResourceBarrier(1, &barrier);

        // 保留上传缓冲区（由调用者确保生命周期）
        m_uploadBuffers.push_back(pUpload);

        return pDest;
    }

    // 在确认 GPU 完成后调用，释放上传缓冲区
    void ReleaseUploadBuffers()
    {
        m_uploadBuffers.clear();
    }

private:
    std::vector<ComPtr<ID3D12Resource>> m_uploadBuffers;
};
```

⚠️ 这个类有一个重要的使用约束：`ReleaseUploadBuffers` 只能在确认 GPU 已经执行完所有拷贝命令后调用（通过 Fence 确认）。如果在 GPU 还没完成拷贝时就释放了上传缓冲区，GPU 读到的就是无效数据。在实际项目中，你通常会把上传缓冲区挂到帧资源上，每帧结束时等待 GPU 完成后再清理。

## 常见问题

### Q: 能不能不用 UPLOAD 堆，直接把数据写到 DEFAULT 堆？

不能。DEFAULT 堆对 CPU 不可见，你没有办法通过 `Map` 或指针来直接写入数据。数据必须通过 GPU 拷贝命令从 UPLOAD 堆（或另一个 DEFAULT 堆资源）传输过来。

### Q: 什么情况下用 CreateCommittedResource，什么情况下用 CreatePlacedResource？

简单原则：如果你只需要创建少量资源（比如几十个缓冲区和纹理），用 `CreateCommittedResource` 就够了。它的内存浪费可以忽略不计。如果你需要创建大量资源（比如一个纹理流式加载系统管理数百个纹理），用 `CreatePlacedResource` 把它们放在少数几个大堆中，减少堆数量和内存碎片。

### Q: 每帧都 Map/Unmap 常量缓冲区会不会影响性能？

对于 UPLOAD 堆中的常量缓冲区，`Map` 的开销几乎为零——它只是返回一个 CPU 端指针。真正的开销在于 CPU 和 GPU 之间的数据传输，但只要你的常量缓冲区在 UPLOAD 堆中，GPU 通过 PCIe 总线读取它的开销是固定的、不可避免的。所以放心 Map/Unmap，这不是性能瓶颈。

## 总结

这篇我们拆解了 D3D12 内存管理的核心内容。

三种堆类型各有分工：DEFAULT 堆是 GPU 的快速本地内存，存放纹理、渲染目标和静态几何数据；UPLOAD 堆是 CPU 可写的上传通道，用于将数据从 CPU 传输到 GPU；READBACK 堆是 GPU 写回 CPU 的回收通道，用于截图和计算结果回读。

三种资源创建方式复杂度递增：`CreateCommittedResource` 最简单但可能有内存浪费，`CreatePlacedResource` 更灵活但需要手动管理堆空间，`CreateReservedResource` 支持虚拟内存映射但使用场景较少。

我们实现了上传顶点数据的标准流程——CPU → Upload Buffer → CopyBufferRegion → Default Buffer → Barrier → 使用，并且封装了一个 `GpuUploadBuffer` 工具类来简化日常开发。踩坑方面，最危险的是 Upload Buffer 的生命周期问题——必须在 GPU 确认完成拷贝后才能释放。

下一篇，我们要把资源管理推向更深的一层——描述符堆和根签名。资源创建好之后，GPU 还需要知道"怎么找到它们"，这就是描述符机制要做的事情。

---

## 练习

1. 使用本文的标准上传流程，创建一个 D3D12 项目，上传一组顶点数据到 DEFAULT 堆，渲染一个彩色三角形。确保 Barrier 正确地从 `COPY_DEST` 转换到 `VERTEX_BUFFER`。

2. 故意去掉上传后的 `ResourceBarrier` 调用，在 Debug 模式下运行。观察调试层的输出信息，理解为什么状态转换是必须的。

3. 封装 `GpuUploadBuffer` 工具类，并添加对纹理上传的支持。提示：纹理上传需要用 `CopyTextureRegion` 而不是 `CopyBufferRegion`，而且需要设置 `D3D12_PLACED_SUBRESOURCE_FOOTPRINT` 来描述纹理在缓冲区中的布局。

4. 实验对比 `CreateCommittedResource` 和 `CreatePlacedResource`：创建一个大堆，然后在其中放置多个小缓冲区。计算在两种方案下 10 个 256 字节的常量缓冲区各自消耗的总内存量（考虑 64KB 的堆对齐），理解为什么大量小资源不应该用 Committed 方式创建。

---

**参考资料**:
- [Memory Management in D3D12 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management)
- [Heap Types - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/heap-types)
- [ID3D12Device::CreateCommittedResource - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommittedresource)
- [ID3D12Device::CreatePlacedResource - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource)
- [ID3D12Resource::Map - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map)
- [Using Resource Barriers - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states)
- [D3D12_RESOURCE_STATES enumeration - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states)
