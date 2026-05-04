/**
 * 16_more_controls - 更多控件示例 (ProgressBar/TrackBar/UpDown/Tab)
 *
 * 演示四种 Win32 通用控件的使用：
 * 1. ProgressBar (PROGRESS_CLASS) - 进度条，支持手动和自动（定时器）两种模式
 * 2. TrackBar (TRACKBAR_CLASS) - 滑块控件，拖动可控制进度条位置
 * 3. UpDown (UPDOWN_CLASS) - 旋转按钮，与 Edit 控件配对实现数值微调
 * 4. TabControl (WC_TABCONTROL) - 标签页，切换不同内容视图
 *
 * 关键知识点：
 * - InitCommonControlsEx 初始化通用控件
 * - PBM_SETRANGE32 / PBM_SETPOS 控制进度条
 * - TBM_SETRANGE / TBM_SETPOS / TBM_GETPOS 操作滑块
 * - UDM_SETBUDDY / UDM_SETRANGE / UDM_SETPOS 操作旋转按钮
 * - TCM_INSERTITEM / TCN_SELCHANGE 管理标签页
 * - GWLP_USERDATA 模式管理应用状态
 * - EnumChildWindows 批量设置子控件字体
 * - WM_TIMER 实现自动进度动画
 *
 * 参考: tutorial/native_win32/8_ProgrammingGUI_NativeWindows_MoreControls.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"MoreControlsClass";

// 控件 ID
#define ID_PROGRESS      1001
#define ID_TRACKBAR      1002
#define ID_UPDOWN_EDIT   1003
#define ID_UPDOWN        1004
#define ID_TABCONTROL    1005
#define ID_EDIT_LOG      1006
#define ID_BTN_AUTO      1007
#define ID_TIMER         1

/**
 * 应用状态结构体
 *
 * 使用 GWLP_USERDATA 模式，将所有控件句柄和状态变量
 * 保存在一个结构体中，通过 SetWindowLongPtr / GetWindowLongPtr 访问。
 * 这种方式比全局变量更安全，也支持多窗口实例。
 */
struct AppState
{
    // 控件句柄
    HWND hProgress;       // 进度条
    HWND hTrackBar;       // 滑块
    HWND hUpDownEdit;     // 旋转按钮的伙伴编辑框
    HWND hUpDown;         // 旋转按钮
    HWND hTabControl;     // 标签页控件
    HWND hEditLog;        // 日志编辑框

    // 自动进度模式状态
    BOOL autoMode;        // 是否处于自动进度模式
    int  autoProgress;    // 自动模式下的当前进度值
};

/**
 * 向日志编辑框追加一行文本
 */
void AddLog(AppState* state, const std::wstring& msg)
{
    int len = GetWindowTextLength(state->hEditLog);
    SendMessage(state->hEditLog, EM_SETSEL, len, len);
    SendMessage(state->hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
}

/**
 * 创建所有子控件并初始化
 *
 * 布局说明（窗口客户区 550x400）：
 * ┌────────────────────────────────────────────────────┐
 * │ [进度条]  [滑块]  [旋转+编辑框]  [自动按钮]        │
 * │ ──────────────────────────────────────────────     │
 * │ [TabControl: 进度条 | 滑块 | 旋转按钮]             │
 * │ ┌──────────────────────────────────────────────┐  │
 * │ │  标签页内容区域                                │  │
 * │ └──────────────────────────────────────────────┘  │
 * │ ──────────────────────────────────────────────     │
 * │ 日志输出区域                                       │
 * └────────────────────────────────────────────────────┘
 */
void CreateControls(HWND hwnd, AppState* state)
{
    auto hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

    // 创建统一的 UI 字体
    HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    // ── 第一行：进度条区域 ──
    CreateWindowEx(0, L"STATIC", L"进度条:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 12, 55, 20, hwnd, nullptr, hInst, nullptr);

    state->hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        75, 10, 200, 22, hwnd, (HMENU)ID_PROGRESS, hInst, nullptr);
    SendMessage(state->hProgress, PBM_SETRANGE32, 0, 100);
    SendMessage(state->hProgress, PBM_SETPOS, 0, 0);

    // ── 第一行：滑块区域 ──
    CreateWindowEx(0, L"STATIC", L"滑块:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        285, 12, 40, 20, hwnd, nullptr, hInst, nullptr);

    state->hTrackBar = CreateWindowEx(0, TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        330, 5, 150, 32, hwnd, (HMENU)ID_TRACKBAR, hInst, nullptr);
    SendMessage(state->hTrackBar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(state->hTrackBar, TBM_SETTICFREQ, 10, 0);
    SendMessage(state->hTrackBar, TBM_SETPOS, TRUE, 0);

    // ── 第一行：UpDown 区域 ──
    state->hUpDownEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_NUMBER,
        15, 45, 55, 22, hwnd, (HMENU)ID_UPDOWN_EDIT, hInst, nullptr);

    state->hUpDown = CreateWindowEx(0, UPDOWN_CLASS, L"",
        WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_WRAP,
        0, 0, 0, 0, hwnd, (HMENU)ID_UPDOWN, hInst, nullptr);
    SendMessage(state->hUpDown, UDM_SETBUDDY, (WPARAM)state->hUpDownEdit, 0);
    SendMessage(state->hUpDown, UDM_SETRANGE, 0, MAKELONG(100, 0));  // HIWORD=最大, LOWORD=最小
    SendMessage(state->hUpDown, UDM_SETPOS, 0, 0);

    // ── 第一行：自动进度复选框 ──
    CreateWindowEx(0, L"BUTTON", L"自动进度",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        90, 43, 90, 26, hwnd, (HMENU)ID_BTN_AUTO, hInst, nullptr);

    // ── 第二行：标签页控件 ──
    CreateWindowEx(0, L"STATIC", L"标签页控件:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 80, 80, 20, hwnd, nullptr, hInst, nullptr);

    state->hTabControl = CreateWindowEx(0, WC_TABCONTROL, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        15, 100, 510, 135, hwnd, (HMENU)ID_TABCONTROL, hInst, nullptr);

    // 添加三个标签页
    TCITEM tie = {};
    tie.mask = TCIF_TEXT;

    tie.pszText = (LPWSTR)L"进度条";
    SendMessage(state->hTabControl, TCM_INSERTITEM, 0, (LPARAM)&tie);

    tie.pszText = (LPWSTR)L"滑块";
    SendMessage(state->hTabControl, TCM_INSERTITEM, 1, (LPARAM)&tie);

    tie.pszText = (LPWSTR)L"旋转按钮";
    SendMessage(state->hTabControl, TCM_INSERTITEM, 2, (LPARAM)&tie);

    // ── 日志输出区域 ──
    CreateWindowEx(0, L"STATIC", L"事件日志:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 245, 80, 20, hwnd, nullptr, hInst, nullptr);

    state->hEditLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        15, 265, 510, 95, hwnd, (HMENU)ID_EDIT_LOG, hInst, nullptr);

    // 使用 EnumChildWindows 批量设置所有子控件字体
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL
    {
        SendMessage(child, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

/**
 * 获取标签页显示区域的（相对于 TabControl 父窗口的）坐标
 *
 * TabControl 本身有一个显示区域 (display rect)，是标签头下方的空白区域。
 * 我们需要获取这个区域来正确定位标签页内容。
 */
RECT GetTabDisplayRect(HWND hTab)
{
    RECT rc;
    GetClientRect(hTab, &rc);
    SendMessage(hTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rc);
    return rc;
}

/**
 * 在标签页切换时，更新标签页区域的描述文本
 *
 * 由于本示例将所有控件放在主窗口上（不使用子窗口标签页），
 * 我们通过日志来反馈当前标签页的内容说明。
 */
void OnTabChanged(AppState* state, int tabIndex)
{
    const wchar_t* descriptions[] = {
        L"进度条 (ProgressBar)\r\n"
        L"  - 使用 PROGRESS_CLASS 创建\r\n"
        L"  - PBM_SETRANGE32 设置范围 (0-100)\r\n"
        L"  - PBM_SETPOS 设置当前位置\r\n"
        L"  - PBS_SMOOTH 启用平滑样式\r\n"
        L"  - 可通过滑块手动控制或开启自动进度模式",

        L"滑块 (TrackBar)\r\n"
        L"  - 使用 TRACKBAR_CLASS 创建\r\n"
        L"  - TBM_SETRANGE 设置范围\r\n"
        L"  - TBM_SETPOS / TBM_GETPOS 操作位置\r\n"
        L"  - TBS_AUTOTICKS 自动显示刻度\r\n"
        L"  - 通过 WM_HSCROLL 消息响应位置变化",

        L"旋转按钮 (UpDown)\r\n"
        L"  - 使用 UPDOWN_CLASS 创建\r\n"
        L"  - UDM_SETBUDDY 绑定伙伴编辑框\r\n"
        L"  - UDS_SETBUDDYINT 自动更新编辑框文字\r\n"
        L"  - UDM_SETRANGE 设置范围 (0-100)\r\n"
        L"  - UDM_SETPOS / UDM_GETPOS 操作位置"
    };

    std::wostringstream oss;
    oss << L"\r\n[标签页切换] 当前: " << descriptions[tabIndex] << L"\r\n";
    AddLog(state, oss.str());
}

/**
 * 窗口过程函数
 *
 * 处理所有窗口消息，包括控件通知、定时器事件等。
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 通过 GWLP_USERDATA 获取应用状态指针
    auto* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建消息
     *
     * 初始化通用控件库，创建应用状态，构建所有子控件。
     */
    case WM_CREATE:
    {
        // 初始化通用控件库
        // 需要同时初始化多种控件类：
        // - ICC_BAR_CLASSES: TrackBar 和 ProgressBar
        // - ICC_UPDOWN_CLASS: UpDown 控件
        // - ICC_TAB_CLASSES: TabControl 控件
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_BAR_CLASSES | ICC_UPDOWN_CLASS | ICC_TAB_CLASSES;
        InitCommonControlsEx(&icc);

        // 创建并绑定状态
        state = new AppState{};
        state->autoMode = FALSE;
        state->autoProgress = 0;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 创建所有控件
        CreateControls(hwnd, state);

        // 输出初始日志
        AddLog(state, L"=== 更多控件示例 ===\r\n");
        AddLog(state, L"本示例演示四种 Win32 通用控件:\r\n");
        AddLog(state, L"  1. ProgressBar - 进度条\r\n");
        AddLog(state, L"  2. TrackBar - 滑块\r\n");
        AddLog(state, L"  3. UpDown - 旋转按钮\r\n");
        AddLog(state, L"  4. TabControl - 标签页\r\n\r\n");
        AddLog(state, L"拖动滑块可控制进度条位置\r\n");
        AddLog(state, L"勾选「自动进度」可启动定时器动画\r\n\r\n");
        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理自动进度复选框的点击事件。
     * BS_AUTOCHECKBOX 样式会自动切换选中状态，
     * 我们通过 BM_GETCHECK 读取当前状态来决定启停定时器。
     */
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_AUTO && HIWORD(wParam) == BN_CLICKED)
        {
            // 获取复选框的选中状态
            LRESULT check = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
            state->autoMode = (check == BST_CHECKED) ? TRUE : FALSE;

            if (state->autoMode)
            {
                // 启动定时器，每 100ms 触发一次
                SetTimer(hwnd, ID_TIMER, 100, nullptr);
                AddLog(state, L"[自动进度] 已启动定时器 (100ms)\r\n");
            }
            else
            {
                // 停止定时器
                KillTimer(hwnd, ID_TIMER);
                AddLog(state, L"[自动进度] 已停止定时器\r\n");
            }
        }
        return 0;
    }

    /**
     * WM_HSCROLL - 水平滚动消息
     *
     * TrackBar 通过 WM_HSCROLL 通知父窗口位置变化。
     * lParam 包含 TrackBar 的窗口句柄。
     * 我们将滑块位置同步到进度条。
     */
    case WM_HSCROLL:
    {
        HWND hTrackBar = (HWND)lParam;

        // 检查是否是我们的 TrackBar 控件
        if (hTrackBar && GetWindowLongPtr(hTrackBar, GWLP_ID) == ID_TRACKBAR)
        {
            // 获取滑块当前位置
            int pos = (int)SendMessage(hTrackBar, TBM_GETPOS, 0, 0);

            // 同步到进度条
            SendMessage(state->hProgress, PBM_SETPOS, pos, 0);

            // 更新自动进度的跟踪值
            state->autoProgress = pos;

            // 输出日志（仅在拖动结束时）
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
            {
                std::wostringstream oss;
                oss << L"[TrackBar] 滑块位置: " << pos << L"\r\n";
                AddLog(state, oss.str());
            }
        }
        return 0;
    }

    /**
     * WM_NOTIFY - 通知消息
     *
     * 处理 TabControl 的标签页切换通知和 UpDown 的数值变化通知。
     */
    case WM_NOTIFY:
    {
        auto* nmhdr = (NMHDR*)lParam;

        // TabControl 标签页切换通知
        if (nmhdr->idFrom == ID_TABCONTROL && nmhdr->code == TCN_SELCHANGE)
        {
            // 获取当前选中的标签索引
            int sel = (int)SendMessage(state->hTabControl, TCM_GETCURSEL, 0, 0);
            OnTabChanged(state, sel);
        }

        // UpDown 数值即将变化通知
        if (nmhdr->code == UDN_DELTAPOS)
        {
            auto* pnmud = (NMUPDOWN*)lParam;
            std::wostringstream oss;
            oss << L"[UpDown] 位置: " << pnmud->iPos
                << L", 变化量: " << pnmud->iDelta << L"\r\n";
            AddLog(state, oss.str());
        }

        return 0;
    }

    /**
     * WM_TIMER - 定时器消息
     *
     * 当 autoMode 为 TRUE 时，定时器每 100ms 触发一次，
     * 自动递增进度条的值，到达 100 后重置为 0 循环播放。
     */
    case WM_TIMER:
    {
        if (wParam == ID_TIMER && state && state->autoMode)
        {
            // 递增自动进度
            state->autoProgress += 2;
            if (state->autoProgress > 100)
            {
                state->autoProgress = 0;
                AddLog(state, L"[Timer] 进度已满，重置为 0\r\n");
            }

            // 更新进度条位置
            SendMessage(state->hProgress, PBM_SETPOS, state->autoProgress, 0);

            // 同步到滑块位置
            SendMessage(state->hTrackBar, TBM_SETPOS, TRUE, state->autoProgress);
        }
        return 0;
    }

    /**
     * WM_DESTROY - 窗口销毁消息
     *
     * 清理定时器，释放状态内存，投递退出消息。
     */
    case WM_DESTROY:
    {
        // 停止定时器
        KillTimer(hwnd, ID_TIMER);

        // 释放状态
        delete state;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);

        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/**
 * wWinMain - Windows 应用程序入口点 (Unicode 版本)
 *
 * 注册窗口类，创建主窗口，运行消息循环。
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassEx(&wc);

    // 创建主窗口
    HWND hwnd = CreateWindowEx(0, WINDOW_CLASS_NAME,
        L"更多控件示例 — ProgressBar/TrackBar/UpDown/Tab",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 400,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
