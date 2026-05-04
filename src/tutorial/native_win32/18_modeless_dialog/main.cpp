/**
 * 18_modeless_dialog - 无模态对话框
 *
 * 演示无模态对话框 (Modeless Dialog) 的创建和使用：
 * - 使用 CreateDialogIndirectParam 在内存中构建 DLGTEMPLATE 来创建无模态对话框
 * - 在消息循环中使用 IsDialogMessage 处理 TAB 键导航
 * - 无模态对话框保持打开时，主窗口仍然可以交互
 * - 主窗口显示一个绘图区域（通过 WM_PAINT 绘制彩色矩形）
 * - 无模态对话框充当"调色板"，包含 3 个单选按钮（红色、绿色、蓝色）
 * - 主窗口颜色根据对话框选择实时变化
 * - 关闭无模态对话框时只是隐藏（ShowWindow SW_HIDE），而非销毁
 * - 主窗口上的"显示调色板"按钮可以重新显示对话框
 *
 * 参考: tutorial/native_win32/10_ProgrammingGUI_NativeWindows_ModelessDialog.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// ============================================================
// 常量和控件 ID 定义
// ============================================================

static const wchar_t* MAIN_WINDOW_CLASS = L"ModelessDemoClass";

// 控件 ID
#define ID_BTN_SHOW_PALETTE     2001
#define ID_RADIO_RED            3001
#define ID_RADIO_GREEN          3002
#define ID_RADIO_BLUE           3003

// 自定义消息：用于通知主窗口颜色已更改
#define WM_COLOR_CHANGED        (WM_USER + 100)

// ============================================================
// 应用程序状态
// ============================================================

struct AppState
{
    HWND hMainWindow;           // 主窗口句柄
    HWND hDlgModeless;          // 无模态对话框句柄
    int  currentColor;          // 当前选中的颜色索引 (0=红, 1=绿, 2=蓝)
};

// 颜色数组：红、绿、蓝
static const COLORREF g_colors[] = {
    RGB(220, 60, 60),    // 红色
    RGB(60, 180, 60),    // 绿色
    RGB(60, 100, 220)    // 蓝色
};

static const wchar_t* g_colorNames[] = {
    L"红色",
    L"绿色",
    L"蓝色"
};

// ============================================================
// 前向声明
// ============================================================

INT_PTR CALLBACK PaletteDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
HWND CreateModelessPaletteDialog(HINSTANCE hInstance, HWND hParent, AppState* state);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
ATOM RegisterMainWindowClass(HINSTANCE hInstance);

// ============================================================
// 辅助函数：在内存中构建无模态对话框模板
// ============================================================

/**
 * 创建无模态对话框的 DLGTEMPLATE
 *
 * 由于我们没有使用 .rc 资源文件，需要在内存中手动构建对话框模板。
 * DLGTEMPLATE 后面紧跟着菜单、类、标题字符串，然后是字体信息，
 * 最后是 DLGITEMTEMPLATE 数组（每个控件一个）。
 *
 * 对话框布局：
 *   - 标题: "调色板"
 *   - 三个单选按钮: 红色、绿色、蓝色
 *   - 底部: 一个"隐藏调色板"按钮
 */
HWND CreateModelessPaletteDialog(HINSTANCE hInstance, HWND hParent, AppState* state)
{
    // 对话框尺寸（以对话框单位计算）
    const int dlgWidth  = 180;
    const int dlgHeight = 160;

    // 分配足够的内存来存放对话框模板
    // 预留足够空间：模板头部 + 4 个控件项（3个单选 + 1个按钮）
    size_t bufSize = 1024;
    BYTE* pBuffer = (BYTE*)LocalAlloc(LPTR, bufSize);
    if (!pBuffer) return nullptr;

    BYTE* p = pBuffer;

    // ---- 填充 DLGTEMPLATE 结构 ----
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)p;
    pDlg->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT;
    pDlg->dwExtendedStyle = 0;
    pDlg->cdit = 4;    // 控件数量：3个单选按钮 + 1个按钮
    pDlg->x = 0;
    pDlg->y = 0;
    pDlg->cx = dlgWidth;
    pDlg->cy = dlgHeight;

    p += sizeof(DLGTEMPLATE);

    // 菜单名称（空字符串 = 无菜单）
    *(WORD*)p = 0; p += sizeof(WORD);

    // 窗口类（空字符串 = 默认对话框类）
    *(WORD*)p = 0; p += sizeof(WORD);

    // 对话框标题
    const wchar_t* title = L"调色板";
    size_t titleLen = wcslen(title) + 1;
    memcpy(p, title, titleLen * sizeof(wchar_t));
    p += titleLen * sizeof(wchar_t);

    // 字体大小 (DS_SETFONT 时需要)
    *(WORD*)p = 9; p += sizeof(WORD);

    // 字体名称
    const wchar_t* fontName = L"Microsoft YaHei UI";
    size_t fontLen = wcslen(fontName) + 1;
    memcpy(p, fontName, fontLen * sizeof(wchar_t));
    p += fontLen * sizeof(wchar_t);

    // ---- 辅助宏：添加控件项 ----
    // 注意：DLGITEMTEMPLATE 必须按 DWORD 对齐
    #define ALIGN_PTR(ptr) \
        { (ptr) = (BYTE*)((((DWORD_PTR)(ptr)) + 3) & ~3); }

    auto AddDlgItem = [&](
        DWORD style, DWORD exStyle,
        int x, int y, int cx, int cy,
        DWORD id,
        const wchar_t* className,
        const wchar_t* text)
    {
        ALIGN_PTR(p);
        DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)p;
        pItem->style = style;
        pItem->dwExtendedStyle = exStyle;
        pItem->x = x;
        pItem->y = y;
        pItem->cx = cx;
        pItem->cy = cy;
        pItem->id = id;
        p += sizeof(DLGITEMTEMPLATE);

        // 窗口类（以 0xFFFF 开头表示使用原子/预定义类）
        if ((ULONG_PTR)className < 0x10000)
        {
            *(WORD*)p = 0xFFFF; p += sizeof(WORD);
            *(WORD*)p = (WORD)(ULONG_PTR)className; p += sizeof(WORD);
        }
        else
        {
            size_t len = wcslen(className) + 1;
            memcpy(p, className, len * sizeof(wchar_t));
            p += len * sizeof(wchar_t);
        }

        // 控件文本
        size_t len = wcslen(text) + 1;
        memcpy(p, text, len * sizeof(wchar_t));
        p += len * sizeof(wchar_t);

        // 创建数据（0 = 无额外数据）
        *(WORD*)p = 0; p += sizeof(WORD);
    };

    // 控件 1: "红色" 单选按钮（默认选中，WS_GROUP 开启新的导航组）
    AddDlgItem(
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
        0,
        15, 15, 140, 18,
        ID_RADIO_RED,
        (const wchar_t*)0x0080,  // BUTTON 原子
        L"红色 (Red)");

    // 控件 2: "绿色" 单选按钮
    AddDlgItem(
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        0,
        15, 40, 140, 18,
        ID_RADIO_GREEN,
        (const wchar_t*)0x0080,
        L"绿色 (Green)");

    // 控件 3: "蓝色" 单选按钮
    AddDlgItem(
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        0,
        15, 65, 140, 18,
        ID_RADIO_BLUE,
        (const wchar_t*)0x0080,
        L"蓝色 (Blue)");

    // 控件 4: "隐藏调色板" 按钮
    AddDlgItem(
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        0,
        30, 100, 110, 30,
        IDOK,
        (const wchar_t*)0x0080,
        L"隐藏调色板");

    #undef ALIGN_PTR

    // 使用 CreateDialogIndirectParam 创建无模态对话框
    HWND hDlg = CreateDialogIndirectParam(
        hInstance,
        (LPCDLGTEMPLATE)pBuffer,
        hParent,
        PaletteDialogProc,
        (LPARAM)state      // 传递应用程序状态指针
    );

    LocalFree(pBuffer);
    return hDlg;
}

// ============================================================
// 无模态对话框过程 (Dialog Procedure)
// ============================================================

/**
 * 调色板对话框过程
 *
 * 对话框过程与普通窗口过程的区别：
 * 1. 返回 TRUE 表示已处理消息，返回 FALSE 表示未处理
 * 2. 不需要调用 DefWindowProc，系统会自动处理
 * 3. WM_INITDIALOG 在对话框创建后、显示前调用（类似 WM_CREATE）
 */
INT_PTR CALLBACK PaletteDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_INITDIALOG - 对话框初始化消息
     *
     * 在对话框创建完成后、显示之前调用。
     * 用于初始化控件状态和保存传入的数据。
     */
    case WM_INITDIALOG:
    {
        // 保存应用状态指针到对话框的用户数据区
        AppState* state = (AppState*)lParam;
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)state);

        // 默认选中"红色"单选按钮
        CheckRadioButton(hDlg, ID_RADIO_RED, ID_RADIO_BLUE, ID_RADIO_RED);
        state->currentColor = 0;  // 红色

        return TRUE;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理单选按钮和按钮的点击事件。
     */
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == ID_RADIO_RED || id == ID_RADIO_GREEN || id == ID_RADIO_BLUE)
        {
            // 更新选中状态
            CheckRadioButton(hDlg, ID_RADIO_RED, ID_RADIO_BLUE, id);

            // 计算颜色索引
            int colorIndex = id - ID_RADIO_RED;

            // 获取应用状态
            AppState* state = (AppState*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (state)
            {
                state->currentColor = colorIndex;

                // 通知主窗口重绘（触发 WM_PAINT）
                InvalidateRect(state->hMainWindow, nullptr, TRUE);
            }
        }
        else if (id == IDOK)
        {
            // "隐藏调色板"按钮 - 只隐藏对话框，不销毁
            ShowWindow(hDlg, SW_HIDE);
        }

        return TRUE;
    }

    /**
     * WM_CLOSE - 关闭对话框
     *
     * 关键区别：对于无模态对话框，关闭时不销毁，而是隐藏。
     * 这样可以反复显示/隐藏，避免重复创建。
     */
    case WM_CLOSE:
    {
        // 隐藏而非销毁！这是无模态对话框的常见模式
        ShowWindow(hDlg, SW_HIDE);
        return TRUE;
    }

    default:
        return FALSE;
    }
}

// ============================================================
// 主窗口过程
// ============================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建消息
     *
     * 初始化应用程序状态，创建"显示调色板"按钮和无模态对话框。
     */
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCT*)lParam;

        // 初始化应用状态
        state = new AppState();
        state->hMainWindow = hwnd;
        state->hDlgModeless = nullptr;
        state->currentColor = 0;   // 默认红色

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 创建"显示调色板"按钮
        CreateWindowEx(
            0, L"BUTTON", L"显示调色板",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            15, 15, 140, 35,
            hwnd, (HMENU)ID_BTN_SHOW_PALETTE,
            cs->hInstance, nullptr
        );

        // 创建提示文本
        CreateWindowEx(
            0, L"STATIC",
            L"点击上方按钮打开调色板，\n选择颜色后绘图区域将变色。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, 60, 200, 40,
            hwnd, nullptr, cs->hInstance, nullptr
        );

        // 创建无模态对话框
        state->hDlgModeless = CreateModelessPaletteDialog(cs->hInstance, hwnd, state);

        // 设置字体
        HFONT hFont = CreateFont(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei UI");

        EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL
        {
            SendMessage(child, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理"显示调色板"按钮的点击事件。
     */
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == ID_BTN_SHOW_PALETTE && state && state->hDlgModeless)
        {
            // 显示并激活无模态对话框
            ShowWindow(state->hDlgModeless, SW_SHOW);
            SetForegroundWindow(state->hDlgModeless);
        }

        return 0;
    }

    /**
     * WM_PAINT - 绘制消息
     *
     * 在主窗口客户区绘制彩色矩形，颜色由调色板对话框的选择决定。
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        if (state)
        {
            int colorIdx = state->currentColor;
            if (colorIdx < 0) colorIdx = 0;
            if (colorIdx > 2) colorIdx = 2;

            // 绘制彩色矩形区域
            RECT rcDraw = {
                230, 15,
                rcClient.right - 15,
                rcClient.bottom - 50
            };

            // 填充背景
            HBRUSH hBrush = CreateSolidBrush(g_colors[colorIdx]);
            FillRect(hdc, &rcDraw, hBrush);
            DeleteObject(hBrush);

            // 绘制边框
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(80, 80, 80));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rcDraw.left, rcDraw.top, rcDraw.right, rcDraw.bottom);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            // 在矩形中央显示当前颜色名称
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            // 绘制带阴影的文字（提高可读性）
            RECT rcText = rcDraw;
            OffsetRect(&rcText, 1, 1);
            SetTextColor(hdc, RGB(0, 0, 0));
            DrawText(hdc, g_colorNames[colorIdx], -1, &rcText,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            OffsetRect(&rcText, -1, -1);
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawText(hdc, g_colorNames[colorIdx], -1, &rcText,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 在底部绘制说明文字
        SetTextColor(hdc, RGB(100, 100, 100));
        SetBkMode(hdc, TRANSPARENT);
        RECT rcBottom = { 15, rcClient.bottom - 35, rcClient.right - 15, rcClient.bottom - 10 };
        DrawText(hdc, L"提示: 无模态对话框关闭后只是隐藏，可通过按钮重新显示",
                 -1, &rcBottom, DT_LEFT | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    /**
     * WM_DESTROY - 窗口销毁消息
     *
     * 销毁无模态对话框并释放资源。
     */
    case WM_DESTROY:
    {
        if (state)
        {
            // 销毁无模态对话框（程序退出时才真正销毁）
            if (state->hDlgModeless)
            {
                DestroyWindow(state->hDlgModeless);
                state->hDlgModeless = nullptr;
            }
            delete state;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================
// 注册窗口类
// ============================================================

ATOM RegisterMainWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    return RegisterClassEx(&wc);
}

// ============================================================
// 应用程序入口点
// ============================================================

/**
 * wWinMain - Windows 应用程序入口点
 *
 * 创建主窗口和无模态对话框，然后进入消息循环。
 *
 * 关键点：无模态对话框的消息循环需要特殊处理：
 * - 使用 IsDialogMessage 检查消息是否属于无模态对话框
 * - 如果是，则由对话框管理器处理（支持 TAB 键导航等）
 * - 如果不是，则正常分发到主窗口
 */
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // 步骤 1: 注册窗口类
    if (RegisterMainWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 步骤 2: 创建主窗口
    HWND hwnd = CreateWindowEx(
        0,
        MAIN_WINDOW_CLASS,
        L"无模态对话框示例",          // 窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 350,                     // 窗口大小 500x350
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 步骤 3: 消息循环
    //
    // 重要：无模态对话框的消息循环与普通消息循环不同！
    // 必须使用 IsDialogMessage 来处理对话框导航键（TAB、方向键等）。
    // 如果不使用 IsDialogMessage，对话框内的 TAB 键将无法切换焦点。
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        // 获取应用状态以访问无模态对话框句柄
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        HWND hDlgModeless = state ? state->hDlgModeless : nullptr;

        // 关键：检查消息是否属于无模态对话框
        // IsDialogMessage 会自动处理对话框的键盘导航（TAB、方向键等）
        // 如果消息已被处理，则跳过后续的 TranslateMessage/DispatchMessage
        if (hDlgModeless && IsDialogMessage(hDlgModeless, &msg))
        {
            continue;   // 消息已被对话框处理，跳过默认分发
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
