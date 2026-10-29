#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cstdio>

// ============================================================================
// 控件 ID
// ============================================================================
#define IDC_COLOR_LIST  1001
#define IDC_COLOR_COMBO 1002

// ============================================================================
// 列表项数据结构
// ============================================================================
struct ListItemData
{
    COLORREF color;
    wchar_t  name[64];
    wchar_t  desc[128];
};

// 预设颜色数据
static ListItemData g_items[] = {
    { RGB(220,  50,  50), L"红色", L"热情与活力" },
    { RGB( 50, 150,  50), L"绿色", L"自然与和平" },
    { RGB( 50,  80, 200), L"蓝色", L"冷静与信任" },
    { RGB(220, 180,  50), L"黄色", L"温暖与乐观" },
    { RGB(150,  50, 180), L"紫色", L"神秘与优雅" },
    { RGB( 50, 180, 180), L"青色", L"清新与创意" },
    { RGB(220, 130,  50), L"橙色", L"活力与热情" },
    { RGB(128, 128, 128), L"灰色", L"沉稳与内敛" },
};

static const int g_itemCount = sizeof(g_items) / sizeof(g_items[0]);

// ============================================================================
// 全局字体——在 WM_CREATE 创建，WM_DESTROY 释放，避免每次绘制重复创建
// ============================================================================
static HFONT g_hNameFont = NULL;
static HFONT g_hDescFont = NULL;
static HFONT g_hLabelFont = NULL;

static void CreateFonts()
{
    g_hNameFont = CreateFontW(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_hDescFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_hLabelFont = CreateFontW(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void DestroyFonts()
{
    if (g_hNameFont)  { DeleteObject(g_hNameFont);  g_hNameFont = NULL; }
    if (g_hDescFont)  { DeleteObject(g_hDescFont);  g_hDescFont = NULL; }
    if (g_hLabelFont) { DeleteObject(g_hLabelFont); g_hLabelFont = NULL; }
}

// ============================================================================
// 绘制 ListBox 列表项
// ============================================================================
static void DrawListItem(DRAWITEMSTRUCT* dis)
{
    ListItemData* data = (ListItemData*)dis->itemData;
    if (!data) return;

    // 1. 背景
    if (dis->itemState & ODS_SELECTED)
        FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
    else
        FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_WINDOW));

    // 2. 颜色方块
    RECT colorRect = dis->rcItem;
    colorRect.left   += 8;
    colorRect.top    += 8;
    colorRect.right   = colorRect.left + 34;
    colorRect.bottom  = colorRect.top  + 34;

    HBRUSH hColorBrush = CreateSolidBrush(data->color);
    FillRect(dis->hDC, &colorRect, hColorBrush);
    DeleteObject(hColorBrush);

    FrameRect(dis->hDC, &colorRect, GetSysColorBrush(COLOR_WINDOWTEXT));

    // 3. 名称文字
    COLORREF textColor = (dis->itemState & ODS_SELECTED)
        ? GetSysColor(COLOR_HIGHLIGHTTEXT)
        : GetSysColor(COLOR_WINDOWTEXT);

    SetTextColor(dis->hDC, textColor);
    SetBkMode(dis->hDC, TRANSPARENT);

    HFONT hOldFont = (HFONT)SelectObject(dis->hDC, g_hNameFont);

    RECT textRect = dis->rcItem;
    textRect.left = colorRect.right + 12;
    textRect.top += 4;
    DrawText(dis->hDC, data->name, -1, &textRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // 4. 描述文字
    SelectObject(dis->hDC, g_hDescFont);
    textRect.top += 22;
    SetTextColor(dis->hDC,
        (dis->itemState & ODS_SELECTED) ? RGB(200, 220, 255) : RGB(100, 100, 100));
    DrawText(dis->hDC, data->desc, -1, &textRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dis->hDC, hOldFont);

    // 5. 焦点虚线框
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dis->hDC, &dis->rcItem);
}

// ============================================================================
// 绘制 ComboBox 项（编辑框 + 下拉列表）
// ============================================================================
static void DrawComboItem(DRAWITEMSTRUCT* dis)
{
    ListItemData* data = (ListItemData*)dis->itemData;
    if (!data) return;

    bool isEditArea = (dis->itemState & ODS_COMBOBOXEDIT) != 0;

    // 背景
    if (dis->itemState & ODS_SELECTED)
    {
        FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
        SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    }
    else
    {
        FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_WINDOW));
        SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
    }
    SetBkMode(dis->hDC, TRANSPARENT);

    HFONT hOldFont = (HFONT)SelectObject(dis->hDC, g_hNameFont);

    if (isEditArea)
    {
        // 编辑框：颜色方块 + 名称
        RECT cr = dis->rcItem;
        cr.left += 4;
        cr.top  += 4;
        int size = dis->rcItem.bottom - dis->rcItem.top - 8;
        cr.right  = cr.left + size;
        cr.bottom = cr.top  + size;

        HBRUSH hBrush = CreateSolidBrush(data->color);
        FillRect(dis->hDC, &cr, hBrush);
        DeleteObject(hBrush);

        RECT tr = dis->rcItem;
        tr.left = cr.right + 8;
        DrawText(dis->hDC, data->name, -1, &tr,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    else
    {
        // 下拉列表：颜色方块 + 名称 + 描述
        RECT cr = dis->rcItem;
        cr.left += 6;
        cr.top  += 6;
        cr.right  = cr.left + 26;
        cr.bottom = cr.top  + 26;

        HBRUSH hBrush = CreateSolidBrush(data->color);
        FillRect(dis->hDC, &cr, hBrush);
        DeleteObject(hBrush);

        RECT tr = dis->rcItem;
        tr.left = cr.right + 10;
        tr.top -= 2;
        DrawText(dis->hDC, data->name, -1, &tr,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        // 描述
        SelectObject(dis->hDC, g_hDescFont);
        SetTextColor(dis->hDC,
            (dis->itemState & ODS_SELECTED) ? RGB(200, 220, 255) : RGB(100, 100, 100));
        tr.left = cr.right + 10 + 60;
        DrawText(dis->hDC, data->desc, -1, &tr,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }

    SelectObject(dis->hDC, hOldFont);

    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dis->hDC, &dis->rcItem);
}

// ============================================================================
// 窗口过程
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CreateFonts();

        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // --- 标签：Owner-Draw ListBox ---
        CreateWindowExW(0, L"STATIC", L"Owner-Draw ListBox",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 10, 300, 24,
            hwnd, NULL, hInst, NULL);

        // --- Owner-Draw ListBox ---
        HWND hList = CreateWindowExW(
            0, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            LBS_OWNERDRAWFIXED |
            LBS_NOTIFY |
            WS_VSCROLL,
            20, 38, 300, 360,
            hwnd, (HMENU)IDC_COLOR_LIST, hInst, nullptr);

        for (int i = 0; i < g_itemCount; i++)
        {
            int idx = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)g_items[i].name);
            SendMessage(hList, LB_SETITEMDATA, idx, (LPARAM)&g_items[i]);
        }

        // --- 标签：Owner-Draw ComboBox ---
        CreateWindowExW(0, L"STATIC", L"Owner-Draw ComboBox",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            360, 10, 300, 24,
            hwnd, NULL, hInst, NULL);

        // --- Owner-Draw ComboBox ---
        HWND hCombo = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            CBS_OWNERDRAWFIXED |
            CBS_DROPDOWNLIST,
            360, 38, 280, 360,
            hwnd, (HMENU)IDC_COLOR_COMBO, hInst, nullptr);

        for (int i = 0; i < g_itemCount; i++)
        {
            int idx = (int)SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)g_items[i].name);
            SendMessage(hCombo, CB_SETITEMDATA, idx, (LPARAM)&g_items[i]);
        }

        // 默认选中第一项
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        return 0;
    }

    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (mis->CtlID == IDC_COLOR_LIST)
            mis->itemHeight = 50;
        else if (mis->CtlID == IDC_COLOR_COMBO)
            mis->itemHeight = 40;
        return TRUE;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == IDC_COLOR_LIST)
            DrawListItem(dis);
        else if (dis->CtlID == IDC_COLOR_COMBO)
            DrawComboItem(dis);
        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    {
        // 让静态标签背景透明
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_DESTROY:
        DestroyFonts();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// 入口
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = L"OwnerDrawDemo";
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"OwnerDrawDemo", L"Owner-Draw 控件示例",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 680, 440,
        NULL, NULL, hInstance, NULL);

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
