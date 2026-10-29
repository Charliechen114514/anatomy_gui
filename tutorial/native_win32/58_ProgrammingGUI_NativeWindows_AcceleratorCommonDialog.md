# 通用GUI编程技术——Win32 原生编程实战（五十八）——加速键表与通用对话框

> 上一篇文章我们聊了定时器——从简单的 `SetTimer`/`WM_TIMER` 到高精度的 `QueryPerformanceCounter`。有了定时器，你的程序就能做动画、做定时任务、做倒计时了。但一个真正"好用"的程序还需要两样东西：快捷键和文件操作。快捷键让用户用键盘快速触发菜单命令（Ctrl+S 保存、Ctrl+O 打开），文件操作让用户能打开和保存文件。今天我们就来把这两样东西彻底搞定。

---

## 为什么需要加速键表和通用对话框

写到这里，你的 Win32 程序已经有了菜单、工具栏、状态栏、各种控件——外观上像一个正经的桌面应用了。但有两个"不起眼"的功能如果缺失，用户会觉得你的程序很不专业：

* **没有快捷键**：用户习惯了 Ctrl+C 复制、Ctrl+V 粘贴、Ctrl+S 保存。如果你的菜单项只能用鼠标点，用户体验会大打折扣。
* **没有文件对话框**：你的程序怎么让用户选择一个文件打开？怎么让用户选择保存位置？总不能让用户手动输入路径吧。

加速键表（Accelerator Table）解决快捷键问题，通用对话框（Common Dialogs）解决文件选择、颜色选择、字体选择等问题。它们都是 Win32 的基础设施，用起来不难，但细节不少。

---

## 环境说明

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）
* **额外依赖**：Comdlg32.lib（通用对话框）

---

## 第一部分：加速键表

### 什么是加速键表

加速键表是一个资源，定义了"哪个键组合触发哪个命令 ID"。它的工作方式和菜单完全一样——按键组合触发后，系统发送 `WM_COMMAND` 消息，`wParam` 的低 16 位就是命令 ID，和菜单点击的 `WM_COMMAND` 完全一致。也就是说，你的消息处理代码不需要区分"用户是点了菜单还是按了快捷键"——它们走的是同一条路径。

### 在资源文件中定义加速键表

加速键表定义在 `.rc` 资源文件中：

```rc
// resource.h
#define IDR_ACCELERATOR    101
#define IDM_FILE_NEW       2001
#define IDM_FILE_OPEN      2002
#define IDM_FILE_SAVE      2003
#define IDM_FILE_SAVEAS    2004
#define IDM_EDIT_UNDO      3001
#define IDM_EDIT_CUT       3002
#define IDM_EDIT_COPY      3003
#define IDM_EDIT_PASTE     3004
#define IDM_EDIT_SELECTALL 3005

// resource.rc
#include "resource.h"

IDR_ACCELERATOR ACCELERATORS
{
    "^N",        IDM_FILE_NEW,       VIRTKEY, CONTROL
    "^O",        IDM_FILE_OPEN,      VIRTKEY, CONTROL
    "^S",        IDM_FILE_SAVE,      VIRTKEY, CONTROL
    "+^S",       IDM_FILE_SAVEAS,    VIRTKEY, SHIFT, CONTROL
    "^Z",        IDM_EDIT_UNDO,      VIRTKEY, CONTROL
    "^X",        IDM_EDIT_CUT,       VIRTKEY, CONTROL
    "^C",        IDM_EDIT_COPY,      VIRTKEY, CONTROL
    "^V",        IDM_EDIT_PASTE,     VIRTKEY, CONTROL
    "^A",        IDM_EDIT_SELECTALL, VIRTKEY, CONTROL
}
```

### 语法说明

加速键定义的格式是：

```
"键组合", 命令ID, [, 类型] [, 修饰键...]
```

* **键字符**：用 `""` 包裹。`^` 表示 Ctrl，`+` 表示 Shift。`"^C"` 就是 Ctrl+C。
* **VIRTKEY**：表示使用虚拟键码（推荐）。不用 VIRTKEY 时，键字符按 ASCII 字符匹配。
* **修饰键**：`CONTROL`（Ctrl）、`SHIFT`、`ALT`。可以组合使用。

也可以用虚拟键码直接定义：

```rc
IDR_ACCELERATOR ACCELERATORS
{
    0x4E, IDM_FILE_NEW, VIRTKEY, CONTROL     // VK_N = 0x4E
    VK_F5, IDM_VIEW_REFRESH, VIRTKEY          // F5 刷新
    VK_DELETE, IDM_EDIT_DELETE, VIRTKEY        // Delete 删除
}
```

### 在消息循环中加载和翻译

加速键表必须在消息循环中调用 `TranslateAccelerator`：

```cpp
#include "resource.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 加载加速键表
    HACCEL hAccel = LoadAccelerators(hInstance,
                                      MAKEINTRESOURCE(IDR_ACCELERATOR));

    // ... 创建窗口 ...

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // 关键：TranslateAccelerator 必须在 TranslateMessage 之前调用
        if (!TranslateAccelerator(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 加速键表句柄不需要手动销毁，系统会在程序退出时自动释放
    return (int)msg.wParam;
}
```

`TranslateAccelerator` 的工作流程：

1. 检查当前键盘消息是否匹配加速键表中的某个条目。
2. 如果匹配，直接向指定窗口发送 `WM_COMMAND`（或 `WM_SYSCOMMAND`）消息，并返回 `TRUE`。
3. 如果不匹配，返回 `FALSE`，消息继续走正常的 `TranslateMessage` → `DispatchMessage` 流程。

**注意**：如果 `TranslateAccelerator` 返回 `TRUE`，消息不会被 `DispatchMessage` 分发——它已经被"吃掉"了，转化成了 `WM_COMMAND`。

### 在窗口过程中处理

```cpp
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND: {
        int cmdId = LOWORD(wParam);

        // 菜单点击和快捷键触发走同一段代码
        switch (cmdId) {
        case IDM_FILE_NEW:
            // 新建文件
            break;
        case IDM_FILE_OPEN:
            // 打开文件——后面用通用对话框实现
            break;
        case IDM_FILE_SAVE:
            // 保存文件
            break;
        case IDM_EDIT_COPY:
            // 复制
            break;
        // ... 其他命令
        }
        return 0;
    }
    // ...
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
```

### 区分菜单、快捷键和控件通知

`WM_COMMAND` 的来源有三种：

| 来源 | wParam 高 16 位 | lParam |
|------|----------------|--------|
| 菜单 | 0 | 0 |
| 加速键 | 1 | 0 |
| 控件通知 | 通知码 | 控件 HWND |

大多数时候你不需要区分——命令 ID 足以决定做什么。但如果需要：

```cpp
case WM_COMMAND: {
    int cmdId = LOWORD(wParam);
    int notifyCode = HIWORD(wParam);
    HWND hCtrl = (HWND)lParam;

    if (hCtrl != nullptr) {
        // 来自控件的通知（如按钮点击、编辑框变化）
    }
    else if (notifyCode == 1) {
        // 来自加速键
    }
    else {
        // 来自菜单（notifyCode == 0）
    }
    break;
}
```

### 运行时创建加速键表

除了在 `.rc` 中静态定义，你也可以在运行时动态创建加速键表：

```cpp
ACCEL accel[] = {
    { FVIRTKEY | FCONTROL, 'N',       IDM_FILE_NEW },
    { FVIRTKEY | FCONTROL, 'O',       IDM_FILE_OPEN },
    { FVIRTKEY | FCONTROL, 'S',       IDM_FILE_SAVE },
    { FVIRTKEY | FCONTROL | FSHIFT, 'S', IDM_FILE_SAVEAS },
    { FVIRTKEY | FCONTROL, 'Z',       IDM_EDIT_UNDO },
    { FVIRTKEY | FCONTROL, 'X',       IDM_EDIT_CUT },
    { FVIRTKEY | FCONTROL, 'C',       IDM_EDIT_COPY },
    { FVIRTKEY | FCONTROL, 'V',       IDM_EDIT_PASTE },
};

HACCEL hAccel = CreateAcceleratorTable(accel, _countof(accel));

// 使用完后销毁
DestroyAcceleratorTable(hAccel);
```

`ACCEL` 结构的 `fVirt` 标志：

| 标志 | 含义 |
|------|------|
| FVIRTKEY | key 字段是虚拟键码（必须设置） |
| FCONTROL | Ctrl 修饰键 |
| FSHIFT | Shift 修饰键 |
| FALT | Alt 修饰键 |
| FNOINVERT | 不高亮对应的菜单项 |

---

## 第二部分：通用对话框

### 什么是通用对话框

Windows 提供了一组标准的对话框，让开发者不用自己写就能实现常见的交互功能：

| 对话框 | 函数 | 用途 |
|--------|------|------|
| 打开文件 | `GetOpenFileName` | 选择一个或多个文件打开 |
| 保存文件 | `GetSaveFileName` | 选择保存位置和文件名 |
| 字体选择 | `ChooseFont` | 选择字体、字号、颜色 |
| 颜色选择 | `ChooseColor` | 选择颜色 |
| 查找/替换 | `FindText` / `ReplaceText` | 文本查找和替换 |
| 页面设置 | `PageSetupDlg` | 打印页面设置 |
| 打印 | `PrintDlg` | 选择打印机和打印选项 |

这些对话框的外观和行为和系统自带的程序（记事本、画图）一致，用户很熟悉。

### 打开文件对话框

```cpp
#include <commdlg.h>
// 链接 Comdlg32.lib

bool OpenFileDlg(HWND hWndOwner, std::wstring& filePath) {
    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0"
                       L"C++ 文件 (*.cpp;*.h)\0*.cpp;*.h\0"
                       L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;     // 不需要文件标题
    ofn.lpstrInitialDir = nullptr;   // 使用默认目录
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        filePath = szFile;
        return true;
    }
    return false;
}
```

### 保存文件对话框

```cpp
bool SaveFileDlg(HWND hWndOwner, std::wstring& filePath) {
    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0"
                       L"所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"txt";  // 用户没输入扩展名时自动添加
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileName(&ofn)) {
        filePath = szFile;
        return true;
    }
    return false;
}
```

### OPENFILENAME 关键字段详解

```cpp
OPENFILENAME ofn = {};
ofn.lStructSize = sizeof(ofn);  // 必须设置
ofn.hwndOwner = hWnd;           // 父窗口，对话框会模态显示
ofn.hInstance = hInst;          // 实例句柄（使用自定义模板时需要）

// 文件路径缓冲区
ofn.lpstrFile = szFile;         // [输入/输出] 文件路径缓冲区
ofn.nMaxFile = MAX_PATH;        // 缓冲区大小

// 过滤器
ofn.lpstrFilter = L"文本 (*.txt)\0*.txt\0所有 (*.*)\0*.*\0";
ofn.nFilterIndex = 1;           // 默认选中的过滤器（1-based）

// 标题和初始目录
ofn.lpstrTitle = L"选择一个文件";          // 对话框标题
ofn.lpstrInitialDir = L"C:\\Users";       // 初始目录

// 标志
ofn.Flags = OFN_FILEMUSTEXIST   // 文件必须存在
           | OFN_PATHMUSTEXIST  // 路径必须存在
           | OFN_NOCHANGEDIR;   // 关闭后恢复当前目录
```

### 过滤器语法

`lpstrFilter` 的格式是成对的以 `\0` 分隔的字符串：

```
"显示文本1\0扩展名1\0显示文本2\0扩展名2\0\0"
```

注意：
* 每对由"显示文本"和"扩展名模式"组成。
* 多个扩展名用分号分隔：`"图片\0*.bmp;*.jpg;*.png\0"`。
* 整个字符串以两个 `\0` 结尾（即 `L"...*\0\0"`）。

### 常用标志

| 标志 | 含义 |
|------|------|
| OFN_FILEMUSTEXIST | 用户只能输入已存在的文件名 |
| OFN_PATHMUSTEXIST | 用户只能输入有效的路径 |
| OFN_OVERWRITEPROMPT | 保存时如果文件已存在，弹出确认提示 |
| OFN_ALLOWMULTISELECT | 允许选择多个文件 |
| OFN_NOCHANGEDIR | 关闭对话框后恢复当前工作目录 |
| OFN_HIDEREADONLY | 隐藏"以只读方式打开"复选框 |
| OFN_CREATEPROMPT | 输入不存在的文件名时提示创建 |
| OFN_EXPLORER | 使用新版资源管理器风格（默认开启） |

### 多文件选择

```cpp
bool OpenMultipleFilesDlg(HWND hWndOwner,
                           std::vector<std::wstring>& filePaths) {
    // 多文件选择需要更大的缓冲区
    const DWORD bufferSize = 65536;
    auto buffer = std::make_unique<wchar_t[]>(bufferSize);
    buffer[0] = L'\0';

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFile = buffer.get();
    ofn.nMaxFile = bufferSize;
    ofn.lpstrFilter = L"所有文件 (*.*)\0*.*\0";
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;

    if (!GetOpenFileName(&ofn)) return false;

    // 解析多文件结果
    // buffer 的格式：目录路径\0文件名1\0文件名2\0\0
    wchar_t* p = buffer.get();
    std::wstring directory = p;
    p += directory.length() + 1;

    if (*p == L'\0') {
        // 只选了一个文件，directory 就是完整路径
        filePaths.push_back(directory);
    } else {
        // 选了多个文件
        while (*p != L'\0') {
            std::wstring fileName = p;
            filePaths.push_back(directory + L"\\" + fileName);
            p += fileName.length() + 1;
        }
    }

    return true;
}
```

### 错误处理

`GetOpenFileName` / `GetSaveFileName` 返回 `FALSE` 时，不一定是"用户取消了"。需要检查是否真的出错了：

```cpp
if (!GetOpenFileName(&ofn)) {
    DWORD err = CommDlgExtendedError();
    if (err != 0) {
        // 真的出错了
        wchar_t msg[256];
        swprintf_s(msg, L"文件对话框出错，错误码: 0x%08X", err);
        MessageBox(hWnd, msg, L"错误", MB_OK | MB_ICONERROR);
    }
    // err == 0 表示用户取消了
}
```

---

### 字体选择对话框

```cpp
bool ChooseFontDlg(HWND hWndOwner, LOGFONT& lf, COLORREF& textColor) {
    CHOOSEFONT cf = {};
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hWndOwner;
    cf.lpLogFont = &lf;               // [输入/输出] 字体信息
    cf.Flags = CF_SCREENFONTS         // 屏幕字体
             | CF_EFFECTS             // 允许选择颜色和效果
             | CF_INITTOLOGFONTSTRUCT; // 用 lf 初始化对话框
    cf.rgbColors = textColor;         // [输入/输出] 文字颜色

    if (ChooseFont(&cf)) {
        // 成功后 lf 包含用户选择的字体
        // textColor = cf.rgbColors 包含选择的颜色
        // cf.iPointSize 是字号（1/10 磅为单位）
        return true;
    }
    return false;
}

// 使用示例
LOGFONT lf = {};
lf.lfHeight = -16;
lf.lfWeight = FW_NORMAL;
wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
COLORREF textColor = RGB(0, 0, 0);

if (ChooseFontDlg(hWnd, lf, textColor)) {
    // 用 lf 创建字体
    HFONT hFont = CreateFontIndirect(&lf);
    // 发送给编辑框控件
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
}
```

### 颜色选择对话框

```cpp
bool ChooseColorDlg(HWND hWndOwner, COLORREF& color) {
    // 自定义颜色数组（16 色）
    static COLORREF customColors[16] = {
        RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
        RGB(255, 255, 0), RGB(255, 0, 255), RGB(0, 255, 255),
    };

    CHOOSECOLOR cc = {};
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hWndOwner;
    cc.rgbResult = color;               // [输入/输出] 初始/选中的颜色
    cc.lpCustColors = customColors;     // 自定义颜色
    cc.Flags = CC_FULLOPEN             // 展开自定义颜色区域
             | CC_RGBINIT;             // 用 rgbResult 作为初始颜色

    if (ChooseColor(&cc)) {
        color = cc.rgbResult;
        return true;
    }
    return false;
}
```

### 查找和替换对话框

查找/替换对话框和其他通用对话框有一个重要的区别：**它是非模态的**。`FindText` 和 `ReplaceText` 创建对话框后立即返回，对话框一直悬浮在窗口上方，用户可以在主窗口和对话框之间自由切换。

```cpp
// 全局变量（因为对话框是非模态的，需要保存句柄）
static HWND g_hFindReplaceDlg = nullptr;
static UINT g_findMsgId = 0;  // 自定义消息 ID

// 初始化：注册自定义消息
g_findMsgId = RegisterWindowMessage(FINDMSGSTRING);

// 打开查找对话框
void ShowFindDialog(HWND hWndOwner, HINSTANCE hInst) {
    if (g_hFindReplaceDlg != nullptr) {
        // 已打开，激活它
        SetFocus(g_hFindReplaceDlg);
        return;
    }

    static FINDREPLACE fr = {};
    static wchar_t szFindWhat[256] = {};

    ZeroMemory(&fr, sizeof(fr));
    fr.lStructSize = sizeof(FINDREPLACE);
    fr.hwndOwner = hWndOwner;
    fr.lpstrFindWhat = szFindWhat;
    fr.wFindWhatLen = 256;
    fr.Flags = FR_DOWN;  // 默认向下搜索

    g_hFindReplaceDlg = FindText(&fr);
}

// 在窗口过程中处理查找消息
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
    // 检查是否是查找/替换消息
    if (uMsg == g_findMsgId && g_findMsgId != 0) {
        FINDREPLACE* fr = (FINDREPLACE*)lParam;

        if (fr->Flags & FR_FINDNEXT) {
            // 用户点击了"查找下一个"
            // fr->lpstrFindWhat 是搜索文本
            // fr->Flags & FR_DOWN 表示向下搜索
            // 在你的文档中执行查找逻辑
        }
        else if (fr->Flags & FR_DIALOGTERM) {
            // 对话框关闭了
            g_hFindReplaceDlg = nullptr;
        }
        return 0;
    }

    // ... 其他消息处理
}
```

查找/替换对话框的要点：

* **非模态**：`FindText` / `ReplaceText` 返回对话框句柄后立即返回，不会阻塞。
* **通信方式**：通过 `RegisterWindowMessage(FINDMSGSTRING)` 注册的自定义消息通知主窗口。
* **内存管理**：`FINDREPLACE` 结构和其中的字符串缓冲区必须在对话框存活期间一直有效——用 `static` 变量是常见做法。
* **关闭检测**：当 `fr->Flags & FR_DIALOGTERM` 时，说明用户关闭了对话框。

---

## 实战示例：简易文本编辑器框架

把加速键表和通用对话框组合起来，搭建一个简易文本编辑器的框架。这个框架支持新建、打开、保存、字体选择，并且有完整的快捷键支持。

```cpp
#include <Windows.h>
#include <commdlg.h>
#include <fstream>
#include <string>
#include "resource.h"

static HWND g_hEdit = nullptr;
static HFONT g_hFont = nullptr;
static LOGFONT g_currentFont = {};
static std::wstring g_currentFile;
static bool g_modified = false;

// 创建工具栏和编辑框
void CreateControls(HWND hWnd, HINSTANCE hInst) {
    // 创建多行编辑框
    g_hEdit = CreateWindowEx(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)100, hInst, nullptr);

    // 设置默认字体
    GetObject(GetStockObject(SYSTEM_FONT), sizeof(LOGFONT), &g_currentFont);
    g_hFont = CreateFontIndirect(&g_currentFont);
    SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, FALSE);
}

// 编辑框填满客户区
void ResizeEdit(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    MoveWindow(g_hEdit, 0, 0, rc.right, rc.bottom, TRUE);
}

// 加载文件内容到编辑框
bool LoadFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // 简单处理：假设 UTF-8 或 ANSI
    int wlen = MultiByteToWideChar(CP_ACP, 0,
                                    content.c_str(), (int)content.size(),
                                    nullptr, 0);
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0,
                         content.c_str(), (int)content.size(),
                         wstr.data(), wlen);

    SetWindowText(g_hEdit, wstr.c_str());
    g_currentFile = path;
    g_modified = false;

    // 更新标题栏
    std::wstring title = L"MiniEdit - " + path;
    SetWindowText(GetParent(g_hEdit), title.c_str());
    return true;
}

// 保存编辑框内容到文件
bool SaveFile(const std::wstring& path) {
    int len = GetWindowTextLength(g_hEdit);
    std::wstring text(len + 1, L'\0');
    GetWindowText(g_hEdit, text.data(), len + 1);

    // 转换为 ANSI/MBCS
    int mlen = WideCharToMultiByte(CP_ACP, 0,
                                    text.c_str(), len,
                                    nullptr, 0, nullptr, nullptr);
    std::string mstr(mlen, '\0');
    WideCharToMultiByte(CP_ACP, 0,
                         text.c_str(), len,
                         mstr.data(), mlen, nullptr, nullptr);

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(mstr.data(), mlen);
    file.close();

    g_currentFile = path;
    g_modified = false;

    std::wstring title = L"MiniEdit - " + path;
    SetWindowText(GetParent(g_hEdit), title.c_str());
    return true;
}

// 检查是否需要保存
bool CheckSave(HWND hWnd) {
    if (!g_modified) return true;

    int result = MessageBox(hWnd,
        L"文件已修改，是否保存？",
        L"MiniEdit", MB_YESNOCANCEL | MB_ICONQUESTION);

    if (result == IDYES) {
        // 如果有文件名就保存，否则弹出保存对话框
        if (!g_currentFile.empty()) {
            return SaveFile(g_currentFile);
        }
        std::wstring path;
        if (SaveFileDlg(hWnd, path)) {
            return SaveFile(path);
        }
        return false; // 用户取消了保存对话框
    }
    else if (result == IDCANCEL) {
        return false; // 用户取消
    }
    return true; // IDNO，不保存继续
}

// 命令处理
void HandleCommand(HWND hWnd, int cmdId, HINSTANCE hInst) {
    switch (cmdId) {
    case IDM_FILE_NEW:
        if (CheckSave(hWnd)) {
            SetWindowText(g_hEdit, L"");
            g_currentFile.clear();
            g_modified = false;
            SetWindowText(hWnd, L"MiniEdit - 未命名");
        }
        break;

    case IDM_FILE_OPEN: {
        if (!CheckSave(hWnd)) break;
        std::wstring path;
        if (OpenFileDlg(hWnd, path)) {
            if (!LoadFile(path)) {
                MessageBox(hWnd, L"无法打开文件", L"错误",
                           MB_OK | MB_ICONERROR);
            }
        }
        break;
    }

    case IDM_FILE_SAVE:
        if (g_currentFile.empty()) {
            // 没有文件名，走另存为
            std::wstring path;
            if (SaveFileDlg(hWnd, path)) {
                SaveFile(path);
            }
        } else {
            SaveFile(g_currentFile);
        }
        break;

    case IDM_FILE_SAVEAS: {
        std::wstring path;
        if (SaveFileDlg(hWnd, path)) {
            SaveFile(path);
        }
        break;
    }

    case IDM_EDIT_UNDO:
        SendMessage(g_hEdit, EM_UNDO, 0, 0);
        break;

    case IDM_EDIT_CUT:
        SendMessage(g_hEdit, WM_CUT, 0, 0);
        break;

    case IDM_EDIT_COPY:
        SendMessage(g_hEdit, WM_COPY, 0, 0);
        break;

    case IDM_EDIT_PASTE:
        SendMessage(g_hEdit, WM_PASTE, 0, 0);
        break;

    case IDM_EDIT_SELECTALL:
        SendMessage(g_hEdit, EM_SETSEL, 0, -1);
        break;

    case IDM_FORMAT_FONT: {
        COLORREF color = RGB(0, 0, 0);
        if (ChooseFontDlg(hWnd, g_currentFont, color)) {
            // 删掉旧字体，创建新字体
            if (g_hFont) DeleteObject(g_hFont);
            g_hFont = CreateFontIndirect(&g_currentFont);
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }
        break;
    }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateControls(hWnd, ((LPCREATESTRUCT)lParam)->hInstance);
        return 0;

    case WM_SIZE:
        ResizeEdit(hWnd);
        return 0;

    case WM_COMMAND:
        HandleCommand(hWnd, LOWORD(wParam),
                      ((LPCREATESTRUCT)GetWindowLongPtr(
                          hWnd, GWLP_HINSTANCE))->hInstance);
        // 编辑框的修改通知
        if (HIWORD(wParam) == EN_CHANGE && g_hEdit == (HWND)lParam) {
            g_modified = true;
            std::wstring title = L"MiniEdit - *";
            title += g_currentFile.empty() ? L"未命名" : g_currentFile;
            SetWindowText(hWnd, title.c_str());
        }
        return 0;

    case WM_CLOSE:
        if (!CheckSave(hWnd)) return 0;
        // fall through

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 加载加速键表
    HACCEL hAccel = LoadAccelerators(hInstance,
                                      MAKEINTRESOURCE(IDR_ACCELERATOR));

    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MiniEdit";
    RegisterClassEx(&wc);

    // 创建主窗口
    HWND hWnd = CreateWindow(L"MiniEdit", L"MiniEdit - 未命名",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                              nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 消息循环（包含加速键翻译）
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}
```

---

## 常见问题与陷阱

### 加速键表

* **忘记在消息循环中调用 `TranslateAccelerator`**：这是最常见的错误。加速键定义了但"不生效"，99% 的原因是忘了在消息循环中加载和翻译。
* **`TranslateAccelerator` 放在 `TranslateMessage` 后面**：顺序错了。必须在 `TranslateMessage` 之前调用，否则某些按键可能被错误翻译。
* **加速键和菜单 ID 不一致**：加速键触发的命令 ID 和菜单项的 ID 必须相同，否则不会触发对应的功能。
* **Alt 键加速键**：默认情况下 `TranslateAccelerator` 在菜单处于跟踪状态时会跳过 Alt 键组合，避免和菜单助记符冲突。

### 通用对话框

* **`lpstrFile` 缓冲区太小**：多文件选择时，缓冲区很容易不够用。建议至少分配 64KB。
* **过滤器字符串格式错误**：`\0` 分隔符最容易出错。建议用 `L"text\0pattern\0\0"` 这种写法，一目了然。
* **忘记检查 `CommDlgExtendedError`**：`GetOpenFileName` 返回 `FALSE` 可能是用户取消（正常），也可能是参数错误。不检查错误码就直接报错会让用户困惑。
* **`FINDREPLACE` 结构被提前销毁**：查找对话框是非模态的，对话框关闭前 `FINDREPLACE` 结构必须保持有效。用 `static` 变量是最简单的解决方案。
* **字体对话框后不删除旧字体**：每次调用 `ChooseFont` 后都会得到新的 `LOGFONT`，如果不删除旧的 `HFONT` 就创建新的，会造成 GDI 对象泄漏。
* **通用对话框的父窗口为 `nullptr`**：虽然可以传 `nullptr`（对话框变成顶级窗口），但通常你应该传你的主窗口句柄，这样对话框才是模态的，用户不能在对话框后面操作主窗口。

---

## 练习

1. **为文本编辑器添加菜单**：为上面的 MiniEdit 示例添加完整的菜单栏（文件、编辑、格式、帮助），菜单项的 ID 和加速键表中的 ID 对应。

2. **多文件打开**：修改 `OpenFileDlg` 支持 `OFN_ALLOWMULTISELECT`，在状态栏中显示选中的文件数量和总大小。

3. **查找和替换**：为 MiniEdit 添加查找功能——使用 `FindText` 对话框，在编辑框中搜索并高亮匹配的文本。提示：用 `EM_GETSEL` 和 `EM_SETSEL` 定位文本。

4. **最近文件列表**：在文件菜单中维护一个"最近打开的文件"列表（MRU），最多显示 5 个。每次打开或保存文件后更新列表。提示：用注册表 `RegSetKeyValue` 持久化存储。

---

**参考资料**:
- [Using Keyboard Accelerators - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/menurc/using-keyboard-accelerators)
- [TranslateAccelerator function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translateaccelerator)
- [OPENFILENAME structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-openfilenamew)
- [GetOpenFileName function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-getopenfilenamew)
- [ChooseFont function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-choosefontw)
- [ChooseColor function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-choosecolorw)
- [FindText function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-findtextw)
- [Common Dialog Box Library - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/dlgbox/common-dialog-box-library)
