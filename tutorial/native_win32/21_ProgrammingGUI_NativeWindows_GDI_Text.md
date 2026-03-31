# 通用GUI编程技术——Win32 原生编程实战（二十一）——GDI 文字渲染：TextOut、DrawText 与文字度量

> 之前我们聊过 GDI 的设备上下文和基本图形绘制，画线画圆这些操作已经很直观了。但当我们需要在窗口上显示文字时，事情就变得微妙起来。说实话，我刚接触 Win32 文字渲染的时候有点懵 —— 为什么不能像其他框架那样直接"显示文字"，非要搞这么多种 API？TextOut、DrawText、ExtTextOut、TabbedTextOut... 它们到底有什么区别？更头疼的是，当我想让文字居中显示、或者计算一段文字占多少像素的时候，发现居然需要专门的度量函数。这篇文章就是要把这些坑都踩一遍，让你在 Win32 里画文字不再头疼。

---

## 前言：文字渲染比想象中复杂

我们首先得承认一个事实：在图形界面上显示文字，从来都不是一件简单的事情。你想想，文字不只是"把字符画在屏幕上"那么简单。你需要考虑字体选择（字体、大小、粗体、斜体）、字符编码（ANSI 还是 Unicode）、文字颜色和背景、多行文本的对齐和换行、文字度量（计算字符串占多少空间）、甚至还有文字轮廓、阴影这些高级效果。

Win32 的 GDI 文字渲染系统是在上世纪 80 年代设计的，那时候的很多设计决策到今天看来确实有点"古老"。比如 TextOut 函数需要你手动传入字符串长度，这对于习惯了 C++ string 或者其他语言的开发者来说很不直观。再比如 DrawText 的那些 DT_* 格式化标志，多到让人记不住。但话说回来，这套系统经受了三十多年的考验，稳定性和兼容性都没得说。

很多朋友可能会问："现在都 2025 年了，为什么不直接用 DirectWrite 或者其他现代文字渲染库？"这确实是个好问题。DirectWrite 更现代，支持 ClearType、更好的字体回退、复杂的文字排版。但对于很多简单场景，GDI 文字渲染依然是最快速、最简单的解决方案。你不需要链接额外的库，不需要学习全新的 API，而且 GDI 文字渲染在任何 Windows 版本上都能正常工作。

这篇文章会从最简单的 TextOut 开始，逐步深入到 DrawText 的格式化功能、文字度量的各种函数，最后还会讲一些高级技巧比如文字路径绘制。我们不仅会介绍 API 的用法，更重要的是理解"为什么这样用"。毕竟，GDI 文字渲染的很多设计在今天看来有点反直觉，理解了背后的原因，用起来就顺手多了。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 2000+ 都支持 GDI 文字渲染）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新，强烈建议使用 Unicode 版本 API）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容 —— 至少知道怎么创建一个基本窗口、什么是 WM_PAINT 消息、怎么获取和使用 HDC。如果这些概念对你来说还比较陌生，建议先去看看前面关于 GDI 的笔记。

---

## 第一步 —— GDI 文字渲染基础

### HDC 与字体的关系

在 GDI 中绘制文字，首先需要一个设备上下文（HDC），这和画其他图形没什么区别。但文字有一个特殊的地方：HDC 必须有一个当前选中的字体，才能正确显示文字。

默认情况下，当你从 BeginPaint 或 GetDC 获得 HDC 时，它已经有一个默认字体选入了。这个默认字体通常是"系统字体"（System Font），在一些旧版本的 Windows 上可能是 MS Sans Serif。这个字体对于简单的文字显示是够用的，但如果你想显示特定字体、特定大小的文字，就需要自己创建字体并选入 HDC。

创建字体的标准函数是 CreateFont 或 CreateFontIndirect：

```cpp
HFONT CreateFont(
    int      nHeight,          // 字体高度
    int      nWidth,           // 字体宽度（0 表示默认）
    int      nEscapement,      // 字符串的旋转角度（0.1 度单位）
    int      nOrientation,     // 字符的旋转角度（0.1 度单位）
    int      fnWeight,         // 字体粗细
    DWORD    fdwItalic,        // 是否斜体
    DWORD    fdwUnderline,     // 是否下划线
    DWORD    fdwStrikeOut,     // 是否删除线
    DWORD    fdwCharSet,       // 字符集
    DWORD    fdwOutputPrecision,   // 输出精度
    DWORD    fdwClipPrecision,     // 裁剪精度
    DWORD    fdwQuality,            // 输出质量
    DWORD    fdwPitchAndFamily,     // 字体间距和家族
    LPCWSTR  lpszFace           // 字体名称
);
```

这个函数的参数多到让人头皮发麻。但实际上大多数情况下你只需要关注几个核心参数：

```cpp
// 创建一个 24 像素高的 Arial 字体
HFONT hFont = CreateFont(
    24,                    // 字体高度
    0,                     // 宽度让系统自动计算
    0,                     // 不旋转
    0,                     // 字符不旋转
    FW_NORMAL,             // 正常粗细（FW_BOLD 表示粗体）
    FALSE,                 // 不斜体
    FALSE,                 // 无下划线
    FALSE,                 // 无删除线
    DEFAULT_CHARSET,       // 使用默认字符集
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,       // 默认输出质量
    DEFAULT_PITCH | FF_SWISS,
    L"Arial"               // 字体名称
);
```

⚠️ 注意

千万别忘了 DeleteObject！CreateFont 创建的字体必须在使用完后删除，否则会泄漏 GDI 对象。而且删除前必须先从所有 DC 中选出该字体，这和其他 GDI 对象的要求是一样的。

字体创建后，需要用 SelectObject 选入 HDC 才能生效：

```cpp
HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

// 现在可以用新字体绘制文字了
TextOut(hdc, 10, 10, L"Hello, New Font!", 17);

// 使用完后恢复旧字体
SelectObject(hdc, hOldFont);

// 现在可以安全删除新字体了
DeleteObject(hFont);
```

### 设备上下文的文字属性

除了字体，HDC 还有几个与文字相关的属性，这些属性会影响文字的显示效果：

* **文本颜色**（Text Color）：由 SetTextColor 设置，决定文字本身的颜色
* **背景颜色**（Background Color）：由 SetBkColor 设置，决定文字间隙填充的颜色
* **背景模式**（Background Mode）：由 SetBkMode 设置，决定文字间隙是填充还是透明
* **文本对齐方式**（Text Alignment）：由 SetTextAlign 设置，决定文字相对于指定坐标的对齐方式

这些属性我们后面会详细讲解。现在你需要知道的是：这些属性都是 HDC 状态的一部分，会持续影响后续的文字绘制操作，直到你再次修改它们。

---

## 第二步 —— TextOut：最简单的文字输出

### 函数原型与参数详解

TextOut 是 GDI 中最简单的文字输出函数。它的 Unicode 版本（我们应当使用的版本）原型如下：

```cpp
BOOL TextOutW(
    HDC     hdc,        // 设备上下文
    int     x,          // 文字起始位置的 X 坐标
    int     y,          // 文字起始位置的 Y 坐标
    LPCWSTR lpString,   // 要绘制的字符串
    int     cchString   // 字符串的长度（字符数）
);
```

参数虽然不多，但有几个需要注意的地方。

首先是 x 和 y 坐标。这两个参数指定的是文字的"起始位置"，但这个"起始位置"具体在哪里，取决于当前的文本对齐方式（SetTextAlign 设置的）。默认情况下，使用 TA_LEFT | TA_TOP，即 (x, y) 是文字字符串的左上角。我们后面会详细讲解对齐方式。

然后是 lpString 和 cchString。这里有个很重要的细节：cchString 是字符数，不是字节数。如果你用的是 Unicode 版本（TextOutW），那就是 wchar_t 的个数，不是字节数。这听起来好像很明显，但在从 ANSI 代码迁移到 Unicode 的时候，很多开发者在这里栽过跟头。

### 一个简单的 TextOut 示例

让我们看一个最简单的例子：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 使用默认字体绘制文字
    TextOut(hdc, 10, 10, L"Hello, GDI!", 12);

    // 修改文字颜色
    SetTextColor(hdc, RGB(255, 0, 0));
    TextOut(hdc, 10, 40, L"Red Text", 9);

    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码会在窗口的 (10, 10) 位置绘制"Hello, GDI!"，然后在 (10, 40) 位置绘制红色的"Red Text"。

### TextOut 的坐标系统

TextOut 的坐标系统可能会让新手困惑。默认情况下（TA_LEFT | TA_TOP 对齐），(x, y) 指定的是文字字符串的"基线起点"。但这并不是字形的视觉左上角，而是文本度量中的一个概念。具体来说：

* x 坐标是文字字符串的起始水平位置（对于左对齐，是第一个字符的左边界）
* y 坐标是文字字符串的基线位置（对于上对齐，是字符上升部的顶部）

如果你对"基线"这个概念不熟悉，可以把它想象成书写文字时的一条虚拟横线。英文字母比如 "a"、"b"、"c" 都"坐"在这条线上，而字母 "g"、"p"、"q" 的"尾巴"会延伸到这条线下面。我们后面讲文字度量的时候会详细解释这个概念。

现在我们只需要记住：默认情况下，TextOut 的 (x, y) 是文字左上角的位置，这个位置大致对应文字包围盒的左上角。对于中文文字来说基本没问题，对于英文来说可能会有点偏差，因为英文的基线和视觉顶部之间有一小段距离（上升部）。

### TextOutW 与 TextOutA 的选择

Windows API 提供了两个版本的 TextOut：TextOutW（Unicode）和 TextOutA（ANSI）。在 <windows.h> 中，根据 UNICODE 宏的定义，TextOut 会被展开成相应的版本。

```cpp
// 如果定义了 UNICODE
#define TextOut  TextOutW

// 如果没有定义 UNICODE
#define TextOut  TextOutA
```

现代 Win32 编程强烈建议使用 Unicode 版本。理由有几个：

1. Windows 内部使用 UTF-16，Unicode 版本的 API 不需要字符编码转换
2. Unicode 可以表示任意语言的字符，ANSI 只能表示当前代码页的字符
3. 从 Windows Vista 开始，系统核心 API 都是 Unicode，ANSI 版本只是一个转换层

如果你在 Visual Studio 里创建新项目，默认会定义 UNICODE 和 _UNICODE 宏，所以你直接用 TextOut 就会调用 Unicode 版本。但如果你需要显式调用某个版本，可以直接用 TextOutW 或 TextOutA：

```cpp
// 显式调用 Unicode 版本
TextOutW(hdc, 10, 10, L"Hello", 5);

// 显式调用 ANSI 版本（不推荐）
TextOutA(hdc, 10, 10, "Hello", 5);
```

---

## 第三步 —— DrawText：格式化文字输出

TextOut 虽然简单，但它的功能很有限。它不支持自动换行、不支持文字对齐、不支持矩形裁剪。当你需要绘制多行文本、或者让文字在矩形区域内居中显示时，DrawText 就派上用场了。

### DrawText 的函数原型

```cpp
int DrawTextW(
    HDC        hdc,          // 设备上下文
    LPCWSTR    lpchText,     // 要绘制的文本
    int        cchText,      // 文本长度（字符数），-1 表示自动计算
    LPRECT     lprc,         // 绘制矩形
    UINT       format        // 格式化选项
);
```

DrawText 比 TextOut 复杂的地方在于它需要一个 RECT 结构和一个 format 参数。RECT 指定文字绘制的区域，format 是一组标志的组合，控制文字的对齐、换行、省略等行为。

### DT_* 格式化标志详解

DrawText 的 format 参数可以接受很多标志，这些标志可以用位或（|）组合使用。我们按功能分类来看：

**水平对齐标志**（三选一，不能同时使用）：

* **DT_LEFT**：左对齐（默认）
* **DT_CENTER**：水平居中
* **DT_RIGHT**：右对齐

**垂直对齐标志**（需要配合 DT_SINGLELINE）：

* **DT_TOP**：顶部对齐（默认）
* **DT_VCENTER**：垂直居中（需要 DT_SINGLELINE）
* **DT_BOTTOM**：底部对齐（需要 DT_SINGLELINE）

**换行和裁剪标志**：

* **DT_SINGLELINE**：单行模式，不自动换行
* **DT_WORDBREAK**：允许单词边界换行（默认）
* **DT_NOPREFIX**：忽略 & 前缀符（默认 & 用于表示下划线快捷键）
* **DT_NOCLIP**：不裁剪，允许绘制到矩形外
* **DT_EDITCONTROL**：模仿编辑控件的绘制方式

**计算标志**：

* **DT_CALCRECT**：不绘制文字，只计算并更新矩形尺寸

**省略号标志**（需要配合 DT_SINGLELINE | DT_WORD_ELLIPSIS 等组合）：

* **DT_WORD_ELLIPSIS**：如果文字太长，在单词边界截断并添加省略号
* **DT_END_ELLIPSIS**：在字符串末尾添加省略号
* **DT_PATH_ELLIPSIS**：对于路径字符串，在中间用省略号替换

**扩展标志**：

* **DT_EXPANDTABS**：扩展制表符（Tab 字符）
* **DT_TABSTOP**：自定义制表符宽度
* **DT_EXTERNALLEADING**：在行高中包含外部行距

### 一个多行居中的示例

让我们看一个实际例子：在窗口中央绘制一行居中的文字。

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 获取客户区矩形
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 绘制居中文字
    DrawText(hdc, L"Centered Text", -1, &rcClient,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码会在窗口的正中央绘制"Centered Text"。DT_CENTER 让文字水平居中，DT_VCENTER 让文字垂直居中，DT_SINGLELINE 指定单行模式（DT_VCENTER 需要配合这个标志）。

### 多行文本与自动换行

DrawText 默认支持自动换行。当文本长度超过矩形宽度时，DrawText 会在单词边界处自动换行：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc = {10, 10, 300, 200};

    // 绘制多行文本
    DrawText(hdc,
             L"This is a long text that will wrap automatically "
             L"when it exceeds the rectangle width.",
             -1, &rc,
             DT_LEFT | DT_WORDBREAK);

    EndPaint(hwnd, &ps);
    return 0;
}
```

如果你不想自动换行，可以加上 DT_SINGLELINE 标志。这样文字会一直延伸到矩形边界外（除非你同时设置了 DT_END_ELLIPSIS）。

### DT_CALCRECT：计算文字高度

DT_CALCRECT 是一个很有用的标志。当设置这个标志时，DrawText 不会真正绘制文字，而是计算并更新传入的 RECT 结构，让它的大小刚好能容纳指定的文字。这个功能在设计自定义控件时特别有用。

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 初始矩形：宽度固定，高度为0
    RECT rc = {10, 10, 300, 10};

    // 计算文字所需的高度
    DrawText(hdc,
             L"This text will be measured to determine "
             L"the required height.",
             -1, &rc,
             DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

    // 现在 rc.bottom 已经被更新为所需高度
    // 让我们绘制一个矩形包围文字
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    // 真正绘制文字
    DrawText(hdc,
             L"This text will be measured to determine "
             L"the required height.",
             -1, &rc,
             DT_LEFT | DT_WORDBREAK);

    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码首先用 DT_CALCRECT 计算文字所需的高度，然后画一个矩形包围文字，最后再把文字真正画出来。

⚠️ 注意

DT_CALCRECT 只会调整矩形的高度，不会改变宽度。如果你传入的矩形宽度是 0 或负数，DrawText 会返回 0 表示失败。

### DrawText 的返回值

DrawText 的返回值是文字的高度（以逻辑单位为单位）。如果 DT_CALCRECT 被设置，返回值是文字的高度；如果设置了 DT_VCENTER 且未设置 DT_CALCRECT，返回值是没有意义的。

在实际使用中，你可能不需要关心返回值，除非你需要在绘制后知道文字实际占用了多少空间。这种情况下，DT_CALCRECT 是更可靠的选择。

---

## 第四步 —— 文字度量详解

在 GDI 中绘制文字，一个很重要的能力是知道文字占多少空间。你需要知道文字的宽度才能计算下一个元素的起始位置，需要知道文字的高度才能计算行距，需要知道基线的位置才能对齐不同大小的文字。这些都需要"文字度量"功能。

### GetTextMetrics：获取字体度量

GetTextMetrics 函数获取当前选中字体的完整度量信息：

```cpp
BOOL GetTextMetricsW(
    HDC        hdc,        // 设备上下文
    LPTEXTMETRICW lptm     // 接收度量信息的结构
);
```

TEXTMETRIC 结构体包含了字体的各种度量信息：

```cpp
typedef struct tagTEXTMETRICW {
    LONG  tmHeight;           // 字符高度（ascent + descent）
    LONG  tmAscent;           // 上升部高度
    LONG  tmDescent;          // 下降部高度
    LONG  tmInternalLeading;  // 内部行距
    LONG  tmExternalLeading;  // 外部行距
    LONG  tmAveCharWidth;     // 平均字符宽度
    LONG  tmMaxCharWidth;     // 最大字符宽度
    LONG  tmWeight;           // 字体粗细
    LONG  tmOverhang;         // 某些合成字体的额外宽度
    LONG  tmDigitizedAspectX; // 字体设计时的水平宽高比
    LONG  tmDigitizedAspectY; // 字体设计时的垂直宽高比
    WCHAR tmFirstChar;        // 字体中第一个字符
    WCHAR tmLastChar;         // 字体中最后一个字符
    WCHAR tmDefaultChar;      // 替换字符（字体中不存在的字符用这个替换）
    WCHAR tmBreakChar;        // 用于换行的字符（通常是空格）
    BYTE  tmItalic;           // 是否斜体
    BYTE  tmUnderlined;       // 是否下划线
    BYTE  tmStruckOut;        // 是否删除线
    BYTE  tmPitchAndFamily;   // 字体间距和家族
    BYTE  tmCharSet;          // 字符集
} TEXTMETRICW;
```

这个结构体包含了很多字段，但大多数情况下我们只需要关注几个关键的字段。

### TEXTMETRIC 结构体关键字段

让我们详细理解几个最重要的字段：

**tmHeight**：字符的总高度，等于 tmAscent + tmDescent。这是绘制一行文字所需的垂直空间，不包括外部行距。

**tmAscent**：上升部高度。这是从基线到字符顶部的距离。对于英文字母 "a"、"b"、"c"，它们的顶部就在基线以上 tmAscent 的位置。

**tmDescent**：下降部高度。这是从基线到字符底部的距离。对于英文字母 "g"、"p"、"q"，它们的"尾巴"会延伸到基线以下 tmDescent 的位置。

**tmInternalLeading**：内部行距。这是在 tmHeight 内部预留的空间，用于重音符号等元素。通常 tmHeight = tmAscent + tmDescent 已经包含了这部分，但某些字体设计会明确区分。

**tmExternalLeading**：外部行距。这是行与行之间的额外空间，由字体设计者推荐。如果你想让多行文字有合适的行距，可以在行高基础上加上 tmExternalLeading。

**tmAveCharWidth**：平均字符宽度。这是字体中小写字母的平均宽度，用于估算文本宽度。注意这只是平均值，实际字符宽度会有差异。

**tmMaxCharWidth**：最大字符宽度。这是字体中最宽字符的宽度，通常用于计算滚动条宽度或者缓冲区大小。

### 行高、基线、上升部、下降部

这些概念可能有点抽象，让我们用一个图示来理解：

```
       上升部 (tmAscent)
         ↑
         │    ┌───┐
         │    │ A │
         │    └───┘
─────────┼───────────────  基线
         │    ┌───┐
         │    │ g │
         │    └─┬─┘
         ↓      └─ 下降部 (tmDescent)
```

字母 "A" 完全在基线以上，只占用上升部。字母 "g" 跨越基线，既占用上升部又占用下降部。字符的总高度 tmHeight = tmAscent + tmDescent。

当绘制多行文字时，行距通常设置为 tmHeight + tmExternalLeading。这样可以保证上下两行文字之间有合适的间距。

### GetTextExtentPoint32：获取字符串尺寸

GetTextMetrics 提供的是字体整体的度量信息，但很多时候我们需要知道特定字符串的尺寸。这时候 GetTextExtentPoint32 就派上用场了：

```cpp
BOOL GetTextExtentPoint32W(
    HDC        hdc,         // 设备上下文
    LPCWSTR    lpString,    // 字符串
    int        c,           // 字符串长度（字符数）
    LPSIZE     lpsz         // 接收尺寸的 SIZE 结构
);
```

这个函数会计算指定字符串在当前字体下的尺寸（宽度和高度），结果存放在 SIZE 结构中：

```cpp
typedef struct tagSIZE {
    LONG cx;  // 宽度
    LONG cy;  // 高度
} SIZE;
```

一个简单的例子：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 创建并选入字体
    HFONT hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // 要测量的字符串
    const wchar_t* text = L"Hello, World!";

    // 获取字符串尺寸
    SIZE size;
    GetTextExtentPoint32(hdc, text, lstrlen(text), &size);

    // 在矩形中绘制文字
    RECT rc = {10, 10, 10 + size.cx, 10 + size.cy};
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    TextOut(hdc, 10, 10, text, lstrlen(text));

    // 清理
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    EndPaint(hwnd, &ps);
    return 0;
}
```

这段代码先测量"Hello, World!"的尺寸，然后画一个刚好包围文字的矩形，最后绘制文字本身。

⚠️ 注意

GetTextExtentPoint32 返回的高度（cy）与 GetTextMetrics 的 tmHeight 不一定相同。GetTextExtentPoint32 返回的是当前特定字符串的实际渲染高度，而 tmHeight 是字体的理论最大高度。对于大多数字符串，两者会很接近，但某些特殊字符或组合可能会有差异。

---

## 第五步 —— 文字属性控制

现在我们已经知道怎么绘制文字和计算文字尺寸了，但文字的颜色、背景、对齐方式这些属性也很重要。GDI 提供了几个函数来控制这些属性。

### SetTextColor / SetBkColor

SetTextColor 设置文字的前景色，SetBkColor 设置文字的背景色：

```cpp
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
```

两个函数都返回之前的颜色值，方便你恢复。

```cpp
// 保存旧颜色
COLORREF oldTextColor = SetTextColor(hdc, RGB(255, 0, 0));
COLORREF oldBkColor = SetBkColor(hdc, RGB(255, 255, 0));

// 绘制红色文字，黄色背景
TextOut(hdc, 10, 10, L"Red on Yellow", 14);

// 恢复旧颜色
SetTextColor(hdc, oldTextColor);
SetBkColor(hdc, oldBkColor);
```

### SetBkMode：OPAQUE vs TRANSPARENT

SetBkMode 控制文字背景的绘制模式：

```cpp
int SetBkMode(HDC hdc, int mode);
```

两种模式：

* **OPAQUE**（默认）：字符间隙用当前背景色填充
* **TRANSPARENT**：字符间隙透明，不填充

```cpp
// 先画一个蓝色矩形
RECT rc = {10, 10, 200, 50};
HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255));
FillRect(hdc, &rc, hBrush);
DeleteObject(hBrush);

// OPAQUE 模式：文字间隙用黄色填充
SetBkMode(hdc, OPAQUE);
SetBkColor(hdc, RGB(255, 255, 0));
SetTextColor(hdc, RGB(0, 0, 0));
TextOut(hdc, 20, 20, L"OPAQUE Mode", 12);

// TRANSPARENT 模式：文字间隙透明，可以看到蓝色矩形
SetBkMode(hdc, TRANSPARENT);
TextOut(hdc, 20, 40, L"TRANSPARENT Mode", 16);
```

⚠️ 注意

默认的背景模式是 OPAQUE，所以如果你在彩色背景上绘制文字且没有设置 SetBkMode(TRANSPARENT)，文字背后会出现一个背景色块（默认是白色或窗口背景色）。这是一个常见的"坑"，很多开发者会困惑为什么文字背后有一块奇怪的背景。

### SetTextAlign：文字对齐方式

SetTextAlign 设置文字绘制时的对齐方式，影响 TextOut、ExtTextOut 等函数中坐标的含义：

```cpp
UINT SetTextAlign(HDC hdc, UINT align);
```

常用的对齐标志：

**水平对齐**（三选一）：

* **TA_LEFT**（默认）：坐标在文字左侧
* **TA_CENTER**：坐标在文字水平中心
* **TA_RIGHT**：坐标在文字右侧

**垂直对齐**（三选一）：

* **TA_TOP**（默认）：坐标在文字顶部
* **TA_BASELINE**：坐标在文字基线位置
* **TA_BOTTOM**：坐标在文字底部

**更新当前位置标志**：

* **TA_UPDATECP**：每次绘制后更新当前位置

一个居中绘制的例子：

```cpp
// 设置对齐方式为居中
SetTextAlign(hdc, TA_CENTER | TA_BASELINE);

// 现在 (100, 50) 是文字的基线中心点
TextOut(hdc, 100, 50, L"Centered", 8);

// 恢复默认对齐
SetTextAlign(hdc, TA_LEFT | TA_TOP);
```

⚠️ 注意

SetTextAlign 影响的是 TextOut、ExtTextOut 等函数的坐标解释，不影响 DrawText（DrawText 有自己的对齐标志）。而且 SetTextAlign 的设置会持续影响后续的绘制操作，直到你再次修改它。

---

## 第六步 —— 高级文字技巧

基础功能讲完了，我们来看看一些更高级的技巧。

### ExtTextOut 的间隔数组

ExtTextOut 是 TextOut 的增强版本，它允许你为每个字符指定单独的间距：

```cpp
BOOL ExtTextOutW(
    HDC         hdc,           // 设备上下文
    int         x,             // X 坐标
    int         y,             // Y 坐标
    UINT        options,       // 选项（通常为0）
    const RECT  *lprect,       // 裁剪矩形（可选）
    LPCWSTR     lpString,      // 字符串
    UINT        c,             // 字符串长度
    const INT   *lpDx          // 字符间距数组（可选）
);
```

最后一个参数 lpDx 是一个整数数组，指定每个字符后面的间距。这可以实现一些特殊的文字效果：

```cpp
const wchar_t* text = L"Spaced Out";
int len = lstrlen(text);

// 创建间距数组：每个字符后面增加额外的间距
std::vector<int> spacing(len);
for (int i = 0; i < len; i++) {
    spacing[i] = 20;  // 每个字符后额外增加 20 像素
}

// 绘制带间距的文字
ExtTextOut(hdc, 10, 10, 0, NULL, text, len, spacing.data());
```

### TabbedTextOut 的制表符处理

TabbedTextOut 专门用于处理包含制表符（Tab）的文本：

```cpp
LONG TabbedTextOutW(
    HDC         hdc,           // 设备上下文
    int         x,             // X 坐标
    int         y,             // Y 坐标
    LPCWSTR     lpString,      // 字符串
    int         cchString,     // 字符串长度
    int         cTabStops,     // 制表符数量
    const INT   *lpnTabStopPositions,  // 制表符位置数组
    int         nTabOrigin     // 制表符原点
);
```

一个简单的例子：

```cpp
const wchar_t* text = L"Name:\tAlice\tAge:\t25";

// 定义制表符位置
int tabStops[] = {100, 200, 300};

// 绘制制表符文本
TabbedTextOut(hdc, 10, 10, text, lstrlen(text),
              sizeof(tabStops) / sizeof(tabStops[0]),
              tabStops, 10);
```

这段代码会在指定的制表符位置对齐文本，类似于简单的表格布局。

### 文字路径绘制轮廓

GDI 路径功能可以用来创建文字的轮廓效果。基本思路是：先用 BeginPath 开始路径，用 TextOut 把文字"画"到路径里，然后用 StrokePath 绘制轮廓：

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // 创建并选入大字体
    HFONT hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // 开始路径
    BeginPath(hdc);

    // 把文字"画"到路径里
    TextOut(hdc, 10, 10, L"Outlined", 8);

    // 结束路径
    EndPath(hdc);

    // 创建并选入画笔（用于轮廓）
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // 绘制轮廓
    StrokePath(hdc);

    // 清理
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    EndPaint(hwnd, &ps);
    return 0;
}
```

如果你想要填充的轮廓文字，可以用 StrokeAndFillPath 替代 StrokePath：

```cpp
// 创建画刷（用于填充）
HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 0));
HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

// 同时绘制轮廓和填充
StrokeAndFillPath(hdc);

// 清理
SelectObject(hdc, hOldBrush);
DeleteObject(hBrush);
```

⚠️ 注意

路径操作只能用一次。EndPath 后路径就"固化"了，StrokePath 或 StrokeAndFillPath 后路径就失效了。如果你想多次使用同一路径，需要用 PathToRegion 转换成区域，或者重新构建路径。

---

## 第七步 —— 实战示例：绘制多行居中文字

让我们把以上知识整合成一个完整的示例。这个程序会在窗口中心绘制多行居中文字，演示字体选择、文字度量、居中计算等核心概念。

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <vector>
#include <string>

// 窗口类名
static const wchar_t g_szClassName[] = L"GDITextDemo";

// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 绘制居中文字的函数
void DrawCenteredText(HWND hwnd, HDC hdc, const std::vector<std::wstring>& lines)
{
    // 获取客户区大小
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;
    int clientHeight = rcClient.bottom - rcClient.top;

    // 创建并选入字体
    HFONT hFont = CreateFont(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // 获取字体度量
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);

    // 计算行高（字体高度 + 外部行距）
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    // 计算所有文字行的总高度
    int totalTextHeight = lines.size() * lineHeight;

    // 计算起始 Y 坐标（垂直居中）
    int startY = (clientHeight - totalTextHeight) / 2;

    // 设置文字颜色和背景模式
    SetTextColor(hdc, RGB(50, 50, 50));
    SetBkMode(hdc, TRANSPARENT);

    // 设置文本对齐为居中（简化水平居中计算）
    UINT oldAlign = SetTextAlign(hdc, TA_CENTER | TA_BASELINE);

    // 逐行绘制文字
    for (size_t i = 0; i < lines.size(); i++) {
        int y = startY + (int)i * lineHeight + tm.tmAscent;
        TextOut(hdc, clientWidth / 2, y, lines[i].c_str(), (int)lines[i].length());
    }

    // 恢复原始设置
    SetTextAlign(hdc, oldAlign);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
    wcex.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"窗口类注册失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0, g_szClassName, L"GDI 文字渲染示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"窗口创建失败！", L"错误",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
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

        // 准备要绘制的文字行
        std::vector<std::wstring> lines;
        lines.push_back(L"欢迎使用 GDI 文字渲染");
        lines.push_back(L"这是第二行文字");
        lines.push_back(L"Win32 编程其实很有趣");

        // 绘制居中文字
        DrawCenteredText(hwnd, hdc, lines);

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

编译运行这个程序，你会看到一个窗口，里面有三行居中显示的文字。窗口大小改变时，文字会自动重新居中。

这个示例展示了几个关键点：
1. 使用 GetTextMetrics 获取字体度量，计算合适的行高
2. 使用 SetTextAlign(TA_CENTER) 简化水平居中计算
3. 手动计算垂直居中位置
4. 使用 SetBkMode(TRANSPARENT) 避免文字背景遮挡

---

## 第八步 —— 常见问题与解决方案

在实际开发中，你可能会遇到各种 GDI 文字渲染的问题。这里分享一些常见的坑和解决方法。

### 乱码问题

乱码是 Win32 文字渲染中最常见的问题之一。原因通常是字符编码不匹配：

```cpp
// 错误：ANSI 字符串传递给 Unicode 版本
TextOut(hdc, 10, 10, (LPCWSTR)"Hello", 5);  // 可能乱码

// 正确：使用 Unicode 字符串
TextOut(hdc, 10, 10, L"Hello", 5);

// 或者显式使用 ANSI 版本
TextOutA(hdc, 10, 10, "Hello", 5);
```

如果你从外部源（比如文件、网络）读取文本，确保正确转换编码：

```cpp
// 从 UTF-8 转换到 UTF-16
int Utf8ToUtf16(const char* utf8, wchar_t* utf16, int utf16Len) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, utf16Len);
}

// 使用
char utf8Text[] = "Hello, UTF-8!";
wchar_t utf16Text[256];
Utf8ToUtf16(utf8Text, utf16Text, 256);
TextOut(hdc, 10, 10, utf16Text, lstrlen(utf16Text));
```

### 文字截断问题

当文字超出绘制区域时，GDI 默认会继续绘制到区域外。如果你需要裁剪，有几个方法：

```cpp
// 方法1：使用 DrawText（自带裁剪）
RECT rc = {10, 10, 200, 50};
DrawText(hdc, L"This is a very long text that should be clipped",
          -1, &rc, DT_LEFT | DT_WORD_ELLIPSIS);

// 方法2：使用 IntersectClipRect 裁剪 DC
IntersectClipRect(hdc, 10, 10, 200, 50);
TextOut(hdc, 10, 10, L"This will be clipped", 22);

// 方法3：使用 ExtTextOut 的矩形参数
RECT rc = {10, 10, 200, 50};
ExtTextOut(hdc, 10, 10, ETO_CLIPPED, &rc, L"Clipped text", 12, NULL);
```

### 文字模糊问题

在 High DPI 显示器上，GDI 文字可能会显得模糊。有几个解决方法：

1. **启用 ClearType**：创建字体时使用 CLEARTYPE_QUALITY

```cpp
HFONT hFont = CreateFont(..., CLEARTYPE_QUALITY, ...);
```

2. **使用 DPI 缩放**：在程序清单中声明 DPI 感知

```xml
<!-- manifest 文件 -->
<dpiAware>true</dpiAware>
<dpiAwareness>PerMonitorV2</dpiAwareness>
```

3. **使用 DirectWrite**：对于需要高质量文字渲染的场景，考虑使用 DirectWrite 而不是 GDI。

### 性能问题

GDI 文字渲染的性能一般足够好，但在某些场景下（比如绘制大量文字）可能会遇到性能瓶颈：

```cpp
// 慢：多次调用 TextOut
for (int i = 0; i < 10000; i++) {
    TextOut(hdc, x[i], y[i], texts[i], lengths[i]);
}

// 快：使用 ExtTextOut 的间隔数组（减少 API 调用）
// 或者使用双缓冲：先在内存 DC 绘制，然后一次性复制
```

另一个性能优化是缓存字体对象，而不是每次绘制都重新创建：

```cpp
// 慢：每次都创建和删除字体
for (int i = 0; i < 100; i++) {
    HFONT hFont = CreateFont(...);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    TextOut(hdc, 10, 10, L"Text", 4);
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
}

// 快：只创建一次字体
HFONT hFont = CreateFont(...);
HFONT hOld = (HFONT)SelectObject(hdc, hFont);
for (int i = 0; i < 100; i++) {
    TextOut(hdc, 10, 10 + i * 20, L"Text", 4);
}
SelectObject(hdc, hOld);
DeleteObject(hFont);
```

---

## 后续可以做什么

到这里，Win32 GDI 文字渲染的基础知识就讲完了。你现在应该能够：
- 使用 TextOut 绘制简单的文字
- 使用 DrawText 绘制格式化的多行文本
- 使用 GetTextMetrics 和 GetTextExtentPoint32 计算文字尺寸
- 控制文字的颜色、背景、对齐方式
- 实现一些高级效果如文字轮廓

但 GDI 文字渲染的世界远不止这些。还有很多内容值得探索：

1. **字体枚举**：使用 EnumFonts 或 ChooseFont 对话框让用户选择字体
2. **复杂文字排版**：处理从右到左的文字（阿拉伯语、希伯来语）、上下文相关的字形
3. **DirectWrite**：现代 Windows 文字渲染 API，支持更好的字体回退、颜色字体、变量字体
4. **无衬线字体渲染**：在特定场景下使用 GDI 的自然无衬线渲染模式
5. **文字动画**：结合定时器实现打字机效果、滚动文字等

下一步，我们可以探讨 Win32 的位图操作、双缓冲技术、或者深入 DirectWrite 的世界。这些都是让你的程序更加专业和生动的关键技能。

在此之前，建议你先把今天的内容消化一下。写一些小练习，巩固一下知识：

1. 创建一个程序，在窗口中心绘制当前时间
2. 实现一个简单的文本编辑器视图，支持多行文本显示
3. 制作一个"打字机效果"动画
4. 尝试文字路径的填充和轮廓效果

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。特别是文字度量这部分，需要多实践才能掌握。

好了，今天的文章就到这里，我们下一篇再见！

---

## 相关资源

- [TextOutW 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-textoutw)
- [DrawTextW 函数 - Microsoft Learn](https://learn.microsoft.com/sk-sk/windows/win32/api/winuser/nf-winuser-drawtexta)
- [ExtTextOutW 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-exttextoutw)
- [TabbedTextOutW 函数 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tabbedtextoutw)
- [GetTextMetricsW 函数 - Microsoft Learn](https://learn.microsoft.com/pl-pl/windows/win32/api/wingdi/nf-wingdi-gettextmetrics)
- [GetTextExtentPoint32 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/wingdi/nf-wingdi-gettextextentpoint32w)
- [SetTextColor 函数 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-settextcolor)
- [SetBkColor 函数 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setbkcolor)
- [SetBkMode 函数 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setbkmode)
- [SetTextAlign 函数 - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-settextalign)
- [Setting the Text Alignment - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/setting-the-text-alignment)
- [Fonts and Text - Win32 apps | Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/gdi/fonts-and-text)
