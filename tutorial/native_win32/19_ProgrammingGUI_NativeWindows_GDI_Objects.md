# 通用GUI编程技术——Win32 原生编程实战（十九）——GDI 绘图对象：画笔、画刷、字体与区域

> 在上一篇文章里，我们深入探讨了 HDC 的本质和使用方式，了解了如何获取设备上下文以及正确管理 DC 状态。但 HDC 只是一个"画布的句柄"，真正决定你画出来的是什么样子的，是选入 DC 的各种 GDI 对象——画笔决定线条的样式，画刷决定填充的颜色，字体决定文字的外观，区域决定绘图的裁剪范围。今天我们要聊的就是这些让无数新手头疼的 GDI 对象，把它们的使用方法、生命周期管理、常见陷阱彻底讲清楚。

---

## 前言：为什么需要单独讲 GDI 对象

说实话，刚开始学 Win32 绘图的时候，我对 GDI 对象这套机制是有抵触的。为什么画一条线要先创建画笔，选入 DC，用完还要恢复，最后还要删除？直接告诉 API "用红色画线"不就行了吗？这种"创建-选入-使用-恢复-删除"的繁琐流程，让很多习惯了现代图形库的朋友望而却步。

但慢慢你就会发现，这套看似繁琐的设计背后，有着非常精妙的考虑。GDI 对象是"可复用的绘图属性模板"。你创建一次红色画笔，可以在多个 DC 上使用；你定义好一种字体样式，可以在任何地方复用。更重要的是，这套设计让 Windows 能够高效地管理绘图资源——系统只需要维护一份画笔定义，多个 DC 可以共享它。

另一个让新手抓狂的问题是 GDI 对象的生命周期管理。选入 DC 的对象不能删除，忘记恢复旧对象会导致资源泄漏，库存对象不能删除……这些规则如果理解不透，写出来的程序表面上能跑，但在某些情况下会悄悄泄漏资源，直到达到 GDI 对象上限导致整个程序崩溃。

这篇文章会从 GDI 对象的通用模型讲起，然后分别深入画笔、画刷、字体和区域这四类对象。我们不只是知道"怎么用"，更重要的是理解"为什么要这么设计"，以及"如何避免常见陷阱"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 2000+ 都支持）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉上一篇文章关于 HDC 的内容——知道怎么获取设备上下文、理解 DC 状态管理、明白 BeginPaint/EndPaint 的正确用法。如果这些概念对你来说还比较陌生，建议先去看看上一篇文章。

---

## 第一步——GDI 对象的通用模型

### HGDIOBJ 类型体系

所有 GDI 对象的句柄都可以用 HGDIOBJ 类型表示。HGDIOBJ 是一个通用的句柄类型，具体可以是 HPEN（画笔）、HBRUSH（画刷）、HFONT（字体）、HRGN（区域）、HBITMAP（位图）、HPALETTE（调色板）等等。

```cpp
typedef void* HANDLE;
typedef HANDLE HGDIOBJ;
typedef HGDIOBJ HPEN;
typedef HGDIOBJ HBRUSH;
typedef HGDIOBJ HFONT;
typedef HGDIOBJ HRGN;
```

这种统一的设计让 SelectObject 这样的函数可以处理多种类型的对象。SelectObject 的签名是：

```cpp
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ ho);
```

无论你传入的是画笔、画刷还是字体，SelectObject 都能正确处理，并返回同类型的旧对象。

### 创建-选入-使用-销毁的完整流程

所有非库存的 GDI 对象都遵循同样的生命周期模式：

```cpp
// 1. 创建对象
HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));

// 2. 选入 DC，保存旧对象
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

// 3. 使用对象进行绘图
Rectangle(hdc, 10, 10, 100, 100);

// 4. 恢复旧对象
SelectObject(hdc, hOldPen);

// 5. 销毁对象
DeleteObject(hPen);
```

⚠️ 注意

千万别跳过第 4 步直接 DeleteObject。当对象被选入 DC 时，DC 内部持有这个对象的引用。如果你在对象还被选入的情况下调用 DeleteObject，删除操作会失败（或者返回一个特殊的"待删除"标记），对象实际上不会被释放，直到它从所有 DC 中被选出。这意味着你的代码会泄漏资源，而且这种泄漏非常隐蔽——程序表面上正常运行，但 GDI 对象计数会不断增长。

一个常见的错误模式是：

```cpp
// 错误示例
HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
SelectObject(hdc, hPen);
// ... 绘图 ...
DeleteObject(hPen);  // 错误！hPen 还在被 DC 使用
```

正确的做法永远是先恢复旧对象，再删除新对象：

```cpp
// 正确示例
HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
// ... 绘图 ...
SelectObject(hdc, hOldPen);  // 先恢复
DeleteObject(hPen);           // 再删除
```

### 库存对象：不需要创建也不需要删除

Windows 提供了一组预定义的"库存对象"（Stock Objects），它们由系统管理，不需要你创建，也不需要删除。常用的库存对象包括：

```cpp
HGDIOBJ GetStockObject(int i);
```

库存对象列表：

| 对象类型 | 常量 | 用途 |
|---------|------|------|
| 画刷 | WHITE_BRUSH | 白色画刷 |
| 画刷 | BLACK_BRUSH | 黑色画刷 |
| 画刷 | GRAY_BRUSH / DKGRAY_BRUSH / LTGRAY_BRUSH | 灰度画刷 |
| 画刷 | HOLLOW_BRUSH / NULL_BRUSH | 空心画刷（不填充） |
| 画刷 | DC_BRUSH | 可通过 SetDCBrushColor 改变颜色的画刷 |
| 画笔 | WHITE_PEN | 白色画笔 |
| 画笔 | BLACK_PEN | 黑色画笔 |
| 画笔 | NULL_PEN | 空笔（不画线） |
| 画笔 | DC_PEN | 可通过 SetDCPenColor 改变颜色的画笔 |
| 字体 | SYSTEM_FONT | 系统字体 |
| 字体 | DEFAULT_GUI_FONT | 默认 GUI 字体 |
| 字体 | ANSI_FIXED_FONT | ANSI 等宽字体 |
| 字体 | ANSI_VAR_FONT | ANSI 比例字体 |

使用库存对象的好处是你不需要管理它们的生命周期：

```cpp
// 使用库存空心画刷绘制只带边框的矩形
SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
Rectangle(hdc, 10, 10, 100, 100);
// 不需要调用 DeleteObject
```

⚠️ 注意

千万别 DeleteObject 库存对象！虽然 DeleteObject 不会立即崩溃（库存对象使用引用计数，DeleteObject 只是减少引用），但这样做是错误的，而且会导致难以追踪的 bug。库存对象是全局共享的，你的删除操作会影响整个系统的绘图行为。

---

## 第二步——画笔（HPEN）详解

画笔决定线条的样式、宽度和颜色。它是 GDI 绘图中最基础的对象之一。

### CreatePen：创建基本画笔

CreatePen 是最常用的画笔创建函数：

```cpp
HPEN CreatePen(
    int      iStyle,    // 画笔样式
    int      cWidth,    // 画笔宽度
    COLORREF color      // 画笔颜色
);
```

画笔样式决定了线条的外观：

| 样式 | 效果 | 宽度限制 |
|------|------|----------|
| PS_SOLID | 实线 | 任意 |
| PS_DASH | 虚线 | 必须为 1 |
| PS_DOT | 点线 | 必须为 1 |
| PS_DASHDOT | 点划线 | 必须为 1 |
| PS_DASHDOTDOT | 双点划线 | 必须为 1 |
| PS_NULL | 空笔（不绘制） | 任意 |
| PS_INSIDEFRAME | 实线，但绘制在矩形内部 | 任意 |

⚠️ 注意

千万别给虚线、点线样式设置大于 1 的宽度。如果你这样做，CreatePen 会返回一个 NULL 句柄，或者创建一个实线画笔（取决于 Windows 版本）。正确的做法是：对于虚线样式，宽度必须为 1；如果需要粗虚线，使用 ExtCreatePen。

一个典型的画笔使用示例：

```cpp
// 创建一支蓝色实线画笔，宽度为 3
HPEN hPenBlue = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));
HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBlue);

// 绘制一系列线条
for (int i = 0; i < 10; i++) {
    MoveToEx(hdc, 10, i * 20 + 10, NULL);
    LineTo(hdc, 300, i * 20 + 10);
}

// 恢复和清理
SelectObject(hdc, hOldPen);
DeleteObject(hPenBlue);
```

### CreatePenIndirect：通过结构体创建

如果你需要通过结构体来定义画笔属性，可以使用 CreatePenIndirect：

```cpp
HPEN CreatePenIndirect(const LOGPEN *plpen);
```

LOGPEN 结构体定义：

```cpp
typedef struct tagLOGPEN {
    UINT     lopnStyle;   // 画笔样式（同 CreatePen 的 iStyle）
    POINT    lopnWidth;   // 画笔宽度（只使用 x 成员）
    COLORREF lopnColor;   // 画笔颜色
} LOGPEN, *PLOGPEN;
```

使用示例：

```cpp
LOGPEN logpen = {};
logpen.lopnStyle = PS_DASH;
logpen.lopnWidth.x = 1;  // 宽度必须为 1（因为是虚线）
logpen.lopnColor = RGB(128, 128, 128);

HPEN hPen = CreatePenIndirect(&logpen);
HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

// 使用画笔...

SelectObject(hdc, hOldPen);
DeleteObject(hPen);
```

### ExtCreatePen：高级画笔特性

ExtCreatePen 是 CreatePen 的扩展版本，支持更多高级特性，比如自定义端点样式、连接样式、以及使用画刷作为画笔颜色。

```cpp
HPEN ExtCreatePen(
    DWORD          iPenStyle,    // 扩展画笔样式
    DWORD          cWidth,       // 画笔宽度
    const LOGBRUSH *plbrush,     // 画刷信息
    DWORD          cStyle,       // 用户样式数组长度
    const DWORD    *pstyle       // 用户样式数组
);
```

iPenStyle 是多个标志的组合，可以用位或运算连接：

**画笔类型（必须指定一个）：**

| 标志 | 含义 |
|------|------|
| PS_GEOMETRIC | 几何画笔（宽度任意，可以是画刷） |
| PS_COSMETIC | 装饰画笔（宽度必须为 1，必须是纯色） |

**画笔样式（可选）：**

| 标志 | 含义 |
|------|------|
| PS_SOLID | 实线 |
| PS_DASH | 虚线 |
| PS_DOT | 点线 |
| PS_DASHDOT | 点划线 |
| PS_DASHDOTDOT | 双点划线 |
| PS_NULL | 空笔 |
| PS_USERSTYLE | 用户自定义样式 |
| PS_ALTERNATE | 每隔一个像素设置一个像素（仅装饰画笔） |
| PS_INSIDEFRAME | 实线在矩形内部绘制 |

**端点样式（仅几何画笔）：**

| 标志 | 含义 |
|------|------|
| PS_ENDCAP_ROUND | 圆形端点（默认） |
| PS_ENDCAP_SQUARE | 方形端点（延伸半个线宽） |
| PS_ENDCAP_FLAT | 平端点（不延伸） |

**连接样式（仅几何画笔）：**

| 标志 | 含义 |
|------|------|
| PS_JOIN_ROUND | 圆角连接（默认） |
| PS_JOIN_BEVEL | 斜角连接 |
| PS_JOIN_MITER | 尖角连接（受 miter limit 限制） |

一个使用 ExtCreatePen 的例子，创建一支带有圆角端点和连接的粗画笔：

```cpp
// 创建一支粗画笔，圆角端点和连接
LOGBRUSH lb = {};
lb.lbStyle = BS_SOLID;
lb.lbColor = RGB(255, 0, 0);

HPEN hPen = ExtCreatePen(
    PS_GEOMETRIC | PS_SOLID | PS_JOIN_ROUND | PS_ENDCAP_ROUND,
    10,          // 宽度 10
    &lb,         // 红色实心画刷
    0, NULL      // 不使用用户样式
);

HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

// 绘制一个带圆角的路径
POINT points[] = {
    {50, 50}, {150, 50}, {200, 100}, {150, 150}, {50, 150}
};
Polyline(hdc, points, 5);

SelectObject(hdc, hOldPen);
DeleteObject(hPen);
```

### 用户自定义虚线样式

使用 PS_USERSTYLE 可以完全自定义虚线的模式：

```cpp
// 自定义虚线样式：长划-短空-短划-长空
DWORD dashStyle[] = {10, 3, 3, 10};

LOGBRUSH lb = {};
lb.lbStyle = BS_SOLID;
lb.lbColor = RGB(0, 128, 0);

HPEN hPen = ExtCreatePen(
    PS_GEOMETRIC | PS_USERSTYLE,
    3,
    &lb,
    4,              // 样式数组长度
    dashStyle       // 样式数组
);
```

样式数组的含义是：第一个值是第一条实线的长度，第二个值是第一个空隙的长度，第三个值是第二条实线的长度，第四个值是第二个空隙的长度，依此类推。数组长度最多为 16。

⚠️ 注意

千万别让样式数组长度超过 16。ExtCreatePen 会失败并返回 NULL。如果你需要更复杂的模式，考虑使用多次分段绘制。

---

## 第三步——画刷（HBRUSH）详解

画刷决定封闭图形的填充方式。它可以是纯色、阴影线、或者位图图案。

### CreateSolidBrush：创建纯色画刷

这是最常用的画刷创建函数：

```cpp
HBRUSH CreateSolidBrush(COLORREF crColor);
```

使用示例：

```cpp
// 创建一个纯蓝色画刷
HBRUSH hBrushBlue = CreateSolidBrush(RGB(0, 0, 255));
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrushBlue);

// 绘制填充的矩形
Rectangle(hdc, 10, 10, 200, 150);

// 恢复和清理
SelectObject(hdc, hOldBrush);
DeleteObject(hBrushBlue);
```

### CreateHatchBrush：创建阴影画刷

阴影画刷使用重复的线条图案填充：

```cpp
HBRUSH CreateHatchBrush(
    int      iHatch,    // 阴影样式
    COLORREF crColor    // 线条颜色
);
```

阴影样式：

| 样式 | 效果 |
|------|------|
| HS_HORIZONTAL | 水平线 |
| HS_VERTICAL | 垂直线 |
| HS_FDIAGONAL | 45度向下斜线（左上到右下） |
| HS_BDIAGONAL | 45度向上斜线（左下到右上） |
| HS_CROSS | 水平和垂直交叉线 |
| HS_DIAGCROSS | 45度交叉线 |

使用示例：

```cpp
// 创建一个交叉阴影画刷
HBRUSH hBrushHatch = CreateHatchBrush(HS_DIAGCROSS, RGB(128, 64, 0));
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrushHatch);

// 设置背景模式，让空隙处显示背景色
SetBkColor(hdc, RGB(255, 255, 240));
SetBkMode(hdc, OPAQUE);

// 绘制填充的椭圆
Ellipse(hdc, 50, 50, 300, 200);

SelectObject(hdc, hOldBrush);
DeleteObject(hBrushHatch);
```

⚠️ 注意

千万别忘记设置背景色。阴影画刷的空隙处会使用当前背景色填充。如果你不设置，空隙会保持透明（在 OPAQUE 模式下）或者显示之前的内容（在 TRANSPARENT 模式下）。

### CreatePatternBrush：创建图案画刷

图案画刷使用位图作为填充图案：

```cpp
HBRUSH CreatePatternBrush(HBITMAP hbmp);
```

使用示例：

```cpp
// 创建一个小的位图作为图案
HBITMAP hbmPattern = CreateBitmap(8, 8, 1, 1, NULL);
// ... 填充位图数据 ...

HBRUSH hBrushPattern = CreatePatternBrush(hbmPattern);
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrushPattern);

// 绘制填充的矩形
Rectangle(hdc, 10, 10, 200, 150);

SelectObject(hdc, hOldBrush);
DeleteObject(hBrushPattern);
DeleteObject(hbmPattern);  // 创建画刷后可以删除位图
```

### CreateBrushIndirect：通过结构体创建

与 CreatePenIndirect 类似，你可以通过 LOGBRUSH 结构体创建画刷：

```cpp
HBRUSH CreateBrushIndirect(const LOGBRUSH *plbrush);
```

LOGBRUSH 结构体定义：

```cpp
typedef struct tagLOGBRUSH {
    UINT      lbStyle;     // 画刷样式
    COLORREF  lbColor;     // 画刷颜色
    ULONG_PTR lbHatch;     // 阴影样式或位图句柄
} LOGBRUSH, *PLOGBRUSH;
```

画刷样式（lbStyle）：

| 样式 | 含义 | lbColor | lbHatch |
|------|------|---------|---------|
| BS_SOLID | 纯色 | 颜色 | 忽略 |
| BS_HOLLOW / BS_NULL | 空心（不填充） | 忽略 | 忽略 |
| BS_HATCHED | 阴影 | 颜色 | HS_* 常量 |
| BS_PATTERN | 图案 | 忽略 | 位图句柄 |
| BS_DIBPATTERN | DIB 图案 | DIB_RGB_COLORS 或 DIB_PAL_COLORS | 全局内存句柄 |
| BS_DIBPATTERNPT | DIB 图案（指针） | DIB_RGB_COLORS 或 DIB_PAL_COLORS | 指针 |

使用示例：

```cpp
LOGBRUSH lb = {};
lb.lbStyle = BS_HATCHED;
lb.lbColor = RGB(0, 128, 255);
lb.lbHatch = HS_CROSS;

HBRUSH hBrush = CreateBrushIndirect(&lb);
// ... 使用画刷 ...
DeleteObject(hBrush);
```

### GetSysColorBrush：获取系统颜色画刷

Windows 提供了一组预定义的系统颜色画刷，对应界面各种元素的颜色：

```cpp
HBRUSH GetSysColorBrush(int nIndex);
```

常用的 nIndex 值：

| 值 | 用途 |
|----|------|
| COLOR_WINDOW | 窗口背景 |
| COLOR_WINDOWTEXT | 窗口文字 |
| COLOR_BTNFACE | 按钮表面 |
| COLOR_BTNTEXT | 按钮文字 |
| COLOR_HIGHLIGHT | 高亮背景 |
| COLOR_HIGHLIGHTTEXT | 高亮文字 |

使用示例：

```cpp
// 使用系统窗口背景色画刷
HBRUSH hBrush = GetSysColorBrush(COLOR_WINDOW);
// ... 使用 ...
// 注意：不需要删除 GetSysColorBrush 返回的对象
```

⚠️ 注意

千万别 DeleteObject GetSysColorBrush 返回的画刷！这些是系统管理的对象，删除它们会导致系统行为异常。

---

## 第四步——字体（HFONT）详解

字体决定了文本的外观，包括字样、大小、粗细、样式等。

### LOGFONT 结构体详解

创建字体最常用的方式是使用 CreateFontIndirect，它接受一个 LOGFONT 结构体：

```cpp
HFONT CreateFontIndirect(const LOGFONT *lplf);
```

LOGFONT 结构体非常复杂，让我们逐个字段理解：

```cpp
typedef struct tagLOGFONTA {
    LONG  lfHeight;           // 字体高度（负数表示字符高度）
    LONG  lfWidth;            // 字符平均宽度（0 表示默认）
    LONG  lfEscapement;       // 文本行角度（1/10 度）
    LONG  lfOrientation;      // 字符基线角度（1/10 度）
    LONG  lfWeight;           // 字体粗细（0-1000，400=正常，700=粗体）
    BYTE  lfItalic;           // 是否斜体
    BYTE  lfUnderline;        // 是否下划线
    BYTE  lfStrikeOut;        // 是否删除线
    BYTE  lfCharSet;          // 字符集
    BYTE  lfOutPrecision;     // 输出精度
    BYTE  lfClipPrecision;    // 裁剪精度
    BYTE  lfQuality;          // 输出质量
    BYTE  lfPitchAndFamily;   // 字距和字体族
    CHAR  lfFaceName[LF_FACESIZE];  // 字体名称
} LOGFONTA, *PLOGFONTA, *NPLOGFONTA, *LPLOGFONTA;
```

### lfHeight 的正负之谜

lfHeight 是最容易让人困惑的字段。它的含义取决于正负：

* **lfHeight > 0**：指定"字符单元格高度"（包括上升部、下降部和内部行距）。字体会缩放到不超过这个高度的最大可能高度。
* **lfHeight = 0**：使用默认高度。
* **lfHeight < 0**：指定"字符高度"（不包括内部行距）。绝对值被当作字符高度匹配。

⚠️ 注意

千万别用正数 lfHeight 来精确控制字体大小！如果你想要特定点数的字体，应该使用负数。计算公式如下：

```cpp
// 指定点数转换为 lfHeight（负数）
int LogFontHeightFromPointSize(int pointSize, HDC hdc)
{
    return -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
}

// 使用示例
LOGFONT lf = {};
lf.lfHeight = LogFontHeightFromPointSize(12, hdc);  // 12 点字体
lf.lfWeight = FW_NORMAL;
wcscpy_s(lf.lfFaceName, L"Arial");

HFONT hFont = CreateFontIndirect(&lf);
```

### lfWeight：字体粗细

lfWeight 控制字体的粗细，范围从 0 到 1000。常用预定义值：

| 常量 | 值 | 效果 |
|------|----|----|
| FW_THIN | 100 | 极细 |
| FW_EXTRALIGHT | 200 | 特细 |
| FW_LIGHT | 300 | 细体 |
| FW_NORMAL | 400 | 正常 |
| FW_MEDIUM | 500 | 中等 |
| FW_SEMIBOLD | 600 | 半粗 |
| FW_BOLD | 700 | 粗体 |
| FW_EXTRABOLD | 800 | 特粗 |
| FW_HEAVY | 900 | 极粗 |

### lfCharSet：字符集

字符集决定了字体使用的字符编码集。常用的值：

| 常量 | 用途 |
|------|------|
| ANSI_CHARSET | ANSI 字符集（西欧语言） |
| DEFAULT_CHARSET | 默认字符集（根据系统区域设置） |
| SYMBOL_CHARSET | 符号字符集 |
| SHIFTJIS_CHARSET | 日文 Shift-JIS |
| GB2312_CHARSET | 简体中文 |
| HANGUL_CHARSET | 韩文 |
| OEM_CHARSET | OEM 字符集 |

⚠️ 注意

千万别使用 DEFAULT_CHARSET 来匹配字体！如果你指定 lfFaceName 为具体字体名，但 lfCharSet 为 DEFAULT_CHARSET，字体匹配可能会失败。正确的做法是：指定与字体名对应的字符集，对于中文使用 GB2312_CHARSET。

### lfQuality：字体质量

lfQuality 控制字体渲染的质量：

| 常量 | 效果 |
|------|------|
| DEFAULT_QUALITY | 默认质量（不关心外观） |
| DRAFT_QUALITY | 草稿质量（优先性能） |
| PROOF_QUALITY | 证明质量（优先精确性） |
| NONANTIALIASED_QUALITY | 不抗锯齿 |
| ANTIALIASED_QUALITY | 抗锯齿 |
| CLEARTYPE_QUALITY | ClearType 抗锯齿 |

对于现代应用程序，推荐使用 CLEARTYPE_QUALITY 或 ANTIALIASED_QUALITY 以获得更好的显示效果。

### lfPitchAndFamily：字距和字体族

这个字段的低 2 位控制字距，高 4 位控制字体族：

**字距（低 2 位）：**

| 常量 | 含义 |
|------|------|
| DEFAULT_PITCH | 默认字距 |
| FIXED_PITCH | 固定字距（等宽字体） |
| VARIABLE_PITCH | 可变字距（比例字体） |

**字体族（高 4 位）：**

| 常量 | 示例 |
|------|------|
| FF_DONTCARE | 不关心 |
| FF_ROMAN | Times New Roman（比例，有衬线） |
| FF_SWISS | Arial（比例，无衬线） |
| FF_MODERN | Courier New（等宽） |
| FF_SCRIPT | 手写体风格 |
| FF_DECORATIVE | 装饰体风格 |

组合示例：

```cpp
lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;  // 等宽现代字体
```

### CreateFont：简化创建函数

如果你不想填充整个 LOGFONT 结构体，可以使用 CreateFont：

```cpp
HFONT CreateFont(
    int      cHeight,            // 高度
    int      cWidth,             // 宽度
    int      cEscapement,        // 旋转角度
    int      cOrientation,       // 方向角度
    int      cWeight,            // 粗细
    DWORD    bItalic,            // 是否斜体
    DWORD    bUnderline,         // 是否下划线
    DWORD    bStrikeOut,         // 是否删除线
    DWORD    iCharSet,           // 字符集
    DWORD    iOutPrecision,      // 输出精度
    DWORD    iClipPrecision,     // 裁剪精度
    DWORD    iQuality,           // 质量
    DWORD    iPitchAndFamily,    // 字距和字体族
    LPCWSTR  pszFaceName         // 字体名
);
```

使用示例：

```cpp
HFONT hFont = CreateFont(
    -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72),  // 12 点
    0,                         // 默认宽度
    0,                         // 不旋转
    0,                         // 不倾斜
    FW_BOLD,                   // 粗体
    TRUE,                      // 斜体
    FALSE,                     // 不下划线
    FALSE,                     // 不删除线
    ANSI_CHARSET,              // ANSI 字符集
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY,         // ClearType
    DEFAULT_PITCH | FF_SWISS,
    L"Arial"
);
```

### 字体使用示例

```cpp
// 创建 12 号 Arial 粗体字
LOGFONT lf = {};
lf.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
lf.lfWeight = FW_BOLD;
lf.lfCharSet = ANSI_CHARSET;
lf.lfQuality = CLEARTYPE_QUALITY;
wcscpy_s(lf.lfFaceName, L"Arial");

HFONT hFont = CreateFontIndirect(&lf);
HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

// 设置文本颜色
SetTextColor(hdc, RGB(0, 0, 128));
SetBkMode(hdc, TRANSPARENT);

// 绘制文本
TextOut(hdc, 10, 10, L"Hello, GDI Fonts!", 18);

// 恢复和清理
SelectObject(hdc, hOldFont);
DeleteObject(hFont);
```

---

## 第五步——区域（HRGN）详解

区域是 GDI 中一个强大但经常被忽视的概念。它本质上是一个"形状描述"，可以用于裁剪绘图、检测点击、或者创建不规则窗口。

### 创建基本区域

Windows 提供了几个函数来创建基本形状的区域：

```cpp
// 矩形区域
HRGN CreateRectRgn(int x1, int y1, int x2, int y2);
HRGN CreateRectRgnIndirect(const RECT *lprc);

// 椭圆区域
HRGN CreateEllipticRgn(int x1, int y1, int x2, int y2);
HRGN CreateEllipticRgnIndirect(const RECT *lprc);

// 多边形区域
HRGN CreatePolygonRgn(const POINT *pptl, int cPoints, int iPolyFillMode);

// 圆角矩形区域
HRGN CreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h);
```

使用示例：

```cpp
// 创建一个矩形区域
HRGN hRgnRect = CreateRectRgn(10, 10, 200, 150);

// 创建一个椭圆区域
HRGN hRgnEllipse = CreateEllipticRgn(50, 50, 300, 250);

// 创建一个多边形区域（三角形）
POINT triangle[] = {
    {100, 10},
    {150, 110},
    {50, 110}
};
HRGN hRgnTriangle = CreatePolygonRgn(triangle, 3, ALTERNATE);

// 使用完后记得删除
DeleteObject(hRgnRect);
DeleteObject(hRgnEllipse);
DeleteObject(hRgnTriangle);
```

### CombineRgn：组合区域

CombineRgn 可以把两个区域组合成一个新的区域，支持多种布尔运算：

```cpp
int CombineRgn(
    HRGN hrgnDst,      // 目标区域（会被修改）
    HRGN hrgnSrc1,     // 源区域1
    HRGN hrgnSrc2,     // 源区域2
    int  iMode         // 组合模式
);
```

组合模式：

| 模式 | 效果 | 数学表示 |
|------|------|----------|
| RGN_AND | 交集 | A AND B |
| RGN_OR | 并集 | A OR B |
| RGN_XOR | 异或 | (A OR B) AND NOT (A AND B) |
| RGN_DIFF | 差集 | A AND NOT B |
| RGN_COPY | 复制 | A |

使用示例：

```cpp
// 创建两个矩形区域
HRGN hRgn1 = CreateRectRgn(10, 10, 100, 100);
HRGN hRgn2 = CreateRectRgn(50, 50, 150, 150);

// 创建目标区域
HRGN hRgnResult = CreateRectRgn(0, 0, 1, 1);

// 计算并集
CombineRgn(hRgnResult, hRgn1, hRgn2, RGN_OR);

// 计算交集
CombineRgn(hRgnResult, hRgn1, hRgn2, RGN_AND);

// 计算差集（hRgn1 减去 hRgn2）
CombineRgn(hRgnResult, hRgn1, hRgn2, RGN_DIFF);

// 清理
DeleteObject(hRgn1);
DeleteObject(hRgn2);
DeleteObject(hRgnResult);
```

⚠️ 注意

千万别让 hrgnDst 与 hrgnSrc1 或 hrgnSrc2 相同！如果它们是同一个区域，结果未定义。如果你希望"原地"修改一个区域，需要创建一个临时区域：

```cpp
// 错误示例
CombineRgn(hRgn1, hRgn1, hRgn2, RGN_AND);  // 危险！

// 正确示例
HRGN hRgnTemp = CreateRectRgn(0, 0, 1, 1);
CombineRgn(hRgnTemp, hRgn1, hRgn2, RGN_AND);
DeleteObject(hRgn1);
hRgn1 = hRgnTemp;
```

### OffsetRgn：移动区域

OffsetRgn 可以平移一个区域：

```cpp
int OffsetRgn(
    HRGN hrgn,
    int  dx,     // X 偏移
    int  dy      // Y 偏移
);
```

使用示例：

```cpp
HRGN hRgn = CreateRectRgn(10, 10, 100, 100);

// 向右移动 50 像素，向下移动 30 像素
OffsetRgn(hRgn, 50, 30);

// 现在 hRgn 是 (60, 40, 150, 130)
```

### 区域在裁剪中的应用

使用 SelectClipRgn 可以将区域选入 DC 作为裁剪区域：

```cpp
int SelectClipRgn(HDC hdc, HRGN hrgn);
```

使用示例：

```cpp
// 创建一个椭圆裁剪区域
HRGN hRgnClip = CreateEllipticRgn(50, 50, 350, 250);

// 选入 DC
SelectClipRgn(hdc, hRgnClip);

// 现在所有绘图都会被裁剪到椭圆内
for (int i = 0; i < 20; i++) {
    MoveToEx(hdc, 0, i * 15, NULL);
    LineTo(hdc, 400, i * 15);
}

// 恢复无裁剪
SelectClipRgn(hdc, NULL);

DeleteObject(hRgnClip);
```

⚠️ 注意

千万别忘记恢复裁剪区域！如果你设置了裁剪区域后不恢复，后续所有绘图都会被裁剪，可能导致很多奇怪的 bug。使用 SelectClipRgn(hdc, NULL) 可以清除裁剪。

### PtInRegion 和 RectInRegion：命中测试

区域常用于检测点或矩形是否在某个形状内：

```cpp
BOOL PtInRegion(HRGN hrgn, int x, int y);
BOOL RectInRegion(HRGN hrgn, const RECT *lprect);
```

使用示例：

```cpp
// 创建一个多边形区域（五角星的大致形状）
POINT star[] = {
    {100, 10}, {120, 80}, {190, 80}, {130, 120},
    {150, 190}, {100, 140}, {50, 190}, {70, 120},
    {10, 80}, {80, 80}
};
HRGN hRgnStar = CreatePolygonRgn(star, 10, ALTERNATE);

// 检测点击
case WM_LBUTTONDOWN:
{
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    if (PtInRegion(hRgnStar, x, y)) {
        MessageBox(hwnd, L"点击了星星！", L"命中测试", MB_OK);
    }
    break;
}

// 清理
DeleteObject(hRgnStar);
```

### 区域用于不规则窗口

你还可以用区域来创建不规则形状的窗口：

```cpp
// 创建一个椭圆窗口
HRGN hRgn = CreateEllipticRgn(0, 0, 400, 300);
SetWindowRgn(hwnd, hRgn, TRUE);
// 注意：调用 SetWindowRgn 后，系统会接管这个区域，不需要 DeleteObject
```

---

## 第六步——实战示例：绘制一个带复杂样式的图形

让我们把今天学到的知识整合成一个完整的示例。这个程序会绘制一个带有复杂边框、填充图案和文字标注的图形。

```cpp
#include <windows.h>

// 窗口类名
static const wchar_t g_szClassName[] = L"GDIObjectsDemo";

// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 绘制复杂图形
void DrawComplexShape(HDC hdc, int x, int y)
{
    // 保存 DC 状态
    int savedDC = SaveDC(hdc);

    // 1. 创建带圆角端点的粗画笔
    LOGBRUSH lb = { BS_SOLID, RGB(0, 100, 200) };
    HPEN hPenOutline = ExtCreatePen(
        PS_GEOMETRIC | PS_SOLID | PS_JOIN_ROUND | PS_ENDCAP_ROUND,
        5,
        &lb,
        0, NULL
    );
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPenOutline);

    // 2. 创建阴影画刷
    HBRUSH hBrushHatch = CreateHatchBrush(HS_DIAGCROSS, RGB(200, 200, 200));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrushHatch);

    // 设置背景色
    SetBkColor(hdc, RGB(240, 240, 240));

    // 3. 绘制圆角矩形
    RoundRect(hdc, x, y, x + 300, y + 200, 30, 30);

    // 4. 创建裁剪区域（圆角矩形内部）
    HRGN hRgnClip = CreateRoundRectRgn(x, y, x + 300, y + 200, 30, 30);
    SelectClipRgn(hdc, hRgnClip);

    // 5. 在裁剪区域内绘制斜线纹理
    HPEN hPenTexture = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hPenTexture);
    for (int i = -200; i < 300; i += 15) {
        MoveToEx(hdc, x + i, y, NULL);
        LineTo(hdc, x + i + 200, y + 200);
    }

    // 6. 恢复画笔和清除裁剪
    SelectObject(hdc, hOldPen);
    SelectClipRgn(hdc, NULL);

    // 7. 创建字体
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(16, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = ANSI_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT hFont = CreateFontIndirect(&lf);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // 8. 绘制文字
    SetTextColor(hdc, RGB(0, 50, 100));
    SetBkMode(hdc, TRANSPARENT);

    RECT rcText = { x + 20, y + 80, x + 280, y + 120 };
    DrawText(hdc, L"GDI Objects Demo", -1, &rcText,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT rcSubText = { x + 20, y + 110, x + 280, y + 140 };
    DrawText(hdc, L"Pen, Brush, Font, Region", -1, &rcSubText,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 9. 清理所有对象
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hPenOutline);
    DeleteObject(hPenTexture);
    DeleteObject(hBrushHatch);
    DeleteObject(hRgnClip);

    // 恢复 DC 状态
    RestoreDC(hdc, savedDC);
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"窗口类注册失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowEx(
        0, g_szClassName, L"GDI 对象演示",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"窗口创建失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        DrawComplexShape(hdc, 50, 50);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
```

这个示例展示了：
1. 使用 ExtCreatePen 创建自定义画笔
2. 使用阴影画刷填充图形
3. 使用区域进行裁剪
4. 使用 LOGFONT 创建自定义字体
5. 正确的 GDI 对象生命周期管理
6. SaveDC/RestoreDC 的使用

---

## 第七步——性能提示：系统对象 vs 自定义对象

在实际开发中，你需要权衡使用系统对象和自定义对象的性能差异。

### 库存对象的优势

库存对象（通过 GetStockObject 获取）有几个优势：

1. **不需要创建和销毁**：节省了创建和删除的开销
2. **系统级缓存**：系统已经缓存了这些对象，访问速度快
3. **无资源泄漏风险**：不需要管理生命周期

如果你的需求是简单的纯色、标准字体或常见样式，优先考虑库存对象：

```cpp
// 简单场景使用库存对象
SelectObject(hdc, GetStockObject(BLACK_PEN));
SelectObject(hdc, GetStockObject(WHITE_BRUSH));
SelectObject(hdc, GetStockObject(SYSTEM_FONT));
```

### 自定义对象的开销

每次调用 CreatePen、CreateSolidBrush 等函数都会：
1. 在 GDI 堆中分配内存
2. 初始化对象结构
3. 可能触发系统资源分配

如果你需要频繁创建和销毁相同的自定义对象，考虑缓存它们：

```cpp
// 类成员变量
HPEN m_hCachedPen;
HBRUSH m_hCachedBrush;

// 初始化时创建
m_hCachedPen = CreatePen(PS_SOLID, 2, RGB(0, 100, 200));
m_hCachedBrush = CreateSolidBrush(RGB(240, 240, 240));

// 绘制时复用
HPEN hOldPen = (HPEN)SelectObject(hdc, m_hCachedPen);
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, m_hCachedBrush);
// ... 绘图 ...
SelectObject(hdc, hOldPen);
SelectObject(hdc, hOldBrush);

// 销毁时删除
DeleteObject(m_hCachedPen);
DeleteObject(m_hCachedBrush);
```

### DC_BRUSH 和 DC_PEN

Windows 2000+ 引入了两个特殊的库存对象：DC_BRUSH 和 DC_PEN。它们的特点是颜色可以通过 SetDCBrushColor 和 SetDCPenColor 动态修改，而不需要重新创建对象：

```cpp
// 使用可变颜色的画刷
SelectObject(hdc, GetStockObject(DC_BRUSH));
SetDCBrushColor(hdc, RGB(255, 0, 0));  // 红色
Rectangle(hdc, 10, 10, 100, 100);

SetDCBrushColor(hdc, RGB(0, 255, 0));  // 绿色
Rectangle(hdc, 120, 10, 210, 100);

// 使用可变颜色的画笔
SelectObject(hdc, GetStockObject(DC_PEN));
SetDCPenColor(hdc, RGB(0, 0, 255));   // 蓝色
MoveToEx(hdc, 10, 120, NULL);
LineTo(hdc, 210, 120);
```

这对于需要频繁改变颜色的场景特别有用，避免了反复创建和销毁画刷/画笔对象。

---

## 后续可以做什么

到这里，Win32 GDI 绘图对象的核心知识就讲完了。你现在应该能够：
* 理解 GDI 对象的生命周期管理规则
* 创建和使用各种画笔（实线、虚线、自定义样式）
* 创建和使用各种画刷（纯色、阴影、图案）
* 使用 LOGFONT 创建自定义字体
* 创建、组合和操作区域
* 在裁剪和命中测试中使用区域

但这些只是 GDI 对象的基础，还有更多高级主题值得探索：

1. **路径（Path）**：使用 BeginPath/EndPath 创建复杂形状，然后用 PathToRegion 转换为区域
2. **字体枚举**：使用 EnumFontFamilies 枚举系统可用字体
3. **文本格式化**：使用 DrawTextEx 进行复杂的文本布局
4. **世界坐标变换**：使用 SetWorldTransform 实现旋转、缩放
5. **位图和 DIB**：深入理解 HBITMAP 和设备无关位图

下一步，我们可以探讨 Win32 的路径与区域、双缓冲动画技术、或者位图与图像处理。这些都是让你的程序更加专业和生动的关键技能。

---

## 相关资源

- [Pen Functions - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/pen-functions)
- [Brush Functions - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/brush-functions)
- [Region Functions - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/region-functions)
- [ExtCreatePen function - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-extcreatepen)
- [LOGPEN structure - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-logpen)
- [LOGBRUSH structure - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-logbrush)
- [LOGFONTA structure - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-logfonta)
- [GetStockObject function - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getstockobject)
- [SelectObject function - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectobject)
- [CreateFontIndirect function - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfontindirect)
