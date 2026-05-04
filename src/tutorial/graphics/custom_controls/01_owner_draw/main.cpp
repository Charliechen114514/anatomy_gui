/**
 * 01_owner_draw - Owner-Draw 控件示例
 *
 * 本程序演示了 Win32 中 Owner-Draw（所有者绘制）控件的用法：
 * 1. BS_OWNERDRAW 按钮样式 — 让父窗口负责按钮的绘制
 * 2. WM_DRAWITEM  — 响应绘制请求，自定义按钮外观
 * 3. WM_MEASUREITEM — 设置 owner-draw 控件的初始尺寸
 * 4. ODS_SELECTED / ODS_FOCUS — 根据按钮状态绘制不同视觉效果
 *
 * 同时演示了 owner-draw 菜单项的用法 (MF_OWNERDRAW)。
 *
 * 参考: tutorial/native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"OwnerDrawDemoClass";

// 四个 owner-draw 按钮的子窗口 ID
static const int ID_BTN_RED    = 101;
static const int ID_BTN_GREEN  = 102;
static const int ID_BTN_BLUE   = 103;
static const int ID_BTN_WARN   = 104;

// 状态标签 ID
static const int ID_LABEL_STATUS = 201;

// 菜单命令 ID
static const int ID_MENU_FILE_NEW   = 301;
static const int ID_MENU_FILE_OPEN  = 302;
static const int ID_MENU_FILE_EXIT  = 303;
static const int ID_MENU_HELP_ABOUT = 401;

// 菜单项在 WM_DRAWITEM 中的标识
// 高 16 位 = 菜单 ID，低 16 位 = 0 表示菜单项
static const ULONG_PTR MENU_TAG_NEW   = 0x00010000;
static const ULONG_PTR MENU_TAG_OPEN  = 0x00020000;
static const ULONG_PTR MENU_TAG_EXIT  = 0x00030000;
static const ULONG_PTR MENU_TAG_ABOUT = 0x00040000;

// ============================================================================
// 辅助函数：绘制圆角矩形
// ============================================================================

/**
 * 使用 GDI 绘制圆角矩形并填充
 * @param hdc     设备上下文
 * @param left    左上角 x
 * @param top     左上角 y
 * @param right   右下角 x
 * @param bottom  右下角 y
 * @param radius  圆角半径
 * @param hBrush  填充画刷
 */
static void FillRoundRect(HDC hdc, int left, int top, int right, int bottom,
                          int radius, HBRUSH hBrush)
{
    HRGN hRgn = CreateRoundRectRgn(left, top, right, bottom, radius * 2, radius * 2);
    FillRgn(hdc, hRgn, hBrush);
    DeleteObject(hRgn);
}

// ============================================================================
// 绘制各个 Owner-Draw 按钮
// ============================================================================

/**
 * 绘制 "红色按钮"
 * - 正常状态：红色背景，白色文字
 * - 按下状态 (ODS_SELECTED)：更深的红色
 * - 焦点状态 (ODS_FOCUS)：外围虚线框
 */
static void DrawRedButton(LPDRAWITEMSTRUCT pDIS)
{
    BOOL selected = pDIS->itemState & ODS_SELECTED;
    BOOL focused  = pDIS->itemState & ODS_FOCUS;

    // 根据状态选择颜色
    COLORREF bgColor = selected ? RGB(160, 20, 20) : RGB(220, 50, 50);

    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // 绘制背景
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    // 按下时偏移文字，产生 "按下" 视觉效果
    int offsetX = selected ? 2 : 0;
    int offsetY = selected ? 2 : 0;

    // 绘制按钮文字
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hFont = CreateFont(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT textRc = rc;
    textRc.left   += offsetX;
    textRc.top    += offsetY;
    textRc.right  += offsetX;
    textRc.bottom += offsetY;
    DrawText(hdc, L"红色按钮", -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    // 焦点时绘制虚线边框
    if (focused)
    {
        HPEN hPen = CreatePen(PS_DOT, 1, RGB(255, 255, 255));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        Rectangle(hdc, rc.left + 3, rc.top + 3, rc.right - 3, rc.bottom - 3);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
    }
}

/**
 * 绘制 "绿色按钮"
 * - 使用绿色渐变效果（通过逐行填充不同深浅的绿色模拟）
 * - 按下时颜色变深
 */
static void DrawGreenButton(LPDRAWITEMSTRUCT pDIS)
{
    BOOL selected = pDIS->itemState & ODS_SELECTED;

    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    // 绘制渐变效果：从浅绿到深绿
    for (int i = 0; i < h; ++i)
    {
        int greenVal = selected ? (100 + (80 * i / h)) : (140 + (80 * i / h));
        int redVal   = selected ? (10 + (20 * i / h))  : (20 + (30 * i / h));

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(redVal, greenVal, 20));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, rc.left, rc.top + i, nullptr);
        LineTo(hdc, rc.right, rc.top + i);
        DeleteObject(hPen);
    }

    // 绘制底部高亮线（模拟 3D 凸起效果）
    if (!selected)
    {
        HPEN hLight = CreatePen(PS_SOLID, 1, RGB(180, 240, 180));
        SelectObject(hdc, hLight);
        MoveToEx(hdc, rc.left, rc.top, nullptr);
        LineTo(hdc, rc.right, rc.top);
        DeleteObject(hLight);
    }

    // 绘制按钮文字
    int offsetX = selected ? 2 : 0;
    int offsetY = selected ? 2 : 0;

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hFont = CreateFont(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT textRc = rc;
    textRc.left   += offsetX;
    textRc.top    += offsetY;
    textRc.right  += offsetX;
    textRc.bottom += offsetY;
    DrawText(hdc, L"绿色按钮", -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/**
 * 绘制 "蓝色按钮"
 * - 使用圆角矩形效果
 * - 按下时颜色变深并略微偏移
 */
static void DrawBlueButton(LPDRAWITEMSTRUCT pDIS)
{
    BOOL selected = pDIS->itemState & ODS_SELECTED;
    BOOL focused  = pDIS->itemState & ODS_FOCUS;

    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // 先用窗口背景色填满整个区域（清除角落）
    HBRUSH hBg = CreateSolidBrush(RGB(240, 240, 240));
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    // 按下偏移
    int padX = selected ? 2 : 0;
    int padY = selected ? 2 : 0;

    // 绘制圆角矩形背景
    COLORREF bgColor = selected ? RGB(20, 80, 160) : RGB(40, 120, 215);
    HBRUSH hBlue = CreateSolidBrush(bgColor);
    FillRoundRect(hdc, rc.left + padX, rc.top + padY,
                  rc.right + padX, rc.bottom + padY, 12, hBlue);
    DeleteObject(hBlue);

    // 绘制顶部高亮弧线（模拟光泽效果）
    if (!selected)
    {
        HPEN hLight = CreatePen(PS_SOLID, 1, RGB(120, 190, 255));
        Arc(hdc, rc.left + 12, rc.top + 2, rc.right - 12, rc.top + 24,
            rc.left + 20, rc.top + 4, rc.right - 20, rc.top + 4);
        DeleteObject(hLight);
    }

    // 绘制按钮文字
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hFont = CreateFont(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT textRc = rc;
    textRc.left   += padX;
    textRc.top    += padY;
    textRc.right  += padX;
    textRc.bottom += padY;
    DrawText(hdc, L"蓝色按钮", -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    // 焦点时绘制外围蓝色边框
    if (focused)
    {
        HPEN hPen = CreatePen(PS_DOT, 1, RGB(40, 120, 215));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        Rectangle(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
    }
}

/**
 * 绘制 "警告" 按钮
 * - 黄色/橙色背景
 * - 带有图标字符（感叹号）
 * - 按下时变为深橙色
 */
static void DrawWarnButton(LPDRAWITEMSTRUCT pDIS)
{
    BOOL selected = pDIS->itemState & ODS_SELECTED;

    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // 根据状态选择颜色
    COLORREF bgColor = selected ? RGB(230, 130, 0) : RGB(255, 180, 30);
    COLORREF borderColor = selected ? RGB(200, 100, 0) : RGB(230, 140, 0);

    // 绘制背景
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    HPEN hBorder = CreatePen(PS_SOLID, 2, borderColor);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hBorder);
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hBorder);

    int offsetX = selected ? 2 : 0;
    int offsetY = selected ? 2 : 0;

    // 绘制感叹号图标字符
    SetTextColor(hdc, RGB(120, 60, 0));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hIconFont = CreateFont(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol"
    );
    HFONT hOldIconFont = (HFONT)SelectObject(hdc, hIconFont);

    // 在按钮左侧绘制警告图标
    RECT iconRc = rc;
    iconRc.left   += offsetX;
    iconRc.top    += offsetY;
    iconRc.right  = rc.left + 40 + offsetX;
    iconRc.bottom += offsetY;
    DrawText(hdc, L"⚠", -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldIconFont);
    DeleteObject(hIconFont);

    // 绘制按钮文字
    HFONT hFont = CreateFont(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT textRc = rc;
    textRc.left  = rc.left + 40 + offsetX;
    textRc.top   += offsetY;
    textRc.right += offsetX;
    textRc.bottom += offsetY;
    DrawText(hdc, L"警告", -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

// ============================================================================
// 绘制 Owner-Draw 菜单项
// ============================================================================

/**
 * 绘制 owner-draw 菜单项
 * 每个菜单项左侧有一个小色块图标，右侧是菜单文字
 */
static void DrawMenuItem(LPDRAWITEMSTRUCT pDIS)
{
    BOOL selected = pDIS->itemState & ODS_SELECTED;

    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // 根据选中状态设置背景色
    if (selected)
    {
        HBRUSH hSelBrush = CreateSolidBrush(RGB(200, 225, 255));
        FillRect(hdc, &rc, hSelBrush);
        DeleteObject(hSelBrush);
    }
    else
    {
        HBRUSH hMenuBrush = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &rc, hMenuBrush);
        DeleteObject(hMenuBrush);
    }

    // 根据 itemData 获取菜单信息
    // itemData 里存的是菜单标签
    const wchar_t* text = nullptr;
    COLORREF iconColor = RGB(0, 0, 0);

    switch ((ULONG_PTR)pDIS->itemData)
    {
    case MENU_TAG_NEW:
        text = L"新建 (New)";
        iconColor = RGB(76, 175, 80);
        break;
    case MENU_TAG_OPEN:
        text = L"打开 (Open)";
        iconColor = RGB(33, 150, 243);
        break;
    case MENU_TAG_EXIT:
        text = L"退出 (Exit)";
        iconColor = RGB(244, 67, 54);
        break;
    case MENU_TAG_ABOUT:
        text = L"关于 (About)";
        iconColor = RGB(156, 39, 176);
        break;
    default:
        text = L"未知菜单项";
        iconColor = RGB(150, 150, 150);
        break;
    }

    // 绘制左侧图标色块
    RECT iconRc = { rc.left + 4, rc.top + 4, rc.left + 20, rc.bottom - 4 };
    HBRUSH hIconBrush = CreateSolidBrush(iconColor);
    FillRect(hdc, &iconRc, hIconBrush);
    DeleteObject(hIconBrush);

    // 绘制菜单文字
    SetTextColor(hdc, selected ? RGB(0, 60, 120) : RGB(40, 40, 40));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT textRc = { rc.left + 28, rc.top, rc.right - 5, rc.bottom };
    DrawText(hdc, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/**
 * 测量 owner-draw 菜单项的高度和宽度
 */
static void MeasureMenuItem(LPMEASUREITEMSTRUCT pMIS)
{
    // 设置菜单项的标准高度
    pMIS->itemHeight = 28;
    pMIS->itemWidth = 160;
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
        // 初始化通用控件
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC  = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;

        // 创建菜单栏
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreatePopupMenu();
        HMENU hHelpMenu = CreatePopupMenu();

        // 添加 owner-draw 菜单项（MF_OWNERDRAW 标志）
        AppendMenu(hFileMenu, MF_OWNERDRAW, ID_MENU_FILE_NEW,  (LPCTSTR)MENU_TAG_NEW);
        AppendMenu(hFileMenu, MF_OWNERDRAW, ID_MENU_FILE_OPEN, (LPCTSTR)MENU_TAG_OPEN);
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hFileMenu, MF_OWNERDRAW, ID_MENU_FILE_EXIT, (LPCTSTR)MENU_TAG_EXIT);

        AppendMenu(hHelpMenu, MF_OWNERDRAW, ID_MENU_HELP_ABOUT, (LPCTSTR)MENU_TAG_ABOUT);

        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"文件(&F)");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"帮助(&H)");
        SetMenu(hwnd, hMenu);

        // 创建四个 owner-draw 按钮
        // BS_OWNERDRAW 样式告知 Windows 不要绘制按钮，而是发送 WM_DRAWITEM 给父窗口
        int btnW = 180;
        int btnH = 50;
        int startX = 30;
        int startY = 30;
        int gap = 20;

        CreateWindowEx(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            startX, startY, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_RED,
            cs->hInstance, nullptr
        );

        CreateWindowEx(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            startX + btnW + gap, startY, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_GREEN,
            cs->hInstance, nullptr
        );

        CreateWindowEx(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            startX, startY + btnH + gap, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_BLUE,
            cs->hInstance, nullptr
        );

        CreateWindowEx(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            startX + btnW + gap, startY + btnH + gap, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_WARN,
            cs->hInstance, nullptr
        );

        // 创建状态标签
        CreateWindowEx(
            0, L"STATIC", L"请点击上方按钮...",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            30, 200, 420, 35,
            hwnd, (HMENU)(LONG_PTR)ID_LABEL_STATUS,
            cs->hInstance, nullptr
        );

        // 设置标签字体
        HFONT hLabelFont = CreateFont(
            18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        SendDlgItemMessage(hwnd, ID_LABEL_STATUS, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;

        if (pDIS->CtlType == ODT_BUTTON)
        {
            // 根据按钮 ID 选择对应的绘制函数
            switch (pDIS->CtlID)
            {
            case ID_BTN_RED:   DrawRedButton(pDIS);   break;
            case ID_BTN_GREEN: DrawGreenButton(pDIS); break;
            case ID_BTN_BLUE:  DrawBlueButton(pDIS);  break;
            case ID_BTN_WARN:  DrawWarnButton(pDIS);  break;
            }
        }
        else if (pDIS->CtlType == ODT_MENU)
        {
            // 绘制 owner-draw 菜单项
            DrawMenuItem(pDIS);
        }

        return TRUE;  // 已处理 WM_DRAWITEM
    }

    case WM_MEASUREITEM:
    {
        LPMEASUREITEMSTRUCT pMIS = (LPMEASUREITEMSTRUCT)lParam;

        if (pMIS->CtlType == ODT_BUTTON)
        {
            // 设置 owner-draw 按钮的高度
            // 对于按钮，Windows 主要使用 itemHeight
            pMIS->itemHeight = 50;
            pMIS->itemWidth  = 180;
        }
        else if (pMIS->CtlType == ODT_MENU)
        {
            // 设置 owner-draw 菜单项的尺寸
            MeasureMenuItem(pMIS);
        }

        return TRUE;  // 已处理 WM_MEASUREITEM
    }

    case WM_COMMAND:
    {
        WORD cmdID = LOWORD(wParam);

        // 处理按钮点击
        const wchar_t* msg = nullptr;
        switch (cmdID)
        {
        case ID_BTN_RED:
            msg = L"你点击了: 红色按钮";
            break;
        case ID_BTN_GREEN:
            msg = L"你点击了: 绿色按钮";
            break;
        case ID_BTN_BLUE:
            msg = L"你点击了: 蓝色按钮";
            break;
        case ID_BTN_WARN:
            msg = L"你点击了: 警告按钮";
            break;
        case ID_MENU_FILE_NEW:
            msg = L"菜单: 新建";
            break;
        case ID_MENU_FILE_OPEN:
            msg = L"菜单: 打开";
            break;
        case ID_MENU_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case ID_MENU_HELP_ABOUT:
            MessageBox(hwnd,
                L"Owner-Draw 控件示例\n\n"
                L"演示 BS_OWNERDRAW、WM_DRAWITEM、\n"
                L"WM_MEASUREITEM 和 MF_OWNERDRAW 菜单项。",
                L"关于", MB_OK | MB_ICONINFORMATION);
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        if (msg)
        {
            SetDlgItemText(hwnd, ID_LABEL_STATUS, msg);
        }

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充背景
        HBRUSH hBg = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 在底部绘制说明文字
        SetTextColor(hdc, RGB(100, 100, 100));
        SetBkMode(hdc, TRANSPARENT);

        HFONT hNoteFont = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        HFONT hOldFont = (HFONT)SelectObject(hdc, hNoteFont);

        RECT noteRc = { 30, 250, clientRc.right - 30, clientRc.bottom - 10 };
        DrawText(hdc,
            L"Owner-Draw 控件演示:\n"
            L"• BS_OWNERDRAW — 按钮由父窗口自行绘制\n"
            L"• WM_DRAWITEM — 接收绘制请求并自定义渲染\n"
            L"• WM_MEASUREITEM — 设置控件的初始尺寸\n"
            L"• ODS_SELECTED / ODS_FOCUS — 按下/焦点状态反馈\n"
            L"• MF_OWNERDRAW — 菜单项也支持 owner-draw",
            -1, &noteRc, DT_LEFT | DT_WORDBREAK);

        SelectObject(hdc, hOldFont);
        DeleteObject(hNoteFont);

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
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"Owner-Draw 控件示例",         // 窗口标题
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
