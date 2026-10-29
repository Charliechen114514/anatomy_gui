/**
 * 12_basic_controls - 基础控件示例
 *
 * 演示 Win32 标准控件的使用：
 * - Button（按钮、复选框、单选按钮、分组框）
 * - Edit（单行、多行、密码模式）
 * - ListBox（列表框）
 * - ComboBox（组合框）
 * - WM_COMMAND 消息处理
 *
 * 参考: tutorial/native_win32/4_ProgrammingGUI_NativeWindows_Controls.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <sstream>

static const wchar_t* WINDOW_CLASS_NAME = L"BasicControlsClass";

// 控件 ID
#define ID_BTN_CLICK        1001
#define ID_BTN_CHECK        1002
#define ID_RADIO_1          1003
#define ID_RADIO_2          1004
#define ID_RADIO_3          1005
#define ID_EDIT_INPUT       1006
#define ID_EDIT_OUTPUT      1007
#define ID_LISTBOX          1008
#define ID_COMBOBOX         1009

struct AppState
{
    HWND hBtnClick;
    HWND hBtnCheck;
    HWND hRadio1, hRadio2, hRadio3;
    HWND hEditInput;
    HWND hEditOutput;
    HWND hListBox;
    HWND hComboBox;
    HWND hGroupBoxCtrl;
    HWND hGroupBoxEdit;
    int clickCount;
};

void AddLog(AppState* state, const std::wstring& msg)
{
    int len = GetWindowTextLength(state->hEditOutput);
    SendMessage(state->hEditOutput, EM_SETSEL, len, len);
    SendMessage(state->hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
}

void CreateControls(HWND hwnd, AppState* state)
{
    auto hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    HFONT hFont = CreateFont(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    int margin = 15;
    int y = margin;

    // === 左侧：按钮区 ===
    state->hGroupBoxCtrl = CreateWindowEx(0, L"BUTTON", L"控件区",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        margin, y, 280, 260, hwnd, nullptr, hInst, nullptr);

    // 按钮
    state->hBtnClick = CreateWindowEx(0, L"BUTTON", L"点我计数",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin + 15, y + 30, 120, 32, hwnd, (HMENU)ID_BTN_CLICK, hInst, nullptr);

    // 复选框
    state->hBtnCheck = CreateWindowEx(0, L"BUTTON", L"启用功能",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin + 15, y + 75, 120, 24, hwnd, (HMENU)ID_BTN_CHECK, hInst, nullptr);

    // 单选按钮组
    CreateWindowEx(0, L"BUTTON", L"选项 A", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        margin + 15, y + 110, 80, 24, hwnd, (HMENU)ID_RADIO_1, hInst, nullptr);
    CreateWindowEx(0, L"BUTTON", L"选项 B", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        margin + 100, y + 110, 80, 24, hwnd, (HMENU)ID_RADIO_2, hInst, nullptr);
    CreateWindowEx(0, L"BUTTON", L"选项 C", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        margin + 185, y + 110, 80, 24, hwnd, (HMENU)ID_RADIO_3, hInst, nullptr);
    SendMessage((HWND)ID_RADIO_1, BM_SETCHECK, BST_CHECKED, 0);

    // 列表框
    CreateWindowEx(0, L"STATIC", L"列表框:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin + 15, y + 145, 60, 20, hwnd, nullptr, hInst, nullptr);
    state->hListBox = CreateWindowEx(0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOTIFY,
        margin + 15, y + 165, 120, 80, hwnd, (HMENU)ID_LISTBOX, hInst, nullptr);

    // 组合框
    CreateWindowEx(0, L"STATIC", L"组合框:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin + 150, y + 145, 60, 20, hwnd, nullptr, hInst, nullptr);
    state->hComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
        margin + 150, y + 165, 110, 200, hwnd, (HMENU)ID_COMBOBOX, hInst, nullptr);

    // 填充列表框和组合框数据
    const wchar_t* items[] = { L"苹果", L"香蕉", L"橘子", L"葡萄", L"西瓜" };
    for (auto item : items)
    {
        SendMessage(state->hListBox, LB_ADDSTRING, 0, (LPARAM)item);
        SendMessage(state->hComboBox, CB_ADDSTRING, 0, (LPARAM)item);
    }

    // === 右侧：编辑区 ===
    int xRight = margin + 300;

    state->hGroupBoxEdit = CreateWindowEx(0, L"BUTTON", L"编辑区",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        xRight, y, 320, 260, hwnd, nullptr, hInst, nullptr);

    // 输入框
    CreateWindowEx(0, L"STATIC", L"输入:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        xRight + 15, y + 25, 40, 20, hwnd, nullptr, hInst, nullptr);
    state->hEditInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        xRight + 55, y + 22, 230, 24, hwnd, (HMENU)ID_EDIT_INPUT, hInst, nullptr);

    // 多行输出
    CreateWindowEx(0, L"STATIC", L"事件日志:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        xRight + 15, y + 55, 80, 20, hwnd, nullptr, hInst, nullptr);
    state->hEditOutput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        xRight + 15, y + 75, 280, 170, hwnd, (HMENU)ID_EDIT_OUTPUT, hInst, nullptr);

    // 设置字体到所有子控件
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
        auto* cs = (CREATESTRUCT*)lParam;
        state = new AppState{ .clickCount = 0 };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        CreateControls(hwnd, state);
        AddLog(state, L"--- 基础控件示例已启动 ---\r\n");
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_CLICK)
        {
            state->clickCount++;
            std::wostringstream oss;
            oss << L"[按钮] 点击次数: " << state->clickCount << L"\r\n";
            AddLog(state, oss.str());
        }
        else if (id == ID_BTN_CHECK)
        {
            BOOL checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
            AddLog(state, checked ? L"[复选框] 已勾选\r\n" : L"[复选框] 已取消\r\n");
        }
        else if (id >= ID_RADIO_1 && id <= ID_RADIO_3)
        {
            const wchar_t* names[] = { L"A", L"B", L"C" };
            std::wostringstream oss;
            oss << L"[单选] 选择了选项 " << names[id - ID_RADIO_1] << L"\r\n";
            AddLog(state, oss.str());
        }
        else if (id == ID_EDIT_INPUT && code == EN_CHANGE)
        {
            wchar_t buf[256] = {};
            GetWindowText(state->hEditInput, buf, 256);
            std::wostringstream oss;
            oss << L"[输入] 内容变更: \"" << buf << L"\"\r\n";
            AddLog(state, oss.str());
        }
        else if (id == ID_LISTBOX && code == LBN_SELCHANGE)
        {
            int sel = (int)SendMessage(state->hListBox, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR)
            {
                wchar_t buf[64] = {};
                SendMessage(state->hListBox, LB_GETTEXT, sel, (LPARAM)buf);
                std::wostringstream oss;
                oss << L"[列表] 选择: " << buf << L"\r\n";
                AddLog(state, oss.str());
            }
        }
        else if (id == ID_COMBOBOX && code == CBN_SELCHANGE)
        {
            int sel = (int)SendMessage(state->hComboBox, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR)
            {
                wchar_t buf[64] = {};
                SendMessage(state->hComboBox, CB_GETLBTEXT, sel, (LPARAM)buf);
                std::wostringstream oss;
                oss << L"[组合框] 选择: " << buf << L"\r\n";
                AddLog(state, oss.str());
            }
        }
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
        L"基础控件示例 — Button/Edit/ListBox/ComboBox",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 660, 320,
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
