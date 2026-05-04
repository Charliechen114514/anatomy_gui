/**
 * 03_hit_test - 命中测试与事件路由示例
 *
 * 本程序演示了 Win32 中非客户区命中测试 (Hit Testing) 的机制：
 * 1. WM_NCHITTEST — 自定义非客户区区域的命中测试
 * 2. 创建自定义标题栏区域（顶部 40 像素）
 * 3. HTCLIENT / HTCAPTION / HTCLOSE / HTMAXBUTTON 返回值
 * 4. PtInRect / PtInRegion 基于区域的命中测试
 * 5. WM_NCLBUTTONDOWN — 处理自定义标题栏按钮的点击
 * 6. WM_NCPAINT — 自定义非客户区渲染
 * 7. 模拟窗口框架，高亮悬停区域（标题栏=蓝色，关闭=红色，主体=白色）
 *
 * 参考: tutorial/native_win32/50_ProgrammingGUI_Graphics_CustomCtrl_HitTest.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"HitTestDemoClass";

// 自定义标题栏高度（像素）
static const int TITLE_BAR_HEIGHT = 40;

// 自定义标题栏按钮区域常量
static const int BTN_SIZE       = 28;        // 按钮大小
static const int BTN_MARGIN     = 8;         // 按钮到边距的距离

// 自定义命中测试返回值（用于扩展标准 HT* 值）
// 我们将标准 HT* 值映射到自定义区域

// 应用程序状态：跟踪鼠标悬停的区域
enum HoverZone
{
    ZONE_NONE = 0,
    ZONE_CAPTION,       // 标题栏区域
    ZONE_CLOSE,         // 关闭按钮
    ZONE_MAXIMIZE,      // 最大化按钮
    ZONE_MINIMIZE,      // 最小化按钮
    ZONE_CLIENT,        // 客户区（主体）
    ZONE_BORDER_LEFT,   // 左边框
    ZONE_BORDER_RIGHT,  // 右边框
    ZONE_BORDER_TOP,    // 上边框
    ZONE_BORDER_BOTTOM  // 下边框
};

struct AppState
{
    HoverZone hoveredZone;      // 当前悬停的区域
    POINT     lastMousePos;     // 上次鼠标位置（屏幕坐标）
    bool      isMaximized;      // 是否最大化
};

// ============================================================================
// 辅助函数：计算各区域的矩形
// ============================================================================

/**
 * 获取关闭按钮的矩形（屏幕坐标）
 */
static RECT GetCloseBtnRect(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;

    RECT btnRc;
    btnRc.left   = rc.left + w - BTN_MARGIN - BTN_SIZE;
    btnRc.top    = rc.top + (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;
    btnRc.right  = btnRc.left + BTN_SIZE;
    btnRc.bottom = btnRc.top + BTN_SIZE;
    return btnRc;
}

/**
 * 获取最大化按钮的矩形（屏幕坐标）
 */
static RECT GetMaxBtnRect(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;

    RECT btnRc;
    btnRc.left   = rc.left + w - BTN_MARGIN - BTN_SIZE * 2 - 4;
    btnRc.top    = rc.top + (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;
    btnRc.right  = btnRc.left + BTN_SIZE;
    btnRc.bottom = btnRc.top + BTN_SIZE;
    return btnRc;
}

/**
 * 获取最小化按钮的矩形（屏幕坐标）
 */
static RECT GetMinBtnRect(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;

    RECT btnRc;
    btnRc.left   = rc.left + w - BTN_MARGIN - BTN_SIZE * 3 - 8;
    btnRc.top    = rc.top + (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;
    btnRc.right  = btnRc.left + BTN_SIZE;
    btnRc.bottom = btnRc.top + BTN_SIZE;
    return btnRc;
}

/**
 * 获取标题栏矩形（屏幕坐标）
 */
static RECT GetCaptionRect(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    RECT captionRc = { rc.left, rc.top, rc.right, rc.top + TITLE_BAR_HEIGHT };
    return captionRc;
}

// ============================================================================
// 自定义命中测试
// ============================================================================

/**
 * 自定义命中测试函数
 *
 * WM_NCHITTEST 消息让应用程序自定义鼠标位于哪个"区域"。
 * 通过返回不同的 HT* 值，Windows 会自动处理对应的鼠标行为：
 * - HTCAPTION: 允许拖拽移动窗口
 * - HTCLOSE: 系统处理关闭
 * - HTMAXBUTTON: 系统处理最大化
 * - HTMINBUTTON: 系统处理最小化
 * - HTCLIENT: 正常的客户区鼠标消息
 * - HTBORDER: 边框（可调整大小）
 *
 * 这里我们使用 PtInRect 判断鼠标是否在特定矩形区域内。
 */
static HoverZone HitTestToZone(LRESULT ht)
{
    switch (ht)
    {
    case HTCAPTION:   return ZONE_CAPTION;
    case HTCLOSE:     return ZONE_CLOSE;
    case HTMAXBUTTON: return ZONE_MAXIMIZE;
    case HTMINBUTTON: return ZONE_MINIMIZE;
    case HTCLIENT:    return ZONE_CLIENT;
    case HTLEFT:      return ZONE_BORDER_LEFT;
    case HTRIGHT:     return ZONE_BORDER_RIGHT;
    case HTTOP:       return ZONE_BORDER_TOP;
    case HTBOTTOM:    return ZONE_BORDER_BOTTOM;
    default:          return ZONE_NONE;
    }
}

// ============================================================================
// 绘制自定义非客户区
// ============================================================================

/**
 * 绘制自定义标题栏
 * 在 WM_NCPAINT 和 WM_PAINT 中调用
 */
static void DrawCustomTitleBar(HWND hwnd, HDC hdc, const AppState* state)
{
    RECT wndRc;
    GetWindowRect(hwnd, &wndRc);
    int w = wndRc.right - wndRc.left;

    // 标题栏背景 — 根据悬停区域变色
    COLORREF captionBg = RGB(40, 44, 52);   // 默认深灰色
    if (state)
    {
        switch (state->hoveredZone)
        {
        case ZONE_CAPTION:
            captionBg = RGB(50, 70, 110);    // 悬停时变蓝
            break;
        default:
            break;
        }
    }

    // 绘制标题栏背景
    RECT captionRc = { 0, 0, w, TITLE_BAR_HEIGHT };
    HBRUSH hCaptionBrush = CreateSolidBrush(captionBg);
    FillRect(hdc, &captionRc, hCaptionBrush);
    DeleteObject(hCaptionBrush);

    // 绘制窗口标题文字
    SetTextColor(hdc, RGB(220, 220, 220));
    SetBkMode(hdc, TRANSPARENT);
    HFONT hTitleFont = CreateFont(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);

    RECT textRc = { 12, 0, w - BTN_SIZE * 3 - BTN_MARGIN * 3 - 12, TITLE_BAR_HEIGHT };
    DrawText(hdc, L"命中测试示例", -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    DeleteObject(hTitleFont);

    // ========== 绘制标题栏按钮 ==========

    // --- 最小化按钮 ---
    {
        int x = w - BTN_MARGIN - BTN_SIZE * 3 - 8;
        int y = (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;

        // 按钮背景
        COLORREF minBg = (state && state->hoveredZone == ZONE_MINIMIZE)
            ? RGB(80, 90, 100) : RGB(50, 55, 65);
        HBRUSH hMinBrush = CreateSolidBrush(minBg);
        RECT minRc = { x, y, x + BTN_SIZE, y + BTN_SIZE };
        FillRect(hdc, &minRc, hMinBrush);
        DeleteObject(hMinBrush);

        // 最小化图标（一条横线）
        HPEN hMinPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hMinPen);
        MoveToEx(hdc, x + 7, y + BTN_SIZE / 2, nullptr);
        LineTo(hdc, x + BTN_SIZE - 7, y + BTN_SIZE / 2);
        SelectObject(hdc, hOldPen);
        DeleteObject(hMinPen);
    }

    // --- 最大化按钮 ---
    {
        int x = w - BTN_MARGIN - BTN_SIZE * 2 - 4;
        int y = (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;

        COLORREF maxBg = (state && state->hoveredZone == ZONE_MAXIMIZE)
            ? RGB(80, 90, 100) : RGB(50, 55, 65);
        HBRUSH hMaxBrush = CreateSolidBrush(maxBg);
        RECT maxRc = { x, y, x + BTN_SIZE, y + BTN_SIZE };
        FillRect(hdc, &maxRc, hMaxBrush);
        DeleteObject(hMaxBrush);

        // 最大化图标（方框）
        HPEN hMaxPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hMaxPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x + 6, y + 6, x + BTN_SIZE - 6, y + BTN_SIZE - 6);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hMaxPen);
    }

    // --- 关闭按钮 ---
    {
        int x = w - BTN_MARGIN - BTN_SIZE;
        int y = (TITLE_BAR_HEIGHT - BTN_SIZE) / 2;

        // 关闭按钮悬停时变红
        COLORREF closeBg = RGB(232, 17, 35);  // 红色
        if (state && state->hoveredZone != ZONE_CLOSE)
        {
            closeBg = RGB(50, 55, 65);        // 默认灰色
        }
        HBRUSH hCloseBrush = CreateSolidBrush(closeBg);
        RECT closeRc = { x, y, x + BTN_SIZE, y + BTN_SIZE };
        FillRect(hdc, &closeRc, hCloseBrush);
        DeleteObject(hCloseBrush);

        // 关闭图标（X 形）
        HPEN hClosePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hClosePen);
        MoveToEx(hdc, x + 8, y + 8, nullptr);
        LineTo(hdc, x + BTN_SIZE - 8, y + BTN_SIZE - 8);
        MoveToEx(hdc, x + BTN_SIZE - 8, y + 8, nullptr);
        LineTo(hdc, x + 8, y + BTN_SIZE - 8);
        SelectObject(hdc, hOldPen);
        DeleteObject(hClosePen);
    }

    // 标题栏底部分隔线
    HPEN hLinePen = CreatePen(PS_SOLID, 1, RGB(70, 75, 85));
    HPEN hOldLinePen = (HPEN)SelectObject(hdc, hLinePen);
    MoveToEx(hdc, 0, TITLE_BAR_HEIGHT - 1, nullptr);
    LineTo(hdc, w, TITLE_BAR_HEIGHT - 1);
    SelectObject(hdc, hOldLinePen);
    DeleteObject(hLinePen);
}

/**
 * 绘制客户区内容 — 显示命中测试信息和区域示意图
 */
static void DrawClientArea(HWND hwnd, HDC hdc, const AppState* state)
{
    RECT clientRc;
    GetClientRect(hwnd, &clientRc);

    // 填充客户区背景
    HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &clientRc, hBg);
    DeleteObject(hBg);

    // ========== 左侧：区域示意图 ==========
    int margin = 20;
    int diagramW = 220;
    int diagramH = 200;
    int dx = margin;
    int dy = margin + 10;

    // 绘制模拟窗口框架
    // 外框
    HPEN hFramePen = CreatePen(PS_SOLID, 2, RGB(60, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hFramePen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // 整个窗口外框
    Rectangle(hdc, dx, dy, dx + diagramW, dy + diagramH);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hFramePen);

    // 标题栏区域 — 高亮颜色
    COLORREF captionColor = RGB(40, 44, 52);
    if (state && state->hoveredZone == ZONE_CAPTION)
        captionColor = RGB(50, 70, 110);   // 悬停变蓝

    HBRUSH hCaptionBrush = CreateSolidBrush(captionColor);
    RECT simCaptionRc = { dx + 1, dy + 1, dx + diagramW - 1, dy + 30 };
    FillRect(hdc, &simCaptionRc, hCaptionBrush);
    DeleteObject(hCaptionBrush);

    // 模拟窗口中的关闭按钮
    COLORREF closeColor = (state && state->hoveredZone == ZONE_CLOSE)
        ? RGB(232, 17, 35) : RGB(80, 80, 80);
    HBRUSH hSimCloseBrush = CreateSolidBrush(closeColor);
    RECT simCloseRc = { dx + diagramW - 26, dy + 4, dx + diagramW - 6, dy + 26 };
    FillRect(hdc, &simCloseRc, hSimCloseBrush);
    DeleteObject(hSimCloseBrush);

    // 客户区 — 高亮颜色
    COLORREF clientColor = RGB(255, 255, 255);
    if (state && state->hoveredZone == ZONE_CLIENT)
        clientColor = RGB(220, 235, 255);  // 悬停变浅蓝

    HBRUSH hClientBrush = CreateSolidBrush(clientColor);
    RECT simClientRc = { dx + 1, dy + 31, dx + diagramW - 1, dy + diagramH - 1 };
    FillRect(hdc, &simClientRc, hClientBrush);
    DeleteObject(hClientBrush);

    // 区域标签
    HFONT hSmallFont = CreateFont(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hSmallFont);

    SetBkMode(hdc, TRANSPARENT);

    // 标题栏标签
    SetTextColor(hdc, RGB(200, 200, 200));
    RECT capLabelRc = { dx + 4, dy + 6, dx + diagramW - 30, dy + 28 };
    DrawText(hdc, L"HTCAPTION", -1, &capLabelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 关闭按钮标签
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT closeLabelRc = simCloseRc;
    DrawText(hdc, L"X", -1, &closeLabelRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 客户区标签
    SetTextColor(hdc, RGB(100, 100, 100));
    RECT cliLabelRc = { dx + 10, dy + 50, dx + diagramW - 10, dy + diagramH - 10 };
    DrawText(hdc, L"HTCLIENT", -1, &cliLabelRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hSmallFont);

    // ========== 右侧：命中测试信息 ==========
    int infoX = dx + diagramW + 30;

    HFONT hInfoFont = CreateFont(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldInfoFont = (HFONT)SelectObject(hdc, hInfoFont);

    SetTextColor(hdc, RGB(40, 40, 40));
    SetBkMode(hdc, TRANSPARENT);

    // 标题
    TextOut(hdc, infoX, margin + 10, L"命中测试区域", 6);

    HFONT hDetailFont = CreateFont(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldDetailFont = (HFONT)SelectObject(hdc, hDetailFont);

    // 显示当前悬停的区域
    const wchar_t* zoneName = L"无";
    COLORREF zoneColor = RGB(150, 150, 150);

    if (state)
    {
        switch (state->hoveredZone)
        {
        case ZONE_CAPTION:      zoneName = L"HTCAPTION (标题栏)";    zoneColor = RGB(50, 70, 110);  break;
        case ZONE_CLOSE:        zoneName = L"HTCLOSE (关闭按钮)";    zoneColor = RGB(232, 17, 35);  break;
        case ZONE_MAXIMIZE:     zoneName = L"HTMAXBUTTON (最大化)";  zoneColor = RGB(40, 160, 80);  break;
        case ZONE_MINIMIZE:     zoneName = L"HTMINBUTTON (最小化)";  zoneColor = RGB(200, 160, 40); break;
        case ZONE_CLIENT:       zoneName = L"HTCLIENT (客户区)";     zoneColor = RGB(40, 120, 215); break;
        case ZONE_BORDER_LEFT:  zoneName = L"HTLEFT (左边框)";       zoneColor = RGB(150, 100, 50); break;
        case ZONE_BORDER_RIGHT: zoneName = L"HTRIGHT (右边框)";      zoneColor = RGB(150, 100, 50); break;
        case ZONE_BORDER_TOP:   zoneName = L"HTTOP (上边框)";        zoneColor = RGB(150, 100, 50); break;
        case ZONE_BORDER_BOTTOM:zoneName = L"HTBOTTOM (下边框)";     zoneColor = RGB(150, 100, 50); break;
        default: break;
        }
    }

    // 悬停区域名称
    TextOut(hdc, infoX, margin + 45, L"当前区域:", 5);
    SetTextColor(hdc, zoneColor);
    TextOut(hdc, infoX + 80, margin + 45, zoneName, (int)wcslen(zoneName));

    // 鼠标坐标
    SetTextColor(hdc, RGB(60, 60, 60));
    if (state)
    {
        wchar_t posText[64];
        wsprintf(posText, L"鼠标位置: (%d, %d)", state->lastMousePos.x, state->lastMousePos.y);
        TextOut(hdc, infoX, margin + 75, posText, (int)wcslen(posText));
    }

    // 绘制区域颜色图例
    int legendY = margin + 110;

    struct LegendItem { const wchar_t* name; COLORREF color; };
    LegendItem legends[] = {
        { L"HTCAPTION — 标题栏 (可拖拽)",   RGB(40, 44, 52)   },
        { L"HTCLOSE — 关闭按钮",            RGB(232, 17, 35)  },
        { L"HTMAXBUTTON — 最大化按钮",      RGB(40, 160, 80)  },
        { L"HTMINBUTTON — 最小化按钮",      RGB(200, 160, 40) },
        { L"HTCLIENT — 客户区",             RGB(40, 120, 215) },
        { L"HTLEFT/HTRIGHT — 边框",         RGB(150, 100, 50) },
    };

    for (int i = 0; i < 6; ++i)
    {
        // 色块
        HBRUSH hLegBrush = CreateSolidBrush(legends[i].color);
        RECT legRc = { infoX, legendY + i * 24, infoX + 14, legendY + i * 24 + 16 };
        FillRect(hdc, &legRc, hLegBrush);
        DeleteObject(hLegBrush);

        // 文字
        SetTextColor(hdc, RGB(60, 60, 60));
        TextOut(hdc, infoX + 20, legendY + i * 24, legends[i].name, (int)wcslen(legends[i].name));
    }

    // 底部说明
    HFONT hNoteFont = CreateFont(
        13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldNoteFont = (HFONT)SelectObject(hdc, hNoteFont);

    SetTextColor(hdc, RGB(120, 120, 120));
    RECT noteRc = { margin, clientRc.bottom - 70, clientRc.right - margin, clientRc.bottom - 10 };
    DrawText(hdc,
        L"命中测试与事件路由演示:\n"
        L"WM_NCHITTEST 决定鼠标位于哪个区域 -> 返回 HT* 值\n"
        L"PtInRect 判断坐标是否在矩形内 -> 自定义非客户区按钮",
        -1, &noteRc, DT_LEFT | DT_WORDBREAK);

    // 清理字体
    SelectObject(hdc, hOldNoteFont);
    DeleteObject(hNoteFont);
    SelectObject(hdc, hOldDetailFont);
    DeleteObject(hDetailFont);
    SelectObject(hdc, hOldInfoFont);
    DeleteObject(hInfoFont);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 初始化状态
        state = new AppState();
        state->hoveredZone = ZONE_NONE;
        state->lastMousePos = { 0, 0 };
        state->isMaximized = false;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 移除标准标题栏，使用自定义标题栏
        // 设置窗口位置以适应自定义标题栏
        RECT rc;
        GetWindowRect(hwnd, &rc);
        SetWindowPos(hwnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_FRAMECHANGED);

        return 0;
    }

    case WM_DESTROY:
    {
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_NCCALCSIZE:
    {
        // 自定义非客户区计算
        // 当 wParam = TRUE 时， lParam 指向 NCCALCSIZE_PARAMS
        // 我们将整个窗口矩形设为客户区，自己绘制标题栏
        if (wParam)
        {
            NCCALCSIZE_PARAMS* nccs = (NCCALCSIZE_PARAMS*)lParam;
            // 不缩减客户区 — 我们在客户区内绘制标题栏
            // 但需要为窗口边框留出空间
            nccs->rgrc[0].top += TITLE_BAR_HEIGHT;
            return 0;
        }
        break;
    }

    case WM_NCHITTEST:
    {
        // 自定义命中测试 — 这是本示例的核心
        POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };

        // 保存鼠标位置
        if (state)
        {
            state->lastMousePos = pt;
        }

        // 先获取默认的命中测试结果
        LRESULT ht = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);

        // 获取窗口矩形
        RECT wndRc;
        GetWindowRect(hwnd, &wndRc);

        // 检查是否在标题栏区域内（顶部 TITLE_BAR_HEIGHT 像素）
        RECT captionRc = GetCaptionRect(hwnd);
        if (PtInRect(&captionRc, pt))
        {
            // 在标题栏区域内，进一步检测是否在按钮上

            // 检测关闭按钮
            RECT closeRc = GetCloseBtnRect(hwnd);
            if (PtInRect(&closeRc, pt))
            {
                // 使用自定义命中测试返回值
                // 这里不使用 HTCLOSE，而是用一个自定义值
                // 以便我们在 WM_NCLBUTTONDOWN 中处理
                return HTCLOSE;
            }

            // 检测最大化按钮
            RECT maxRc = GetMaxBtnRect(hwnd);
            if (PtInRect(&maxRc, pt))
            {
                return HTMAXBUTTON;
            }

            // 检测最小化按钮
            RECT minRc = GetMinBtnRect(hwnd);
            if (PtInRect(&minRc, pt))
            {
                return HTMINBUTTON;
            }

            // 标题栏空白区域 — 允许拖拽
            return HTCAPTION;
        }

        // 边框区域命中测试（允许调整窗口大小）
        int borderWidth = 5;
        if (pt.x >= wndRc.left && pt.x < wndRc.left + borderWidth)
            return HTLEFT;
        if (pt.x > wndRc.right - borderWidth && pt.x <= wndRc.right)
            return HTRIGHT;
        if (pt.y > wndRc.bottom - borderWidth && pt.y <= wndRc.bottom)
            return HTBOTTOM;
        if (pt.y >= wndRc.top && pt.y < wndRc.top + borderWidth)
            return HTTOP;

        return ht;
    }

    case WM_NCLBUTTONDOWN:
    {
        // 处理自定义标题栏按钮的点击
        switch (wParam)
        {
        case HTCLOSE:
            // 关闭窗口
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;

        case HTMAXBUTTON:
            // 切换最大化/还原
            if (state && state->isMaximized)
            {
                ShowWindow(hwnd, SW_RESTORE);
                state->isMaximized = false;
            }
            else
            {
                ShowWindow(hwnd, SW_MAXIMIZE);
                if (state) state->isMaximized = true;
            }
            return 0;

        case HTMINBUTTON:
            // 最小化
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        // 客户区内的鼠标移动（更新示意图高亮）
        if (state)
        {
            // 获取鼠标在窗口中的位置
            POINT pt;
            GetCursorPos(&pt);
            state->lastMousePos = pt;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_NCMOUSEMOVE:
    {
        // 非客户区内的鼠标移动 — 更新悬停区域并重绘
        if (state)
        {
            POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
            state->lastMousePos = pt;

            // 根据命中测试结果更新悬停区域
            LRESULT ht = DefWindowProc(hwnd, WM_NCHITTEST, 0, lParam);

            // 重新做一次自定义命中测试
            RECT captionRc = GetCaptionRect(hwnd);
            if (PtInRect(&captionRc, pt))
            {
                RECT closeRc = GetCloseBtnRect(hwnd);
                RECT maxRc   = GetMaxBtnRect(hwnd);
                RECT minRc   = GetMinBtnRect(hwnd);

                if (PtInRect(&closeRc, pt))      state->hoveredZone = ZONE_CLOSE;
                else if (PtInRect(&maxRc, pt))   state->hoveredZone = ZONE_MAXIMIZE;
                else if (PtInRect(&minRc, pt))   state->hoveredZone = ZONE_MINIMIZE;
                else                              state->hoveredZone = ZONE_CAPTION;
            }
            else
            {
                state->hoveredZone = ZONE_CLIENT;
            }

            // 重绘非客户区（更新按钮高亮）
            RedrawWindow(hwnd, nullptr, nullptr,
                         RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
            // 重绘客户区（更新信息面板）
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_NCPAINT:
    {
        // 自定义非客户区绘制
        HDC hdc = GetWindowDC(hwnd);
        if (hdc)
        {
            int savedState = SaveDC(hdc);
            DrawCustomTitleBar(hwnd, hdc, state);
            RestoreDC(hdc, savedState);
            ReleaseDC(hwnd, hdc);
        }
        return 0;  // 已处理，不调用 DefWindowProc
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int savedState = SaveDC(hdc);
        DrawClientArea(hwnd, hdc, state);
        RestoreDC(hdc, savedState);

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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
    // 使用 WS_POPUP 去掉标准标题栏和边框，完全自定义绘制
    // 或者使用 WS_OVERLAPPED 并在 WM_NCCALCSIZE 中调整
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"命中测试示例",                // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 400,                       // 窗口大小
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
