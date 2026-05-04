/**
 * 14_listview - ListView 报表视图示例
 *
 * 演示 ListView 控件的使用：
 * - InitCommonControlsEx 初始化
 * - LVS_REPORT 报表视图模式
 * - 插入列（ListView_InsertColumn）
 * - 插入行（ListView_InsertItem / ListView_SetItemText）
 * - 选中项变化通知（LVN_ITEMCHANGED）
 *
 * 参考: tutorial/native_win32/6_ProgrammingGUI_NativeWindows_ListView.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* WINDOW_CLASS_NAME = L"ListViewClass";

#define ID_LISTVIEW     1001
#define ID_EDIT_LOG     1002
#define ID_BTN_ADD      1003
#define ID_BTN_DEL      1004

struct AppState
{
    HWND hListView;
    HWND hEditLog;
    int itemCount;
};

void AddLog(AppState* state, const std::wstring& msg)
{
    int len = GetWindowTextLength(state->hEditLog);
    SendMessage(state->hEditLog, EM_SETSEL, len, len);
    SendMessage(state->hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
}

void InsertSampleData(HWND hListView)
{
    struct ItemData
    {
        const wchar_t* name;
        const wchar_t* type;
        const wchar_t* size;
        const wchar_t* modified;
    };

    ItemData items[] = {
        { L"README.md",        L"Markdown",  L"4.2 KB",  L"2025-03-22" },
        { L"main.cpp",         L"C++ 源码",  L"12.8 KB", L"2025-04-01" },
        { L"CMakeLists.txt",   L"CMake",     L"1.1 KB",  L"2025-03-15" },
        { L"resource.h",       L"头文件",    L"0.8 KB",  L"2025-03-10" },
        { L"app.ico",          L"图标",      L"24.6 KB", L"2025-02-28" },
        { L"build.bat",        L"批处理",    L"0.3 KB",  L"2025-03-20" },
        { L"tutorial.md",      L"Markdown",  L"89.1 KB", L"2025-04-05" },
        { L"image.png",        L"图片",      L"156 KB",  L"2025-03-18" },
    };

    for (int i = 0; i < _countof(items); i++)
    {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)items[i].name;
        ListView_InsertItem(hListView, &lvi);

        ListView_SetItemText(hListView, i, 1, (LPWSTR)items[i].type);
        ListView_SetItemText(hListView, i, 2, (LPWSTR)items[i].size);
        ListView_SetItemText(hListView, i, 3, (LPWSTR)items[i].modified);
    }
}

void CreateControls(HWND hwnd, AppState* state)
{
    auto hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    // 按钮
    CreateWindowEx(0, L"BUTTON", L"添加项",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, 10, 90, 28, hwnd, (HMENU)ID_BTN_ADD, hInst, nullptr);

    CreateWindowEx(0, L"BUTTON", L"删除选中",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        115, 10, 90, 28, hwnd, (HMENU)ID_BTN_DEL, hInst, nullptr);

    // ListView 控件
    state->hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        15, 45, 450, 200, hwnd, (HMENU)ID_LISTVIEW, hInst, nullptr);

    // 设置扩展样式
    ListView_SetExtendedListViewStyle(state->hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // 插入列
    LVCOLUMN lvc = {};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 150;
    lvc.pszText = (LPWSTR)L"文件名";
    ListView_InsertColumn(state->hListView, 0, &lvc);

    lvc.cx = 80;
    lvc.pszText = (LPWSTR)L"类型";
    ListView_InsertColumn(state->hListView, 1, &lvc);

    lvc.fmt = LVCFMT_RIGHT;
    lvc.cx = 80;
    lvc.pszText = (LPWSTR)L"大小";
    ListView_InsertColumn(state->hListView, 2, &lvc);

    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 100;
    lvc.pszText = (LPWSTR)L"修改日期";
    ListView_InsertColumn(state->hListView, 3, &lvc);

    InsertSampleData(state->hListView);
    state->itemCount = 8;

    // 日志输出
    CreateWindowEx(0, L"STATIC", L"事件日志:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 255, 80, 20, hwnd, nullptr, hInst, nullptr);

    state->hEditLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        15, 275, 450, 80, hwnd, (HMENU)ID_EDIT_LOG, hInst, nullptr);

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
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        state = new AppState{ .itemCount = 8 };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        CreateControls(hwnd, state);
        AddLog(state, L"ListView 报表视图示例已启动\r\n");
        AddLog(state, L"单击选择行，观察 LVN_ITEMCHANGED 通知\r\n\r\n");
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_ADD)
        {
            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = state->itemCount;
            lvi.pszText = (LPWSTR)L"new_file.txt";
            ListView_InsertItem(state->hListView, &lvi);
            ListView_SetItemText(state->hListView, state->itemCount, 1, (LPWSTR)L"文本");
            ListView_SetItemText(state->hListView, state->itemCount, 2, (LPWSTR)L"0.1 KB");
            ListView_SetItemText(state->hListView, state->itemCount, 3, (LPWSTR)L"2025-05-04");
            state->itemCount++;
            AddLog(state, L"[添加] 新增了一项\r\n");
        }
        else if (LOWORD(wParam) == ID_BTN_DEL)
        {
            int sel = ListView_GetNextItem(state->hListView, -1, LVNI_SELECTED);
            if (sel >= 0)
            {
                wchar_t buf[64] = {};
                ListView_GetItemText(state->hListView, sel, 0, buf, 64);
                ListView_DeleteItem(state->hListView, sel);
                std::wostringstream oss;
                oss << L"[删除] 移除了: " << buf << L"\r\n";
                AddLog(state, oss.str());
            }
            else
            {
                AddLog(state, L"[删除] 未选中任何项\r\n");
            }
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        auto* nmhdr = (NMHDR*)lParam;
        if (nmhdr->idFrom == ID_LISTVIEW)
        {
            switch (nmhdr->code)
            {
            case LVN_ITEMCHANGED:
            {
                auto* pnmv = (NMLISTVIEW*)lParam;
                if (pnmv->uNewState & LVIS_SELECTED)
                {
                    wchar_t buf[64] = {};
                    ListView_GetItemText(state->hListView, pnmv->iItem, 0, buf, 64);
                    std::wostringstream oss;
                    oss << L"[LVN_ITEMCHANGED] 选中第 " << pnmv->iItem
                        << L" 行: " << buf << L"\r\n";
                    AddLog(state, oss.str());
                }
                break;
            }
            case NM_DBLCLK:
            {
                auto* pnmitem = (NMITEMACTIVATE*)lParam;
                if (pnmitem->iItem >= 0)
                {
                    wchar_t buf[64] = {};
                    ListView_GetItemText(state->hListView, pnmitem->iItem, 0, buf, 64);
                    std::wostringstream oss;
                    oss << L"[NM_DBLCLK] 双击了: " << buf << L"\r\n";
                    AddLog(state, oss.str());
                }
                break;
            }
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
        L"ListView 报表视图示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 420,
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
