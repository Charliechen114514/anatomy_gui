/**
 * 04_text_rendering - GDI 文字渲染示例
 *
 * 本程序演示了 Win32 GDI 中文字绘制相关的 API：
 * 1. TextOut                         — 在固定位置绘制简单文本
 * 2. DrawText (DT_CENTER 等)         — 在矩形区域内绘制格式化文本
 * 3. SetTextColor / SetBkColor       — 文字颜色和背景色控制
 *    SetBkMode(TRANSPARENT)          — 透明背景模式
 * 4. SetTextAlign                    — 文本对齐方式（左/中/右）
 * 5. GetTextMetrics                  — 获取字体度量信息
 * 6. LOGFONT / CreateFontIndirect    — 创建自定义字体
 *
 * 参考: tutorial/native_win32/21_ProgrammingGUI_NativeWindows_GDI_Text.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cstdio>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GDITextRenderClass";

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    int placeholder;     // 预留
};

// ============================================================================
// 辅助绘图函数
// ============================================================================

/**
 * 绘制分区标题栏
 */
static void DrawSectionTitle(HDC hdc, const RECT& rc, const wchar_t* title, COLORREF bgColor)
{
    int saved = SaveDC(hdc);

    HBRUSH hBrush = CreateSolidBrush(bgColor);
    RECT titleRc = { rc.left, rc.top, rc.right, rc.top + 24 };
    FillRect(hdc, &titleRc, hBrush);
    DeleteObject(hBrush);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, title, -1, &titleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 分区边框
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    SelectObject(hdc, hPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    DeleteObject(hPen);

    RestoreDC(hdc, saved);
}

// ============================================================================
// 各分区绘制函数
// ============================================================================

/**
 * 分区 1: TextOut — 简单文本绘制
 *
 * TextOut 是最基本的文字绘制函数，在指定的坐标位置输出字符串。
 * 它不关心文本的布局和格式，只是简单地从 (x, y) 开始绘制。
 */
static void DrawTextOutSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"1. TextOut 简单文本", RGB(33, 150, 243));

    int saved = SaveDC(hdc);

    SetBkMode(hdc, TRANSPARENT);

    // 使用默认字体绘制
    SetTextColor(hdc, RGB(33, 33, 33));
    TextOut(hdc, rc.left + 12, rc.top + 32, L"TextOut 默认字体",
            (int)wcslen(L"TextOut 默认字体"));

    // 创建一个大号字体
    HFONT hBigFont = CreateFont(
        -22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hBigFont);
    SetTextColor(hdc, RGB(33, 150, 243));
    TextOut(hdc, rc.left + 12, rc.top + 55, L"TextOut 大号粗体",
            (int)wcslen(L"TextOut 大号粗体"));

    // 创建一个小号字体
    HFONT hSmallFont = CreateFont(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hSmallFont);
    SetTextColor(hdc, RGB(100, 100, 100));
    TextOut(hdc, rc.left + 12, rc.top + 85, L"TextOut 小号字体 — 坐标 (x, y) 直接指定位置",
            (int)wcslen(L"TextOut 小号字体 — 坐标 (x, y) 直接指定位置"));

    // 绘制参考点标记
    HPEN hDotPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    SelectObject(hdc, hDotPen);
    MoveToEx(hdc, rc.left + 12, rc.top + 32, nullptr);
    LineTo(hdc, rc.left + 22, rc.top + 32);

    SetTextColor(hdc, RGB(200, 50, 50));
    TextOut(hdc, rc.left + 25, rc.top + 25, L"<- 绘制起点", 8);
    DeleteObject(hDotPen);

    DeleteObject(hBigFont);
    DeleteObject(hSmallFont);
    RestoreDC(hdc, saved);
}

/**
 * 分区 2: DrawText — 格式化文本绘制
 *
 * DrawText 支持在矩形区域内按照指定的格式绘制文本：
 * - DT_CENTER: 水平居中
 * - DT_VCENTER: 垂直居中（需配合 DT_SINGLELINE）
 * - DT_WORDBREAK: 自动换行
 */
static void DrawDrawTextSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"2. DrawText 格式化文本", RGB(76, 175, 80));

    int saved = SaveDC(hdc);

    // DT_CENTER | DT_VCENTER | DT_SINGLELINE — 居中单行
    RECT box1 = { rc.left + 8, rc.top + 30, rc.right - 8, rc.top + 60 };
    HBRUSH hBg1 = CreateSolidBrush(RGB(232, 245, 233));
    FillRect(hdc, &box1, hBg1);
    DeleteObject(hBg1);

    SetTextColor(hdc, RGB(27, 94, 32));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"DT_CENTER | DT_VCENTER | DT_SINGLELINE",
             -1, &box1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // DT_LEFT — 左对齐
    RECT box2 = { rc.left + 8, rc.top + 66, rc.right - 8, rc.top + 86 };
    SetTextColor(hdc, RGB(33, 33, 33));
    DrawText(hdc, L"DT_LEFT 左对齐文本", -1, &box2, DT_LEFT | DT_SINGLELINE);

    // DT_WORDBREAK — 自动换行
    RECT box3 = { rc.left + 8, rc.top + 92, rc.right - 8, rc.bottom - 8 };
    HBRUSH hBg3 = CreateSolidBrush(RGB(255, 249, 196));
    FillRect(hdc, &box3, hBg3);
    DeleteObject(hBg3);

    SetTextColor(hdc, RGB(130, 100, 0));
    DrawText(hdc,
        L"DT_WORDBREAK 自动换行模式："
        L"DrawText 会根据矩形宽度自动在单词边界处断行，"
        L"实现多行文本的排版效果。",
        -1, &box3, DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL);

    // 绘制边框参考线
    HPEN hRefPen = CreatePen(PS_DOT, 1, RGB(76, 175, 80));
    SelectObject(hdc, hRefPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, box3.left, box3.top, box3.right, box3.bottom);
    DeleteObject(hRefPen);

    RestoreDC(hdc, saved);
}

/**
 * 分区 3: SetTextColor / SetBkColor / SetBkMode — 文字颜色控制
 *
 * 演示：
 * - SetTextColor 设置文字颜色
 * - SetBkColor 设置背景颜色（当 SetBkMode 为 OPAQUE 时有效）
 * - SetBkMode(TRANSPARENT) 使文字背景透明
 * - SetBkMode(OPAQUE) 使用当前背景色填充文字背景
 */
static void DrawColorSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"3. 文字颜色 SetTextColor / SetBkMode", RGB(244, 67, 54));

    int saved = SaveDC(hdc);

    HFONT hDemoFont = CreateFont(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hDemoFont);

    int y = rc.top + 30;

    // 1. OPAQUE 模式 — 用背景色填充文字区域
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(244, 67, 54));
    SetBkMode(hdc, OPAQUE);
    TextOut(hdc, rc.left + 12, y, L"OPAQUE: 白字红底 SetBkColor(RGB(244,67,54))",
            (int)wcslen(L"OPAQUE: 白字红底 SetBkColor(RGB(244,67,54))"));

    y += 26;

    // 2. OPAQUE 模式 — 换一种颜色
    SetTextColor(hdc, RGB(33, 33, 33));
    SetBkColor(hdc, RGB(255, 235, 59));
    SetBkMode(hdc, OPAQUE);
    TextOut(hdc, rc.left + 12, y, L"OPAQUE: 黑字黄底 SetBkColor(RGB(255,235,59))",
            (int)wcslen(L"OPAQUE: 黑字黄底 SetBkColor(RGB(255,235,59))"));

    y += 26;

    // 3. TRANSPARENT 模式 — 背景透明
    // 先画一条灰色底纹
    RECT stripeRc = { rc.left + 8, y - 2, rc.right - 8, y + 22 };
    HBRUSH hStripe = CreateSolidBrush(RGB(220, 220, 220));
    FillRect(hdc, &stripeRc, hStripe);
    DeleteObject(hStripe);

    SetTextColor(hdc, RGB(33, 150, 243));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, rc.left + 12, y, L"TRANSPARENT: 蓝色文字，背景透出灰色底纹",
            (int)wcslen(L"TRANSPARENT: 蓝色文字，背景透出灰色底纹"));

    y += 30;

    // 4. 颜色混合演示
    SetTextColor(hdc, RGB(156, 39, 176));
    SetBkColor(hdc, RGB(225, 190, 231));
    SetBkMode(hdc, OPAQUE);
    TextOut(hdc, rc.left + 12, y, L"SetTextColor(RGB(156,39,176))",
            (int)wcslen(L"SetTextColor(RGB(156,39,176))"));

    y += 26;
    SetTextColor(hdc, RGB(0, 150, 136));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, rc.left + 12, y, L"SetBkMode(TRANSPARENT) 透明背景",
            (int)wcslen(L"SetBkMode(TRANSPARENT) 透明背景"));

    DeleteObject(hDemoFont);
    RestoreDC(hdc, saved);
}

/**
 * 分区 4: SetTextAlign — 文本对齐方式
 *
 * 演示三种基本对齐方式：
 * - TA_LEFT: 文本从参考点向右延伸
 * - TA_CENTER: 文本以参考点为中心
 * - TA_RIGHT: 文本从参考点向左延伸
 *
 * 同时演示 TA_TOP 和 TA_BOTTOM。
 */
static void DrawAlignSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"4. SetTextAlign 对齐方式", RGB(156, 39, 176));

    int saved = SaveDC(hdc);

    HFONT hDemoFont = CreateFont(
        -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hDemoFont);
    SetBkMode(hdc, TRANSPARENT);

    // 中心参考线（垂直）
    int refX = (rc.left + rc.right) / 2;
    HPEN hRefPen = CreatePen(PS_DOT, 1, RGB(200, 200, 200));
    SelectObject(hdc, hRefPen);
    MoveToEx(hdc, refX, rc.top + 28, nullptr);
    LineTo(hdc, refX, rc.bottom - 5);
    DeleteObject(hRefPen);

    // 在参考线上绘制红点标记
    HBRUSH hRedDot = CreateSolidBrush(RGB(255, 0, 0));
    int y = rc.top + 38;

    // TA_LEFT — 参考点在文本左侧
    {
        SelectObject(hdc, hRedDot);
        Ellipse(hdc, refX - 3, y + 5, refX + 3, y + 11);

        SetTextAlign(hdc, TA_LEFT | TA_TOP);
        SetTextColor(hdc, RGB(33, 150, 243));
        TextOut(hdc, refX + 5, y, L"TA_LEFT <- 参考点在左侧",
                (int)wcslen(L"TA_LEFT <- 参考点在左侧"));
        y += 28;
    }

    // TA_CENTER — 参考点在文本中间
    {
        SelectObject(hdc, hRedDot);
        Ellipse(hdc, refX - 3, y + 5, refX + 3, y + 11);

        SetTextAlign(hdc, TA_CENTER | TA_TOP);
        SetTextColor(hdc, RGB(76, 175, 80));
        TextOut(hdc, refX, y, L"-> TA_CENTER <- 参考点居中",
                (int)wcslen(L"-> TA_CENTER <- 参考点居中"));
        y += 28;
    }

    // TA_RIGHT — 参考点在文本右侧
    {
        SelectObject(hdc, hRedDot);
        Ellipse(hdc, refX - 3, y + 5, refX + 3, y + 11);

        SetTextAlign(hdc, TA_RIGHT | TA_TOP);
        SetTextColor(hdc, RGB(244, 67, 54));
        TextOut(hdc, refX - 5, y, L"参考点在右侧 -> TA_RIGHT",
                (int)wcslen(L"参考点在右侧 -> TA_RIGHT"));
        y += 28;
    }

    // 恢复对齐方式
    SetTextAlign(hdc, TA_LEFT | TA_TOP);
    DeleteObject(hRedDot);

    // 说明文字
    HFONT hSmallFont = CreateFont(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    SelectObject(hdc, hSmallFont);
    SetTextColor(hdc, RGB(120, 120, 120));
    RECT noteRc = { rc.left + 8, y + 2, rc.right - 8, rc.bottom - 5 };
    DrawText(hdc, L"红点为参考点位置，SetTextAlign 决定文本相对于参考点的对齐方向",
             -1, &noteRc, DT_LEFT | DT_WORDBREAK);
    DeleteObject(hSmallFont);

    DeleteObject(hDemoFont);
    RestoreDC(hdc, saved);
}

/**
 * 分区 5: GetTextMetrics — 字体度量
 *
 * 获取并显示当前字体的度量信息：
 * - tmHeight: 字符总高度
 * - tmAscent: 基线以上的高度
 * - tmDescent: 基线以下的高度
 * - tmInternalLeading: 内部行距
 * - tmExternalLeading: 外部行距
 * - tmAveCharWidth: 平均字符宽度
 */
static void DrawMetricsSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"5. GetTextMetrics 字体度量", RGB(0, 150, 136));

    int saved = SaveDC(hdc);

    // 使用等宽字体显示度量数据
    HFONT hMetricFont = CreateFont(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );
    SelectObject(hdc, hMetricFont);

    // 获取字体度量
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);

    SetTextColor(hdc, RGB(33, 33, 33));
    SetBkMode(hdc, TRANSPARENT);

    // 显示度量信息
    wchar_t lines[8][80];
    wsprintf(lines[0], L"tmHeight         = %d", tm.tmHeight);
    wsprintf(lines[1], L"tmAscent         = %d", tm.tmAscent);
    wsprintf(lines[2], L"tmDescent        = %d", tm.tmDescent);
    wsprintf(lines[3], L"tmInternalLeading= %d", tm.tmInternalLeading);
    wsprintf(lines[4], L"tmExternalLeading= %d", tm.tmExternalLeading);
    wsprintf(lines[5], L"tmAveCharWidth   = %d", tm.tmAveCharWidth);
    wsprintf(lines[6], L"tmMaxCharWidth   = %d", tm.tmMaxCharWidth);
    wsprintf(lines[7], L"tmWeight         = %d", tm.tmWeight);

    int y = rc.top + 30;
    for (int i = 0; i < 8; ++i)
    {
        TextOut(hdc, rc.left + 12, y, lines[i], (int)wcslen(lines[i]));
        y += 20;
    }

    // 绘制字体度量示意图
    int diagramY = rc.bottom - 55;
    int diagramH = 40;

    // 基线
    HPEN hBasePen = CreatePen(PS_SOLID, 2, RGB(0, 150, 136));
    SelectObject(hdc, hBasePen);
    MoveToEx(hdc, rc.left + 12, diagramY + tm.tmAscent, nullptr);
    // 用实际 ascent 比例绘制
    int scale = diagramH / (tm.tmHeight > 0 ? tm.tmHeight : 1);
    int baseY = diagramY + tm.tmAscent * scale;
    int topY = diagramY;

    MoveToEx(hdc, rc.left + 12, baseY, nullptr);
    LineTo(hdc, rc.right - 12, baseY);
    DeleteObject(hBasePen);

    // 标注基线
    SetTextColor(hdc, RGB(0, 150, 136));
    TextOut(hdc, rc.right - 60, baseY - 18, L"基线", 2);

    // Ascent 区域
    HBRUSH hAscBrush = CreateSolidBrush(RGB(178, 223, 219));
    RECT ascRc = { rc.left + 12, topY, rc.right - 12, baseY };
    FillRect(hdc, &ascRc, hAscBrush);
    DeleteObject(hAscBrush);

    SetTextColor(hdc, RGB(0, 77, 64));
    TextOut(hdc, rc.left + 15, topY + 2, L"Ascent", 6);

    // Descent 区域
    int descBottom = baseY + tm.tmDescent * scale;
    HBRUSH hDescBrush = CreateSolidBrush(RGB(255, 204, 188));
    RECT descRc = { rc.left + 12, baseY, rc.right - 12, descBottom };
    FillRect(hdc, &descRc, hDescBrush);
    DeleteObject(hDescBrush);

    SetTextColor(hdc, RGB(191, 54, 12));
    TextOut(hdc, rc.left + 15, baseY + 2, L"Descent", 7);

    DeleteObject(hMetricFont);
    RestoreDC(hdc, saved);
}

/**
 * 分区 6: LOGFONT / CreateFontIndirect — 自定义字体
 *
 * 使用 LOGFONT 结构体定义三种不同风格的字体并渲染文字：
 * - 大号粗体（标题风格）
 * - 斜体（引用风格）
 * - 小号（注释风格）
 */
static void DrawCustomFontSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"6. LOGFONT / CreateFontIndirect", RGB(103, 58, 183));

    int saved = SaveDC(hdc);

    // 字体 1: 大号粗体
    {
        LOGFONT lf = {};
        lf.lfHeight         = -26;
        lf.lfWeight         = FW_BOLD;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, L"微软雅黑");

        HFONT hFont = CreateFontIndirect(&lf);
        SelectObject(hdc, hFont);

        SetTextColor(hdc, RGB(103, 58, 183));
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, rc.left + 12, rc.top + 30,
                L"大号粗体 26px FW_BOLD",
                (int)wcslen(L"大号粗体 26px FW_BOLD"));

        // 显示 LOGFONT 参数
        HFONT hSmallFont = CreateFont(
            -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
        );
        SelectObject(hdc, hSmallFont);
        SetTextColor(hdc, RGB(150, 120, 180));
        TextOut(hdc, rc.left + 12, rc.top + 60,
                L"lfHeight=-26 lfWeight=FW_BOLD",
                (int)wcslen(L"lfHeight=-26 lfWeight=FW_BOLD"));
        DeleteObject(hSmallFont);
        DeleteObject(hFont);
    }

    // 字体 2: 斜体
    {
        LOGFONT lf = {};
        lf.lfHeight         = -20;
        lf.lfItalic         = TRUE;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, L"微软雅黑");

        HFONT hFont = CreateFontIndirect(&lf);
        SelectObject(hdc, hFont);

        SetTextColor(hdc, RGB(33, 150, 243));
        TextOut(hdc, rc.left + 12, rc.top + 80,
                L"斜体 20px lfItalic=TRUE",
                (int)wcslen(L"斜体 20px lfItalic=TRUE"));
        DeleteObject(hFont);
    }

    // 字体 3: 小号带下划线
    {
        LOGFONT lf = {};
        lf.lfHeight         = -14;
        lf.lfUnderline      = TRUE;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, L"微软雅黑");

        HFONT hFont = CreateFontIndirect(&lf);
        SelectObject(hdc, hFont);

        SetTextColor(hdc, RGB(76, 175, 80));
        TextOut(hdc, rc.left + 12, rc.top + 108,
                L"小号下划线 14px lfUnderline=TRUE",
                (int)wcslen(L"小号下划线 14px lfUnderline=TRUE"));
        DeleteObject(hFont);
    }

    // 说明 LOGFONT 结构
    {
        HFONT hNoteFont = CreateFont(
            -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        SelectObject(hdc, hNoteFont);
        SetTextColor(hdc, RGB(120, 120, 120));
        RECT noteRc = { rc.left + 8, rc.top + 135, rc.right - 8, rc.bottom - 5 };
        DrawText(hdc,
            L"LOGFONT 定义字体属性:\n"
            L"lfHeight, lfWeight, lfItalic, lfUnderline, lfFaceName...",
            -1, &noteRc, DT_LEFT | DT_WORDBREAK);
        DeleteObject(hNoteFont);
    }

    RestoreDC(hdc, saved);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        AppState* state = new AppState();
        state->placeholder = 0;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;
    }

    case WM_DESTROY:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int saved = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充背景
        HBRUSH hBg = CreateSolidBrush(RGB(250, 250, 250));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 3 列 2 行网格布局
        int cols = 3;
        int rows = 2;
        int padX = 5, padY = 5;
        int cellW = (clientRc.right - padX * (cols + 1)) / cols;
        int cellH = (clientRc.bottom - padY * (rows + 1)) / rows;

        RECT cells[6];
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                int idx = r * cols + c;
                cells[idx].left   = padX + c * (cellW + padX);
                cells[idx].top    = padY + r * (cellH + padY);
                cells[idx].right  = cells[idx].left + cellW;
                cells[idx].bottom = cells[idx].top + cellH;
            }
        }

        // 绘制六个分区
        DrawTextOutSection(hdc, cells[0]);
        DrawDrawTextSection(hdc, cells[1]);
        DrawColorSection(hdc, cells[2]);
        DrawAlignSection(hdc, cells[3]);
        DrawMetricsSection(hdc, cells[4]);
        DrawCustomFontSection(hdc, cells[5]);

        RestoreDC(hdc, saved);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 创建主窗口
// ============================================================================

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"GDI 文字渲染示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        650, 550,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr)
        return nullptr;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
