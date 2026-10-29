/**
 * 19_dialog_proc - DialogProc 高级模式
 *
 * 演示 DialogProc 的高级用法和数据交换：
 * - 主窗口打开一个"用户信息"对话框并进行数据交换
 * - WM_INITDIALOG：向对话框控件填充初始值
 * - 输入验证：姓名不能为空，年龄必须在 1-150 之间
 * - 自定义消息 (WM_USER+1) 用于请求父窗口刷新数据
 * - 对话框控件：姓名 Edit、年龄 Edit、性别 ComboBox、确定/取消
 * - 点击确定：验证输入，通过 DWLP_USERDATA 将结果传回主窗口
 * - 主窗口在对话框关闭后显示收集到的用户信息
 *
 * 参考: tutorial/native_win32/11_ProgrammingGUI_NativeWindows_DialogProc.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string>

// ============================================================
// 常量和控件 ID 定义
// ============================================================

static const wchar_t* MAIN_WINDOW_CLASS = L"DialogProcDemoClass";

// 对话框控件 ID
#define IDC_EDIT_NAME           4001
#define IDC_EDIT_AGE            4002
#define IDC_COMBO_GENDER        4004
#define IDC_BTN_REFRESH         4005
#define IDC_STATIC_PREVIEW      4006
#define IDC_STATIC_NAMELABEL    4007
#define IDC_STATIC_AGELABEL     4008
#define IDC_STATIC_GENDERLABEL  4009

// 主窗口控件 ID
#define IDC_BTN_EDIT            5001
#define IDC_EDIT_DISPLAY        5002

// 自定义消息：对话框请求父窗口刷新显示
#define WM_REFRESH_DISPLAY      (WM_USER + 1)

// ============================================================
// 用户信息数据结构
// ============================================================

/**
 * UserInfo - 用于在主窗口和对话框之间传递数据
 *
 * 这个结构体通过 DialogBoxIndirectParam 的 lParam 传递给对话框，
 * 在 WM_INITDIALOG 中保存到对话框的 DWLP_USER。
 * 对话框关闭前将结果写回此结构体。
 */
struct UserInfo
{
    wchar_t name[64];       // 姓名
    int     age;            // 年龄
    int     gender;         // 性别 (0=男, 1=女, 2=其他)
    BOOL    confirmed;      // 用户是否点击了"确定"（TRUE=确定，FALSE=取消）
};

// 性别选项
static const wchar_t* g_genderOptions[] = {
    L"男",
    L"女",
    L"其他"
};

// ============================================================
// 前向声明
// ============================================================

INT_PTR CALLBACK UserInfoDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdatePreviewText(HWND hDlg);
void CreateUserInfoDialog(HINSTANCE hInstance, HWND hParent, UserInfo* info);
void UpdateMainWindowDisplay(HWND hwnd, const UserInfo* info);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
ATOM RegisterMainWindowClass(HINSTANCE hInstance);

// ============================================================
// 对话框过程 (DialogProc)
// ============================================================

/**
 * 用户信息对话框过程
 *
 * DialogProc 与 WindowProc 的关键区别：
 * 1. 返回 TRUE 表示消息已处理，FALSE 表示未处理
 * 2. 不需要调用 DefWindowProc
 * 3. 使用 WM_INITDIALOG 代替 WM_CREATE 进行初始化
 * 4. 对话框管理器自动处理大部分默认行为
 */
INT_PTR CALLBACK UserInfoDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_INITDIALOG - 对话框初始化
     *
     * 这是对话框最重要的初始化时机，相当于普通窗口的 WM_CREATE。
     * 在这里做以下事情：
     * 1. 保存传入的 UserInfo 指针到 DWLP_USER
     * 2. 向控件填充初始值（数据交换的"读"方向）
     */
    case WM_INITDIALOG:
    {
        // 保存 UserInfo 指针到对话框实例数据
        // DWLP_USER 是对话框专用的用户数据存储位置
        UserInfo* info = (UserInfo*)lParam;
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)info);

        // --- 填充初始值到控件 ---

        // 姓名编辑框：设置初始文本
        SetDlgItemText(hDlg, IDC_EDIT_NAME, info->name);

        // 年龄编辑框：设置初始数值
        SetDlgItemInt(hDlg, IDC_EDIT_AGE, info->age, FALSE);

        // 性别组合框：添加选项并选中当前值
        HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_GENDER);
        for (int i = 0; i < 3; i++)
        {
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)g_genderOptions[i]);
        }
        SendMessage(hCombo, CB_SETCURSEL, (WPARAM)info->gender, 0);

        // 设置年龄输入的范围提示（通过 EM_LIMITTEXT 限制输入长度）
        SendDlgItemMessage(hDlg, IDC_EDIT_AGE, EM_LIMITTEXT, 3, 0);

        // 更新预览文本
        UpdatePreviewText(hDlg);

        return TRUE;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理按钮点击和控件通知。
     */
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        switch (id)
        {
        /**
         * IDOK - "确定"按钮
         *
         * 进行输入验证，验证通过后将数据保存回 UserInfo 结构体。
         */
        case IDOK:
        {
            UserInfo* info = (UserInfo*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (!info) return TRUE;

            // ---- 输入验证 ----

            // 验证姓名：不能为空
            wchar_t nameBuf[64] = {};
            GetDlgItemText(hDlg, IDC_EDIT_NAME, nameBuf, 64);

            // 去除首尾空白检查是否为空
            std::wstring nameStr = nameBuf;
            size_t start = nameStr.find_first_not_of(L" \t\r\n");
            size_t end = nameStr.find_last_not_of(L" \t\r\n");

            if (start == std::wstring::npos)
            {
                // 姓名为空，显示验证错误消息框
                MessageBox(hDlg,
                    L"姓名不能为空！\n请输入有效的姓名。",
                    L"输入验证错误",
                    MB_ICONWARNING | MB_OK);

                // 将焦点设置到姓名输入框并选中内容
                SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
                SendMessage(GetDlgItem(hDlg, IDC_EDIT_NAME), EM_SETSEL, 0, -1);
                return TRUE;
            }

            // 验证年龄：必须是 1-150 之间的整数
            BOOL bTranslated = FALSE;
            UINT ageVal = GetDlgItemInt(hDlg, IDC_EDIT_AGE, &bTranslated, FALSE);

            if (!bTranslated || ageVal < 1 || ageVal > 150)
            {
                MessageBox(hDlg,
                    L"年龄必须是 1 到 150 之间的整数！\n请输入有效的年龄。",
                    L"输入验证错误",
                    MB_ICONWARNING | MB_OK);

                SetFocus(GetDlgItem(hDlg, IDC_EDIT_AGE));
                SendMessage(GetDlgItem(hDlg, IDC_EDIT_AGE), EM_SETSEL, 0, -1);
                return TRUE;
            }

            // ---- 验证通过，保存数据 ----

            // 保存姓名（去除首尾空白后的）
            std::wstring trimmed = nameStr.substr(start, end - start + 1);
            wcsncpy_s(info->name, trimmed.c_str(), _TRUNCATE);

            // 保存年龄
            info->age = (int)ageVal;

            // 保存性别
            HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_GENDER);
            info->gender = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (info->gender < 0) info->gender = 0;

            // 标记为"确认"
            info->confirmed = TRUE;

            // 关闭模态对话框
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        /**
         * IDCANCEL - "取消"按钮
         *
         * 用户取消操作，不保存任何数据。
         */
        case IDCANCEL:
        {
            UserInfo* info = (UserInfo*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (info)
            {
                info->confirmed = FALSE;
            }
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }

        /**
         * IDC_BTN_REFRESH - "刷新预览"按钮
         *
         * 向父窗口发送自定义消息，请求刷新主窗口的显示。
         * 这演示了子窗口向父窗口通信的模式。
         */
        case IDC_BTN_REFRESH:
        {
            // 向父窗口发送自定义消息 WM_REFRESH_DISPLAY
            HWND hParent = GetParent(hDlg);
            if (hParent)
            {
                PostMessage(hParent, WM_REFRESH_DISPLAY, 0, 0);
            }
            // 同时更新对话框内的预览文本
            UpdatePreviewText(hDlg);
            return TRUE;
        }

        /**
         * 编辑框内容变化时更新预览
         */
        case IDC_EDIT_NAME:
        case IDC_EDIT_AGE:
        {
            if (code == EN_CHANGE)
            {
                UpdatePreviewText(hDlg);
            }
            break;
        }

        case IDC_COMBO_GENDER:
        {
            if (code == CBN_SELCHANGE)
            {
                UpdatePreviewText(hDlg);
            }
            break;
        }
        }

        return TRUE;
    }

    /**
     * WM_CLOSE - 关闭对话框
     *
     * 点击右上角 X 按钮时触发，等同于"取消"操作。
     */
    case WM_CLOSE:
    {
        UserInfo* info = (UserInfo*)GetWindowLongPtr(hDlg, DWLP_USER);
        if (info)
        {
            info->confirmed = FALSE;
        }
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    default:
        return FALSE;
    }
}

// ============================================================
// 辅助函数
// ============================================================

/**
 * 更新对话框中的预览文本
 *
 * 根据当前控件的输入内容，实时生成预览字符串。
 * 演示从对话框控件读取数据的"读"方向。
 */
void UpdatePreviewText(HWND hDlg)
{
    wchar_t nameBuf[64] = {};
    GetDlgItemText(hDlg, IDC_EDIT_NAME, nameBuf, 64);

    BOOL bTranslated = FALSE;
    UINT age = GetDlgItemInt(hDlg, IDC_EDIT_AGE, &bTranslated, FALSE);

    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_GENDER);
    int genderIdx = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

    wchar_t preview[256] = {};
    if (bTranslated && wcslen(nameBuf) > 0 && genderIdx >= 0)
    {
        swprintf_s(preview, L"预览: %s, %d 岁, %s",
                   nameBuf, age, g_genderOptions[genderIdx]);
    }
    else
    {
        wcscpy_s(preview, L"预览: （请填写完整信息）");
    }

    SetDlgItemText(hDlg, IDC_STATIC_PREVIEW, preview);
}

// ============================================================
// 在内存中构建对话框模板并创建模态对话框
// ============================================================

/**
 * 创建用户信息对话框
 *
 * 在内存中手动构建模态对话框模板（DLGTEMPLATE），包含以下控件：
 *   - 姓名标签 + 编辑框
 *   - 年龄标签 + 编辑框
 *   - 性别标签 + 组合框
 *   - 预览文本
 *   - 刷新预览按钮
 *   - 确定 / 取消按钮
 *
 * 使用 DialogBoxIndirectParam 创建模态对话框。
 * 模态对话框会禁用父窗口，直到对话框关闭才返回。
 */
void CreateUserInfoDialog(HINSTANCE hInstance, HWND hParent, UserInfo* info)
{
    const int dlgWidth  = 300;
    const int dlgHeight = 200;

    // 分配足够的内存来存放对话框模板
    size_t bufSize = 2048;
    BYTE* pBuffer = (BYTE*)LocalAlloc(LPTR, bufSize);
    if (!pBuffer) return;

    BYTE* p = pBuffer;

    // ---- DLGTEMPLATE 头部 ----
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)p;
    pDlg->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT;
    pDlg->dwExtendedStyle = 0;
    pDlg->cdit = 11;   // 控件总数：11 个
    pDlg->x = 0;
    pDlg->y = 0;
    pDlg->cx = dlgWidth;
    pDlg->cy = dlgHeight;

    p += sizeof(DLGTEMPLATE);

    // 菜单（空 = 无菜单）
    *(WORD*)p = 0; p += sizeof(WORD);

    // 窗口类（空 = 使用默认对话框类 #32770）
    *(WORD*)p = 0; p += sizeof(WORD);

    // 对话框标题
    const wchar_t* title = L"用户信息";
    size_t tLen = wcslen(title) + 1;
    memcpy(p, title, tLen * sizeof(wchar_t));
    p += tLen * sizeof(wchar_t);

    // 字体大小（DS_SETFONT 必须提供）
    *(WORD*)p = 9; p += sizeof(WORD);

    // 字体名称
    const wchar_t* fontName = L"Microsoft YaHei UI";
    size_t fLen = wcslen(fontName) + 1;
    memcpy(p, fontName, fLen * sizeof(wchar_t));
    p += fLen * sizeof(wchar_t);

    // ---- 辅助宏：内存对齐 ----
    // DLGITEMTEMPLATE 必须在 DWORD 边界上对齐
    #define ALIGN_DWORD(ptr) \
        { (ptr) = (BYTE*)((((DWORD_PTR)(ptr)) + 3) & ~3); }

    // ---- 辅助 lambda：添加一个控件项 ----
    auto AddItem = [&](
        DWORD style, DWORD exStyle,
        int x, int y, int cx, int cy,
        DWORD id,
        const wchar_t* className,
        const wchar_t* text)
    {
        // 确保对齐到 DWORD 边界
        ALIGN_DWORD(p);

        DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)p;
        pItem->style = style;
        pItem->dwExtendedStyle = exStyle;
        pItem->x = x;
        pItem->y = y;
        pItem->cx = cx;
        pItem->cy = cy;
        pItem->id = id;
        p += sizeof(DLGITEMTEMPLATE);

        // 窗口类
        // 如果 className 的值小于 0x10000，表示使用预定义原子类
        // 0x0080 = BUTTON, 0x0081 = EDIT, 0x0082 = STATIC, 0x0083 = COMBOBOX
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

    // ---- 定义控件布局 ----

    // 行1: 姓名标签 + 编辑框
    AddItem(WS_CHILD | WS_VISIBLE | SS_RIGHT, 0,
        10, 12, 45, 14, IDC_STATIC_NAMELABEL,
        (const wchar_t*)0x0082, L"姓名:");

    AddItem(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0,
        60, 10, 220, 16, IDC_EDIT_NAME,
        (const wchar_t*)0x0081, L"");

    // 行2: 年龄标签 + 编辑框
    AddItem(WS_CHILD | WS_VISIBLE | SS_RIGHT, 0,
        10, 36, 45, 14, IDC_STATIC_AGELABEL,
        (const wchar_t*)0x0082, L"年龄:");

    AddItem(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_NUMBER, 0,
        60, 34, 60, 16, IDC_EDIT_AGE,
        (const wchar_t*)0x0081, L"");

    // 行2 附加：年龄范围提示
    AddItem(WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
        130, 36, 80, 14, (DWORD)-1,
        (const wchar_t*)0x0082, L"(范围: 1-150)");

    // 行3: 性别标签 + 组合框
    AddItem(WS_CHILD | WS_VISIBLE | SS_RIGHT, 0,
        10, 60, 45, 14, IDC_STATIC_GENDERLABEL,
        (const wchar_t*)0x0082, L"性别:");

    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0,
        60, 58, 100, 80, IDC_COMBO_GENDER,
        (const wchar_t*)0x0083, L"");

    // 行4: 预览文本
    AddItem(WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
        10, 90, 270, 16, IDC_STATIC_PREVIEW,
        (const wchar_t*)0x0082, L"预览: （请填写完整信息）");

    // 行5: 刷新预览按钮
    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
        190, 115, 90, 24, IDC_BTN_REFRESH,
        (const wchar_t*)0x0080, L"刷新预览");

    // 行6: 确定 / 取消按钮
    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
        90, 155, 80, 28, IDOK,
        (const wchar_t*)0x0080, L"确定");

    AddItem(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
        190, 155, 80, 28, IDCANCEL,
        (const wchar_t*)0x0080, L"取消");

    #undef ALIGN_DWORD

    // 使用 DialogBoxIndirectParam 创建模态对话框
    // 模态对话框：禁用父窗口，阻塞直到对话框关闭
    // 参数 (LPARAM)info 传递给 WM_INITDIALOG 的 lParam
    DialogBoxIndirectParam(
        hInstance,
        (LPCDLGTEMPLATE)pBuffer,
        hParent,
        UserInfoDialogProc,
        (LPARAM)info
    );

    LocalFree(pBuffer);
}

// ============================================================
// 主窗口过程
// ============================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    /**
     * WM_CREATE - 窗口创建
     *
     * 创建"编辑用户信息"按钮和初始显示文本。
     */
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCT*)lParam;

        // 创建"编辑用户信息"按钮
        CreateWindowEx(
            0, L"BUTTON", L"编辑用户信息",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            15, 15, 160, 35,
            hwnd, (HMENU)IDC_BTN_EDIT,
            cs->hInstance, nullptr
        );

        // 创建信息显示区域（多行只读编辑框）
        CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY | WS_VSCROLL,
            15, 60, 455, 240,
            hwnd, (HMENU)IDC_EDIT_DISPLAY,
            cs->hInstance, nullptr
        );

        // 创建字体并应用到所有子控件
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

        // 初始化显示
        UpdateMainWindowDisplay(hwnd, nullptr);

        return 0;
    }

    /**
     * WM_COMMAND - 命令消息
     *
     * 处理"编辑用户信息"按钮的点击事件。
     */
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_BTN_EDIT)
        {
            // 准备用户信息结构（设置初始默认值）
            UserInfo info = {};
            wcscpy_s(info.name, L"张三");
            info.age = 25;
            info.gender = 0;    // 默认"男"
            info.confirmed = FALSE;

            // 获取应用程序实例句柄
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

            // 打开模态对话框
            // DialogBoxIndirectParam 会阻塞，直到对话框关闭才返回
            CreateUserInfoDialog(hInst, hwnd, &info);

            // 对话框关闭后，检查用户是否点击了确定
            if (info.confirmed)
            {
                // 数据已通过 info 结构体传回，更新主窗口显示
                UpdateMainWindowDisplay(hwnd, &info);
            }
        }

        return 0;
    }

    /**
     * WM_REFRESH_DISPLAY - 自定义消息 (WM_USER + 1)
     *
     * 由对话框通过 PostMessage 发送，请求主窗口刷新显示。
     * 这演示了子控件通过自定义消息与父窗口通信的模式。
     */
    case WM_REFRESH_DISPLAY:
    {
        // 在实际应用中，这里可以更新主窗口的部分显示
        // 例如：实时预览对话框中的输入内容
        return 0;
    }

    /**
     * WM_PAINT - 绘制消息
     */
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    /**
     * WM_DESTROY - 窗口销毁
     */
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================
// 更新主窗口显示
// ============================================================

/**
 * 更新主窗口显示区域
 *
 * 根据传入的 UserInfo 更新显示文本。
 * 如果 info 为 nullptr，显示初始提示信息。
 * 如果 info->confirmed 为 TRUE，显示用户填写的完整信息。
 */
void UpdateMainWindowDisplay(HWND hwnd, const UserInfo* info)
{
    HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_DISPLAY);
    if (!hEdit) return;

    std::wstring text;

    if (info && info->confirmed)
    {
        // 显示收集到的用户信息
        text += L"========== 用户信息 ==========\r\n\r\n";
        text += L"  姓名: ";
        text += info->name;
        text += L"\r\n";

        wchar_t ageBuf[32] = {};
        swprintf_s(ageBuf, L"%d", info->age);
        text += L"  年龄: ";
        text += ageBuf;
        text += L" 岁\r\n";

        text += L"  性别: ";
        if (info->gender >= 0 && info->gender <= 2)
        {
            text += g_genderOptions[info->gender];
        }
        else
        {
            text += L"未知";
        }
        text += L"\r\n\r\n";

        text += L"===============================\r\n\r\n";
        text += L"提示: 点击上方\"编辑用户信息\"按钮\r\n";
        text += L"      可重新修改用户信息。\r\n\r\n";
        text += L"DialogProc 高级模式演示:\r\n";
        text += L"  - WM_INITDIALOG: 初始化控件数据\r\n";
        text += L"  - 输入验证: 姓名/年龄格式检查\r\n";
        text += L"  - DWLP_USER: 数据回传\r\n";
        text += L"  - 自定义消息: WM_REFRESH_DISPLAY\r\n";
    }
    else
    {
        text += L"===== DialogProc 高级模式示例 =====\r\n\r\n";
        text += L"尚未收集用户信息。\r\n\r\n";
        text += L"请点击上方\"编辑用户信息\"按钮\r\n";
        text += L"打开对话框并输入信息。\r\n\r\n";
        text += L"-----------------------------------\r\n\r\n";
        text += L"本示例演示以下技术要点:\r\n\r\n";
        text += L"1. WM_INITDIALOG\r\n";
        text += L"   - 在对话框显示前初始化控件\r\n";
        text += L"   - 填充 ComboBox、设置默认值\r\n\r\n";
        text += L"2. 输入验证\r\n";
        text += L"   - 姓名不能为空\r\n";
        text += L"   - 年龄范围 1-150\r\n";
        text += L"   - 验证失败时 MessageBox 提示并聚焦\r\n\r\n";
        text += L"3. 数据交换\r\n";
        text += L"   - 通过结构体指针传递数据\r\n";
        text += L"   - DWLP_USER 存储指针\r\n";
        text += L"   - 确定后写回结果\r\n\r\n";
        text += L"4. 自定义消息\r\n";
        text += L"   - WM_REFRESH_DISPLAY (WM_USER+1)\r\n";
        text += L"   - 子窗口向父窗口通信\r\n";
    }

    SetWindowText(hEdit, text.c_str());
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
        L"DialogProc 高级模式示例",    // 窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 350,                      // 窗口大小 500x350
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
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
