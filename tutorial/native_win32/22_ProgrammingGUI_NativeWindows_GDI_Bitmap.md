# 通用GUI编程技术——Win32 原生编程实战（二十二）——GDI 位图操作：BitBlt、StretchBlt 与图像处理

> 之前我们聊过图标、光标、位图资源的加载和显示，但那只是冰山一角。当你真正开始在 Win32 中做图像处理的时候，会发现 GDI 的位图操作远不止"加载一张图片然后贴上去"这么简单。你需要缩放、需要透明、需要半透明混合、需要对像素进行各种奇奇怪怪的操作。这些需求指向同一个主题——GDI 位图操作的完整体系。今天我们要深入的就是这个体系中最核心的部分：BitBlt、StretchBlt、TransparentBlt 和 AlphaBlend。

---

## 为什么要专门聊位图操作

说实话，我在刚开始学 Win32 图形编程的时候，对位图操作这件事其实有点轻视。那时候觉得不就是贴个图嘛，`BitBlt` 一行代码搞定的事有什么好说的？结果当你真正需要做一个图片查看器、或者实现一个自定义控件的时候，才会发现这个问题有多坑。

为什么这么说？因为位图操作涉及到的细节实在是太多了。你需要理解 DDB 和 DIB 的区别，需要搞懂兼容 DC 到底是什么，需要知道什么时候用 `BitBlt` 什么时候用 `StretchBlt`，需要掌握各种光栅操作码的魔法，还需要处理透明和半透明混合。而且每一个环节都有无数个坑等着你——内存泄漏、色键错误、缩放质量差、多显示器兼容性问题……

但位图操作的重要性是不言而喻的。它是 GUI 编程的基础素材，几乎所有视觉效果都建立在位图操作之上。从简单的背景图片到复杂的图像处理滤镜，从双缓冲绘图到游戏引擎的渲染管线，位图操作都是不可或缺的技能。

另一个现实原因是：现代框架把这些东西封装得太好了，你可能只需要调用一个 `DrawImage` 方法就行了。但一旦你需要用纯 Win32 写程序，或者需要做一些底层优化（比如实现自定义的图像特效、优化渲染性能），这些知识就会成为你的短板。而且，理解了 GDI 的位图操作机制之后，你会对所有图形框架的底层实现有更清晰的认识。

这篇文章会带着你从零开始，把 GDI 位图操作彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 2000+ 都支持核心 API）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）
* **链接库**：gdi32.lib（核心）、msimg32.lib（AlphaBlend 和 TransparentBlt）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的窗口、怎么处理 `WM_PAINT` 消息、什么是 DC。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——理解位图的基础概念

在我们深入操作之前，先要把位图的基础概念搞清楚。Win32 中的位图世界分为两大阵营：DDB 和 DIB。

### DDB vs DIB

**DDB（Device-Dependent Bitmap，设备相关位图）** 是 Win32 早期的位图格式，它的像素存储格式依赖于当前显示设备的特性。DDB 的内部格式是硬件相关的，同样的 DDB 在不同颜色深度的设备上显示效果可能完全不同。`CreateCompatibleBitmap` 创建的就是 DDB，这也是为什么它叫"兼容"位图——它的格式与指定 DC 兼容，或者说与该 DC 关联的设备兼容。

**DIB（Device-Independent Bitmap，设备无关位图）** 是后来引入的格式，解决了 DDB 的设备依赖问题。DIB 包含了自己的颜色信息，不依赖于显示设备。BMP 文件就是 DIB 格式，可以在任何设备上正确显示。`CreateDIBSection` 创建的也是 DIB，但它同时提供了 DDB 的性能优势。

你可以把 DDB 理解为"给显卡准备的格式"，把 DIB 理解为"给文件存储准备的格式"。DDB 直接操作效率高，但不适合跨设备；DIB 格式标准，适合存储和传输，但需要转换才能高效显示。

### HBITMAP 类型

无论是 DDB 还是 DIB，在代码中都用 `HBITMAP` 句柄表示：

```cpp
typedef HANDLE HBITMAP;
```

`HBITMAP` 是一个 GDI 对象句柄，需要用 `DeleteObject` 释放。这里有一个很重要的点：GDI 对象不是 C++ 对象，没有 RAII，没有自动析构，你必须手动管理生命周期。这一点真的坑了我不少次。

### 位图的设备依赖性

位图的设备依赖性主要体现在颜色格式上。一个 32 位色显示器上创建的 DDB，在 16 位色显示器上可能会显示异常，因为两者的像素格式不同。而 DIB 带有颜色表或颜色掩码，可以在任何设备上正确解析。

这就是为什么 `CreateCompatibleBitmap` 需要一个 DC 参数——它需要知道要与哪个设备兼容。如果你传入的是内存 DC（刚创建的兼容 DC，还没有选入位图），它会创建一个 1×1 的单色位图！这个坑太经典了，我们后面详细说。

---

## 第二步——兼容 DC 与内存位图

现在我们来理解 GDI 位图操作的核心机制：兼容 DC 和内存位图。

### CreateCompatibleDC 的作用

`CreateCompatibleDC` 创建一个"内存设备上下文"，这是一个不与任何实际设备关联的 DC，它的存在纯粹是为了在内存中绘图：

```cpp
HDC hdcMem = CreateCompatibleDC(hdc);  // hdc 是显示 DC
```

内存 DC 有一块小小的绘图表面，默认大小是 1×1 像素，单色。这块表面太小了，基本上没什么实际用途。你需要在这块表面上"贴"一张更大的位图才能正常使用。

你可以把内存 DC 理解为"画板的架子"，它本身只是一个框架，你需要往上放画纸（位图）才能画画。

### CreateCompatibleBitmap 创建内存位图

`CreateCompatibleBitmap` 创建一张与指定设备兼容的位图：

```cpp
HBITMAP hbm = CreateCompatibleBitmap(
    hdc,        // 与这个 DC 兼容
    width,      // 宽度（像素）
    height      // 高度（像素）
);
```

⚠️ 注意

千万别把内存 DC 传给 `CreateCompatibleBitmap`！如果你这样做：

```cpp
// 错误示例！
HDC hdcMem = CreateCompatibleDC(hdc);
HBITMAP hbm = CreateCompatibleBitmap(hdcMem, 800, 600);  // 错！
```

刚创建的内存 DC 默认只有 1×1 单色表面，所以创建出来的位图也是 1×1 单色位图，根本不是你想要的 800×600。正确的做法是传入显示 DC：

```cpp
// 正确示例
HDC hdcMem = CreateCompatibleDC(hdc);
HBITMAP hbm = CreateCompatibleBitmap(hdc, 800, 600);  // 对！传入显示 DC
```

### 选入位图的正确流程

有了内存 DC 和位图之后，你需要把位图"选入"DC 才能使用：

```cpp
// 创建内存 DC
HDC hdcMem = CreateCompatibleDC(hdc);

// 创建位图
HBITMAP hbm = CreateCompatibleBitmap(hdc, 800, 600);

// 选入位图，保存旧位图句柄
HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

// 现在可以在 hdcMem 上绘图了...

// 用完后恢复旧位图
SelectObject(hdcMem, hbmOld);

// 清理资源
DeleteDC(hdcMem);
DeleteObject(hbm);
```

`SelectObject` 把 GDI 对象选入 DC，返回的是之前的对象。你必须保存这个旧对象，用完之后要恢复回去。这一点真的不能偷懒，否则会导致内存泄漏或者奇怪的绘图问题。

### 为什么要这么麻烦

你可能会问：为什么不直接在显示 DC 上绘图？主要有三个原因：

1. **双缓冲**：直接在屏幕上绘图会看到闪烁，因为每个绘图操作都是立即可见的。在内存 DC 上画好整幅图，然后一次性复制到屏幕，可以避免闪烁。

2. **离屏渲染**：有些图像处理操作需要在后台完成，准备好之后再显示。

3. **复用绘图结果**：如果同样的内容要绘制多次，在内存 DC 上画一次然后重复复制，比每次都重新绘制效率高得多。

---

## 第三步——BitBlt：块传输

`BitBlt`（Bit Block Transfer）是 GDI 最基础的位图传输函数，它把一块像素从源 DC 复制到目标 DC。

### 参数详解与坐标系

```cpp
BOOL BitBlt(
    HDC   hdc,     // 目标 DC
    int   x,       // 目标左上角 X 坐标
    int   y,       // 目标左上角 Y 坐标
    int   cx,      // 宽度
    int   cy,      // 高度
    HDC   hdcSrc,  // 源 DC
    int   x1,      // 源左上角 X 坐标
    int   y1,      // 源左上角 Y 坐标
    DWORD rop      // 光栅操作代码
);
```

坐标都是相对于各自 DC 的逻辑坐标。如果源和目标 DC 的映射模式不同，GDI 会自动转换坐标。

### 光栅操作代码（ROP3）详解

`BitBlt` 的强大之处在于 `rop` 参数，它指定了如何组合源像素和目标像素。这些操作码叫作 ROP3（Raster Operation 3-operand），因为它们涉及三个操作数：源（Source）、目标（Destination）、模式（Pattern）。

最常用的光栅操作码包括：

| 操作码 | 值 | 含义 |
|--------|-----|------|
| BLACKNESS | 0x00000042 | 目标 = 黑色（所有位清零） |
| DSTINVERT | 0x00550009 | 目标 = ~目标（取反） |
| MERGECOPY | 0x00C000CA | 目标 = 源 & 模式 |
| MERGEPAINT | 0x00BB0226 | 目标 = ~源 \| 目标 |
| NOTSRCCOPY | 0x00330008 | 目标 = ~源（源取反） |
| NOTSRCERASE | 0x001100A6 | 目标 = ~(源 \| 目标) |
| PATCOPY | 0x00F00021 | 目标 = 模式 |
| PATINVERT | 0x005A0049 | 目标 ^= 模式 |
| PATPAINT | 0x00FB0A09 | 目标 = (~源 \| 模式) \| 目标 |
| SRCAND | 0x008800C6 | 目标 &= 源 |
| SRCCOPY | 0x00CC0020 | 目标 = 源（直接复制） |
| SRCERASE | 0x00440328 | 目标 = 源 & ~目标 |
| SRCINVERT | 0x00660046 | 目标 ^= 源 |
| SRCPAINT | 0x00EE0086 | 目标 \|= 源 |
| WHITENESS | 0x00FF0062 | 目标 = 白色（所有位置一） |

这些值的十六进制形式看起来很奇怪，但实际上是有规律的。ROP3 码是一个 8 位十六进制数（32 位），低 24 位是三个布尔操作的编码。但这个细节太底层了，实际使用中记住常用的几个就行。

### 常用光栅操作：SRCCOPY、SRCAND、SRCINVERT

**SRCCOPY** 是最常用的——直接复制：

```cpp
BitBlt(hdc, 0, 0, 100, 100, hdcMem, 0, 0, SRCCOPY);
```

**SRCAND** 用于实现遮罩效果：

```cpp
// 先用 SRCAND 把目标变暗
BitBlt(hdc, 0, 0, 100, 100, hdcMask, 0, 0, SRCAND);

// 再用 SRCPAINT 叠加前景
BitBlt(hdc, 0, 0, 100, 100, hdcForeground, 0, 0, SRCPAINT);
```

**SRCINVERT** 可以用来实现光标闪烁效果：

```cpp
// 第一次调用：显示
BitBlt(hdc, x, y, w, h, hdcMem, 0, 0, SRCINVERT);

// 第二次调用：恢复（因为 XOR 两次等于原值）
BitBlt(hdc, x, y, w, h, hdcMem, 0, 0, SRCINVERT);
```

### 实现图像特效（反色、灰度）

用光栅操作可以实现一些简单的图像特效：

**反色效果**：

```cpp
// 方法1：使用 DSTINVERT 反转目标
BitBlt(hdc, 0, 0, width, height, NULL, 0, 0, DSTINVERT);

// 方法2：使用 NOTSRCCOPY 复制反色的源
BitBlt(hdc, 0, 0, width, height, hdcSrc, 0, 0, NOTSRCCOPY);
```

但更复杂的特效（如灰度化）就需要直接操作像素了，光栅操作做不了。这时候就需要 DIB Section 或者其他方法。

---

## 第四步——StretchBlt：缩放位图

`StretchBlt` 是 `BitBlt` 的增强版，可以在复制的同时缩放位图。

### 拉伸参数与纵横比

```cpp
BOOL StretchBlt(
    HDC   hdcDest,  // 目标 DC
    int   xDest,    // 目标 X
    int   yDest,    // 目标 Y
    int   wDest,    // 目标宽度
    int   hDest,    // 目标高度
    HDC   hdcSrc,   // 源 DC
    int   xSrc,     // 源 X
    int   ySrc,     // 源 Y
    int   wSrc,     // 源宽度
    int   hSrc,     // 源高度
    DWORD rop       // 光栅操作
);
```

如果 `wDest != wSrc` 或 `hDest != hSrc`，位图会被拉伸或压缩。

**保持纵横比缩放**：

```cpp
// 计算保持纵横比的目标尺寸
int srcWidth = 800, srcHeight = 600;
int destWidth = 400, destHeight = 300;

// 如果目标尺寸不匹配，按比例调整
if (destWidth * srcHeight != destHeight * srcWidth)
{
    // 按宽度适配
    destHeight = destWidth * srcHeight / srcWidth;
}

StretchBlt(hdc, 0, 0, destWidth, destHeight,
           hdcMem, 0, 0, srcWidth, srcHeight, SRCCOPY);
```

⚠️ 注意

千万别忘记检查纵横比！直接拉伸会导致图像变形，圆变成椭圆，人变瘦或变胖。除非这确实是你想要的效果。

### HALFTONE 模式与 SetStretchBltMode

默认的拉伸质量通常不太好，`StretchBlt` 使用简单的像素丢弃或复制算法。要提高质量，需要设置拉伸模式：

```cpp
// 设置拉伸模式为 HALFTONE（高质量）
SetStretchBltMode(hdc, HALFTONE);

// 设置 HALFTONE 时应该设置刷子原点
SetBrushOrgEx(hdc, 0, 0, NULL);

StretchBlt(hdc, 0, 0, destWidth, destHeight,
           hdcMem, 0, 0, srcWidth, srcHeight, SRCCOPY);
```

拉伸模式包括：

| 模式 | 描述 |
|------|------|
| BLACKONWHITE | 在消除锯齿时，优先使用黑色像素（默认） |
| WHITEONBLACK | 在消除锯齿时，优先使用白色像素 |
| COLORONCOLOR | 直接删除像素，不进行插值 |
| HALFTONE | 使用高质量插值和半色调处理 |

`HALFTONE` 模式质量最好，但速度最慢。对于实时渲染的场景可能需要权衡。

### 缩放质量设置

`SetStretchBltMode` 设置的是当前 DC 的拉伸模式，影响之后所有 `StretchBlt` 调用。不同的 DC 有独立的设置：

```cpp
// 设置内存 DC 的拉伸模式
SetStretchBltMode(hdcMem, HALFTONE);
SetBrushOrgEx(hdcMem, 0, 0, NULL);

// 设置显示 DC 的拉伸模式
SetStretchBltMode(hdc, HALFTONE);
SetBrushOrgEx(hdc, 0, 0, NULL);
```

---

## 第五步——TransparentBlt：透明位图

`TransparentBlt` 是一个专门用于透明绘图的函数，它在复制位图时可以把指定的颜色当作透明处理。

### 透明色参数

```cpp
BOOL TransparentBlt(
    HDC   hdcDest,       // 目标 DC
    int   xoriginDest,   // 目标 X
    int   yoriginDest,   // 目标 Y
    int   wDest,         // 目标宽度
    int   hDest,         // 目标高度
    HDC   hdcSrc,        // 源 DC
    int   xoriginSrc,    // 源 X
    int   yoriginSrc,    // 源 Y
    int   wSrc,          // 源宽度
    int   hSrc,          // 源高度
    UINT  crTransparent  // 透明色（RGB 值）
);
```

`crTransparent` 是 RGB 颜色值，源位图中这个颜色的像素不会被复制：

```cpp
// 把 RGB(255, 0, 255)（洋红色）当作透明色
TransparentBlt(hdc, 100, 100, 64, 64,
               hdcMem, 0, 0, 64, 64,
               RGB(255, 0, 255));
```

⚠️ 注意

千万别搞错透明色的格式！`RGB` 宏返回的是 `COLORREF` 值，格式是 `0x00bbggrr`（蓝绿红），而不是常见的 `0x00rrggbb`。如果你用 `0xFF00FF`（纯洋红的常见表示），实际上得到的是 RGB(255, 0, 255) —— 但这个巧合是因为红绿相等。对于其他颜色，一定要用 `RGB` 宏。

### 与 BitBlt 的性能对比

`TransparentBlt` 在内部是通过多次 `BitBlt` 调用实现的。它创建一个掩码位图，然后用 `SRCAND` 和 `SRCPAINT` 组合实现透明效果。这意味着它的性能比单纯的 `BitBlt` 差，特别是对于大位图。

如果你需要频繁绘制透明位图，考虑预先生成掩码位图，然后用 `BitBlt` 组合：

```cpp
// 预先生成掩码（只做一次）
HBITMAP CreateMaskBitmap(HBITMAP hbmColor, COLORREF crTransparent)
{
    // 获取位图尺寸
    BITMAP bm;
    GetObject(hbmColor, sizeof(bm), &bm);

    // 创建单色掩码位图
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HDC hdcMask = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmMask = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);
    HBITMAP hbmOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);
    HBITMAP hbmOldMem = (HBITMAP)SelectObject(hdcMem, hbmColor);

    // 设置背景色为透明色
    COLORREF crOldBk = SetBkColor(hdcMem, crTransparent);

    // 创建掩码：透明色部分为 1（白色），其他为 0（黑色）
    BitBlt(hdcMask, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

    // 恢复并清理
    SetBkColor(hdcMem, crOldBk);
    SelectObject(hdcMem, hbmOldMem);
    SelectObject(hdcMask, hbmOldMask);
    DeleteDC(hdcMem);
    DeleteDC(hdcMask);
    ReleaseDC(NULL, hdcScreen);

    return hbmMask;
}

// 使用掩码快速绘制透明位图
void DrawTransparentBitmap(HDC hdc, int x, int y, HBITMAP hbmColor, HBITMAP hbmMask, int width, int height)
{
    HDC hdcMem = CreateCompatibleDC(hdc);
    HDC hdcMask = CreateCompatibleDC(hdc);

    HBITMAP hbmOldMem = (HBITMAP)SelectObject(hdcMem, hbmColor);
    HBITMAP hbmOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);

    // 先用掩码清空目标区域（AND 操作）
    BitBlt(hdc, x, y, width, height, hdcMask, 0, 0, SRCAND);

    // 再绘制彩色位图（OR 操作 - 需要预先处理彩色位图）
    // 这里简化了，实际实现需要更多步骤

    SelectObject(hdcMem, hbmOldMem);
    SelectObject(hdcMask, hbmOldMask);
    DeleteDC(hdcMem);
    DeleteDC(hdcMask);
}
```

---

## 第六步——AlphaBlend：半透明混合

`AlphaBlend` 是 Windows 2000 引入的高级函数，支持真正的 Alpha 混合（半透明效果）。

### BLENDFUNCTION 结构体

```cpp
typedef struct _BLENDFUNCTION {
    BYTE BlendOp;       // 必须为 AC_SRC_OVER
    BYTE BlendFlags;    // 必须为 0
    BYTE SourceConstantAlpha;  // 全局 Alpha 值（0-255）
    BYTE AlphaFormat;   // 格式标志
} BLENDFUNCTION;
```

**BlendOp**：必须是 `AC_SRC_OVER`（0），表示源覆盖在目标之上。

**BlendFlags**：必须为 0，保留字段。

**SourceConstantAlpha**：应用到整个源位图的 Alpha 值。0 表示完全透明，255 表示完全不透明。

**AlphaFormat**：如果源位图有 per-pixel alpha，设置为 `AC_SRC_ALPHA`（1）。

### 源常量 alpha 与 per-pixel alpha

有两种 Alpha 混合方式：

**源常量 Alpha**：整个位图使用相同的透明度：

```cpp
BLENDFUNCTION bf = {};
bf.BlendOp = AC_SRC_OVER;
bf.SourceConstantAlpha = 128;  // 50% 不透明

AlphaBlend(hdc, 0, 0, 200, 200,
           hdcMem, 0, 0, 200, 200,
           sizeof(bf), bf);  // 注意这里传的是结构体大小，不是结构体指针！
```

⚠️ 注意

千万别搞错 `AlphaBlend` 的最后一个参数！它不是 `BLENDFUNCTION*`，而是 `DWORD` 类型的结构体大小。官方签名是这样的：

```cpp
BOOL AlphaBlend(
    HDC           hdcDest,
    int           xoriginDest,
    int           yoriginDest,
    int           wDest,
    int           hDest,
    HDC           hdcSrc,
    int           xoriginSrc,
    int           yoriginSrc,
    int           wSrc,
    int           hSrc,
    BLENDFUNCTION blend  // 这里！不是指针，是结构体按值传递
);
```

但实际上在 mingw 或某些头文件中，最后一个参数是 `DWORD`，你需要用 `sizeof(BLENDFUNCTION)` 传入。这也是一个很经典的坑。

**Per-pixel Alpha**：每个像素有自己的 Alpha 值：

```cpp
BLENDFUNCTION bf = {};
bf.BlendOp = AC_SRC_OVER;
bf.SourceConstantAlpha = 255;  // 不使用全局 alpha
bf.AlphaFormat = AC_SRC_ALPHA;  // 使用 per-pixel alpha

// 假设 hdcMem 包含 32bpp 位图，每个像素有 alpha 通道
AlphaBlend(hdc, 0, 0, 200, 200,
           hdcMem, 0, 0, 200, 200,
           sizeof(bf), bf);
```

要使用 per-pixel alpha，源位图必须是 32 位格式，并且 alpha 通道有效。`LoadImage` 带 `LR_CREATEDIBSECTION` 标志加载的 32 位 BMP 通常包含 alpha 通道，但需要确认。

### Windows 2000+ 的可用性

`AlphaBlend` 是在 Windows 2000 中引入的，链接到 `msimg32.lib`。如果你的代码需要在 Windows 98/NT 4 上运行，需要动态加载：

```cpp
// 动态加载 AlphaBlend
typedef BOOL (WINAPI* PFNALPHABLEND)(
    HDC, int, int, int, int,
    HDC, int, int, int, int,
    BLENDFUNCTION);

PFNALPHABLEND g_pfnAlphaBlend = NULL;

void InitAlphaBlend()
{
    HMODULE hMsimg32 = LoadLibrary(TEXT("msimg32.dll"));
    if (hMsimg32)
    {
        g_pfnAlphaBlend = (PFNALPHABLEND)GetProcAddress(hMsimg32, "AlphaBlend");
    }
}

// 使用时
if (g_pfnAlphaBlend)
{
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 128, 0 };
    g_pfnAlphaBlend(hdc, 0, 0, w, h, hdcMem, 0, 0, w, h, bf);
}
else
{
    // 降级到不透明绘制
    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
}
```

---

## 第七步——位图资源加载

我们在前面的文章中已经介绍过 `LoadImage` 和 `LoadBitmap`，但这里要更深入地讨论它们的区别和正确用法。

### LoadImage 加载资源位图

`LoadImage` 是推荐的现代方法：

```cpp
HBITMAP hBitmap = (HBITMAP)LoadImage(
    hInstance,                      // 应用程序实例
    MAKEINTRESOURCE(IDB_MYBITMAP),  // 资源 ID
    IMAGE_BITMAP,                   // 类型
    0, 0,                           // 使用实际尺寸
    LR_CREATEDIBSECTION             // 创建 DIB Section
);
```

`LR_CREATEDIBSECTION` 标志创建一个 DIB Section，允许你直接访问像素数据。如果只需要显示，不需要这个标志。

**从文件加载**：

```cpp
HBITMAP hBitmap = (HBITMAP)LoadImage(
    NULL,                   // 不使用资源
    L"image.bmp",           // 文件路径
    IMAGE_BITMAP,
    0, 0,
    LR_LOADFROMFILE | LR_CREATEDIBSECTION
);
```

### LoadBitmap 的遗留问题

`LoadBitmap` 是遗留 API，从 Windows 1.0 就存在了：

```cpp
HBITMAP hBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_MYBITMAP));
```

`LoadBitmap` 的问题包括：
1. 只能从资源加载，不能从文件加载
2. 只能加载位图，不能加载图标或光标
3. 不支持指定尺寸
4. 加载的是 DDB，不是 DIB
5. 不支持 32 位带 alpha 的位图

除非你需要维护遗留代码，否则新代码应该使用 `LoadImage`。

### DeleteObject 的调用时机

`LoadImage` 加载的位图在不用时必须用 `DeleteObject` 释放：

```cpp
HBITMAP hBitmap = (HBITMAP)LoadImage(...);

// 使用位图...

// 用完后释放
DeleteObject(hBitmap);
```

但如果使用了 `LR_SHARED` 标志，**不要**释放！系统会管理共享资源的生命周期：

```cpp
// LR_SHARED 标志
HBITMAP hBitmap = (HBITMAP)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDB_MYBITMAP),
    IMAGE_BITMAP,
    0, 0,
    LR_CREATEDIBSECTION | LR_SHARED  // 共享资源
);

// 不要调用 DeleteObject！系统会管理
```

⚠️ 注意

千万别搞混共享和非共享的资源管理！对于用 `LR_SHARED` 加载的资源，调用 `DeleteObject` 会导致问题，因为其他地方可能还在使用这个资源。对于非共享资源，不调用 `DeleteObject` 会内存泄漏。

---

## 第八步——实战示例：完整的图片查看器

让我们把以上知识整合成一个完整的示例程序。这个程序可以加载和显示 BMP 文件，支持缩放、透明效果和 Alpha 混合演示。

### 主程序结构

```cpp
#include <windows.h>
#include <commdlg.h>
#include <msimg32.h>  // AlphaBlend 和 TransparentBlt

#pragma comment(lib, "msimg32.lib")

// 窗口类名
static const wchar_t g_szClassName[] = L"GdiBitmapDemo";

// 全局变量
static HBITMAP g_hBitmap = NULL;
static int g_nScale = 100;  // 缩放比例（百分比）
static BOOL g_bTransparent = FALSE;
static int g_nAlpha = 255;

// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void PaintWindow(HWND hwnd);

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        L"GDI 位图操作演示",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, L"窗口创建失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建菜单
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreatePopupMenu();
        AppendMenuW(hFileMenu, MF_STRING, 1001, L"打开位图(&O)...");
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hFileMenu, MF_STRING, 1002, L"退出(&X)");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"文件(&F)");

        HMENU hEffectMenu = CreatePopupMenu();
        AppendMenuW(hEffectMenu, MF_STRING, 2001, L"50% 大小");
        AppendMenuW(hEffectMenu, MF_STRING, 2002, L"100% 大小");
        AppendMenuW(hEffectMenu, MF_STRING, 2003, L"200% 大小");
        AppendMenuW(hEffectMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hEffectMenu, MF_STRING, 2004, L"透明模式（开/关）");
        AppendMenuW(hEffectMenu, MF_STRING, 2005, L"Alpha 混合演示");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEffectMenu, L"效果(&E)");

        SetMenu(hwnd, hMenu);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case 1001:  // 打开位图
        {
            OPENFILENAMEW ofn = {};
            wchar_t szFile[260] = { 0 };

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"位图文件 (*.bmp)\0*.bmp\0所有文件 (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameW(&ofn))
            {
                // 释放旧位图
                if (g_hBitmap)
                {
                    DeleteObject(g_hBitmap);
                    g_hBitmap = NULL;
                }

                // 加载新位图
                g_hBitmap = (HBITMAP)LoadImage(
                    NULL,
                    szFile,
                    IMAGE_BITMAP,
                    0, 0,
                    LR_LOADFROMFILE | LR_CREATEDIBSECTION
                );

                if (g_hBitmap)
                {
                    g_nScale = 100;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                else
                {
                    MessageBoxW(hwnd, L"加载位图失败！", L"错误", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case 1002:  // 退出
            DestroyWindow(hwnd);
            break;

        case 2001:  // 50% 大小
            g_nScale = 50;
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case 2002:  // 100% 大小
            g_nScale = 100;
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case 2003:  // 200% 大小
            g_nScale = 200;
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case 2004:  // 透明模式
            g_bTransparent = !g_bTransparent;
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case 2005:  // Alpha 混合演示
        {
            static int s_alphaValues[] = { 255, 200, 150, 100, 50 };
            static int s_index = 0;
            g_nAlpha = s_alphaValues[s_index];
            s_index = (s_index + 1) % (sizeof(s_alphaValues) / sizeof(int));

            wchar_t szTitle[64];
            swprintf_s(szTitle, 64, L"Alpha = %d", g_nAlpha);
            SetWindowTextW(hwnd, szTitle);

            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        }
        return 0;
    }

    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;

    case WM_DESTROY:
    {
        // 释放位图资源
        if (g_hBitmap)
        {
            DeleteObject(g_hBitmap);
            g_hBitmap = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// 绘制窗口
void PaintWindow(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 获取客户区
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 填充背景
    FillRect(hdc, &rcClient, (HBRUSH)GetStockObject(WHITE_BRUSH));

    if (g_hBitmap)
    {
        // 获取位图尺寸
        BITMAP bm;
        GetObject(g_hBitmap, sizeof(bm), &bm);

        // 创建内存 DC
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, g_hBitmap);

        // 计算缩放后的尺寸
        int scaledWidth = bm.bmWidth * g_nScale / 100;
        int scaledHeight = bm.bmHeight * g_nScale / 100;

        // 计算居中位置
        int x = (rcClient.right - scaledWidth) / 2;
        int y = (rcClient.bottom - scaledHeight) / 2;

        if (g_bTransparent)
        {
            // 使用 TransparentBlt，把白色当作透明
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, NULL);

            TransparentBlt(hdc, x, y, scaledWidth, scaledHeight,
                          hdcMem, 0, 0, bm.bmWidth, bm.bmHeight,
                          RGB(255, 255, 255));
        }
        else if (g_nAlpha < 255)
        {
            // 使用 AlphaBlend
            BLENDFUNCTION bf = {};
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.SourceConstantAlpha = g_nAlpha;
            bf.AlphaFormat = 0;

            AlphaBlend(hdc, x, y, scaledWidth, scaledHeight,
                      hdcMem, 0, 0, bm.bmWidth, bm.bmHeight,
                      bf);
        }
        else
        {
            // 使用 StretchBlt
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, NULL);

            StretchBlt(hdc, x, y, scaledWidth, scaledHeight,
                      hdcMem, 0, 0, bm.bmWidth, bm.bmHeight,
                      SRCCOPY);
        }

        // 清理
        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);

        // 显示信息
        wchar_t szInfo[256];
        swprintf_s(szInfo, 256, L"尺寸: %d × %d | 缩放: %d%%",
                   bm.bmWidth, bm.bmHeight, g_nScale);
        TextOutW(hdc, 10, 10, szInfo, wcslen(szInfo));
    }
    else
    {
        // 没有位图时显示提示
        const wchar_t* szHint = L"使用「文件」菜单打开位图文件";
        TextOutW(hdc, 10, 10, szHint, wcslen(szHint));
    }

    EndPaint(hwnd, &ps);
}
```

### 验证输出

编译运行这个程序，你可以：
1. 使用「文件」菜单打开 BMP 文件
2. 使用「效果」菜单调整缩放比例
3. 开启透明模式（白色背景变透明）
4. 演示 Alpha 混合效果

试试加载一张带白色背景的图片，然后开启透明模式，看看效果如何。

---

## 第九步——常见问题：内存泄漏、色键错误

在实际开发中，你一定会遇到各种问题。这里总结一些最常见的问题和解决方案。

### 内存泄漏检测

GDI 对象泄漏不像内存泄漏那么明显，但累积多了会导致系统绘图性能下降甚至崩溃。Windows 的任务管理器可以显示 GDI 对象数量：

1. 打开任务管理器
2. 在「详细信息」标签页，右键点击列标题
3. 选择「选择列」
4. 勾选「GDI 对象」

如果你的程序的 GDI 对象数量持续增长，就是有泄漏了。

**常见泄漏原因**：

1. 忘记调用 `DeleteObject` 释放位图
2. 忘记调用 `DeleteDC` 释放内存 DC
3. `SelectObject` 后没有恢复旧对象
4. 在循环中重复创建 GDI 对象而不释放

**正确的资源管理模式**：

```cpp
// 模式1：使用时创建，用完立即释放
{
    HBITMAP hbm = CreateCompatibleBitmap(hdc, 100, 100);
    // 使用 hbm...
    DeleteObject(hbm);
}

// 模式2：缓存对象，WM_DESTROY 时释放
static HBITMAP g_hCachedBitmap = NULL;

case WM_CREATE:
    g_hCachedBitmap = CreateCompatibleBitmap(hdc, 100, 100);
    break;

case WM_DESTROY:
    if (g_hCachedBitmap)
    {
        DeleteObject(g_hCachedBitmap);
        g_hCachedBitmap = NULL;
    }
    break;

// 模式3：RAII 包装器（推荐）
class GdiBitmap
{
public:
    GdiBitmap(HBITMAP hbm = NULL) : m_hbm(hbm) {}
    ~GdiBitmap() { if (m_hbm) DeleteObject(m_hbm); }

    GdiBitmap(const GdiBitmap&) = delete;
    GdiBitmap& operator=(const GdiBitmap&) = delete;

    GdiBitmap(GdiBitmap&& other) noexcept : m_hbm(other.m_hbm)
    {
        other.m_hbm = NULL;
    }

    HBITMAP get() const { return m_hbm; }
    operator HBITMAP() const { return m_hbm; }

private:
    HBITMAP m_hbm;
};

// 使用
GdiBitmap hbm(CreateCompatibleBitmap(hdc, 100, 100));
// 自动释放，不用担心
```

### 色键错误

`TransparentBlt` 的 `crTransparent` 参数使用 `RGB` 格式，但很容易搞错：

```cpp
// 错误：直接使用十六进制颜色值
// 0xFF00FF 在 RGB 宏中是 RGB(0, 255, 255)（青色），不是洋红色！
TransparentBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, w, h, 0xFF00FF);

// 正确：使用 RGB 宏
TransparentBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, w, h, RGB(255, 0, 255));
```

`RGB` 宏的定义是 `((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))`，结果的字节顺序是 BBGGRR（蓝-绿-红）。

### 多显示器问题

在多显示器系统中，源 DC 和目标 DC 必须属于同一个设备，否则 `BitBlt` 会失败：

```cpp
// 检查 DC 是否属于同一设备
HDC hdcScreen1 = GetDC(NULL);
HDC hdcScreen2 = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);

// 这两个 DC 可能属于不同的显示器
// BitBlt(hdcScreen1, 0, 0, 100, 100, hdcScreen2, 0, 0, SRCCOPY);
// 可能失败！

// 解决方案：使用 DIB 作为中间格式
```

解决方案是使用 `GetDIBits` 和 `SetDIBits` 转换为设备无关格式：

```cpp
// 从源 DC 获取 DIB 数据
HBITMAP hbmSource = ...;
int width = ..., height = ...;

// 分配缓冲区
int dataSize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
BYTE* pBuffer = new BYTE[dataSize + width * height * 4];

BITMAPINFO* pBI = (BITMAPINFO*)pBuffer;
pBI->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
pBI->bmiHeader.biWidth = width;
pBI->bmiHeader.biHeight = height;
pBI->bmiHeader.biPlanes = 1;
pBI->bmiHeader.biBitCount = 32;
pBI->bmiHeader.biCompression = BI_RGB;

// 获取位图数据
GetDIBits(hdcSrc, hbmSource, 0, height,
          pBuffer + dataSize, pBI, DIB_RGB_COLORS);

// 在目标 DC 上设置位图数据
SetDIBits(hdcDest, hbmDest, 0, height,
          pBuffer + dataSize, pBI, DIB_RGB_COLORS);

delete[] pBuffer;
```

---

## 后续可以做什么

到这里，GDI 位图操作的核心内容就讲完了。你现在应该能够：
- 理解 DDB 和 DIB 的区别
- 正确使用兼容 DC 和内存位图
- 熟练使用 `BitBlt` 和各种光栅操作码
- 用 `StretchBlt` 实现缩放
- 用 `TransparentBlt` 实现透明效果
- 用 `AlphaBlend` 实现半透明混合
- 正确加载和管理位图资源
- 避免常见的内存泄漏问题

但这些只是基础，Win32 图形编程还有更多内容值得探索：

1. **DIB Section 深入**：直接访问和修改像素数据
2. **区域（Region）操作**：用 `CreateRectRgn`、`CombineRgn` 等实现复杂裁剪
3. **路径（Path）**：用 `BeginPath`、`EndPath` 实现复杂图形绘制
4. **元文件（Metafile）**：记录和回放绘图命令
5. **WIC（Windows Imaging Component）**：加载 PNG、JPEG 等现代图像格式
6. **Direct2D**：Windows 7+ 的现代图形 API，硬件加速

建议你先做一些练习巩固一下：
1. 实现一个完整的图片查看器，支持缩放、平移
2. 做一个简单的图像滤镜：灰度化、反色、亮度调整
3. 实现一个带透明背景的精灵动画系统
4. 用双缓冲技术消除闪烁

下一步，我们可以探讨 DIB Section 的深入使用，或者进入 Windows Imaging Component 的世界，学习如何加载现代图像格式。

---

## 相关资源

- [BitBlt function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt)
- [StretchBlt function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchblt)
- [TransparentBlt function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-transparentblt)
- [AlphaBlend function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-alphablend)
- [CreateDIBSection function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection)
- [Raster Operation Codes - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/raster-operation-codes)
- [LoadImage function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagea)
- [Memory Device Contexts - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/memory-device-contexts)
