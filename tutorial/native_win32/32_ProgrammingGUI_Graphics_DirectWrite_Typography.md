# 通用GUI编程技术——图形渲染实战（三十二）——DirectWrite高质量文字排版：从基础到富文本

> 上一篇我们玩转了 Direct2D 的效果系统和图层系统，高斯模糊、投影阴影、毛玻璃卡片这些现代 UI 效果都搞定了。但有一个问题我们一直在回避——文字。回顾之前的所有示例，我们在绘制文字时用的是 `DrawText` 或 `DrawTextW`，传一个 IDWriteTextFormat 进去就完事了。这确实够用了……如果你只需要"把字画出来"的话。但如果你想要更精细的控制——比如同一段文字里不同部分用不同的颜色和字号、精确的命中测试（点击某个文字的位置）、图文混排（在文字行内嵌入小图标）——那就必须深入 DirectWrite 的核心了。今天这篇就是来啃 DirectWrite 三层 API 架构的。

## 环境说明

本篇开发环境如下：

- **操作系统**: Windows 11 Pro 10.0.26200
- **编译器**: MSVC (Visual Studio 2022, v17.x)
- **目标平台**: Win32 API 原生开发
- **图形库**: DirectWrite（dwrite.h, dwrite_2.h），配合 Direct2D 进行渲染
- **关键头文件**: dwrite.h, d2d1.h, d2d1_1.h

DirectWrite 从 Windows 7 开始引入，核心接口在 dwrite.h 中。Windows 8.1 增加了 dwrite_2.h（IDWriteFactory2），Windows 10 增加了 dwrite_3.h（IDWriteFactory3）。本篇我们主要使用 dwrite.h 中的基础接口，这些接口在所有受支持的 Windows 版本上都可用。

## DirectWrite 三层架构总览

在开始写代码之前，我们先来理解 DirectWrite 的三层 API 设计。这个架构设计得相当优雅，每一层解决不同抽象级别的问题。

最底层是 IDWriteTextFormat，你可以把它理解为"文字样式模板"。它定义了字体族（Font Family）、字号（Font Size）、字体粗细（Font Weight）、字体样式（Font Style，比如斜体）、文字拉伸（Font Stretch）、文字方向（Flow Direction）、对齐方式（Text Alignment）、段落对齐（Paragraph Alignment）等基础属性。一旦创建完成，IDWriteTextFormat 是不可变的（Immutable）——你不能修改它的属性，只能创建新的。

中间层是 IDWriteTextLayout，这是 DirectWrite 的核心。它接受一个字符串、一个 IDWriteTextFormat、以及布局约束（最大宽度和高度），然后对文字进行完整的排版分析——断行、断字、双向文字处理、字体回退等全部在这里完成。和 IDWriteTextFormat 不同的是，IDWriteTextLayout 是可变的，你可以对文字的任意范围（通过 DWRITE_TEXT_RANGE）设置不同的格式——不同颜色、不同字号、不同字体等。

最顶层是 IDWriteTextRenderer，这是一个你自定义实现的回调接口。当 DirectWrite 需要绘制文字时，它会调用你实现的 TextRenderer 中的方法，把每个字形（Glyph Run）、下划线、删除线、内联对象等信息传递给你。这给了你完全的绘制控制权，但代价是你需要自己处理所有的绘制细节。

对于大多数场景，我们使用前两层就够了——IDWriteTextFormat 定义基础格式，IDWriteTextLayout 进行排版和局部格式化，然后用 Direct2D 的 DrawTextLayout 方法来渲染。只有在需要自定义绘制行为（比如自定义下划线样式、特殊的文字效果）时，才需要实现 IDWriteTextRenderer。

## 第一步——创建 IDWriteFactory 和基础文字格式

一切从 IDWriteFactory 开始。根据 [IDWriteFactory interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefactory) 的文档，创建 Factory 时需要选择一个类型：SHARED 还是 ISOLATED。

```cpp
#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

IDWriteFactory* g_pDWriteFactory = nullptr;
IDWriteTextFormat* g_pTextFormat = nullptr;

HRESULT CreateTextResources()
{
    // 创建 DirectWrite Factory
    // DWRITE_FACTORY_TYPE_SHARED 表示共享系统字体缓存，推荐大多数场景使用
    // DWRITE_FACTORY_TYPE_ISOLATED 表示独立的字体缓存，适用于需要隔离的场景
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_pDWriteFactory)
    );
    if (FAILED(hr)) return hr;

    // 创建文字格式
    hr = g_pDWriteFactory->CreateTextFormat(
        L"Microsoft YaHei",         // 字体族名称
        nullptr,                     // 字体集合（nullptr 表示系统字体集合）
        DWRITE_FONT_WEIGHT_NORMAL,  // 字体粗细
        DWRITE_FONT_STYLE_NORMAL,   // 字体样式（正常/斜体/倾斜）
        DWRITE_FONT_STRETCH_NORMAL, // 字体拉伸
        18.0f,                       // 字号（DIP，与 D2D 的 DPI 无关单位）
        L"zh-CN",                    // 区域设置名称
        &g_pTextFormat
    );

    if (SUCCEEDED(hr)) {
        // 设置段落对齐和文字对齐
        g_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        g_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    return hr;
}
```

这里有几个值得说一说的细节。`DWRITE_FACTORY_TYPE_SHARED` 和 `DWRITE_FACTORY_TYPE_ISOLATED` 的选择：在绝大多数情况下你应该用 SHARED。SHARED 模式下，你的进程和系统其他使用 DirectWrite 的进程共享字体缓存，这意味着字体数据在内存中只有一份拷贝，整体内存使用效率更高。只有在极少数特殊场景下（比如你的应用需要加载大量私有字体，不想和系统字体缓存交互）才需要用 ISOLATED。

字号单位用的是 DIP（Device Independent Pixel），和 Direct2D 的坐标系一致。在 96 DPI 下 1 DIP = 1 像素，在高 DPI 下会自动缩放。这意味着你不需要手动处理 DPI 缩放——18.0f 的字号在任何 DPI 下看起来都是一样大的。

区域设置名称 `L"zh-CN"` 对于中文字体渲染非常重要。DirectWrite 会根据这个区域设置来选择合适的字体回退链，确保中文字符能够正确渲染。

### ⚠️ 中文字体回退的坑

说到中文字体，这里有一个非常常见的坑：如果你在 CreateTextFormat 里指定了一个不包含中文字符的字体族（比如 "Segoe UI"），然后传入包含中文的文字内容，你可能会看到方块字或者问号——这是字体不支持对应字符的标志。

DirectWrite 有内置的字体回退机制，它会自动尝试用其他字体来渲染不支持的字符。但这个回退机制依赖于两个条件：一是系统安装了包含对应字符的字体，二是区域设置正确。如果你发现中文显示异常，首先检查区域设置是否是 `L"zh-CN"`，然后确认系统是否安装了中文字体（Windows 默认安装了 Microsoft YaHei，一般不会缺）。

如果你的应用需要精确控制字体回退行为（比如你想要日文字符用 Meiryo 而不是 YaHei），可以使用 IDWriteFontFallback 接口（dwrite_2.h，Windows 8.1+）来自定义回退规则。

## 第二步——IDWriteTextLayout：精确排版控制

IDWriteTextFormat 只是定义了"默认样式"，而 IDWriteTextLayout 才是真正干活的角色。它对文字进行完整的排版分析，输出的是一个经过换行、断字、双向处理后的排版结果。

根据 [IDWriteTextLayout interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextlayout) 的文档，CreateTextLayout 接受文字内容、格式、布局约束等参数：

```cpp
IDWriteTextLayout* g_pTextLayout = nullptr;

HRESULT CreateTextLayout(const wchar_t* text, float maxWidth, float maxHeight)
{
    if (g_pTextLayout) {
        g_pTextLayout->Release();
        g_pTextLayout = nullptr;
    }

    return g_pDWriteFactory->CreateTextLayout(
        text,                       // 文字内容
        (UINT32)wcslen(text),       // 文字长度
        g_pTextFormat,              // 基础文字格式
        maxWidth,                   // 最大宽度
        maxHeight,                  // 最大高度
        &g_pTextLayout              // 输出的 TextLayout
    );
}
```

这个函数创建了 [IDWriteTextLayout](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritefactory-createtextlayout)，它对传入的文字进行了完整的排版分析。这个过程包括但不限于：字形选择（从字体中查找最佳匹配的字形）、双向文字排列（处理从左到右和从右到左混合的文字）、断行（根据布局宽度决定在哪里换行）、字体回退（遇到当前字体不支持的字符时自动查找替代字体）。

一旦你有了 IDWriteTextLayout，就可以对它的任意文字范围进行格式调整。

### DWRITE_TEXT_RANGE：文字范围操作的核心

[DRITE_TEXT_RANGE](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_text_range) 是 DirectWrite 中非常基础的一个结构体，它由两个字段组成：`startPosition`（起始位置，0-based）和 `length`（长度）。所有对 TextLayout 的局部格式操作都通过它来指定范围。

```cpp
typedef struct DWRITE_TEXT_RANGE {
    UINT32 startPosition;
    UINT32 length;
};
```

下面我们来看如何用 DWRITE_TEXT_RANGE 对同一 TextLayout 内的不同文字段设置不同的样式：

```cpp
void ApplyRangeFormatting(IDWriteTextLayout* pLayout)
{
    // 假设文字内容是: "Hello 世界，Welcome to DirectWrite！"
    // 我们要对 "Hello" 设置红色粗体
    // 对 "世界" 设置蓝色大号
    // 对 "DirectWrite" 设置绿色斜体

    // 1. "Hello" —— 位置 0~5，设置粗体
    DWRITE_TEXT_RANGE range1 = { 0, 5 };
    pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range1);

    // 创建红色画刷（通过 DrawingEffect）
    // 注意：IDWriteTextLayout 本身不存储颜色画刷
    // 颜色需要通过 SetDrawingEffect 设置
    // SetDrawingEffect 接受的是 IUnknown* 接口
    // Direct2D 的 brush 恰好实现了 IUnknown，所以可以直接传入

    // 2. "世界" —— 位置 6~2，设置大号字体
    DWRITE_TEXT_RANGE range2 = { 6, 2 };
    pLayout->SetFontSize(28.0f, range2);

    // 3. "DirectWrite" —— 找到它的位置
    // 假设 "DirectWrite" 从位置 22 开始，长度 12
    DWRITE_TEXT_RANGE range3 = { 22, 12 };
    pLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range3);
    pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, range3);
}
```

这里有一个很容易让新手困惑的地方：IDWriteTextLayout 有 `SetFontWeight`、`SetFontSize`、`SetFontStyle` 这些方法可以直接设置格式属性，但没有 `SetFontColor` 方法。文字颜色是通过 `SetDrawingEffect` 来间接设置的。

### SetDrawingEffect 设置文字颜色

根据 [IDWriteTextLayout::SetDrawingEffect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritetextlayout-setdrawingeffect) 的文档，`SetDrawingEffect` 接受一个 `IUnknown*` 参数。这个参数会在渲染时传递给 TextRenderer（或者 D2D 内置的渲染器），由渲染器来解释这个对象。

当使用 Direct2D 的 `DrawTextLayout` 进行渲染时，D2D 内置的渲染器会将这个 `IUnknown*` 解释为 ID2D1Brush。所以我们可以直接传入一个 ID2D1SolidColorBrush 来设置文字颜色：

```cpp
void ApplyColorFormatting(ID2D1DeviceContext* pDC, IDWriteTextLayout* pLayout)
{
    // 创建不同颜色的画刷
    ID2D1SolidColorBrush* pRedBrush = nullptr;
    ID2D1SolidColorBrush* pBlueBrush = nullptr;
    ID2D1SolidColorBrush* pGreenBrush = nullptr;

    pDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &pRedBrush);
    pDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::RoyalBlue), &pBlueBrush);
    pDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green), &pGreenBrush);

    // 设置不同范围的颜色
    DWRITE_TEXT_RANGE rangeHello = { 0, 5 };
    pLayout->SetDrawingEffect(pRedBrush, rangeHello);

    DWRITE_TEXT_RANGE rangeWorld = { 6, 2 };
    pLayout->SetDrawingEffect(pBlueBrush, rangeWorld);

    DWRITE_TEXT_RANGE rangeDW = { 22, 12 };
    pLayout->SetDrawingEffect(pGreenBrush, rangeDW);

    // 注意：画刷的引用计数会被 TextLayout 增加
    // 使用完毕后需要 Release
    pRedBrush->Release();
    pBlueBrush->Release();
    pGreenBrush->Release();
}
```

⚠️ 这里有一个引用计数的陷阱。IDWriteTextLayout 会对通过 SetDrawingEffect 传入的对象调用 AddRef，所以在布局对象存在期间，画刷对象不会被意外释放。但你需要确保在调用 SetDrawingEffect 之后、布局对象释放之前，画刷对象一直是有效的。最安全的做法是画刷和布局都作为类成员或全局变量来管理生命周期。

## 第三步——DrawTextLayout 渲染

排版和格式化都设置好之后，就该渲染了。根据 [ID2D1RenderTarget::DrawTextLayout - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-drawtextlayout) 的文档，这个方法比 DrawText 效率更高，因为 TextLayout 已经完成了排版分析，不需要每次调用都重新计算。

```cpp
void RenderText(ID2D1DeviceContext* pDC, IDWriteTextLayout* pLayout)
{
    pDC->BeginDraw();
    pDC->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 创建默认文字画刷（用于没有通过 SetDrawingEffect 设置颜色的部分）
    ID2D1SolidColorBrush* pDefaultBrush = nullptr;
    pDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pDefaultBrush);

    // 绘制 TextLayout
    pDC->DrawTextLayout(
        D2D1::Point2F(20.0f, 20.0f),   // 绘制起始位置
        pLayout,                         // TextLayout 对象
        pDefaultBrush,                   // 默认画刷（当没有 DrawingEffect 时使用）
        D2D1_DRAW_TEXT_OPTIONS_NONE      // 绘制选项
    );

    pDefaultBrush->Release();
    pDC->EndDraw();
}
```

DrawTextLayout 的第三个参数 `defaultBrush` 用于那些没有通过 SetDrawingEffect 指定颜色的文字范围。如果你已经对所有文字范围都设置了 DrawingEffect，这个默认画刷就不会被使用，但仍然不能传 nullptr。

第四个参数 `D2D1_DRAW_TEXT_OPTIONS` 提供了一些控制选项。其中 `D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT` 可以启用彩色字体渲染（比如 Emoji），`D2D1_DRAW_TEXT_OPTIONS_DISABLE_COLOR_BITMAP_SNAPPING` 可以禁用彩色位图字体的像素对齐。

### DrawText vs DrawTextLayout 的选择

什么时候用 DrawText，什么时候用 DrawTextLayout？这取决于你的场景。DrawText 适用于简单的"一段文字、一种格式"的场景——你只需要传入文字、格式、矩形区域和一个画刷就行，简单直接。DrawTextLayout 适用于需要多格式文字、精确布局控制、或者需要反复重绘相同内容的场景。因为 TextLayout 的排版结果会被缓存，所以如果文字内容不变只是位置变了，用 DrawTextLayout 的性能优势会非常明显。

## 第四步——TextLayout 的测量与命中测试

IDWriteTextLayout 不仅仅是一个排版引擎，它还提供了丰富的测量和查询能力。这些能力在实现文字编辑器、交互式文字 UI 时非常关键。

### 获取文字度量信息

```cpp
void GetTextMetrics(IDWriteTextLayout* pLayout)
{
    // 获取整体度量信息
    DWRITE_TEXT_METRICS metrics;
    pLayout->GetMetrics(&metrics);

    // metrics.width        - 排版后文字的实际宽度
    // metrics.height       - 排版后文字的实际高度
    // metrics.widthIncludingTrailingWhitespace - 包含尾部空白的宽度
    // metrics.layoutWidth  - 布局约束宽度（你传入的 maxWidth）
    // metrics.layoutHeight - 布局约束高度（你传入的 maxHeight）
    // metrics.maxBidiDepthReordering - 最大双向文字重排深度
    // metrics.lineCount    - 行数

    wchar_t msg[256];
    swprintf_s(msg, 256, L"Text: %.1f x %.1f, Lines: %d",
              metrics.width, metrics.height, metrics.lineCount);
    // 输出: "Text: 350.5 x 45.0, Lines: 1"
}
```

### 行级度量

有时候你需要知道每一行的高度和位置，比如实现自定义的行间距控制：

```cpp
void GetLineMetrics(IDWriteTextLayout* pLayout)
{
    // 先获取行数
    DWRITE_TEXT_METRICS textMetrics;
    pLayout->GetMetrics(&textMetrics);
    UINT32 lineCount = textMetrics.lineCount;

    // 分配足够的空间
    DWRITE_LINE_METRICS* pLineMetrics = new DWRITE_LINE_METRICS[lineCount];
    UINT32 actualLineCount = 0;
    pLayout->GetLineMetrics(pLineMetrics, lineCount, &actualLineCount);

    for (UINT32 i = 0; i < actualLineCount; i++) {
        // pLineMetrics[i].length       - 这一行的字符数
        // pLineMetrics[i].height       - 这一行的总高度
        // pLineMetrics[i].baseline     - 基线高度（从行顶部算起）
        // pLineMetrics[i].isNewLine    - 是否是换行后的新行
    }

    delete[] pLineMetrics;
}
```

### 命中测试

命中测试（Hit Testing）是文字交互的核心——你需要在鼠标点击的位置确定它对应的是哪个字符。这在实现文字选择、光标定位、超链接点击等功能时非常关键。

```cpp
void HitTest(IDWriteTextLayout* pLayout, float x, float y)
{
    BOOL isTrailingHit;     // 是否击中了字符的尾部（用于光标放置）
    BOOL isInside;          // 点是否在文字区域内
    DWRITE_HIT_TEST_METRICS hitMetrics;

    pLayout->HitTestPoint(x, y, &isTrailingHit, &isInside, &hitMetrics);

    // hitMetrics.textPosition     - 击中的字符位置（0-based）
    // hitMetrics.length           - 命中范围的字符长度（通常是 1）
    // hitMetrics.left, top        - 命中区域的左上角坐标
    // hitMetrics.width, height    - 命中区域的尺寸
    // hitMetrics.bidiLevel        - 双向文字级别

    if (isInside) {
        wchar_t msg[128];
        swprintf_s(msg, 128, L"Hit char at position %d",
                  hitMetrics.textPosition);
    }
}
```

`HitTestPoint` 的 `isTrailingHit` 参数很有意思。它告诉你鼠标点落在字符的前半部分还是后半部分。在实现文字编辑器时，如果点落在字符前半部分，光标应该放在字符前面；如果点落在后半部分（尾部），光标应该放在字符后面。

除了 `HitTestPoint`，还有一个 `HitTestTextPosition` 方法，它的作用正好反过来——给定一个文字位置，返回这个位置在布局中的坐标。这在实现光标绘制时非常常用：

```cpp
void GetCaretPosition(IDWriteTextLayout* pLayout, UINT32 textPosition, BOOL isTrailing)
{
    FLOAT x, y;
    DWRITE_HIT_TEST_METRICS hitMetrics;

    pLayout->HitTestTextPosition(textPosition, isTrailing, &x, &y, &hitMetrics);

    // x, y 就是光标在 TextLayout 坐标系中的位置
    // 加上 DrawTextLayout 时指定的偏移量，就是屏幕上的实际位置
}
```

## 第五步——代码高亮渲染器实战

现在我们把前面学到的所有东西组合起来，实现一个简单的代码高亮渲染器。这个渲染器会对 C++ 代码中的关键字、字符串、注释分别着色。

```cpp
#include <vector>
#include <string>

// 简单的词法分析器——识别关键字、字符串、注释
struct TokenRange {
    DWRITE_TEXT_RANGE range;
    enum TokenType {
        TOKEN_KEYWORD,
        TOKEN_STRING,
        TOKEN_COMMENT,
        TOKEN_NUMBER,
        TOKEN_NORMAL
    } type;
};

// C++ 关键字列表（简化版）
const wchar_t* g_Keywords[] = {
    L"int", L"void", L"return", L"if", L"else", L"for", L"while",
    L"class", L"struct", L"public", L"private", L"protected",
    L"const", L"static", L"new", L"delete", L"nullptr", L"true",
    L"false", L"auto", L"namespace", L"using", L"include", L"template",
    L"typename", L"virtual", L"override", L"final", L"enum", L"switch",
    L"case", L"break", L"continue", L"do", L"typedef", L"sizeof"
};

bool IsKeyword(const std::wstring& word)
{
    for (int i = 0; i < _countof(g_Keywords); i++) {
        if (word == g_Keywords[i]) return true;
    }
    return false;
}

std::vector<TokenRange> Tokenize(const std::wstring& code)
{
    std::vector<TokenRange> tokens;
    size_t i = 0;
    size_t len = code.length();

    while (i < len) {
        // 跳过空白
        if (iswspace(code[i])) {
            i++;
            continue;
        }

        // 单行注释 //
        if (i + 1 < len && code[i] == L'/' && code[i+1] == L'/') {
            size_t start = i;
            while (i < len && code[i] != L'\n') i++;
            tokens.push_back({ {(UINT32)start, (UINT32)(i - start)},
                              TokenRange::TOKEN_COMMENT });
            continue;
        }

        // 多行注释 /* */
        if (i + 1 < len && code[i] == L'/' && code[i+1] == L'*') {
            size_t start = i;
            i += 2;
            while (i + 1 < len && !(code[i] == L'*' && code[i+1] == L'/')) i++;
            if (i + 1 < len) i += 2;
            tokens.push_back({ {(UINT32)start, (UINT32)(i - start)},
                              TokenRange::TOKEN_COMMENT });
            continue;
        }

        // 字符串 "..."
        if (code[i] == L'"') {
            size_t start = i;
            i++;
            while (i < len && code[i] != L'"') {
                if (code[i] == L'\\') i++;  // 跳过转义字符
                i++;
            }
            if (i < len) i++;  // 包含结束引号
            tokens.push_back({ {(UINT32)start, (UINT32)(i - start)},
                              TokenRange::TOKEN_STRING });
            continue;
        }

        // 标识符/关键字
        if (iswalpha(code[i]) || code[i] == L'_') {
            size_t start = i;
            while (i < len && (iswalnum(code[i]) || code[i] == L'_')) i++;
            std::wstring word = code.substr(start, i - start);
            tokens.push_back({ {(UINT32)start, (UINT32)(i - start)},
                              IsKeyword(word) ? TokenRange::TOKEN_KEYWORD
                                              : TokenRange::TOKEN_NORMAL });
            continue;
        }

        // 数字
        if (iswdigit(code[i])) {
            size_t start = i;
            while (i < len && (iswdigit(code[i]) || code[i] == L'.')) i++;
            tokens.push_back({ {(UINT32)start, (UINT32)(i - start)},
                              TokenRange::TOKEN_NUMBER });
            continue;
        }

        // 其他字符
        i++;
    }

    return tokens;
}
```

这个词法分析器虽然简陋，但足以演示 DirectWrite 的多格式排版能力。接下来我们把分析结果应用到 TextLayout 上：

```cpp
// 颜色方案
struct SyntaxColors {
    D2D1_COLOR_F keyword;
    D2D1_COLOR_F string;
    D2D1_COLOR_F comment;
    D2D1_COLOR_F number;
    D2D1_COLOR_F normal;
};

// 经典暗色主题
const SyntaxColors g_DarkTheme = {
    D2D1::ColorF(0x569CD6),    // 关键字：蓝色
    D2D1::ColorF(0xCE9178),    // 字符串：橙色
    D2D1::ColorF(0x6A9955),    // 注释：绿色
    D2D1::ColorF(0xB5CEA8),    // 数字：浅绿
    D2D1::ColorF(0xD4D4D4)     // 普通：浅灰
};

void ApplySyntaxHighlighting(ID2D1DeviceContext* pDC,
                              IDWriteTextLayout* pLayout,
                              const std::vector<TokenRange>& tokens,
                              const SyntaxColors& colors)
{
    for (const auto& token : tokens) {
        ID2D1SolidColorBrush* pBrush = nullptr;

        switch (token.type) {
        case TokenRange::TOKEN_KEYWORD:
            pDC->CreateSolidColorBrush(colors.keyword, &pBrush);
            // 关键字加粗
            pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, token.range);
            break;
        case TokenRange::TOKEN_STRING:
            pDC->CreateSolidColorBrush(colors.string, &pBrush);
            break;
        case TokenRange::TOKEN_COMMENT:
            pDC->CreateSolidColorBrush(colors.comment, &pBrush);
            // 注释用斜体
            pLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, token.range);
            break;
        case TokenRange::TOKEN_NUMBER:
            pDC->CreateSolidColorBrush(colors.number, &pBrush);
            break;
        default:
            pDC->CreateSolidColorBrush(colors.normal, &pBrush);
            break;
        }

        pLayout->SetDrawingEffect(pBrush, token.range);
        pBrush->Release();
    }
}
```

现在我们把所有部分组装在一起。完整的渲染流程是这样的：先创建 TextLayout，然后对代码文本进行词法分析，再把分析结果应用到 TextLayout 上，最后用 DrawTextLayout 渲染：

```cpp
void RenderCodeView(ID2D1DeviceContext* pDC, IDWriteTextFormat* pCodeFormat,
                    const wchar_t* code, float maxWidth, float maxHeight)
{
    // 1. 创建 TextLayout
    IDWriteTextLayout* pLayout = nullptr;
    g_pDWriteFactory->CreateTextLayout(
        code, (UINT32)wcslen(code), pCodeFormat,
        maxWidth, maxHeight, &pLayout
    );
    if (!pLayout) return;

    // 2. 词法分析
    std::vector<TokenRange> tokens = Tokenize(code);

    // 3. 应用语法高亮
    ApplySyntaxHighlighting(pDC, pLayout, tokens, g_DarkTheme);

    // 4. 渲染
    pDC->BeginDraw();
    pDC->Clear(D2D1::ColorF(0x1E1E1E));  // VS Code 暗色背景

    // 绘制行号区域背景
    ID2D1SolidColorBrush* pLineNumBg = nullptr;
    pDC->CreateSolidColorBrush(D2D1::ColorF(0x252526), &pLineNumBg);
    pDC->FillRectangle(D2D1::RectF(0, 0, 50, maxHeight), pLineNumBg);

    // 绘制代码文字
    ID2D1SolidColorBrush* pDefaultBrush = nullptr;
    pDC->CreateSolidColorBrush(g_DarkTheme.normal, &pDefaultBrush);

    pDC->DrawTextLayout(
        D2D1::Point2F(60.0f, 10.0f),
        pLayout,
        pDefaultBrush
    );

    pLineNumBg->Release();
    pDefaultBrush->Release();
    pLayout->Release();

    pDC->EndDraw();
}
```

这个代码高亮渲染器虽然功能简单，但它展示了 DirectWrite 多格式排版的核心工作流程：先创建统一的 TextLayout，然后通过 DWRITE_TEXT_RANGE 对不同文字范围设置不同的格式和颜色，最后一次性渲染。这种方式的性能远优于"每种颜色单独调用一次 DrawText"的做法，因为文字只需要进行一次排版分析。

### 性能优化提示

如果你的代码编辑器需要频繁更新（比如用户正在输入），每次输入都重新创建 TextLayout 和所有画刷是不现实的。更好的做法是缓存 TextLayout 对象，只在文字内容变化时重新创建；画刷对象则应该缓存为类成员，不要在每次绘制时创建和释放。

另外，对于很长的代码文件，你可以只对可见区域创建 TextLayout，不可见部分不需要排版。这是一种"虚拟化"的思路，在实现滚动编辑器时非常重要。

## 内联对象：图文混排

最后我们简单提一下 IDWriteTextLayout 的另一个强大功能——内联对象（Inline Object）。内联对象允许你在文字行内嵌入任意的非文字内容，比如小图标、公式、进度条等。

实现内联对象需要继承 IDWriteInlineObject 接口并实现三个方法：Draw（绘制内容）、GetMetrics（返回对象的尺寸）、GetOverhangMetrics（返回对象的悬挂尺寸）、GetBreakConditions（返回断行条件）。

这个功能比较高级，本篇不展开完整实现，但给出一个使用场景的思路：假设你要在文字中嵌入一个小图标，你可以创建一个实现了 IDWriteInlineObject 的图标对象，然后用 `pLayout->SetInlineObject(pIconObject, range)` 把它插入到指定位置。DirectWrite 在排版时会为这个对象预留空间，然后在渲染时调用它的 Draw 方法来绘制图标。

## 常见问题与踩坑总结

### IDWriteFactory SHARED vs ISOLATED 到底选哪个

99% 的场景选 SHARED 就对了。ISOLATED 模式下你的应用会维护独立的字体缓存，这意味着更大的内存消耗和更长的字体加载时间。只有在特殊场景下才需要——比如你的应用是一个设计工具，需要加载大量私有字体，不希望和系统的字体缓存互相干扰。

### 中文字体出现方块字

这通常是因为指定的字体族不包含中文字符，且字体回退没有正确工作。解决方案：一是确保区域设置正确（`L"zh-CN"`），二是确保系统安装了中文字体（Windows 默认有 Microsoft YaHei），三是如果你使用了自定义字体集合，确保它包含了支持中文的字体。

### SetDrawingEffect 的对象生命周期

IDWriteTextLayout 会对传入的 DrawingEffect 对象调用 AddRef，但不会在布局销毁时通知你。如果你在设置完 DrawingEffect 后立即释放了画刷对象，而布局还在使用，可能会导致访问已释放的内存。正确的做法是确保画刷对象的生命周期覆盖布局对象的生命周期。

### TextLayout 不可复用

IDWriteTextLayout 是和特定的文字内容绑定的。如果文字内容变了（比如用户输入了新字符），你不能修改现有 TextLayout 的文字——必须销毁旧的，创建一个新的。这是因为排版分析是针对特定文字内容进行的，文字变了就需要重新分析。

### DWRITE_TEXT_RANGE 的位置计算

DWRITE_TEXT_RANGE 的 startPosition 是以字符为单位的，不是字节。在 Unicode 环境下，一个中文字符占一个 position，一个英文字母也占一个 position。但要注意，Surrogate Pair（代理对，比如 Emoji）占两个 position。如果你的文字包含 Emoji，在计算 range 时需要特别小心。

## 总结

这一篇我们把 DirectWrite 的三层 API 架构过了一遍。IDWriteTextFormat 是不可变的基础格式定义，IDWriteTextLayout 是可变的排版引擎，支持通过 DWRITE_TEXT_RANGE 对任意文字范围设置不同的格式和颜色，IDWriteTextRenderer 则提供了完全的绘制控制权。

我们重点实践了 TextLayout 的多格式排版能力，通过一个代码高亮渲染器的例子，展示了如何对同一段文字的不同部分设置关键字加粗蓝色、字符串橙色、注释绿色斜体等效果。同时我们也学习了 TextLayout 的测量和命中测试功能，这些在实现交互式文字 UI 时非常关键。

下一篇我们要进入 Direct2D 与 GDI 的互操作世界——在实际项目中，你不可能一次性把所有 GDI 代码都重写成 D2D，渐进式迁移才是现实的做法。我们来看看 DCRenderTarget 和 GdiInteropRenderTarget 这两个关键接口。

---

## 练习

1. **多格式富文本编辑器**：创建一个简单的文本显示窗口，加载一段包含中英文混合的文字内容，对中文部分设置 Microsoft YaHei 字体，对英文部分设置 Segoe UI 字体，对标题部分设置更大的字号和加粗。

2. **文字选择高亮**：实现鼠标拖拽选择文字的功能——用 HitTestPoint 获取起始和结束位置，然后用 SetDrawingEffect 给选中范围添加半透明的蓝色背景。提示：你可能需要实现 IDWriteTextRenderer 来绘制选择背景。

3. **行号显示**：在代码高亮渲染器的基础上，使用 GetLineMetrics 获取行信息，在左侧行号区域绘制行号。要求行号和代码行精确对齐。

4. **URL 超链接检测**：在一段文字中检测 URL 模式（以 http:// 或 https:// 开头），对这些范围设置蓝色下划线格式。当鼠标悬停在 URL 上时（使用 HitTestPoint 检测），显示手形光标。

---

**参考资料**:
- [IDWriteFactory interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefactory)
- [IDWriteFactory::CreateTextFormat - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritefactory-createtextformat)
- [IDWriteFactory::CreateTextLayout - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritefactory-createtextlayout)
- [IDWriteTextLayout interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextlayout)
- [IDWriteTextLayout::SetDrawingEffect - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritetextlayout-setdrawingeffect)
- [DWRITE_TEXT_RANGE structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_text_range)
- [ID2D1RenderTarget::DrawTextLayout - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-drawtextlayout)
- [Text Formatting and Layout - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directwrite/text-formatting-and-layout)
- [Getting Started with DirectWrite - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directwrite/getting-started-with-directwrite)
- [IDWriteFontFallback interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_2/nn-dwrite_2-idwritefontfallback)
- [How to Add Client Drawing Effects - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directwrite/how-to-add-custom-drawing-efffects-to-a-text-layout)
- [Rendering by using Direct2D - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/directwrite/rendering-by-using-direct2d)
