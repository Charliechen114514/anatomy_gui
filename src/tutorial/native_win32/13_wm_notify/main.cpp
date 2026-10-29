/**
 * 13_wm_notify - WM_NOTIFY 通知机制示例
 *
 * 演示 WM_NOTIFY 消息处理：
 * - InitCommonControlsEx 初始化公共控件
 * - WM_NOTIFY 与 WM_COMMAND 的区别
 * - NMHDR 结构体解析
 * - 通用通知码（NM_CLICK, NM_DBLCLK 等）
 *
 * 参考: tutorial/native_win32/5_ProgrammingGUI_NativeWindows_WM_NOTIFY.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* WINDOW_CLASS_NAME = L"WmNotifyClass";

#define ID_LISTBOX      1001
#define ID_EDIT_LOG     1002

struct AppState
{
    HWND hListBox;
    HWND hEditLog;
};

void AddLog(AppState* state, const std::wstring& msg)
{
    int len = GetWindowTextLength(state->hEditLog);
    SendMessage(state->hEditLog, EM_SETSEL, len, len);
    SendMessage(state->hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
}

void CreateControls(HWND hwnd, AppState* state)
{
    auto hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    // 左侧说明
    CreateWindowEx(0, L"STATIC",
        L"WM_NOTIFY 演示：\n"
        L"1. 单击列表项 → 触发 LBN_SELCHANGE (WM_COMMAND)\n"
        L"2. 双击列表项 → 触发 NM_DBLCLK (WM_NOTIFY)\n"
        L"3. 对比 WM_COMMAND 和 WM_NOTIFY 的参数结构",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 10, 350, 60, hwnd, nullptr, hInst, nullptr);

    // 列表框（通过 WM_COMMAND 发送通知）
    state->hListBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOTIFY,
        15, 80, 200, 200, hwnd, (HMENU)ID_LISTBOX, hInst, nullptr);

    const wchar_t* items[] = { L"鼠标事件", L"键盘事件", L"窗口消息", L"定时器", L"绘制事件" };
    for (auto item : items)
    {
        SendMessage(state->hListBox, LB_ADDSTRING, 0, (LPARAM)item);
    }

    // 日志输出
    CreateWindowEx(0, L"STATIC", L"事件日志（区分 WM_COMMAND 和 WM_NOTIFY）:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        230, 70, 300, 20, hwnd, nullptr, hInst, nullptr);

    state->hEditLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        230, 90, 340, 190, hwnd, (HMENU)ID_EDIT_LOG, hInst, nullptr);

    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL
    {
        SendMessage(child, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 初始化公共控件
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        state = new AppState{};
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        CreateControls(hwnd, state);
        AddLog(state, L"=== WM_NOTIFY 通知机制示例 ===\r\n");
        AddLog(state, L"提示：单击列表项观察 WM_COMMAND\r\n");
        AddLog(state, L"      双击列表项观察 WM_NOTIFY\r\n\r\n");
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_LISTBOX && code == LBN_SELCHANGE)
        {
            int sel = (int)SendMessage(state->hListBox, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR)
            {
                wchar_t buf[64] = {};
                SendMessage(state->hListBox, LB_GETTEXT, sel, (LPARAM)buf);
                std::wostringstream oss;
                oss << L"[WM_COMMAND] LBN_SELCHANGE\r\n"
                    << L"  wParam: id=" << id << L", code=" << code << L"\r\n"
                    << L"  lParam: hwndFrom=列表框句柄\r\n"
                    << L"  选择: " << buf << L"\r\n\r\n";
                AddLog(state, oss.str());
            }
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        int id = (int)wParam;
        auto* nmhdr = (NMHDR*)lParam;

        std::wostringstream oss;
        oss << L"[WM_NOTIFY]\r\n"
            << L"  wParam (idFrom): " << id << L"\r\n"
            << L"  NMHDR.idFrom: " << nmhdr->idFrom << L"\r\n"
            << L"  NMHDR.code: 0x" << std::hex << nmhdr->code << std::dec << L"\r\n";

        switch (nmhdr->code)
        {
        case NM_CLICK:
            oss << L"  → NM_CLICK (单击)\r\n\r\n";
            break;
        case NM_DBLCLK:
            oss << L"  → NM_DBLCLK (双击)\r\n\r\n";
            break;
        case NM_RCLICK:
            oss << L"  → NM_RCLICK (右键单击)\r\n\r\n";
            break;
        case NM_RETURN:
            oss << L"  → NM_RETURN (Enter 键)\r\n\r\n";
            break;
        case NM_KILLFOCUS:
            oss << L"  → NM_KILLFOCUS (失去焦点)\r\n\r\n";
            break;
        case NM_SETFOCUS:
            oss << L"  → NM_SETFOCUS (获得焦点)\r\n\r\n";
            break;
        default:
            oss << L"  → 未知通知码\r\n\r\n";
            break;
        }
        AddLog(state, oss.str());
        return 0;
    }

    case WM_DESTROY:
        delete state;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(0, WINDOW_CLASS_NAME,
        L"WM_NOTIFY 通知机制示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 330,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
