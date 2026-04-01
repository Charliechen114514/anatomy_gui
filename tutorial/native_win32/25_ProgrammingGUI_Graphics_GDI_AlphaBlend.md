# 通用GUI编程技术——图形渲染实战（二十五）——Alpha混合与透明效果：分层窗口实战

> 上一篇文章我们用 GDI Region 实现了圆角窗口和星形异形窗口，通过 `SetWindowRgn` 让窗口呈现出不规则的形状。但 Region 有一个根本性的局限——它只支持"非有即无"的硬边界裁切。窗口的每个像素要么完全可见，要么完全不可见，没有中间状态。如果你想要一个边缘柔和的圆角、一个半透明的悬浮面板、或者一个带有渐变阴影的弹窗，Region 就无能为力了。这正是 Alpha 混合技术出场的时候——它允许我们控制每个像素的透明度，实现真正意义上的"半透明"效果。

今天我们要深入探讨 Win32 平台上的 Alpha 混合技术。这涉及三个层面的内容：`AlphaBlend` 函数的逐像素混合原理，`TransparentBlt` 的颜色键透明，以及最重要的——`WS_EX_LAYERED` 分层窗口配合 `UpdateLayeredWindow` 实现真正的逐像素透明窗口。最后我们会实现一个半透明悬浮球，整合 Region 和 Alpha 混合两种技术。

## 环境说明

这次的开发环境和之前略有不同，因为我们需要链接额外的库：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022)
- **目标平台**: Win32 API 原生开发
- **图形库**: GDI (Graphics Device Interface)
- **链接库**: 除了常规的 `gdi32.lib`、`user32.lib`，还需要 `msimg32.lib`

⚠️ `msimg32.lib` 这个库是 `AlphaBlend` 和 `TransparentBlt` 函数所在的库。如果你在链接时看到类似 `unresolved external symbol __imp__AlphaBlend@44` 的错误，那就是忘记链接这个库了。在 Visual Studio 中可以通过项目属性 → 链接器 → 输入 → 附加依赖项中添加 `msimg32.lib`，或者在代码中用 `#pragma comment(lib, "msimg32.lib")` 来自动链接。

## Alpha 混合的数学原理

在写代码之前，我们得先搞清楚 Alpha 混合到底在数学上做了什么。这不是什么高深的数学，但理解它对后面正确使用 API 至关重要。

### Alpha 通道的含义

在 32 位颜色中，每个像素由四个分量组成：R（红）、G（绿）、B（蓝）、A（Alpha）。Alpha 通道表示这个像素的"不透明度"——0 表示完全透明（不可见），255 表示完全不透明，中间值表示半透明。Alpha 不是"透明度"而是"不透明度"，这个方向性很重要。

### 混合公式

当我们把一个带 Alpha 的源像素画到目标上时，最终颜色的计算公式是：

```
Result = Source * SourceAlpha / 255 + Destination * (255 - SourceAlpha) / 255
```

用更简洁的写法：

```
Result = Source × α + Destination × (1 - α)
```

其中 α 是源像素的 Alpha 值归一化到 [0, 1] 区间。这个公式叫"Over"混合（Porter-Duff Over 操作），是最常用的 Alpha 混合模式。

举个具体的例子：如果源像素是红色 (255, 0, 0)，Alpha 是 128（大约 50% 透明），目标是白色 (255, 255, 255)，那混合后的结果就是：

```
R = 255 × (128/255) + 255 × (1 - 128/255) = 128 + 127 = 255
G = 0   × (128/255) + 255 × (1 - 128/255) = 0   + 127 = 127
B = 0   × (128/255) + 255 × (1 - 128/255) = 0   + 127 = 127
```

结果是一种浅粉红色——红色和白色各占一半。

### SourceConstantAlpha vs 像素 Alpha

这里有一个很重要的概念区分。GDI 的 `AlphaBlend` 函数支持两种透明度控制方式。

一种是"全局 Alpha"（SourceConstantAlpha），它对整个源图像施加统一的透明度。比如你设 SourceConstantAlpha 为 128，那整个图像的每个像素都会变成 50% 透明。

另一种是"逐像素 Alpha"（Per-pixel Alpha），它使用源图像每个像素自带的 Alpha 通道值。这样图像的不同区域可以有不同的透明度——比如中心完全不透明，边缘逐渐变透明。

这两种方式还可以叠加——最终的有效 Alpha = SourceConstantAlpha × 像素 Alpha / 255。这个叠加关系是很多人踩坑的地方，我们后面在讲 `UpdateLayeredWindow` 时会详细展开。

## AlphaBlend：逐像素混合函数

现在我们来看具体的 API。`AlphaBlend` 是 GDI 中执行 Alpha 混合的核心函数，它的声明如下：

```cpp
BOOL AlphaBlend(
    HDC           hdcDest,       // 目标 DC
    int           xoriginDest,   // 目标 x 起点
    int           yoriginDest,   // 目标 y 起点
    int           wDest,         // 目标宽度
    int           hDest,         // 目标高度
    HDC           hdcSrc,        // 源 DC
    int           xoriginSrc,    // 源 x 起点
    int           yoriginSrc,    // 源 y 起点
    int           wSrc,          // 源宽度
    int           hSrc,          // 源高度
    BLENDFUNCTION ftn            // 混合函数参数
);
```

参数和 `BitBlt` 非常类似，唯一的区别是最后一个参数——`BLENDFUNCTION` 结构体。这个结构体定义了混合的方式：

```cpp
typedef struct _BLENDFUNCTION {
    BYTE BlendOp;             // 混合操作，目前只有 AC_SRC_OVER (0x00)
    BYTE BlendFlags;          // 必须为 0
    BYTE SourceConstantAlpha; // 全局 Alpha 值 (0-255)
    BYTE AlphaFormat;         // Alpha 格式标志
} BLENDFUNCTION;
```

`BlendOp` 目前唯一有效的值是 `AC_SRC_OVER`，表示把源图像"叠"在目标上面。`SourceConstantAlpha` 是全局 Alpha 值，设为 255 时表示完全依赖像素自身的 Alpha 通道。`AlphaFormat` 可以设为 0（只使用 SourceConstantAlpha）或 `AC_SRC_ALPHA`（使用像素 Alpha 通道）。

### 一个简单的 AlphaBlend 演示

下面我们写一个最简单的演示，用 `AlphaBlend` 把一个半透明的红色方块画到白色背景上：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = rc.right;
    int cy = rc.bottom;

    // 双缓冲
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, cx, cy);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    // 白色背景
    HBRUSH hbrWhite = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdcMem, &rc, hbrWhite);
    DeleteObject(hbrWhite);

    // 创建一个 32 位 DIB Section 来承载带 Alpha 的图像
    HDC hdcSrc = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = 200;
    bmi.bmiHeader.biHeight      = 200;  // 正值 = 底朝上
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbmSrc = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    if (hbmSrc != NULL && pBits != NULL) {
        HBITMAP hbmSrcOld = (HBITMAP)SelectObject(hdcSrc, hbmSrc);

        // 填充半透明红色，每个像素：BGRA 格式
        DWORD* pixels = (DWORD*)pBits;
        for (int i = 0; i < 200 * 200; i++) {
            pixels[i] = 0x800000FF;  // Alpha=128, R=255, G=0, B=0
            // 注意内存中是 BGRA 顺序：0x80(B) 00(G) 00(B) FF(R) → 不对
            // 实际上 DWORD 在小端序中：字节顺序是 B G R A
            // 0x80(A) 00(R) 00(G) FF(B) → 也不对
            // 正确：COLORREF 是 0x00BBGGRR，但 DIB 中是 BGRA 顺序
        }

        // 重新来，正确设置 BGRA
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 200; x++) {
                BYTE* pPixel = (BYTE*)pBits + (y * 200 + x) * 4;
                pPixel[0] = 0;      // Blue
                pPixel[1] = 0;      // Green
                pPixel[2] = 255;    // Red
                pPixel[3] = 128;    // Alpha = 50% 透明
            }
        }

        // 使用 AlphaBlend 绘制
        BLENDFUNCTION bf = {};
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;  // 不额外添加全局 Alpha
        bf.AlphaFormat         = AC_SRC_ALPHA;  // 使用像素 Alpha

        AlphaBlend(hdcMem, 50, 50, 200, 200,
                   hdcSrc, 0, 0, 200, 200, bf);

        SelectObject(hdcSrc, hbmSrcOld);
        DeleteObject(hbmSrc);
    }

    DeleteDC(hdcSrc);

    // 拷贝到屏幕
    BitBlt(hdc, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);

    EndPaint(hwnd, &ps);
    return 0;
}
```

⚠️ 这段代码中有一个很容易踩的坑——DIB Section 中像素的字节顺序。DIB（Device Independent Bitmap）中的像素字节顺序是 **B G R A**（蓝、绿、红、Alpha），而不是很多开发者直觉以为的 R G B A。这一点在 MSDN 中有明确说明，但仍然有大量开发者在这个问题上栽跟头。上面的代码用逐字节赋值的方式避免了这个问题。

注意我们在 `BLENDFUNCTION` 中设了 `SourceConstantAlpha = 255` 和 `AlphaFormat = AC_SRC_ALPHA`。这意味着我们完全依赖像素自带的 Alpha 通道来控制透明度。如果你把 `SourceConstantAlpha` 设成 128，那最终的有效 Alpha 会变成 `128 * 128 / 255 ≈ 64`，大约只有 25% 的不透明度——这就是前面提到的叠加效应。

## TransparentBlt：颜色键透明

除了 AlphaBlend，GDI 还提供了另一种实现透明效果的方式——颜色键透明（Color Key Transparency）。`TransparentBlt` 的原理很简单：你指定一个颜色作为"透明色"，源图像中所有等于这个颜色的像素都不会被画到目标上。

```cpp
BOOL TransparentBlt(
    HDC  hdcDest,        // 目标 DC
    int  xoriginDest,    // 目标 x 起点
    int  yoriginDest,    // 目标 y 起点
    int  wDest,          // 目标宽度
    int  hDest,          // 目标高度
    HDC  hdcSrc,         // 源 DC
    int  xoriginSrc,     // 源 x 起点
    int  yoriginSrc,     // 源 y 起点
    int  wSrc,           // 源宽度
    int  hSrc,           // 源高度
    UINT crTransparent   // 透明色 (COLORREF)
);
```

使用起来非常直观：

```cpp
// 把源图像中所有洋红色 (RGB(255,0,255)) 的像素都变透明
TransparentBlt(hdcDest, 100, 100, 200, 200,
               hdcSrc, 0, 0, 200, 200,
               RGB(255, 0, 255));
```

`TransparentBlt` 的优势是不需要 32 位位图——普通的 24 位位图就能用。它的缺点是只有"全透明"和"全不透明"两种状态，不支持半透明。另外，选择透明色也是个技术活——你必须确保源图像中没有"不应该透明"的像素恰好等于透明色。洋红色 (255, 0, 255) 是一个常见的透明色选择，因为它在自然图像中几乎不会出现。

`TransparentBlt` 同样需要 `msimg32.lib`。

## 分层窗口：WS_EX_LAYERED

前面讲的 `AlphaBlend` 和 `TransparentBlt` 都是在已有窗口内进行混合绘制。但如果我们要让窗口本身变成半透明的——比如一个半透明的悬浮面板——就需要用到分层窗口了。

### 什么是分层窗口

分层窗口（Layered Window）是 Windows 2000 引入的一种特殊窗口类型。普通窗口通过 `WM_PAINT` 消息来绘制内容，系统会把窗口内容合成到桌面上。而分层窗口则跳过了 `WM_PAINT` 这一步——应用程序直接提供一个位图给系统，系统负责把这个位图以指定的透明度合成到桌面上。

创建分层窗口只需要在 `CreateWindowEx` 的第一个参数（`dwExStyle`）中加上 `WS_EX_LAYERED` 标志：

```cpp
HWND hwnd = CreateWindowEx(
    WS_EX_LAYERED | WS_EX_TOPMOST,  // 分层窗口 + 置顶
    CLASS_NAME,
    L"Layered Window",
    WS_POPUP,
    100, 100, 300, 300,
    NULL, NULL, hInstance, NULL
);
```

也可以在创建后通过 `SetWindowLong` 添加这个标志：

```cpp
SetWindowLong(hwnd, GWL_EXSTYLE,
              GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
```

从 Windows 8 开始，`WS_EX_LAYERED` 不仅支持顶层窗口，还支持子窗口。在之前的 Windows 版本中，只有顶层窗口才能是分层窗口。

### 两种更新方式

分层窗口有两种更新方式，适用于不同的场景。

第一种是 `SetLayeredWindowAttributes`，它比较简单，可以对整个窗口施加统一的透明度或颜色键透明。适合那些只需要"整体半透明"的场景，比如一个半透明的通知面板。

第二种是 `UpdateLayeredWindow`，它更加强大，可以提供带有逐像素 Alpha 的位图，实现真正的像素级透明控制。适合那些需要复杂透明效果的场景，比如圆角带阴影的悬浮球、不规则形状的半透明控件等。

⚠️ 这两种方式是互斥的——一旦你对一个分层窗口调用了 `SetLayeredWindowAttributes`，后续的 `UpdateLayeredWindow` 调用就会失败，直到你清除并重新设置 `WS_EX_LAYERED` 标志。反过来也一样。根据 [Microsoft Learn 的官方文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes)，这一点经常被忽略。

### SetLayeredWindowAttributes：整窗口透明度

我们先看简单的方式。`SetLayeredWindowAttributes` 的函数签名如下：

```cpp
BOOL SetLayeredWindowAttributes(
    HWND     hwnd,     // 分层窗口句柄
    COLORREF crKey,    // 颜色键
    BYTE     bAlpha,   // 整体 Alpha 值 (0-255)
    DWORD    dwFlags   // 标志
);
```

`dwFlags` 可以是 `LWA_ALPHA`（使用 `bAlpha` 作为整体透明度）、`LWA_COLORKEY`（使用 `crKey` 作为透明颜色键），或者两者的组合。

最简单的用法——让整个窗口变成 50% 透明：

```cpp
SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA);
```

用颜色键让某种颜色变透明：

```cpp
// 窗口中所有白色的像素都变透明
SetLayeredWindowAttributes(hwnd, RGB(255, 255, 255), 0, LWA_COLORKEY);
```

这两种方式还可以组合使用，但实际开发中很少这样做。

`SetLayeredWindowAttributes` 最大的优点是简单——你不需要改变窗口的绘制方式，正常处理 `WM_PAINT` 就行，系统会在最终合成时自动应用透明度。但缺点也很明显：要么整体半透明，要么颜色键透明，无法做到不同区域有不同的透明度。

## UpdateLayeredWindow：逐像素透明的核心

现在我们来啃硬骨头。`UpdateLayeredWindow` 是实现高级透明效果的核心函数，但它的使用方式和我们之前习惯的 `WM_PAINT` 完全不同。

### 函数签名

```cpp
BOOL UpdateLayeredWindow(
    HWND           hWnd,     // 分层窗口句柄
    HDC            hdcDst,   // 屏幕 DC（通常传 NULL）
    POINT*         pptDst,   // 新的窗口位置（可传 NULL）
    SIZE*          psize,    // 窗口大小
    HDC            hdcSrc,   // 源 DC（包含要显示的内容）
    POINT*         pptSrc,   // 源位置（通常传 (0,0)）
    COLORREF       crKey,    // 颜色键
    BLENDFUNCTION* pblend,   // 混合参数
    DWORD          dwFlags   // 标志
);
```

参数看起来很多，但大部分可以传 NULL 或默认值。关键的参数是 `hdcSrc`（你要显示的图像）、`psize`（窗口大小）和 `pblend`（混合参数）。

`dwFlags` 可以是 `ULW_ALPHA`（使用 `pblend` 混合）、`ULW_COLORKEY`（使用 `crKey` 颜色键）、`ULW_OPAQUE`（不透明）。

### 分层窗口不接收 WM_PAINT

这是 `UpdateLayeredWindow` 最重要的特性——使用它的分层窗口不会收到 `WM_PAINT` 消息。因为窗口的内容完全由你通过 `UpdateLayeredWindow` 提供，系统不需要通过 `WM_PAINT` 来触发绘制。

这意味着你不能在 `WM_PAINT` 里用 `BeginPaint` / `EndPaint` 来更新分层窗口——那些代码根本不会被执行。你需要自己决定何时更新窗口内容，然后主动调用 `UpdateLayeredWindow`。

这种模式有点像游戏中的"立即模式"渲染——不是系统通知你"该画了"，而是你主动说"我要更新画面了"。

### 分层窗口绘制模板

下面我们给出一个通用的分层窗口绘制模板。这个模板的核心流程是：创建 32 位 DIB Section → 在内存 DC 上用 GDI 绘制 → `UpdateLayeredWindow` 提交给系统。

```cpp
// 分层窗口绘制模板核心函数
void UpdateLayeredWindowContent(HWND hwnd, int width, int height)
{
    HDC hdcScreen = GetDC(NULL);  // 获取屏幕 DC

    // 创建源 DC
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);

    // 创建 32 位 DIB Section
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;  // 负值 = 顶朝下，更符合直觉
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;       // 32 位，包含 Alpha 通道
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbmSrc = CreateDIBSection(
        hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    if (hbmSrc && pBits) {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcSrc, hbmSrc);

        // ===== 在这里进行 GDI 绘制 =====
        // 清零整个位图（全透明黑色）
        memset(pBits, 0, width * height * 4);

        // 示例：绘制一个半透明的蓝色圆形
        // 先用 GDI 画图（不处理 Alpha 通道）
        HBRUSH hbrCircle = CreateSolidBrush(RGB(0, 120, 255));
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 80, 200));
        HGDIOBJ hbrOld = SelectObject(hdcSrc, hbrCircle);
        HGDIOBJ hpenOld = SelectObject(hdcSrc, hPen);
        Ellipse(hdcSrc, 10, 10, width - 10, height - 10);
        SelectObject(hdcSrc, hbrOld);
        SelectObject(hdcSrc, hpenOld);
        DeleteObject(hbrCircle);
        DeleteObject(hPen);

        // GDI 绘制后，需要手动设置 Alpha 通道
        // 因为 GDI 函数不会设置 Alpha 通道（始终为 0）
        DWORD* pixels = (DWORD*)pBits;
        for (int i = 0; i < width * height; i++) {
            BYTE* p = (BYTE*)&pixels[i];
            if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
                p[3] = 220;  // 非黑色像素设为 220 的 Alpha（略带透明）
            }
        }
        // =================================

        // 配置混合参数
        BLENDFUNCTION bf = {};
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat         = AC_SRC_ALPHA;

        SIZE size = { width, height };
        POINT ptSrc = { 0, 0 };

        // 提交给系统
        UpdateLayeredWindow(hwnd, NULL, NULL, &size,
                           hdcSrc, &ptSrc, 0, &bf, ULW_ALPHA);

        SelectObject(hdcSrc, hbmOld);
        DeleteObject(hbmSrc);
    }

    DeleteDC(hdcSrc);
    ReleaseDC(NULL, hdcScreen);
}
```

⚠️ 这段模板代码中有几个非常关键的细节需要逐一说明。

### DIB Section 的 biHeight 为什么用负值

`BITMAPINFOHEADER` 的 `biHeight` 字段决定了位图的扫描线方向。正值表示位图数据从底部开始存储（底朝上，bottom-up），负值表示从顶部开始存储（顶朝下，top-down）。对于 DIB Section 用作绘制表面时，顶朝下更符合我们的直觉——Y 坐标向下增长，和 GDI 的坐标系统一致。如果使用正值（底朝上），你会发现 GDI 绘制的内容上下翻转。

### GDI 不写 Alpha 通道的问题

这是一个巨大的坑。当你用 GDI 函数（如 `Ellipse`、`Rectangle`、`TextOut` 等）在 32 位 DIB Section 上绘制时，GDI 只会写 RGB 三个通道，**Alpha 通道始终保持为 0**。这意味着如果你直接拿这个 DIB Section 去做 Alpha 混合，结果就是——什么都看不见，因为 Alpha 全是 0（完全透明）。

解决方法有两种。第一种是像模板代码中那样，GDI 绘制完成后手动遍历像素，给非空像素设置 Alpha 值。第二种是在绘制之前先把所有像素的 Alpha 都设为 255（完全不透明），然后在需要透明的地方把 Alpha 减小。

第一种方法的问题是需要遍历所有像素来判断"哪些像素被画了"，对于复杂绘制可能不太好判断。但优点是你可以实现不同的 Alpha 值来创造渐变透明效果。

### SourceConstantAlpha 与像素 Alpha 的叠加

前面我们提到了这两者的叠加关系。当你在 `BLENDFUNCTION` 中同时设了 `SourceConstantAlpha`（设它为 `SA`）和 `AC_SRC_ALPHA`（启用像素 Alpha），每个像素的最终有效 Alpha 计算公式是：

```
EffectiveAlpha = SA × PixelAlpha / 255
```

所以如果 `SourceConstantAlpha = 200`，像素 Alpha = 180，那有效 Alpha = 200 × 180 / 255 ≈ 141。

很多时候你会想把 `SourceConstantAlpha` 设为 255，完全依赖像素自身的 Alpha——这是我们模板代码中的做法。但如果你需要动态调节整个窗口的透明度（比如鼠标悬停时变亮），可以通过改变 `SourceConstantAlpha` 来实现，而不需要重新生成整个位图。

## 实战：半透明悬浮球窗口

现在我们把前面学的所有知识整合起来，实现一个半透明悬浮球窗口。这个悬浮球是圆形的（用 Region 裁切形状），带有 Alpha 渐变效果（中心不透明、边缘半透明），支持鼠标拖动，鼠标悬停时会渐变变亮。

### 完整代码：悬浮球窗口

我们先给出完整的代码框架，然后逐段解释。

```cpp
#include <windows.h>
#include <cmath>

#pragma comment(lib, "msimg32.lib")

// 全局变量
HWND   g_hwnd = NULL;
int    g_nAlpha = 180;        // 当前 Alpha 值
int    g_nTargetAlpha = 180;  // 目标 Alpha 值
int    g_nHoverAlpha = 240;   // 悬停时的 Alpha
int    g_nNormalAlpha = 180;  // 正常时的 Alpha
bool   g_bDragging = false;
POINT  g_ptDragStart = {0};
int    g_nBallSize = 120;     // 悬浮球直径

// 函数声明
void RenderFloatBall(HWND hwnd);
void CalcGradientAlpha(BYTE* pBits, int w, int h);

LRESULT CALLBACK FloatWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建圆形 Region
        HRGN hrgn = CreateEllipticRgn(0, 0, g_nBallSize, g_nBallSize);
        SetWindowRgn(hwnd, hrgn, TRUE);

        // 启动定时器用于动画
        SetTimer(hwnd, 1, 30, NULL);

        // 首次渲染
        RenderFloatBall(hwnd);
        return 0;
    }
```

这段代码的 `WM_CREATE` 处理做了三件事：设置圆形 Region、启动动画定时器、首次渲染。注意我们同时使用了 Region（控制窗口形状）和分层窗口（控制透明度）——这两个机制是互补的。

### 鼠标交互：拖动与悬停检测

```cpp
    case WM_MOUSEMOVE:
    {
        if (g_bDragging) {
            int dx = LOWORD(lParam) - g_ptDragStart.x;
            int dy = HIWORD(lParam) - g_ptDragStart.y;

            RECT rc;
            GetWindowRect(hwnd, &rc);
            MoveWindow(hwnd, rc.left + dx, rc.top + dy,
                       g_nBallSize, g_nBallSize, TRUE);
        } else {
            // 鼠标悬停时增加 Alpha
            g_nTargetAlpha = g_nHoverAlpha;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        // 鼠标离开时恢复 Alpha
        g_nTargetAlpha = g_nNormalAlpha;
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        g_ptDragStart.x = LOWORD(lParam);
        g_ptDragStart.y = HIWORD(lParam);
        g_bDragging = true;
        SetCapture(hwnd);

        // 开始跟踪鼠标离开事件
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        g_bDragging = false;
        ReleaseCapture();
        return 0;
    }
```

这里我们用了 `TRACKMOUSEEVENT` 和 `TrackMouseEvent` 来检测鼠标离开窗口的事件。分层窗口默认不跟踪鼠标进入和离开，所以需要主动注册。`TME_LEAVE` 标志告诉系统"当鼠标离开这个窗口时，给我发 `WM_MOUSELEAVE` 消息"。注意 `WM_MOUSELEAVE` 是一次性的——每次收到后如果还需要继续跟踪，必须在下次 `WM_LBUTTONDOWN` 时重新注册。

### 定时器驱动动画

```cpp
    case WM_TIMER:
    {
        if (wParam == 1) {
            // 平滑过渡 Alpha 值
            if (g_nAlpha != g_nTargetAlpha) {
                int step = (g_nTargetAlpha > g_nAlpha) ? 5 : -5;
                g_nAlpha += step;

                // 防止越过目标值
                if ((step > 0 && g_nAlpha > g_nTargetAlpha) ||
                    (step < 0 && g_nAlpha < g_nTargetAlpha)) {
                    g_nAlpha = g_nTargetAlpha;
                }

                RenderFloatBall(hwnd);
            }
        }
        return 0;
    }
```

我们用定时器来驱动 Alpha 的渐变动画。每 30ms 检查一次当前 Alpha 和目标 Alpha 的差距，逐步逼近。这样当鼠标悬停时，悬浮球会平滑地变亮；鼠标离开时，又会平滑地变暗。`RenderFloatBall` 函数会在每次 Alpha 变化时重新渲染窗口内容。

### 核心渲染函数

这是整个悬浮球的核心——`RenderFloatBall` 函数负责创建 DIB Section、绘制内容、设置 Alpha 渐变、提交给分层窗口：

```cpp
void RenderFloatBall(HWND hwnd)
{
    int size = g_nBallSize;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);

    // 创建 32 位 DIB Section（顶朝下）
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = -size;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
                                    &pBits, NULL, 0);

    if (hbm && pBits) {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcSrc, hbm);

        // 先全部清零（全透明）
        memset(pBits, 0, size * size * 4);

        // 用 GDI 绘制圆形球体
        // 画一个蓝色圆形作为底色
        HBRUSH hbrFill = CreateSolidBrush(RGB(30, 140, 255));
        SelectObject(hdcSrc, hbrFill);
        // 稍微缩小一点，避免被 Region 边缘裁切
        Ellipse(hdcSrc, 2, 2, size - 2, size - 2);
        DeleteObject(hbrFill);

        // 画高光效果——一个偏上的白色椭圆
        HBRUSH hbrHighlight = CreateSolidBrush(RGB(200, 230, 255));
        SelectObject(hdcSrc, hbrHighlight);
        Ellipse(hdcSrc, size/4, size/8, size*3/4, size/2);
        DeleteObject(hbrHighlight);

        // 现在处理 Alpha 通道
        // 策略：中心完全不透明，边缘逐渐变透明
        int cxCenter = size / 2;
        int cyCenter = size / 2;
        int radius = size / 2 - 1;

        BYTE* pRow = (BYTE*)pBits;
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int dx = x - cxCenter;
                int dy = y - cyCenter;
                double dist = sqrt((double)(dx * dx + dy * dy));

                BYTE* pPixel = pRow + x * 4;

                if (pPixel[0] != 0 || pPixel[1] != 0 || pPixel[2] != 0) {
                    // 距离中心越远，Alpha 越低
                    double ratio = dist / radius;
                    if (ratio > 1.0) ratio = 1.0;

                    // 边缘渐变：从 1.0 到 0.0
                    double edgeFade = 1.0;
                    if (ratio > 0.7) {
                        edgeFade = 1.0 - (ratio - 0.7) / 0.3;
                    }

                    // 应用全局 Alpha（用于悬停效果）
                    pPixel[3] = (BYTE)(edgeFade * g_nAlpha);

                    // 高光区域更亮
                    if (pPixel[2] > 200 && pPixel[1] > 200 && y < size / 2) {
                        // 高光区域保持较亮的颜色
                        // 但 Alpha 值已经由边缘渐变决定
                    }
                }
                // 在圆外的像素保持 Alpha=0（完全透明）
            }
            pRow += size * 4;
        }

        // 提交给分层窗口
        BLENDFUNCTION bf = {};
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;  // 不额外应用全局 Alpha
        bf.AlphaFormat         = AC_SRC_ALPHA;

        SIZE sz = { size, size };
        POINT ptSrc = { 0, 0 };

        UpdateLayeredWindow(hwnd, NULL, NULL, &sz,
                           hdcSrc, &ptSrc, 0, &bf, ULW_ALPHA);

        SelectObject(hdcSrc, hbmOld);
        DeleteObject(hbm);
    }

    DeleteDC(hdcSrc);
    ReleaseDC(NULL, hdcScreen);
}
```

这段渲染函数的关键在于 Alpha 渐变的处理。我们计算每个像素到圆心的距离，然后根据距离设置 Alpha 值——中心完全不透明，越靠近边缘越透明。这样悬浮球的边缘就会呈现出柔和的渐变效果，没有 Region 那种硬边界的感觉。

在圆外的像素（没有被 GDI 绘制过的）保持 Alpha=0，所以完全透明——这就是为什么我们同时需要 Region 和 Alpha：Region 确定窗口的形状，Alpha 确定窗口内的透明度。实际上，对于使用了逐像素 Alpha 的分层窗口来说，Region 并不是严格必需的——Alpha=0 的区域在视觉上就是透明的。但 Region 可以帮助系统进行命中测试，决定鼠标消息是否发送给窗口。

### 入口函数

最后是程序的入口：

```cpp
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"FloatBallClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = FloatWndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = CLASS_NAME;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,  // 分层窗口 + 置顶
        CLASS_NAME,
        L"Float Ball",
        WS_POPUP,
        800, 400,                        // 初始位置
        g_nBallSize, g_nBallSize,        // 窗口大小等于球的大小
        NULL, NULL, hInstance, NULL
    );

    if (g_hwnd == NULL) return 0;

    ShowWindow(g_hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

    // 补充 WM_DESTROY 处理
    // case WM_DESTROY:
    //     KillTimer(hwnd, 1);
    //     PostQuitMessage(0);
    //     return 0;
```

编译运行后，你会看到一个蓝色半透明的悬浮球出现在桌面上。当你把鼠标移到球上时，它会平滑地变亮；移走后又会平滑地变暗。你可以拖动它到桌面任意位置。这个效果用纯粹的 Region 是绝对做不到的——只有 Alpha 混合才能实现这种柔和的透明渐变。

## 常见问题与调试

在实现分层窗口和 Alpha 混合时，有几个问题几乎每个人都会遇到。

### UpdateLayeredWindow 后窗口完全不可见

这通常是因为 DIB Section 的 Alpha 通道全为 0。正如前面反复强调的，GDI 绘制函数不会写 Alpha 通道，所以如果你用 `FillRect`、`Ellipse` 等 GDI 函数画了图但没有手动设置 Alpha，所有像素都是完全透明的。解决办法是在 GDI 绘制完成后遍历像素，给需要显示的区域设置正确的 Alpha 值。

另外一个可能的原因是 `BLENDFUNCTION` 的 `AlphaFormat` 设为了 0 而不是 `AC_SRC_ALPHA`，这会让系统忽略像素 Alpha 通道，只使用 `SourceConstantAlpha`。

### 边缘锯齿和颜色偏移

当你用 GDI 函数绘制图形时，GDI 使用的是逐像素整数坐标，所以边缘会有锯齿。而 DWM 合成分层窗口时，会对窗口边缘做一些抗锯齿处理——但这依赖于像素 Alpha 通道的正确设置。如果 Alpha 通道只有 0 和 255 两个值（没有中间值），DWM 的抗锯齿也帮不上忙。

要获得真正平滑的边缘，你需要在 Alpha 通道中也做渐变——边缘像素的 Alpha 值应该从内到外逐渐减小，这样 DWM 合成时就能产生自然的抗锯齿效果。我们的悬浮球代码中的 `edgeFade` 计算就是做这件事的。

### 性能问题：频繁调用 UpdateLayeredWindow

`UpdateLayeredWindow` 每次调用都会触发 DWM 重新合成窗口。如果你在动画循环中频繁调用（比如每帧都调用），可能会影响性能。优化方法包括：只在内容真正变化时才调用，使用脏区域追踪避免全窗口更新，或者把一些静态内容缓存到位图中只更新变化的部分。

对于我们的悬浮球来说，因为只有在 Alpha 渐变动画进行时才需要更新（不是每帧都更新），性能开销可以接受。

### msimg32.lib 链接问题

如果你在使用 MinGW（GCC）编译器而不是 MSVC，`msimg32.lib` 的文件名可能不同——MinGW 中通常叫 `libmsimg32.a`。链接方式是 `-lmsimg32`。确保你的 MinGW 版本包含这个库，一些较老或精简的发行版可能没有。

## 总结

这篇文章我们深入探讨了 Win32 平台上的 Alpha 混合和透明效果实现。从 Alpha 混合的数学原理，到 `AlphaBlend` 函数的逐像素混合，到 `TransparentBlt` 的颜色键透明，再到 `WS_EX_LAYERED` 分层窗口的两种更新方式——`SetLayeredWindowAttributes` 的整体透明度和 `UpdateLayeredWindow` 的逐像素透明。最后我们把这些知识整合起来，实现了一个带有渐变透明效果的半透明悬浮球窗口。

关于 GDI 的 Region 和 Alpha 混合这两篇文章，我们基本上把 GDI 在形状和透明度方面的能力都覆盖了。但 GDI 作为一套诞生于上世纪 90 年代的图形 API，在很多方面已经力不从心——它的坐标精度只有整数、Alpha 通道支持不完善、性能受限于 CPU 绘制。下一篇文章我们就要跨入 GDI+ 的世界，看看微软是如何用 C++ 类封装来解决 GDI 的这些痛点的，同时也会对比 GDI 和 GDI+ 在架构设计上的根本差异。

---

## 练习

1. **用 AlphaBlend 实现图像淡入淡出**：加载一张位图（可以用 `LoadImage` 从文件加载），用定时器逐步改变 `BLENDFUNCTION` 的 `SourceConstantAlpha`，实现图像从完全透明到完全不透明的淡入效果，以及反向的淡出效果。

2. **实现一个带圆角的半透明面板**：使用 `WS_EX_LAYERED` + `UpdateLayeredWindow`，在 DIB Section 上绘制一个带圆角的矩形，圆角区域和边缘区域通过 Alpha 渐变实现抗锯齿，面板内容区域完全不透明。

3. **实现鼠标穿透的透明区域**：在悬浮球的基础上，对于 Alpha=0 的区域实现鼠标穿透（使用 `SetWindowLong` 添加 `WS_EX_TRANSPARENT` 样式，或者通过 `WM_NCHITTEST` 返回 `HTTRANSPARENT`）。注意：`WS_EX_TRANSPARENT` 会影响整个窗口的鼠标事件处理，需要谨慎使用。

4. **实现颜色键透明窗口**：创建一个分层窗口，用 `SetLayeredWindowAttributes` 设置 `LWA_COLORKEY`，使窗口中所有白色像素透明。在窗口中绘制一些非白色的图形，观察白色区域是否"镂空"显示桌面内容。

---

**参考资料**:
- [AlphaBlend function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-alphablend)
- [BLENDFUNCTION structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-blendfunction)
- [TransparentBlt function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-transparentblt)
- [UpdateLayeredWindow function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)
- [SetLayeredWindowAttributes function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes)
- [CreateDIBSection function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection)
- [Alpha Blending overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/alpha-blending)
- [Layered Windows - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows)
