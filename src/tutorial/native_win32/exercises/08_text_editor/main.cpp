/**
 * 练习8: 全功能文本编辑器
 *
 * 综合 Win32 教程前 7 章知识，实现一个带 RichEdit 控件的文本编辑器。
 *
 * 功能:
 *   - 菜单栏: 文件(新建/打开/保存/另存为/退出)、编辑(撤销/剪切/复制/粘贴/全选)、帮助(关于)
 *   - 工具栏: 新建/打开/保存/剪切/复制/粘贴 (使用标准位图)
 *   - 状态栏: 行号/列号/文件名/修改状态
 *   - RichEdit 2.0 编辑区 (加载 riched20.dll)
 *   - 加速键表: Ctrl+N/O/S/Z/X/C/V/A
 *   - 文件操作: GetOpenFileName/GetSaveFileName, 支持 UTF-8/UTF-16
 *   - 未保存确认 + 标题栏修改标记
 *
 * 涉及知识点:
 *   - RichEdit 控件 (LoadLibrary riched20.dll, RICHEDIT_CLASS)
 *   - 工具栏与状态栏 (TOOLBARCLASSNAME, STATUSCLASSNAME)
 *   - 菜单栏 (CreateMenu/AppendMenu, 无 .rc 资源文件)
 *   - 运行时加速键表 (CreateAcceleratorTable)
 *   - 通用文件对话框 (GetOpenFileName/GetSaveFileName)
 *   - WM_NOTIFY 通知 (EN_SELCHANGE, TTN_GETDISPINFO)
 *   - UTF-8/UTF-16 文件编解码 (BOM 检测, MultiByteToWideChar)
 */

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>

// ============================================================================
// 命令 ID 定义
// ============================================================================

// 文件菜单
#define IDM_NEW         4001
#define IDM_OPEN        4002
#define IDM_SAVE        4003
#define IDM_SAVEAS      4004
#define IDM_EXIT        4005

// 编辑菜单
#define IDM_UNDO        4010
#define IDM_CUT         4011
#define IDM_COPY        4012
#define IDM_PASTE       4013
#define IDM_SELECTALL   4014

// 帮助菜单
#define IDM_ABOUT       4020

// 控件 ID
#define IDC_RICHEDIT    5001
#define IDC_TOOLBAR     5002
#define IDC_STATUSBAR   5003

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    HWND      hRichEdit   = nullptr;
    HWND      hToolBar    = nullptr;
    HWND      hStatusBar  = nullptr;
    HACCEL    hAccelTable = nullptr;
    HMODULE   hRichEditDll = nullptr;
    HFONT     hFont       = nullptr;
    wchar_t   filePath[MAX_PATH] = {};
    wchar_t   fileName[256]      = {};
    bool      modified    = false;
};

// ============================================================================
// 辅助函数声明
// ============================================================================

AppState* GetState(HWND hWnd);
void      CreateMenuBar(HWND hWnd);
void      CreateToolBar(HWND hWnd, HINSTANCE hInst);
void      CreateStatusBar(HWND hWnd, HINSTANCE hInst);
void      CreateRichEdit(HWND hWnd, HINSTANCE hInst, AppState* state);
HACCEL    CreateAccelerators();
void      ResizeControls(HWND hWnd, AppState* state);
void      UpdateTitleBar(HWND hWnd, AppState* state);
void      UpdateStatusBar(AppState* state);

bool      CheckUnsaved(HWND hWnd, AppState* state);
void      DoFileNew(HWND hWnd, AppState* state);
void      DoFileOpen(HWND hWnd, AppState* state);
void      DoFileSave(HWND hWnd, AppState* state);
void      DoFileSaveAs(HWND hWnd, AppState* state);

bool      ReadFileContent(const wchar_t* path, std::wstring& content);
bool      WriteFileContent(const wchar_t* path, const std::wstring& content);

// ============================================================================
// 获取 AppState 指针
// ============================================================================

AppState* GetState(HWND hWnd)
{
    return reinterpret_cast<AppState*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
}

// ============================================================================
// 创建菜单栏
// ============================================================================

void CreateMenuBar(HWND hWnd)
{
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    HMENU hEditMenu = CreateMenu();
    HMENU hHelpMenu = CreateMenu();

    // 文件菜单
    AppendMenu(hFileMenu, MF_STRING, IDM_NEW,       L"新建\tCtrl+N");
    AppendMenu(hFileMenu, MF_STRING, IDM_OPEN,      L"打开...\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, IDM_SAVE,      L"保存\tCtrl+S");
    AppendMenu(hFileMenu, MF_STRING, IDM_SAVEAS,    L"另存为...");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFileMenu, MF_STRING, IDM_EXIT,      L"退出");

    // 编辑菜单
    AppendMenu(hEditMenu, MF_STRING, IDM_UNDO,      L"撤销\tCtrl+Z");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hEditMenu, MF_STRING, IDM_CUT,       L"剪切\tCtrl+X");
    AppendMenu(hEditMenu, MF_STRING, IDM_COPY,      L"复制\tCtrl+C");
    AppendMenu(hEditMenu, MF_STRING, IDM_PASTE,     L"粘贴\tCtrl+V");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hEditMenu, MF_STRING, IDM_SELECTALL, L"全选\tCtrl+A");

    // 帮助菜单
    AppendMenu(hHelpMenu, MF_STRING, IDM_ABOUT,     L"关于...");

    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"文件");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, L"编辑");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"帮助");

    SetMenu(hWnd, hMenuBar);
}

// ============================================================================
// 创建工具栏
// ============================================================================

void CreateToolBar(HWND hWnd, HINSTANCE hInst)
{
    HWND hTB = CreateWindowEx(
        0, TOOLBARCLASSNAME, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CCS_ADJUSTABLE,
        0, 0, 0, 0,
        hWnd, (HMENU)IDC_TOOLBAR, hInst, nullptr
    );

    SendMessage(hTB, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    // 加载标准系统位图
    TBADDBITMAP tbab;
    tbab.hInst = HINST_COMMCTRL;
    tbab.nID   = IDB_STD_SMALL_COLOR;
    int nBmpOffset = (int)SendMessage(hTB, TB_ADDBITMAP, 0, (LPARAM)&tbab);

    TBBUTTON tbb[] = {
        { STD_FILENEW  + nBmpOffset, IDM_NEW,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        { STD_FILEOPEN + nBmpOffset, IDM_OPEN,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        { STD_FILESAVE + nBmpOffset, IDM_SAVE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0 },
        { STD_CUT      + nBmpOffset, IDM_CUT,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        { STD_COPY     + nBmpOffset, IDM_COPY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        { STD_PASTE    + nBmpOffset, IDM_PASTE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
    };

    SendMessage(hTB, TB_ADDBUTTONS, _countof(tbb), (LPARAM)tbb);
    SendMessage(hTB, TB_AUTOSIZE, 0, 0);

    GetState(hWnd)->hToolBar = hTB;
}

// ============================================================================
// 创建状态栏
// ============================================================================

void CreateStatusBar(HWND hWnd, HINSTANCE hInst)
{
    HWND hSB = CreateWindowEx(
        0, STATUSCLASSNAME, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hWnd, (HMENU)IDC_STATUSBAR, hInst, nullptr
    );

    // 4 个分区: 行号 | 列号 | 文件名 | 修改状态
    int parts[] = { 100, 200, 500, -1 };
    SendMessage(hSB, SB_SETPARTS, 4, (LPARAM)parts);
    SendMessage(hSB, SB_SETTEXT, 0, (LPARAM)L"行: 1");
    SendMessage(hSB, SB_SETTEXT, 1, (LPARAM)L"列: 1");
    SendMessage(hSB, SB_SETTEXT, 3, (LPARAM)L"未修改");

    GetState(hWnd)->hStatusBar = hSB;
}

// ============================================================================
// 创建 RichEdit 控件
// ============================================================================

void CreateRichEdit(HWND hWnd, HINSTANCE hInst, AppState* state)
{
    // RichEdit 2.0: 加载 DLL 注册窗口类
    state->hRichEditDll = LoadLibrary(L"riched20.dll");

    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        RICHEDIT_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        0, 0, 0, 0,
        hWnd, (HMENU)IDC_RICHEDIT, hInst, nullptr
    );

    // 设置字体
    state->hFont = CreateFont(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE,
        L"Consolas"
    );
    SendMessage(hEdit, WM_SETFONT, (WPARAM)state->hFont, TRUE);

    // 注册事件通知: 内容变化 + 选择变化
    SendMessage(hEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);

    state->hRichEdit = hEdit;
}

// ============================================================================
// 创建加速键表 (运行时, 无需 .rc 文件)
// ============================================================================

HACCEL CreateAccelerators()
{
    ACCEL accel[] = {
        { FVIRTKEY | FCONTROL, 'N', IDM_NEW },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN },
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
        { FVIRTKEY | FCONTROL, 'Z', IDM_UNDO },
        { FVIRTKEY | FCONTROL, 'X', IDM_CUT },
        { FVIRTKEY | FCONTROL, 'C', IDM_COPY },
        { FVIRTKEY | FCONTROL, 'V', IDM_PASTE },
        { FVIRTKEY | FCONTROL, 'A', IDM_SELECTALL },
    };
    return CreateAcceleratorTable(accel, _countof(accel));
}

// ============================================================================
// 调整控件布局
// ============================================================================

void ResizeControls(HWND hWnd, AppState* state)
{
    RECT rc;
    GetClientRect(hWnd, &rc);

    SendMessage(state->hToolBar, TB_AUTOSIZE, 0, 0);

    RECT rcTB, rcSB;
    GetWindowRect(state->hToolBar, &rcTB);
    GetWindowRect(state->hStatusBar, &rcSB);
    int nToolBarH   = rcTB.bottom - rcTB.top;
    int nStatusBarH = rcSB.bottom - rcSB.top;

    MoveWindow(
        state->hRichEdit,
        0, nToolBarH,
        rc.right, rc.bottom - nToolBarH - nStatusBarH,
        TRUE
    );
}

// ============================================================================
// 更新标题栏
// ============================================================================

void UpdateTitleBar(HWND hWnd, AppState* state)
{
    wchar_t title[512];
    const wchar_t* name = state->fileName[0] ? state->fileName : L"未命名";
    if (state->modified)
        wsprintf(title, L"%s * - 文本编辑器", name);
    else
        wsprintf(title, L"%s - 文本编辑器", name);
    SetWindowText(hWnd, title);
}

// ============================================================================
// 更新状态栏 (行号/列号/文件名/修改状态)
// ============================================================================

void UpdateStatusBar(AppState* state)
{
    // 获取当前光标位置
    DWORD dwStart = 0, dwEnd = 0;
    SendMessage(state->hRichEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    // 行号 (0-based → 显示为 1-based)
    int line = (int)SendMessage(state->hRichEdit, EM_LINEFROMCHAR, (WPARAM)dwStart, 0);
    // 列号: 当前位置 - 当前行起始位置
    int lineStart = (int)SendMessage(state->hRichEdit, EM_LINEINDEX, (WPARAM)line, 0);
    int col = (int)(dwStart - lineStart);

    wchar_t buf[64];
    wsprintf(buf, L"行: %d", line + 1);
    SendMessage(state->hStatusBar, SB_SETTEXT, 0, (LPARAM)buf);

    wsprintf(buf, L"列: %d", col + 1);
    SendMessage(state->hStatusBar, SB_SETTEXT, 1, (LPARAM)buf);

    // 文件名
    const wchar_t* name = state->fileName[0] ? state->fileName : L"未命名";
    SendMessage(state->hStatusBar, SB_SETTEXT, 2, (LPARAM)name);

    // 修改状态
    SendMessage(state->hStatusBar, SB_SETTEXT, 3,
        (LPARAM)(state->modified ? L"已修改" : L"未修改"));
}

// ============================================================================
// 未保存确认对话框
// 返回 true 表示可以继续, false 表示用户取消
// ============================================================================

bool CheckUnsaved(HWND hWnd, AppState* state)
{
    if (!state->modified)
        return true;

    const wchar_t* name = state->fileName[0] ? state->fileName : L"未命名";
    wchar_t msg[512];
    wsprintf(msg, L"是否将更改保存到 %s？", name);

    int result = MessageBox(hWnd, msg, L"文本编辑器",
        MB_YESNOCANCEL | MB_ICONQUESTION);

    if (result == IDYES)
    {
        DoFileSave(hWnd, state);
        // 保存可能被取消 (另存为对话框点了取消)
        return !state->modified;
    }
    if (result == IDNO)
        return true;

    return false; // IDCANCEL
}

// ============================================================================
// 文件操作: 新建
// ============================================================================

void DoFileNew(HWND hWnd, AppState* state)
{
    if (!CheckUnsaved(hWnd, state))
        return;

    SetWindowText(state->hRichEdit, L"");
    state->filePath[0] = L'\0';
    lstrcpy(state->fileName, L"");
    state->modified = false;

    UpdateTitleBar(hWnd, state);
    UpdateStatusBar(state);
}

// ============================================================================
// 文件操作: 打开
// ============================================================================

void DoFileOpen(HWND hWnd, AppState* state)
{
    if (!CheckUnsaved(hWnd, state))
        return;

    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner   = hWnd;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0"
                      L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn))
        return;

    std::wstring content;
    if (!ReadFileContent(szFile, content))
    {
        MessageBox(hWnd, L"无法打开文件。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 设置文本到 RichEdit
    SendMessage(state->hRichEdit, WM_SETTEXT, 0, (LPARAM)content.c_str());

    // 更新状态
    lstrcpy(state->filePath, szFile);
    // 提取文件名
    const wchar_t* pName = szFile + lstrlen(szFile);
    while (pName > szFile && *(pName - 1) != L'\\' && *(pName - 1) != L'/')
        pName--;
    lstrcpy(state->fileName, pName);

    state->modified = false;
    UpdateTitleBar(hWnd, state);
    UpdateStatusBar(state);
}

// ============================================================================
// 文件操作: 保存
// ============================================================================

void DoFileSave(HWND hWnd, AppState* state)
{
    if (state->filePath[0] == L'\0')
    {
        DoFileSaveAs(hWnd, state);
        return;
    }

    // 获取文本
    int len = GetWindowTextLength(state->hRichEdit);
    std::wstring text(len + 1, L'\0');
    GetWindowText(state->hRichEdit, &text[0], len + 1);
    text.resize(len);

    if (!WriteFileContent(state->filePath, text))
    {
        MessageBox(hWnd, L"无法保存文件。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    state->modified = false;
    UpdateTitleBar(hWnd, state);
    UpdateStatusBar(state);
}

// ============================================================================
// 文件操作: 另存为
// ============================================================================

void DoFileSaveAs(HWND hWnd, AppState* state)
{
    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner   = hWnd;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0"
                      L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt  = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return;

    // 获取文本
    int len = GetWindowTextLength(state->hRichEdit);
    std::wstring text(len + 1, L'\0');
    GetWindowText(state->hRichEdit, &text[0], len + 1);
    text.resize(len);

    if (!WriteFileContent(szFile, text))
    {
        MessageBox(hWnd, L"无法保存文件。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 更新文件路径和名称
    lstrcpy(state->filePath, szFile);
    const wchar_t* pName = szFile + lstrlen(szFile);
    while (pName > szFile && *(pName - 1) != L'\\' && *(pName - 1) != L'/')
        pName--;
    lstrcpy(state->fileName, pName);

    state->modified = false;
    UpdateTitleBar(hWnd, state);
    UpdateStatusBar(state);
}

// ============================================================================
// 文件读取: 支持 UTF-8 / UTF-16 BOM 检测
// ============================================================================

bool ReadFileContent(const wchar_t* path, std::wstring& content)
{
    // 二进制模式打开
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> raw(static_cast<size_t>(size));
    if (!file.read(raw.data(), size))
        return false;
    file.close();

    if (raw.size() < 2)
    {
        // 空文件或单字节文件，视为 UTF-8
        if (raw.empty())
        {
            content.clear();
            return true;
        }
        int wlen = MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), nullptr, 0);
        if (wlen <= 0) { content.clear(); return true; }
        content.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), &content[0], wlen);
        return true;
    }

    // 检测 BOM
    unsigned char* data = reinterpret_cast<unsigned char*>(raw.data());
    size_t dataLen = raw.size();

    // UTF-16 LE: FF FE
    if (dataLen >= 2 && data[0] == 0xFF && data[1] == 0xFE)
    {
        size_t wcharCount = (dataLen - 2) / sizeof(wchar_t);
        content.assign(reinterpret_cast<const wchar_t*>(data + 2), wcharCount);
        return true;
    }

    // UTF-16 BE: FE FF
    if (dataLen >= 2 && data[0] == 0xFE && data[1] == 0xFF)
    {
        size_t wcharCount = (dataLen - 2) / sizeof(wchar_t);
        content.resize(wcharCount);
        const wchar_t* src = reinterpret_cast<const wchar_t*>(data + 2);
        for (size_t i = 0; i < wcharCount; i++)
        {
            // 字节交换
            unsigned char b1 = reinterpret_cast<const unsigned char*>(&src[i])[0];
            unsigned char b2 = reinterpret_cast<const unsigned char*>(&src[i])[1];
            wchar_t ch = static_cast<wchar_t>((b1 << 8) | b2);
            content[i] = ch;
        }
        return true;
    }

    // UTF-8 BOM: EF BB BF
    const char* utf8Data = raw.data();
    int utf8Len = (int)dataLen;
    if (dataLen >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
    {
        utf8Data += 3;
        utf8Len  -= 3;
    }

    // 尝试作为 UTF-8 解码
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Data, utf8Len, nullptr, 0);
    if (wlen > 0)
    {
        content.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, utf8Data, utf8Len, &content[0], wlen);
        return true;
    }

    // 回退到系统默认编码 (ANSI)
    wlen = MultiByteToWideChar(CP_ACP, 0, utf8Data, utf8Len, nullptr, 0);
    if (wlen > 0)
    {
        content.resize(wlen);
        MultiByteToWideChar(CP_ACP, 0, utf8Data, utf8Len, &content[0], wlen);
        return true;
    }

    content.clear();
    return true;
}

// ============================================================================
// 文件写入: 始终写 UTF-8 + BOM
// ============================================================================

bool WriteFileContent(const wchar_t* path, const std::wstring& content)
{
    // 转换为 UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8Len < 0)
        utf8Len = 0;

    std::vector<char> utf8Buf(utf8Len);
    if (utf8Len > 0)
    {
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(),
                            utf8Buf.data(), utf8Len, nullptr, nullptr);
    }

    // 写入 UTF-8 BOM + 内容
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    // UTF-8 BOM
    const char bom[] = { static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF) };
    file.write(bom, 3);

    if (utf8Len > 0)
        file.write(utf8Buf.data(), utf8Len);

    return file.good();
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppState* state = GetState(hWnd);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 分配状态并绑定到窗口
        auto* s = new AppState();
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)s);
        state = s;

        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);

        CreateMenuBar(hWnd);
        CreateToolBar(hWnd, cs->hInstance);
        CreateStatusBar(hWnd, cs->hInstance);
        CreateRichEdit(hWnd, cs->hInstance, state);
        state->hAccelTable = CreateAccelerators();

        UpdateTitleBar(hWnd, state);
        ResizeControls(hWnd, state);

        // 初始焦点到编辑区
        SetFocus(state->hRichEdit);
        return 0;
    }

    case WM_SIZE:
    {
        if (!state) break;
        SendMessage(state->hToolBar, WM_SIZE, wParam, lParam);
        SendMessage(state->hStatusBar, WM_SIZE, wParam, lParam);
        ResizeControls(hWnd, state);
        return 0;
    }

    case WM_COMMAND:
    {
        if (!state) break;

        // RichEdit 控件通知
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_RICHEDIT)
        {
            if (!state->modified)
            {
                state->modified = true;
                UpdateTitleBar(hWnd, state);
                UpdateStatusBar(state);
            }
            return 0;
        }

        // 菜单/工具栏/加速键命令
        switch (LOWORD(wParam))
        {
        case IDM_NEW:       DoFileNew(hWnd, state);              return 0;
        case IDM_OPEN:      DoFileOpen(hWnd, state);             return 0;
        case IDM_SAVE:      DoFileSave(hWnd, state);             return 0;
        case IDM_SAVEAS:    DoFileSaveAs(hWnd, state);           return 0;
        case IDM_EXIT:      SendMessage(hWnd, WM_CLOSE, 0, 0);  return 0;
        case IDM_UNDO:      SendMessage(state->hRichEdit, EM_UNDO, 0, 0);        return 0;
        case IDM_CUT:       SendMessage(state->hRichEdit, WM_CUT, 0, 0);         return 0;
        case IDM_COPY:      SendMessage(state->hRichEdit, WM_COPY, 0, 0);        return 0;
        case IDM_PASTE:     SendMessage(state->hRichEdit, WM_PASTE, 0, 0);       return 0;
        case IDM_SELECTALL: SendMessage(state->hRichEdit, EM_SETSEL, 0, -1);     return 0;
        case IDM_ABOUT:
            MessageBox(hWnd,
                L"练习8: 全功能文本编辑器\n\n"
                L"使用 RichEdit 2.0 控件实现\n"
                L"支持菜单栏、工具栏、状态栏、加速键、文件操作",
                L"关于", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (!state) break;

        NMHDR* pnmh = reinterpret_cast<NMHDR*>(lParam);

        // RichEdit 选择变化通知 → 更新状态栏行/列
        if (pnmh->idFrom == IDC_RICHEDIT && pnmh->code == EN_SELCHANGE)
        {
            UpdateStatusBar(state);
            return 0;
        }

        // 工具栏提示
        if (pnmh->idFrom == IDC_TOOLBAR && pnmh->code == TTN_GETDISPINFO)
        {
            NMTTDISPINFO* pTTDI = reinterpret_cast<NMTTDISPINFO*>(lParam);
            switch (pTTDI->hdr.idFrom)
            {
            case IDM_NEW:   pTTDI->lpszText = const_cast<LPWSTR>(L"新建文件");    break;
            case IDM_OPEN:  pTTDI->lpszText = const_cast<LPWSTR>(L"打开文件");    break;
            case IDM_SAVE:  pTTDI->lpszText = const_cast<LPWSTR>(L"保存文件");    break;
            case IDM_CUT:   pTTDI->lpszText = const_cast<LPWSTR>(L"剪切");        break;
            case IDM_COPY:  pTTDI->lpszText = const_cast<LPWSTR>(L"复制");        break;
            case IDM_PASTE: pTTDI->lpszText = const_cast<LPWSTR>(L"粘贴");        break;
            default:        pTTDI->lpszText = const_cast<LPWSTR>(L"");            break;
            }
            return 0;
        }
        break;
    }

    case WM_SETFOCUS:
        if (state) SetFocus(state->hRichEdit);
        return 0;

    case WM_CLOSE:
        if (state && !CheckUnsaved(hWnd, state))
            return 0; // 用户取消关闭
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        if (state)
        {
            if (state->hAccelTable)
                DestroyAcceleratorTable(state->hAccelTable);
            if (state->hFont)
                DeleteObject(state->hFont);
            if (state->hRichEditDll)
                FreeLibrary(state->hRichEditDll);
            delete state;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 初始化通用控件
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&iccex);

    // 注册窗口类
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TextEditorClass";
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassEx(&wc);

    // 创建主窗口
    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"文本编辑器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 550,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 消息循环 (集成加速键翻译)
    AppState* state = GetState(hWnd);
    HACCEL hAccel = state ? state->hAccelTable : nullptr;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(hWnd, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}
