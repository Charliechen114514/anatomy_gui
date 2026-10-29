# 通用GUI编程技术——图形渲染实战（二十八）——图像格式与编解码：PNG/JPEG全掌握

> 在上一篇文章中，我们用 GDI+ 的矩阵变换实现了一个仪表盘——通过 `Graphics::SetTransform` 做旋转和缩放，用 `Matrix` 类管理变换的叠加顺序。如果你跟着做了练习，应该已经体会到了 GDI+ 在坐标变换方面相比纯 GDI 的巨大优势——不再需要手动计算每个旋转后的坐标点，一个矩阵乘法搞定一切。
>
> 今天我们要聊的是 GDI+ 的另一大卖点：图像格式的原生支持。还记得在 GDI 时代加载一张 PNG 图片有多痛苦吗？你得找第三方库（libpng），手动解析文件头，处理交错扫描，最后还得操心内存对齐。GDI+ 把这些全部内化了——BMP、PNG、JPEG、GIF、TIFF、WMF/EMF，一行 `Image::FromFile` 搞定。但事情没有表面看起来那么简单，今天我们就来拆解 GDI+ 的图像编解码体系，看看它到底能做什么，以及一个会坑得你怀疑人生的文件锁定问题。

## 环境说明

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x 工具集)
- **目标平台**: Win32 原生桌面应用
- **图形库**: GDI+（链接 `gdiplus.lib`，头文件 `<gdiplus.h>`）
- **字符集**: Unicode

## GDI+ 内置的图像格式支持

GDI+ 通过内置的编解码器（Codec）支持以下图像格式：

BMP 是 Windows 的原生位图格式，无压缩，加载最快但文件体积大。PNG 是无损压缩格式，支持 Alpha 透明通道，是现代 GUI 开发中最常用的图片格式。JPEG 是有损压缩格式，适合照片类图片，不支持透明通道。GIF 支持动画和索引色透明，但颜色上限只有 256 色。TIFF 是专业影像领域的格式，支持多页和多种压缩方式。WMF/EMF 是 Windows 元文件格式，存储的是 GDI 绘制命令而非像素数据。

这些编解码器不需要你手动注册或加载，`GdiplusStartup` 初始化时会自动注册系统内所有可用的图像编解码器。你可以通过 `GetImageDecoders` 和 `GetImageEncoders` 函数来查询当前系统支持哪些格式。

### 查询系统支持的编解码器

我们来看一个实用的工具函数——遍历并打印当前系统支持的所有图像编解码器：

```cpp
void ListImageEncoders()
{
    UINT num = 0;      // 编码器数量
    UINT size = 0;     // 编码器信息数组总大小

    // 第一次调用：获取数量和大小
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        OutputDebugString(L"没有找到任何图像编码器\n");
        return;
    }

    // 分配内存
    Gdiplus::ImageCodecInfo* pCodecInfo =
        (Gdiplus::ImageCodecInfo*)malloc(size);
    if (pCodecInfo == NULL) return;

    // 第二次调用：获取实际的编码器信息
    Gdiplus::GetImageEncoders(num, size, pCodecInfo);

    // 遍历打印
    for (UINT i = 0; i < num; i++) {
        WCHAR msg[512];
        wsprintf(msg, L"编码器 %d: %s (扩展名: %s, MIME: %s)\n",
            i,
            pCodecInfo[i].FormatDescription,
            pCodecInfo[i].FilenameExtension,
            pCodecInfo[i].MimeType);
        OutputDebugString(msg);
    }

    free(pCodecInfo);
}
```

这里有一个很容易忽略的细节：`GetImageEncodersSize` 和 `GetImageEncoders` 是两次调用模式。第一次调用获取需要的缓冲区大小，分配好内存之后再做第二次调用。这个模式和很多 Windows API（比如 `EnumFonts`）的处理方式类似，GDI+ 里也沿用了这个传统。

## 加载图片：FromFile vs FromStream

GDI+ 提供了两种加载图片的方式——从文件加载和从流加载。两者看似等价，但在实际使用中有一个致命的区别。

### Image::FromFile：最简单的加载方式

```cpp
// 从文件加载图片——一行代码搞定
Gdiplus::Image* pImage = Gdiplus::Image::FromFile(L"photo.png");
if (pImage == NULL || pImage->GetLastStatus() != Gdiplus::Ok) {
    // 加载失败处理
    delete pImage;
    return;
}

// 获取图片尺寸
UINT width = pImage->GetWidth();
UINT height = pImage->GetHeight();

// 在 WM_PAINT 中绘制
Gdiplus::Graphics graphics(hdc);
graphics.DrawImage(pImage, 0, 0, width, height);

// 清理
delete pImage;
```

就这么简单。不管你的文件是 PNG、JPEG 还是 GIF，`Image::FromFile` 都能自动识别格式并解码。GDI+ 通过文件扩展名和文件头魔数（Magic Number）双重判断来识别格式。

⚠️ 注意，`Image::FromFile` 有一个非常烦人的行为：**它会锁定文件**。也就是说，在 `Image` 对象被 `delete` 之前，你无法删除、重命名或覆盖这个文件。如果你的程序需要频繁更新图片文件（比如从网络下载新图片覆盖旧图片），你会发现自己怎么也删不掉那个文件，而且 `DeleteFile` 返回的是 `ERROR_ACCESS_DENIED`。

这个问题的根源在于 GDI+ 内部使用了内存映射文件（Memory-Mapped File）来加载图片数据，而内存映射文件会持有文件的句柄直到对象销毁。这不是 Bug，是设计如此——但如果你不知道这个机制，排查起来会非常痛苦。

### Image::FromStream：解决文件锁定问题

要绕过文件锁定，我们需要先把文件内容完整读入内存，然后用流的方式加载：

```cpp
Gdiplus::Image* LoadImageWithoutLocking(const WCHAR* filePath)
{
    // 第一步：读取文件全部内容到内存
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    DWORD fileSize = GetFileSize(hFile, NULL);
    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, fileSize);
    if (hBuffer == NULL) {
        CloseHandle(hFile);
        return NULL;
    }

    void* pBuffer = GlobalLock(hBuffer);
    DWORD bytesRead;
    ReadFile(hFile, pBuffer, fileSize, &bytesRead, NULL);
    GlobalUnlock(hBuffer);
    CloseHandle(hFile);  // 读完后立刻关闭文件句柄

    // 第二步：创建 IStream
    IStream* pStream = NULL;
    HRESULT hr = CreateStreamOnHGlobal(hBuffer, TRUE, &pStream);
    if (FAILED(hr)) {
        GlobalFree(hBuffer);
        return NULL;
    }

    // 第三步：从流加载图片
    Gdiplus::Image* pImage = Gdiplus::Image::FromStream(pStream);

    // 释放流（注意：GDI+ 会自己持有数据的副本）
    pStream->Release();

    return pImage;
}
```

这个函数做了三件事：先用 Win32 API 把文件完整读入内存并关闭文件句柄，然后创建一个基于内存的 `IStream`，最后用 `Image::FromStream` 从这个流加载图片。由于文件句柄在第一步结束时就已经关闭了，所以图片文件不会被锁定。

⚠️ 注意，`CreateStreamOnHGlobal` 的第二个参数 `TRUE` 表示当 `IStream` 被释放时自动调用 `GlobalFree` 释放内存。但这里有一个微妙的时序问题：`Image::FromStream` 在创建 `Image` 对象时可能只读取了流的部分数据（比如只读了头部信息），实际的像素解码可能在第一次 `DrawImage` 时才进行。所以**在 `Image` 对象存活期间，绝对不能释放底层缓冲区**。上面的代码中 `pStream->Release()` 在 GDI+ 已经完成数据拷贝之后调用，是安全的。如果你的场景更复杂，建议保留 `IStream` 直到 `Image` 对象销毁。

## 保存图片：编码器 CLSID 的获取

加载图片是简单的，但保存图片就没那么直接了。GDI+ 的 `Image::Save` 方法需要一个编码器的 CLSID（全局唯一标识符）来指定输出格式。问题是，这个 CLSID 不是硬编码在头文件里的常量——你需要通过 `GetImageEncoders` 遍历查找。

### 获取指定格式的编码器 CLSID

```cpp
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;  // 失败

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    // 遍历查找匹配的 MIME 类型
    for (UINT j = 0; j < num; j++) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // 返回找到的索引
        }
    }

    free(pImageCodecInfo);
    return -1;  // 没找到
}
```

`format` 参数是 MIME 类型字符串，比如 `L"image/png"`、`L"image/jpeg"`、`L"image/bmp"` 等。调用方式如下：

```cpp
CLSID pngClsid;
if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
    pImage->Save(L"output.png", &pngClsid, NULL);
}
```

这个 `GetEncoderClsid` 函数是 GDI+ 文档中的经典示例代码，几乎所有 GDI+ 教程都会收录它。建议你把它放到项目的工具函数库中，因为你会反复用到它。

### 保存为 PNG

```cpp
void SaveToPng(Gdiplus::Bitmap* pBitmap, const WCHAR* filePath)
{
    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) < 0) {
        OutputDebugString(L"找不到 PNG 编码器\n");
        return;
    }

    Gdiplus::Status status = pBitmap->Save(filePath, &clsid, NULL);
    if (status != Gdiplus::Ok) {
        WCHAR msg[256];
        wsprintf(msg, L"保存失败，错误码：%d\n", status);
        OutputDebugString(msg);
    }
}
```

PNG 是无损压缩，不需要额外的质量参数，所以 `Save` 的第三个参数传 `NULL`。

### 保存为 JPEG 并控制质量

JPEG 格式支持质量参数（0-100），这是一个通过 `EncoderParameters` 传递的可选配置：

```cpp
void SaveToJpeg(Gdiplus::Bitmap* pBitmap, const WCHAR* filePath, long quality)
{
    CLSID clsid;
    if (GetEncoderClsid(L"image/jpeg", &clsid) < 0) {
        OutputDebugString(L"找不到 JPEG 编码器\n");
        return;
    }

    // 设置 JPEG 质量参数
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    Gdiplus::Status status = pBitmap->Save(filePath, &clsid, &encoderParams);
    if (status != Gdiplus::Ok) {
        OutputDebugString(L"JPEG 保存失败\n");
    }
}

// 使用示例：质量设为 85（推荐值，文件大小和质量的良好平衡点）
SaveToJpeg(pBitmap, L"output.jpg", 85);
```

`EncoderParameters` 结构体可以包含多个参数，但 JPEG 编码最常用的就是 `EncoderQuality` 这一个。质量值 100 表示最高质量（几乎无压缩），0 表示最低质量（极度压缩，画面糊成色块）。实际开发中 80-90 之间的值是比较实用的选择。

## 实战：图片格式批量转换工具

我们把前面的知识串起来，实现一个实用的格式转换功能——将任意支持的图片格式转换为 PNG：

```cpp
bool ConvertToPng(const WCHAR* srcPath, const WCHAR* dstPath)
{
    // 使用 FromStream 加载，避免锁定源文件
    Gdiplus::Image* pImage = LoadImageWithoutLocking(srcPath);
    if (pImage == NULL || pImage->GetLastStatus() != Gdiplus::Ok) {
        delete pImage;
        return false;
    }

    // 获取 PNG 编码器 CLSID
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) {
        delete pImage;
        return false;
    }

    // 保存为 PNG
    Gdiplus::Status status = pImage->Save(dstPath, &pngClsid, NULL);
    delete pImage;

    return (status == Gdiplus::Ok);
}
```

这个函数接受源文件路径和目标文件路径，自动处理格式转换。由于我们使用了 `LoadImageWithoutLocking` 而非 `Image::FromFile`，即使源文件正在被其他进程使用，也不会出问题。

## 在 WM_PAINT 中显示图片的完整模板

最后，我们来看一个在 Win32 窗口中显示图片的完整代码模板。这个模板包含了 GDI+ 初始化、图片加载、绘制和清理的全过程：

```cpp
#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

ULONG_PTR g_gdiplusToken = NULL;
Image* g_pImage = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        // 加载图片（使用 FromStream 避免文件锁定）
        g_pImage = Image::FromFile(L"test.png");
        if (g_pImage && g_pImage->GetLastStatus() != Ok) {
            delete g_pImage;
            g_pImage = NULL;
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_pImage) {
            Graphics graphics(hdc);
            graphics.DrawImage(g_pImage, 0, 0,
                g_pImage->GetWidth(), g_pImage->GetHeight());
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        delete g_pImage;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    GdiplusStartupInput si;
    GdiplusStartup(&g_gdiplusToken, &si, NULL);

    const wchar_t cls[] = L"ImageViewer";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(cls, L"GDI+ 图片查看器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
```

这个模板看起来很简洁，但有几个值得注意的地方。图片在 `WM_CREATE` 时加载一次，之后每次 `WM_PAINT` 只需要调用 `DrawImage` 绘制即可。`Image` 对象在 `WM_DESTROY` 时释放。如果你需要频繁更换显示的图片，记得在加载新图片前先 `delete` 旧的 `Image` 对象。

## 常见问题与调试

### 问题1：GetImageEncodersSize 返回 0

如果你发现 `GetImageEncodersSize` 返回的 `size` 为 0，最可能的原因是你忘记调用 `GdiplusStartup`。GDI+ 的编解码器信息在初始化时才会被加载到内存中，没有初始化就查询，结果当然是空的。

### 问题2：Save 返回 Win32Error 或 GenericError

保存图片失败时，检查以下几点：目标目录是否存在？程序是否有写入权限？磁盘空间是否充足？如果目标是网络路径（UNC 路径），确保网络连接正常。另外，`Image::FromFile` 加载的图片如果底层流已经被释放，保存时也可能失败。

### 问题3：PNG 透明通道丢失

如果你发现 PNG 图片的透明区域变成了黑色，检查你绘制时是否使用了正确的 `DrawImage` 重载。`Graphics::DrawImage` 有 30 多个重载版本，部分重载会忽略 Alpha 通道。确保你的 `Graphics` 对象没有设置 `CompositingModeSourceCopy`——这个合成模式会直接覆盖目标像素，包括 Alpha 通道。

## 总结

到这里，GDI+ 的图像编解码体系我们就讲清楚了。从 `Image::FromFile` 的一行加载，到 `GetImageEncoders` 的 CLSID 查找，再到 JPEG 质量参数的精细控制——GDI+ 在图像处理方面确实比纯 GDI 方便了一个数量级。

但别忘了那个文件锁定的坑。`FromFile` 看起来简单，实则会锁住文件直到 `Image` 对象销毁。如果你的程序需要读写同一张图片，`FromStream` 才是正确的选择。

下一步，我们要正式进入 GPU 加速渲染的世界了。前面所有 GDI 和 GDI+ 的内容，本质上都是 CPU 绘制——每一条线、每一个像素都由 CPU 逐个计算。从下一篇文章开始，我们要认识 Direct2D——微软提供的 GPU 加速 2D 渲染 API。你会发现，从 GDI+ 到 Direct2D 的跨度，比从 GDI 到 GDI+ 要大得多，因为整个渲染架构从 CPU 转移到了 GPU。

---

## 练习

1. 实现一个批量图片格式转换工具：遍历指定目录下所有 BMP 文件，统一转换为 PNG 格式保存到另一个目录。
2. 实现一个 JPEG 质量对比工具：同一张图片分别以质量 10、50、85、100 保存，然后在窗口中并排显示，直观对比质量差异。
3. 用 `FromStream` 方式实现一个图片缓存管理器：维护一个 `std::map<std::wstring, Image*>` 缓存已加载的图片，支持按路径查找和 LRU 淘汰。
4. 实现截屏保存为 PNG 的功能：用 `BitBlt` 将屏幕内容拷贝到 `Bitmap` 对象，然后保存为文件。

---

**参考资料**:
- [Image::FromFile method - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusheaders/nf-gdiplusheaders-image-fromfile)
- [Image::Save method - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusheaders/nf-gdiplusheaders-image-save)
- [GetImageEncoders function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusimagecodec/nf-gdiplusimagecodec-getimageencoders)
- [EncoderParameter structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusimaging/nl-gdiplusimaging-encoderparameter)
- [ImageCodecInfo structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusimaging/ns-gdiplusimaging-imagecodecinfo)
- [CreateStreamOnHGlobal function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-createstreamonhglobal)
