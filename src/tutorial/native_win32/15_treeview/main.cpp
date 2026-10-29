/**
 * 15_treeview - TreeView 树形控件示例
 *
 * 演示 TreeView 控件的使用：
 * - TreeView_InsertItem 插入节点
 * - TVINSERTSTRUCT 和 TVITEM 结构体
 * - TVN_SELCHANGED 选中项变化通知
 * - 多层级树形结构
 * - 节点展开/折叠
 *
 * 参考: tutorial/native_win32/7_ProgrammingGUI_NativeWindows_TreeView.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* WINDOW_CLASS_NAME = L"TreeViewClass";

#define ID_TREEVIEW     1001
#define ID_EDIT_LOG     1002
#define ID_BTN_ADD      1003
#define ID_BTN_EXPAND   1004

struct AppState
{
    HWND hTreeView;
    HWND hEditLog;
    HTREEITEM hSelectedParent;
};

void AddLog(AppState* state, const std::wstring& msg)
{
    int len = GetWindowTextLength(state->hEditLog);
    SendMessage(state->hEditLog, EM_SETSEL, len, len);
    SendMessage(state->hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
}

HTREEITEM InsertTreeItem(HWND hTV, const wchar_t* text, HTREEITEM hParent, HTREEITEM hAfter)
{
    TVINSERTSTRUCT tvis = {};
    tvis.hParent = hParent;
    tvis.hInsertAfter = hAfter;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = (LPWSTR)text;
    return TreeView_InsertItem(hTV, &tvis);
}

void BuildTree(HWND hTreeView)
{
    // 根节点: 项目
    HTREEITEM hRoot = InsertTreeItem(hTreeView, L"anatomy_gui", TVI_ROOT, TVI_LAST);

    // 二级: 目录
    HTREEITEM hSrc = InsertTreeItem(hTreeView, L"src/", hRoot, TVI_LAST);
    HTREEITEM hTutorial = InsertTreeItem(hTreeView, L"tutorial/", hRoot, TVI_LAST);
    HTREEITEM hTodo = InsertTreeItem(hTreeView, L"todo/", hRoot, TVI_LAST);

    // 三级: src 子目录
    InsertTreeItem(hTreeView, L"CMakeLists.txt", hSrc, TVI_LAST);
    HTREEITEM hNativeWin32 = InsertTreeItem(hTreeView, L"native_win32/", hSrc, TVI_LAST);
    HTREEITEM hGraphics = InsertTreeItem(hTreeView, L"graphics/", hSrc, TVI_LAST);

    // 四级: native_win32 示例
    InsertTreeItem(hTreeView, L"01_hello_world/", hNativeWin32, TVI_LAST);
    InsertTreeItem(hTreeView, L"02_register_window_class/", hNativeWin32, TVI_LAST);
    InsertTreeItem(hTreeView, L"...", hNativeWin32, TVI_LAST);

    // 四级: graphics 系列
    InsertTreeItem(hTreeView, L"gdi/", hGraphics, TVI_LAST);
    InsertTreeItem(hTreeView, L"d3d11/", hGraphics, TVI_LAST);

    // 三级: tutorial 文件
    InsertTreeItem(hTreeView, L"native_win32/", hTutorial, TVI_LAST);
    InsertTreeItem(hTreeView, L"index.md", hTutorial, TVI_LAST);
    InsertTreeItem(hTreeView, L"tags.md", hTutorial, TVI_LAST);

    // 三级: todo 文件
    InsertTreeItem(hTreeView, L"00_overview.md", hTodo, TVI_LAST);
    InsertTreeItem(hTreeView, L"01_quick_wins.md", hTodo, TVI_LAST);
    InsertTreeItem(hTreeView, L"...", hTodo, TVI_LAST);

    // 展开根节点
    TreeView_Expand(hTreeView, hRoot, TVE_EXPAND);
    TreeView_Expand(hTreeView, hSrc, TVE_EXPAND);
}

void CreateControls(HWND hwnd, AppState* state)
{
    auto hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    // 按钮
    CreateWindowEx(0, L"BUTTON", L"添加子节点",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, 10, 100, 28, hwnd, (HMENU)ID_BTN_ADD, hInst, nullptr);
    CreateWindowEx(0, L"BUTTON", L"全部展开",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        125, 10, 100, 28, hwnd, (HMENU)ID_BTN_EXPAND, hInst, nullptr);

    // TreeView
    state->hTreeView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
        TVS_SHOWSELALWAYS | TVS_TRACKSELECT,
        15, 45, 280, 310, hwnd, (HMENU)ID_TREEVIEW, hInst, nullptr);

    BuildTree(state->hTreeView);

    // 日志
    CreateWindowEx(0, L"STATIC", L"选中节点信息:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        310, 45, 200, 20, hwnd, nullptr, hInst, nullptr);

    state->hEditLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        310, 65, 240, 290, hwnd, (HMENU)ID_EDIT_LOG, hInst, nullptr);

    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL
    {
        SendMessage(child, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

int GetItemDepth(HWND hTreeView, HTREEITEM hItem)
{
    int depth = 0;
    HTREEITEM hParent = TreeView_GetParent(hTreeView, hItem);
    while (hParent)
    {
        depth++;
        hParent = TreeView_GetParent(hTreeView, hParent);
    }
    return depth;
}

void ExpandAll(HWND hTreeView, HTREEITEM hItem, BOOL expand)
{
    if (!hItem) return;
    TreeView_Expand(hTreeView, hItem, expand ? TVE_EXPAND : TVE_COLLAPSE);
    HTREEITEM hChild = TreeView_GetChild(hTreeView, hItem);
    while (hChild)
    {
        ExpandAll(hTreeView, hChild, expand);
        hChild = TreeView_GetNextSibling(hTreeView, hChild);
    }
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
        icc.dwICC = ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        state = new AppState{};
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        CreateControls(hwnd, state);
        AddLog(state, L"TreeView 示例已启动\r\n");
        AddLog(state, L"单击节点查看信息\r\n");
        AddLog(state, L"双击节点展开/折叠\r\n\r\n");
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_ADD)
        {
            HTREEITEM hSel = TreeView_GetSelection(state->hTreeView);
            if (hSel)
            {
                InsertTreeItem(state->hTreeView, L"新节点", hSel, TVI_LAST);
                TreeView_Expand(state->hTreeView, hSel, TVE_EXPAND);
                AddLog(state, L"[添加] 在选中节点下添加了子节点\r\n");
            }
            else
            {
                AddLog(state, L"[添加] 请先选中一个节点\r\n");
            }
        }
        else if (LOWORD(wParam) == ID_BTN_EXPAND)
        {
            HTREEITEM hRoot = TreeView_GetRoot(state->hTreeView);
            ExpandAll(state->hTreeView, hRoot, TRUE);
            AddLog(state, L"[展开] 已展开所有节点\r\n");
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        auto* nmhdr = (NMHDR*)lParam;
        if (nmhdr->idFrom == ID_TREEVIEW && nmhdr->code == TVN_SELCHANGED)
        {
            auto* pnmtv = (NMTREEVIEW*)lParam;
            HTREEITEM hItem = pnmtv->itemNew.hItem;
            if (hItem)
            {
                wchar_t buf[128] = {};
                TVITEM tvi = {};
                tvi.mask = TVIF_TEXT | TVIF_CHILDREN;
                tvi.hItem = hItem;
                tvi.pszText = buf;
                tvi.cchTextMax = 128;
                TreeView_GetItem(state->hTreeView, &tvi);

                int depth = GetItemDepth(state->hTreeView, hItem);
                bool hasChildren = tvi.cChildren > 0;

                std::wostringstream oss;
                oss << L"[TVN_SELCHANGED]\r\n"
                    << L"  文本: " << buf << L"\r\n"
                    << L"  深度: " << depth << L"\r\n"
                    << L"  子节点: " << (hasChildren ? L"是" : L"否") << L"\r\n\r\n";
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
        L"TreeView 树形控件示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 410,
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
