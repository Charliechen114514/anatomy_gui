/**
 * 02_gdi_objects - GDI 绘图对象示例
 *
 * 本程序演示了 Win32 GDI 绘图对象的生命周期管理：
 * 1. CreatePen / DeleteObject          — 三种画笔样式（实线、虚线、点线）
 * 2. CreateSolidBrush / CreateHatchBrush — 填充画刷（纯色与阴影图案）
 * 3. CreateFontIndirect                — 自定义字体渲染文字
 * 4. SelectObject / 恢复               — 正确的选入/恢复模式
 *
 * 窗口底部显示一个 "泄漏计数器"，创建对象时递增，删除对象时递减。
 * 帮助理解 GDI 对象必须配对创建和删除的原则。
 *
 * 参考: tutorial/native_win32/19_ProgrammingGUI_NativeWindows_GDI_Objects.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GDIObjectsClass";

static const int ID_BTN_RESET_LEAK = 101;

// ============================================================================
// 应用程序状态
// ============================================================================

/**
 * GDI 对象泄漏计数器
 *
 * 每次创建 GDI 对象时递增，删除时递减。
 * 如果最终不为零，说明存在 GDI 对象泄漏。
 */
struct AppState
{
    int createCount;        // 已创建的 GDI 对象数量
    int deleteCount;        // 已删除的 GDI 对象数量
};

// 全局状态指针，方便辅助函数访问（也通过 GWLP_USERDATA 绑定到窗口）
static AppState* g_state = nullptr;

/**
 * 记录一次 GDI 对象创建
 */
template<typename T>
static T TrackCreate(T obj)
{
    if (obj && g_state) g_state->createCount++;
    return obj;
}

/**
 * 记录一次 GDI 对象删除
 */
static void TrackDelete(HGDIOBJ obj)
{
    if (obj && g_state) g_state->deleteCount++;
    DeleteObject(obj);
}

// ============================================================================
// 辅助绘图函数
// ============================================================================

/**
 * 绘制分区标题栏
 */
static void DrawSectionTitle(HDC hdc, const RECT& rc, const wchar_t* title, COLORREF bgColor)
{
    int saved = SaveDC(hdc);

    HBRUSH hBrush = TrackCreate(CreateSolidBrush(bgColor));
    RECT titleRc = { rc.left, rc.top, rc.right, rc.top + 26 };
    FillRect(hdc, &titleRc, hBrush);
    TrackDelete(hBrush);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, title, -1, &titleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    HPEN hPen = TrackCreate(CreatePen(PS_SOLID, 1, RGB(180, 180, 180)));
    SelectObject(hdc, hPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    TrackDelete(hPen);

    RestoreDC(hdc, saved);
}

// ============================================================================
// 分区绘制函数
// ============================================================================

/**
 * 分区 1: CreatePen — 三种画笔样式
 *
 * 演示 PS_SOLID（实线）、PS_DASH（虚线）、PS_DOT（点线）三种画笔。
 * 展示 CreatePen 和 DeleteObject 的配对使用。
 */
static void DrawPenSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"1. CreatePen 画笔样式", RGB(33, 150, 243));

    int saved = SaveDC(hdc);

    // 定义三种画笔
    struct PenInfo {
        int style;
        const wchar_t* name;
        COLORREF color;
    };

    PenInfo pens[] = {
        { PS_SOLID, L"PS_SOLID (实线)",  RGB(33, 150, 243) },
        { PS_DASH,  L"PS_DASH (虚线)",   RGB(244, 67, 54)  },
        { PS_DOT,   L"PS_DOT (点线)",    RGB(76, 175, 80)  },
    };

    int y = rc.top + 38;
    int lineSpacing = 35;

    for (int i = 0; i < 3; ++i)
    {
        // 创建画笔 — 计入泄漏计数器
        HPEN hPen = TrackCreate(CreatePen(pens[i].style, 2, pens[i].color));
        SelectObject(hdc, hPen);

        // 绘制水平线
        MoveToEx(hdc, rc.left + 15, y + 10, nullptr);
        LineTo(hdc, rc.right - 15, y + 10);

        // 标注画笔名称
        SetTextColor(hdc, RGB(80, 80, 80));
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, rc.left + 15, y - 5, pens[i].name,
                (int)wcslen(pens[i].name));

        // 删除画笔 — 计入泄漏计数器
        TrackDelete(hPen);

        y += lineSpacing;
    }

    RestoreDC(hdc, saved);
}

/**
 * 分区 2: CreateSolidBrush / CreateHatchBrush — 填充画刷
 *
 * 演示纯色画刷和各种阴影图案画刷 (HS_HORIZONTAL, HS_VERTICAL,
 * HS_FDIAGONAL, HS_CROSS, HS_DIAGCROSS)。
 */
static void DrawBrushSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"2. CreateBrush 画刷样式", RGB(255, 152, 0));

    int saved = SaveDC(hdc);

    struct BrushInfo {
        bool  isHatch;
        int   hatchStyle;
        COLORREF color;
        const wchar_t* name;
    };

    BrushInfo brushes[] = {
        { false, 0,              RGB(255, 152, 0),   L"纯色" },
        { true,  HS_HORIZONTAL,  RGB(76, 175, 80),   L"HS_HORIZONTAL" },
        { true,  HS_VERTICAL,    RGB(33, 150, 243),   L"HS_VERTICAL" },
        { true,  HS_FDIAGONAL,   RGB(156, 39, 176),   L"HS_FDIAGONAL" },
        { true,  HS_CROSS,       RGB(244, 67, 54),    L"HS_CROSS" },
        { true,  HS_DIAGCROSS,   RGB(0, 150, 136),    L"HS_DIAGCROSS" },
    };

    int cols = 3;
    int rows = 2;
    int padX = 12, padY = 8;
    int startY = rc.top + 32;
    int availW = (rc.right - rc.left) - padX * (cols + 1);
    int availH = (rc.bottom - startY) - padY * (rows + 1);
    int cellW = availW / cols;
    int cellH = availH / rows;

    for (int i = 0; i < 6; ++i)
    {
        int col = i % cols;
        int row = i / cols;
        int cx = rc.left + padX + col * (cellW + padX);
        int cy = startY + padY + row * (cellH + padY);

        // 创建画刷
        HBRUSH hBrush;
        if (brushes[i].isHatch)
            hBrush = TrackCreate(CreateHatchBrush(brushes[i].hatchStyle, brushes[i].color));
        else
            hBrush = TrackCreate(CreateSolidBrush(brushes[i].color));

        // 绘制填充矩形
        RECT fillRc = { cx + 2, cy + 16, cx + cellW - 2, cy + cellH - 2 };
        FillRect(hdc, &fillRc, hBrush);

        // 绘制矩形边框
        HPEN hPen = TrackCreate(CreatePen(PS_SOLID, 1, RGB(100, 100, 100)));
        SelectObject(hdc, hPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, cx + 2, cy + 16, cx + cellW - 2, cy + cellH - 2);
        TrackDelete(hPen);

        // 标注画刷名称
        SetTextColor(hdc, RGB(60, 60, 60));
        SetBkMode(hdc, TRANSPARENT);
        RECT nameRc = { cx, cy, cx + cellW, cy + 16 };
        DrawText(hdc, brushes[i].name, -1, &nameRc,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 删除画刷
        TrackDelete(hBrush);
    }

    RestoreDC(hdc, saved);
}

/**
 * 分区 3: CreateFontIndirect — 自定义字体
 *
 * 使用 LOGFONT 结构体创建三种不同风格和大小的字体：
 * - 大号粗体
 * - 中号斜体
 * - 小号下划线
 */
static void DrawFontSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"3. CreateFontIndirect 字体", RGB(156, 39, 176));

    int saved = SaveDC(hdc);

    // 字体 1: 大号粗体
    LOGFONT lf1 = {};
    lf1.lfHeight         = -28;
    lf1.lfWeight         = FW_BOLD;
    lf1.lfCharSet        = DEFAULT_CHARSET;
    lf1.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf1.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf1.lfQuality        = CLEARTYPE_QUALITY;
    lf1.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf1.lfFaceName, L"微软雅黑");

    HFONT hFont1 = TrackCreate(CreateFontIndirect(&lf1));
    SelectObject(hdc, hFont1);
    SetTextColor(hdc, RGB(156, 39, 176));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, rc.left + 15, rc.top + 35, L"大号粗体 - 28px",
            (int)wcslen(L"大号粗体 - 28px"));
    TrackDelete(hFont1);

    // 字体 2: 中号斜体
    LOGFONT lf2 = {};
    lf2.lfHeight         = -20;
    lf2.lfItalic         = TRUE;
    lf2.lfCharSet        = DEFAULT_CHARSET;
    lf2.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf2.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf2.lfQuality        = CLEARTYPE_QUALITY;
    lf2.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf2.lfFaceName, L"微软雅黑");

    HFONT hFont2 = TrackCreate(CreateFontIndirect(&lf2));
    SelectObject(hdc, hFont2);
    SetTextColor(hdc, RGB(33, 150, 243));
    TextOut(hdc, rc.left + 15, rc.top + 75, L"中号斜体 - 20px",
            (int)wcslen(L"中号斜体 - 20px"));
    TrackDelete(hFont2);

    // 字体 3: 小号下划线
    LOGFONT lf3 = {};
    lf3.lfHeight         = -16;
    lf3.lfUnderline      = TRUE;
    lf3.lfCharSet        = DEFAULT_CHARSET;
    lf3.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf3.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf3.lfQuality        = CLEARTYPE_QUALITY;
    lf3.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf3.lfFaceName, L"微软雅黑");

    HFONT hFont3 = TrackCreate(CreateFontIndirect(&lf3));
    SelectObject(hdc, hFont3);
    SetTextColor(hdc, RGB(76, 175, 80));
    TextOut(hdc, rc.left + 15, rc.top + 110, L"小号下划线 - 16px",
            (int)wcslen(L"小号下划线 - 16px"));
    TrackDelete(hFont3);

    RestoreDC(hdc, saved);
}

/**
 * 分区 4: SelectObject / 恢复 — 正确的保存恢复模式
 *
 * 演示使用 SaveDC/RestoreDC 以及 SelectObject 返回旧对象两种
 * 恢复 DC 状态的方式。强调 GDI 对象必须正确恢复的重要性。
 */
static void DrawSelectRestoreSection(HDC hdc, const RECT& rc)
{
    DrawSectionTitle(hdc, rc, L"4. SelectObject 保存/恢复", RGB(0, 150, 136));

    int saved = SaveDC(hdc);

    // 方法一: SaveDC / RestoreDC
    {
        int savedInner = SaveDC(hdc);

        HPEN hRedPen = TrackCreate(CreatePen(PS_SOLID, 3, RGB(244, 67, 54)));
        SelectObject(hdc, hRedPen);
        HBRUSH hBlueBrush = TrackCreate(CreateSolidBrush(RGB(200, 220, 255)));
        SelectObject(hdc, hBlueBrush);

        Rectangle(hdc, rc.left + 15, rc.top + 35, rc.left + 140, rc.top + 100);

        // RestoreDC 会自动恢复原来的画笔和画刷
        RestoreDC(hdc, savedInner);

        // 删除我们创建的 GDI 对象
        TrackDelete(hRedPen);
        TrackDelete(hBlueBrush);
    }

    // 方法二: 保存旧对象，恢复选回
    {
        HPEN hNewPen = TrackCreate(CreatePen(PS_SOLID, 3, RGB(76, 175, 80)));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hNewPen);
        HBRUSH hNewBrush = TrackCreate(CreateSolidBrush(RGB(200, 255, 200)));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);

        Rectangle(hdc, rc.left + 155, rc.top + 35, rc.left + 280, rc.top + 100);

        // 恢复原来的对象
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);

        // 删除新创建的 GDI 对象
        TrackDelete(hNewPen);
        TrackDelete(hNewBrush);
    }

    // 标注说明
    SetTextColor(hdc, RGB(80, 80, 80));
    SetBkMode(hdc, TRANSPARENT);
    RECT labelRc1 = { rc.left + 15, rc.top + 105, rc.left + 140, rc.top + 130 };
    DrawText(hdc, L"SaveDC/RestoreDC", -1, &labelRc1,
             DT_CENTER | DT_WORDBREAK);

    RECT labelRc2 = { rc.left + 155, rc.top + 105, rc.left + 280, rc.top + 130 };
    DrawText(hdc, L"保存旧对象/恢复", -1, &labelRc2,
             DT_CENTER | DT_WORDBREAK);

    // 综合说明
    HFONT hNoteFont = TrackCreate(CreateFont(
        13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    ));
    SelectObject(hdc, hNoteFont);
    SetTextColor(hdc, RGB(100, 100, 100));
    RECT noteRc = { rc.left + 10, rc.top + 135, rc.right - 10, rc.bottom - 5 };
    DrawText(hdc,
        L"要点：\n"
        L"1. SaveDC/RestoreDC 可保存全部DC状态\n"
        L"2. SelectObject 返回被替换的旧对象\n"
        L"3. 用完后必须恢复旧对象再删除新对象",
        -1, &noteRc, DT_LEFT | DT_WORDBREAK);
    TrackDelete(hNoteFont);

    RestoreDC(hdc, saved);
}

/**
 * 绘制底部的泄漏计数器
 */
static void DrawLeakCounter(HDC hdc, const RECT& clientRc, const AppState* state)
{
    int saved = SaveDC(hdc);

    int barHeight = 40;
    RECT barRc = { 0, clientRc.bottom - barHeight, clientRc.right, clientRc.bottom };

    // 背景
    HBRUSH hBarBg = TrackCreate(CreateSolidBrush(RGB(50, 50, 50)));
    FillRect(hdc, &barRc, hBarBg);
    TrackDelete(hBarBg);

    // 计数器文字
    int leaked = state->createCount - state->deleteCount;
    wchar_t counter[128];
    wsprintf(counter,
        L"GDI 对象计数 — 创建: %d  删除: %d  差值: %d  %s",
        state->createCount, state->deleteCount, leaked,
        leaked == 0 ? L"[OK 无泄漏]" : L"[! 注意泄漏]");

    SetTextColor(hdc, leaked == 0 ? RGB(76, 175, 80) : RGB(255, 82, 82));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hCounterFont = TrackCreate(CreateFont(
        15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
    ));
    SelectObject(hdc, hCounterFont);
    RECT textRc = { 15, barRc.top, clientRc.right - 100, barRc.bottom };
    DrawText(hdc, counter, -1, &textRc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    TrackDelete(hCounterFont);

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
        state->createCount = 0;
        state->deleteCount = 0;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        g_state = state;

        // 创建 "重置计数器" 按钮
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        CreateWindowEx(
            0, L"BUTTON", L"重置计数",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 80, 28,   // 位置在 WM_SIZE 中调整
            hwnd, (HMENU)(LONG_PTR)ID_BTN_RESET_LEAK,
            cs->hInstance, nullptr
        );
        return 0;
    }

    case WM_SIZE:
    {
        // 调整按钮位置到右下角
        int cx = LOWORD(lParam);
        int cy = HIWORD(lParam);
        HWND hBtn = GetDlgItem(hwnd, ID_BTN_RESET_LEAK);
        if (hBtn)
            MoveWindow(hBtn, cx - 90, cy - 35, 80, 28, TRUE);
        return 0;
    }

    case WM_DESTROY:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state)
        {
            int leaked = state->createCount - state->deleteCount;
            if (leaked != 0)
            {
                wchar_t msg[128];
                wsprintf(msg, L"GDI 对象泄漏! 差值: %d", leaked);
                MessageBox(hwnd, msg, L"警告", MB_ICONWARNING);
            }
            delete state;
        }
        g_state = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_RESET_LEAK)
        {
            AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (state)
            {
                state->createCount = 0;
                state->deleteCount = 0;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state)
        {
            EndPaint(hwnd, &ps);
            return 0;
        }

        int saved = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        int barHeight = 40;
        int midX = clientRc.right / 2;
        int topH = (clientRc.bottom - barHeight) / 2;
        int botY = topH;

        // 背景
        HBRUSH hBg = TrackCreate(CreateSolidBrush(RGB(245, 245, 245)));
        FillRect(hdc, &clientRc, hBg);
        TrackDelete(hBg);

        // 四个分区
        RECT sec1 = { 0,     0,     midX,          topH };
        RECT sec2 = { midX,  0,     clientRc.right, topH };
        RECT sec3 = { 0,     botY,  midX,          clientRc.bottom - barHeight };
        RECT sec4 = { midX,  botY,  clientRc.right, clientRc.bottom - barHeight };

        DrawPenSection(hdc, sec1);
        DrawBrushSection(hdc, sec2);
        DrawFontSection(hdc, sec3);
        DrawSelectRestoreSection(hdc, sec4);

        // 绘制泄漏计数器
        DrawLeakCounter(hdc, clientRc, state);

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
        L"GDI 绘图对象示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500,
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
