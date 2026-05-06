#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#define ID_LISTVIEW 1001

HWND g_hListView = NULL;

void AddDroppedFiles(HWND hListView, HDROP hDrop)
{
    UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    for (UINT i = 0; i < fileCount; i++)
    {
        UINT len = DragQueryFile(hDrop, i, NULL, 0);
        wchar_t* path = new wchar_t[len + 1];
        DragQueryFile(hDrop, i, path, len + 1);

        // 提取文件名
        const wchar_t* fileName = wcsrchr(path, L'\\');
        fileName = fileName ? fileName + 1 : path;

        // 提取扩展名
        const wchar_t* ext = wcsrchr(path, L'.');
        ext = ext ? ext + 1 : L"";

        // 添加到 ListView
        int idx = ListView_GetItemCount(hListView);

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = (LPWSTR)fileName;
        ListView_InsertItem(hListView, &lvi);

        ListView_SetItemText(hListView, idx, 1, (LPWSTR)ext);
        ListView_SetItemText(hListView, idx, 2, path);

        delete[] path;
    }

    DragFinish(hDrop);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        // 创建 ListView
        g_hListView = CreateWindowEx(0, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_BORDER,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_LISTVIEW, hInst, NULL);

        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // 添加列
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

        wchar_t colName[] = L"文件名";
        lvc.cx = 150; lvc.pszText = colName; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(g_hListView, 0, &lvc);

        wchar_t colType[] = L"类型";
        lvc.cx = 80; lvc.pszText = colType;
        ListView_InsertColumn(g_hListView, 1, &lvc);

        wchar_t colPath[] = L"完整路径";
        lvc.cx = 350; lvc.pszText = colPath;
        ListView_InsertColumn(g_hListView, 2, &lvc);

        // 注册窗口接受文件拖放
        DragAcceptFiles(hwnd, TRUE);

        return 0;
    }

    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        MoveWindow(g_hListView, 0, 0, rc.right, rc.bottom, TRUE);
        return 0;
    }

    case WM_DROPFILES:
        AddDroppedFiles(g_hListView, (HDROP)wParam);
        return 0;

    case WM_DESTROY:
        DragAcceptFiles(hwnd, FALSE);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FileDropDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES,  // 也可以在窗口类中设置
        L"FileDropDemo", L"文件拖放示例 - 将文件拖入此窗口",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 400,
        NULL, NULL, hInstance, NULL
    );

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
