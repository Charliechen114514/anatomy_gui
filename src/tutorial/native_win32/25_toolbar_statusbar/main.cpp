/**
 * 工具栏与状态栏示例 (Example 25)
 *
 * 本示例演示如何使用通用控件创建工具栏 (Toolbar) 和状态栏 (Status Bar)，
 * 无需 .rc 资源文件，所有控件均在代码中动态创建。
 *
 * 功能概览:
 *   - 工具栏: 新建 / 打开 / 保存 | 剪切 / 复制 / 粘贴
 *   - 状态栏: 默认显示"就绪"，鼠标悬停按钮时显示提示，点击按钮显示操作名称
 *   - 主区域: 多行编辑控件，自动填满工具栏与状态栏之间的空间
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>

// 链接通用控件库 (comctl32.lib)
#pragma comment(lib, "comctl32.lib")

// ============================================================================
// 控件与命令 ID 定义
// ============================================================================
#define ID_TOOLBAR          2001
#define ID_STATUSBAR        2002
#define ID_EDITCTRL         2003

// 工具栏按钮命令 ID
#define IDM_NEW             3001
#define IDM_OPEN            3002
#define IDM_SAVE            3003
#define IDM_CUT             3004
#define IDM_COPY            3005
#define IDM_PASTE           3006

// ============================================================================
// 全局变量
// ============================================================================
static HINSTANCE g_hInst      = nullptr;   // 应用程序实例句柄
static HWND      g_hToolBar   = nullptr;   // 工具栏句柄
static HWND      g_hStatusBar = nullptr;   // 状态栏句柄
static HWND      g_hEdit      = nullptr;   // 编辑控件句柄

// 工具栏按钮描述文本（用于状态栏提示）
static const LPCTSTR g_ButtonTips[] = {
    _T("新建文件"),
    _T("打开文件"),
    _T("保存文件"),
    nullptr,   // 分隔符
    _T("剪切"),
    _T("复制"),
    _T("粘贴")
};

// 工具栏按钮对应的命令 ID
static const int g_ButtonIDs[] = {
    IDM_NEW, IDM_OPEN, IDM_SAVE,
    0,        // 分隔符
    IDM_CUT, IDM_COPY, IDM_PASTE
};

// ============================================================================
// 前向声明
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
void    CreateToolBar(HWND hWndParent);
void    CreateStatusBar(HWND hWndParent);
void    CreateEditControl(HWND hWndParent);
void    ResizeControls();
void    UpdateStatusBar(LPCTSTR lpszText);

// ============================================================================
// 创建工具栏
// ============================================================================
void CreateToolBar(HWND hWndParent)
{
    // ---- 创建工具栏窗口 ----
    g_hToolBar = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CCS_ADJUSTABLE,
        0, 0, 0, 0,
        hWndParent,
        (HMENU)ID_TOOLBAR,
        g_hInst,
        nullptr
    );

    // 发送 TB_BUTTONSTRUCTSIZE 消息，告知系统按钮结构大小
    SendMessage(g_hToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    // ---- 加载标准系统位图 ----
    // IDB_STD_SMALL_COLOR 包含一组标准小图标（新建、打开、保存、剪切等）
    TBADDBITMAP tbab;
    tbab.hInst = HINST_COMMCTRL;
    tbab.nID   = IDB_STD_SMALL_COLOR;
    int nBmpOffset = (int)SendMessage(g_hToolBar, TB_ADDBITMAP, 0, (LPARAM)&tbab);

    // ---- 定义工具栏按钮 ----
    // 每个按钮: 位图索引 + 命令 ID + 状态 + 样式 + 数据 + 字符串
    TBBUTTON tbb[] = {
        // 新建
        { STD_FILENEW  + nBmpOffset, IDM_NEW,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("新建") },
        // 打开
        { STD_FILEOPEN + nBmpOffset, IDM_OPEN,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("打开") },
        // 保存
        { STD_FILESAVE + nBmpOffset, IDM_SAVE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("保存") },
        // 分隔符
        { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0 },
        // 剪切
        { STD_CUT      + nBmpOffset, IDM_CUT,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("剪切") },
        // 复制
        { STD_COPY     + nBmpOffset, IDM_COPY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("复制") },
        // 粘贴
        { STD_PASTE    + nBmpOffset, IDM_PASTE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)_T("粘贴") },
    };

    // 添加按钮到工具栏
    SendMessage(g_hToolBar, TB_ADDBUTTONS, _countof(tbb), (LPARAM)tbb);

    // 自动调整工具栏大小
    SendMessage(g_hToolBar, TB_AUTOSIZE, 0, 0);
}

// ============================================================================
// 创建状态栏
// ============================================================================
void CreateStatusBar(HWND hWndParent)
{
    // ---- 使用 CreateWindowEx 创建状态栏 ----
    g_hStatusBar = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hWndParent,
        (HMENU)ID_STATUSBAR,
        g_hInst,
        nullptr
    );

    // 将状态栏分为 3 个部分: 提示文字 | 坐标信息 | 时间
    int nPartEdges[] = { 350, 500, -1 };
    SendMessage(g_hStatusBar, SB_SETPARTS, 3, (LPARAM)nPartEdges);

    // 设置默认文本
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)_T("就绪"));
}

// ============================================================================
// 创建多行编辑控件
// ============================================================================
void CreateEditControl(HWND hWndParent)
{
    g_hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        _T("EDIT"),
        _T("在此输入文本...\r\n\r\n工具栏按钮: 新建 | 打开 | 保存 | 剪切 | 复制 | 粘贴\r\n将鼠标悬停在工具栏按钮上，状态栏会显示对应提示。"),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hWndParent,
        (HMENU)ID_EDITCTRL,
        g_hInst,
        nullptr
    );

    // 设置编辑控件字体
    HFONT hFont = CreateFont(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        _T("Microsoft YaHei UI")
    );
    SendMessage(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// ============================================================================
// 调整所有控件的位置和大小
// ============================================================================
void ResizeControls()
{
    RECT rcClient;
    GetClientRect(GetParent(g_hToolBar), &rcClient);

    // ---- 重新调整工具栏 ----
    SendMessage(g_hToolBar, TB_AUTOSIZE, 0, 0);

    // ---- 获取工具栏和状态栏的高度 ----
    RECT rcTB, rcSB;
    GetWindowRect(g_hToolBar, &rcTB);
    GetWindowRect(g_hStatusBar, &rcSB);
    int nToolBarH   = rcTB.bottom - rcTB.top;
    int nStatusBarH = rcSB.bottom - rcSB.top;

    // ---- 调整编辑控件以填满剩余空间 ----
    MoveWindow(
        g_hEdit,
        0, nToolBarH,
        rcClient.right,
        rcClient.bottom - nToolBarH - nStatusBarH,
        TRUE
    );
}

// ============================================================================
// 更新状态栏文本
// ============================================================================
void UpdateStatusBar(LPCTSTR lpszText)
{
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)lpszText);
}

// ============================================================================
// 获取工具栏按钮索引对应的按钮描述
// ============================================================================
LPCTSTR GetButtonToolTip(int nIndex)
{
    // 映射真实按钮索引到描述数组
    // tbb 数组: [新建, 打开, 保存, 分隔符, 剪切, 复制, 粘贴]
    int nBtnCount = (int)SendMessage(g_hToolBar, TB_BUTTONCOUNT, 0, 0);
    if (nIndex < 0 || nIndex >= _countof(g_ButtonTips))
        return nullptr;
    return g_ButtonTips[nIndex];
}

// ============================================================================
// 主窗口过程
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // ---- 依次创建工具栏、状态栏、编辑控件 ----
        CreateToolBar(hWnd);
        CreateStatusBar(hWnd);
        CreateEditControl(hWnd);

        // 首次调整布局
        ResizeControls();
        return 0;
    }

    case WM_SIZE:
    {
        // ---- 窗口大小改变时重新调整控件布局 ----
        SendMessage(g_hToolBar,   WM_SIZE, wParam, lParam);
        SendMessage(g_hStatusBar, WM_SIZE, wParam, lParam);
        ResizeControls();
        return 0;
    }

    case WM_NOTIFY:
    {
        // ---- 处理工具栏通知消息 ----
        NMHDR* pnmh = (NMHDR*)lParam;

        if (pnmh->idFrom == ID_TOOLBAR)
        {
            switch (pnmh->code)
            {
            case TTN_GETDISPINFO:
            {
                // 工具栏请求提示文本
                NMTTDISPINFO* pTTDI = (NMTTDISPINFO*)lParam;
                // 从工具栏按钮获取提示信息
                int nIndex = (int)pTTDI->hdr.idFrom;

                // idFrom 在 TTN_GETDISPINFO 中实际上是命令 ID
                switch (nIndex)
                {
                case IDM_NEW:   pTTDI->lpszText = (LPTSTR)_T("新建文件");   break;
                case IDM_OPEN:  pTTDI->lpszText = (LPTSTR)_T("打开文件");   break;
                case IDM_SAVE:  pTTDI->lpszText = (LPTSTR)_T("保存文件");   break;
                case IDM_CUT:   pTTDI->lpszText = (LPTSTR)_T("剪切所选内容"); break;
                case IDM_COPY:  pTTDI->lpszText = (LPTSTR)_T("复制所选内容"); break;
                case IDM_PASTE: pTTDI->lpszText = (LPTSTR)_T("粘贴到编辑区"); break;
                default:        pTTDI->lpszText = (LPTSTR)_T("工具栏按钮");  break;
                }
                return 0;
            }
            }
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        // ---- 鼠标移动时检测是否悬停在工具栏按钮上 ----
        // 获取鼠标位置
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int nHitTest = (int)SendMessage(g_hToolBar, TB_HITTEST, 0, (LPARAM)&pt);

        if (nHitTest >= 0 && nHitTest < (int)_countof(g_ButtonTips))
        {
            LPCTSTR lpszTip = GetButtonToolTip(nHitTest);
            if (lpszTip)
            {
                TCHAR szBuf[128];
                _stprintf_s(szBuf, _T("提示: %s"), lpszTip);
                UpdateStatusBar(szBuf);
            }
        }
        else
        {
            // 鼠标不在工具栏按钮上，恢复默认文本
            UpdateStatusBar(_T("就绪"));
        }
        return 0;
    }

    case WM_MENUSELECT:
    {
        // ---- 菜单选择时更新状态栏（此处无菜单，预留扩展） ----
        if (LOWORD(wParam) == 0xFFFF && lParam == 0)
        {
            // 菜单关闭，恢复默认文本
            UpdateStatusBar(_T("就绪"));
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        // ---- 工具栏按钮命令 ----
        case IDM_NEW:
            UpdateStatusBar(_T("操作: 新建文件"));
            SetWindowText(g_hEdit, _T(""));
            return 0;

        case IDM_OPEN:
            UpdateStatusBar(_T("操作: 打开文件（演示）"));
            MessageBox(hWnd, _T("打开文件功能（仅演示）"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            return 0;

        case IDM_SAVE:
            UpdateStatusBar(_T("操作: 保存文件（演示）"));
            MessageBox(hWnd, _T("保存文件功能（仅演示）"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            return 0;

        case IDM_CUT:
            UpdateStatusBar(_T("操作: 剪切"));
            SendMessage(g_hEdit, WM_CUT, 0, 0);
            return 0;

        case IDM_COPY:
            UpdateStatusBar(_T("操作: 复制"));
            SendMessage(g_hEdit, WM_COPY, 0, 0);
            return 0;

        case IDM_PASTE:
            UpdateStatusBar(_T("操作: 粘贴"));
            SendMessage(g_hEdit, WM_PASTE, 0, 0);
            return 0;
        }
        break;
    }

    case WM_SETFOCUS:
        // 窗口获得焦点时，将焦点转到编辑控件
        SetFocus(g_hEdit);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// WinMain —— 程序入口
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    g_hInst = hInstance;

    // ---- 初始化通用控件 ----
    // ICC_BAR_CLASSES: 注册工具栏、状态栏、工具提示等控件类
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC  = ICC_BAR_CLASSES;   // 工具栏 + 状态栏
    InitCommonControlsEx(&iccex);

    // ---- 注册主窗口类 ----
    WNDCLASSEX wc   = { sizeof(WNDCLASSEX) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("ToolBarStatusBarDemoClass");
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassEx(&wc);

    // ---- 创建主窗口 ----
    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        _T("工具栏与状态栏示例"),           // 窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        650, 450,                           // 窗口尺寸 650×450
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // ---- 消息循环 ----
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
