# 通用GUI编程技术——Win32 原生编程实战（五十五）——系统托盘

> 上一篇文章我们聊了 Hook 机制——拦截系统级别的键盘和鼠标输入。Hook 是一种底层能力，用好了非常强大，但大部分时候你不会天天用它。今天我们要聊的东西则恰恰相反——几乎每个正经的桌面程序都会用到，但很多人不知道里面有多少细节：系统托盘（System Tray / Notification Area）。你的程序怎么最小化到托盘？怎么显示托盘图标和右键菜单？怎么弹出气泡通知？任务栏崩溃后怎么恢复图标？这些问题今天全部搞定。

---

## 为什么需要系统托盘

系统托盘（也叫通知区域）是 Windows 任务栏右下角的那个区域，里面放着时钟、音量、网络状态等图标。很多应用程序也会在这里放一个图标，常见的有：

* **后台常驻程序**：杀毒软件、输入法、云同步工具——它们不需要一直显示主窗口，但需要在后台运行，用户需要时可以通过托盘图标调出。
* **最小化到托盘**：下载管理器、音乐播放器——用户点击关闭按钮时不是退出程序，而是最小化到托盘继续工作。
* **状态通知**：邮件客户端收到新邮件时弹出气泡通知、系统更新时提示用户。

从技术角度说，系统托盘本质上就是一个图标 + 一个回调消息。系统不会帮你管理窗口、不会帮你创建菜单——所有这些都得你自己处理。这也就意味着有很多细节需要注意。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）
* **额外依赖**：Shell32.lib

---

## 第一步——Shell_NotifyIcon 与 NOTIFYICONDATA

系统托盘的所有操作都通过一个函数完成：`Shell_NotifyIcon`。

```cpp
BOOL Shell_NotifyIcon(
    DWORD dwMessage,           // 操作类型
    PNOTIFYICONDATA lpData     // 数据结构
);
```

### 四种操作

| dwMessage | 含义 |
|-----------|------|
| NIM_ADD | 添加托盘图标 |
| NIM_MODIFY | 修改托盘图标（更新图标、提示文字等） |
| NIM_DELETE | 删除托盘图标 |
| NIM_SETVERSION | 设置通知接口版本（推荐设置） |

### NOTIFYICONDATA 结构

这个结构体随着 Windows 版本演进变得越来越长。这里列出最常用的字段：

```cpp
// 使用最新的结构体版本
typedef struct {
    DWORD dwSize;              // 结构体大小 = sizeof(NOTIFYICONDATA)
    HWND  hWnd;                // 接收回调消息的窗口
    UINT  uID;                 // 图标 ID（一个窗口可以有多个托盘图标）
    UINT  uFlags;              // 指定哪些字段有效
    UINT  uCallbackMessage;    // 自定义回调消息 ID
    HICON hIcon;               // 图标句柄
    WCHAR szTip[128];          // 工具提示文字（鼠标悬停时显示）
    DWORD dwState;             // 图标状态
    DWORD dwStateMask;         // 状态掩码
    WCHAR szInfo[256];         // 气泡通知文字
    union {
        UINT uTimeout;         // 气泡超时时间（毫秒）
        UINT uVersion;         // 版本号（NIM_SETVERSION 时用）
    };
    WCHAR szInfoTitle[64];     // 气泡标题
    DWORD dwInfoFlags;         // 气泡图标类型
    GUID  guidItem;            // 图标 GUID（用于识别）
    HICON hBalloonIcon;        // 自定义气泡图标
} NOTIFYICONDATA;
```

### uFlags 常用标志

| 标志 | 含义 |
|------|------|
| NIF_MESSAGE | uCallbackMessage 有效 |
| NIF_ICON | hIcon 有效 |
| NIF_TIP | szTip 有效 |
| NIF_INFO | 气泡通知相关字段有效 |
| NIF_SHOWTIP | 显示工具提示（Win2000+ 要求显式启用） |

⚠️ 注意

**dwSize 必须正确**：用 `sizeof(NOTIFYICONDATA)` 初始化。如果大小不正确，`Shell_NotifyIcon` 会失败。不要手动填写这个值。

---

## 第二步——添加和删除托盘图标

### 添加托盘图标

```cpp
#define WM_TRAYICON (WM_APP + 100)
#define ID_TRAY_ICON 1

BOOL AddTrayIcon(HWND hwnd, HINSTANCE hInst)
{
    NOTIFYICONDATA nid = {};
    nid.dwSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);  // 使用系统默认图标
    wcscpy_s(nid.szTip, L"托盘示例程序");

    if (!Shell_NotifyIcon(NIM_ADD, &nid))
    {
        return FALSE;
    }

    // 设置通知版本为 NOTIFYICON_VERSION_4
    // 这样回调消息的 wParam/lParam 语义更清晰
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    return TRUE;
}
```

### 删除托盘图标

```cpp
void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.dwSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;

    Shell_NotifyIcon(NIM_DELETE, &nid);
}
```

⚠️ 注意

**程序退出前必须删除托盘图标**。如果你不调用 `Shell_NotifyIcon(NIM_DELETE, ...)`，图标会一直留在托盘里，直到用户把鼠标移上去——这时系统才会发现你的程序已经不在了，然后删除图标。这会显得很不专业。

---

## 第三步——处理托盘回调消息

当用户在托盘图标上操作时（单击、双击、右键等），系统会向你指定的窗口发送 `uCallbackMessage` 消息。

### NOTIFYICON_VERSION_4 的消息格式

如果设置了 `NOTIFYICON_VERSION_4`（推荐），回调消息的参数如下：

```cpp
case WM_TRAYICON:
{
    // wParam = 图标 ID（即 uID）
    // lParam = 鼠标事件或通知事件
    switch (LOWORD(lParam))
    {
    case WM_LBUTTONUP:
        // 左键单击——通常用来还原/显示主窗口
        break;

    case WM_RBUTTONUP:
        // 右键单击——通常用来显示弹出菜单
        break;

    case WM_LBUTTONDBLCLK:
        // 左键双击——通常用来还原/显示主窗口
        break;

    case NIN_BALLOONUSERCLICK:
        // 用户点击了气泡通知
        break;

    case NIN_BALLOONTIMEOUT:
        // 气泡通知超时消失
        break;

    case NIN_POPUPOPEN:
        // 鼠标悬停在图标上，弹出提示
        break;

    case NIN_SELECT:
        // 通知区域图标被选中（键盘导航）
        break;
    }
    return 0;
}
```

### 右键菜单

右键点击托盘图标时显示弹出菜单是最常见的交互模式。这里有几个坑需要注意：

```cpp
void ShowTrayContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"显示主窗口");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, L"设置...");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"退出");

    // 重要：必须设置前台窗口，否则菜单可能不会正确关闭
    SetForegroundWindow(hwnd);

    // 显示弹出菜单
    TrackPopupMenu(hMenu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, hwnd, NULL);

    // 重要：再次设置前台窗口，确保菜单能正确响应
    SetForegroundWindow(hwnd);

    DestroyMenu(hMenu);
}
```

⚠️ 注意

**SetForegroundWindow 的必要性**：如果不调用 `SetForegroundWindow`，弹出菜单可能不会在用户点击其他地方时自动关闭。这是 Windows 的一个已知行为——TrackPopupMenu 需要调用它的线程拥有前台焦点。在托盘图标上右键时，焦点在 Shell 上而不是你的程序上，所以需要先抢一下前台。

---

## 第四步——最小化到托盘

"关闭按钮不退出，而是最小化到托盘"是托盘程序最常见的行为模式。

### 核心思路

1. 用户点击关闭按钮时，拦截 WM_CLOSE
2. 隐藏主窗口（而不是销毁）
3. 添加托盘图标
4. 用户双击托盘图标时，显示主窗口并删除托盘图标

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static BOOL g_inTray = FALSE;

    switch (uMsg)
    {
    case WM_CLOSE:
        // 不销毁窗口，而是隐藏并最小化到托盘
        if (!g_inTray)
        {
            AddTrayIcon(hwnd, ((LPCREATESTRUCT)GetWindowLongPtr(hwnd, GWLP_WNDPROC))->hInstance);
            g_inTray = TRUE;
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;  // 不调用 DefWindowProc，阻止销毁

    case WM_TRAYICON:
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            // 还原窗口
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case WM_RBUTTONUP:
            ShowTrayContextMenu(hwnd);
            break;
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_RESTORE:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case IDM_EXIT:
            RemoveTrayIcon(hwnd);
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

---

## 第五步——气泡通知

气泡通知（Balloon Notification）是从托盘图标弹出的一个小气泡框，用于向用户显示提示信息。

### 显示气泡

```cpp
void ShowBalloonNotification(HWND hwnd, const wchar_t* title,
                              const wchar_t* text, DWORD infoFlags)
{
    NOTIFYICONDATA nid = {};
    nid.dwSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = infoFlags;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, text);

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// 使用示例
ShowBalloonNotification(hwnd,
    L"提示", L"操作已成功完成！", NIIF_INFO);

ShowBalloonNotification(hwnd,
    L"警告", L"磁盘空间不足", NIIF_WARNING);

ShowBalloonNotification(hwnd,
    L"错误", L"无法连接到服务器", NIIF_ERROR);
```

### dwInfoFlags 图标类型

| 值 | 图标 |
|---|------|
| NIIF_NONE | 无图标 |
| NIIF_INFO | 信息（蓝色 i） |
| NIIF_WARNING | 警告（黄色 !） |
| NIIF_ERROR | 错误（红色 X） |
| NIIF_USER | 使用 hBalloonIcon 字段的自定义图标 |

### 气泡 vs Toast

在 Windows 10/11 上，气泡通知可能会被系统转换为 Toast 通知（从屏幕右侧弹出）。这取决于用户的系统设置。你无法控制这个转换——如果你的目标是 Windows 10+，建议直接使用 Windows.UI.Notifications API（Toast 通知框架），它是更现代的通知方式。

---

## 第六步——WM_TASKBARCREATED：处理任务栏重启

这是一个很多人忽略但非常重要的细节。

Windows 的任务栏（explorer.exe）偶尔会崩溃并重启。当它重启时，所有托盘图标都会消失——但你的程序还在运行，用户再也看不到你的图标了。

解决方案：注册一个自定义消息 `WM_TASKBARCREATED`。当任务栏重启时，系统会向所有顶级窗口广播这个消息。你收到后重新添加托盘图标即可。

```cpp
// 在全局或静态变量中保存消息 ID
UINT g_uTaskbarRestart = 0;

// 在 WinMain 中注册
g_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");

// 在 WndProc 中处理
// 由于这是注册的消息，不能用 switch-case，必须用 if
if (uMsg == g_uTaskbarRestart)
{
    // 任务栏重启了，重新添加托盘图标
    if (g_inTray)
    {
        AddTrayIcon(hwnd, hInstance);
    }
    return 0;
}
```

注意：`RegisterWindowMessage` 每次调用返回同一个值（对于同一个字符串），所以可以在任何地方调用。

---

## 第七步——完整示例

这个示例把今天所有知识整合起来：一个窗口，关闭时最小化到托盘，有右键菜单，支持双击还原，处理任务栏重启。

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON  (WM_APP + 100)
#define ID_TRAY_ICON 1

// 菜单命令
#define IDM_RESTORE  2001
#define IDM_ABOUT    2002
#define IDM_EXIT     2003

// 全局变量
UINT  g_uTaskbarRestart = 0;
BOOL  g_inTray = FALSE;
HWND  g_hWnd = NULL;

// 前向声明
BOOL AddTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
void ShowTrayContextMenu(HWND hwnd);

BOOL AddTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.dwSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"托盘示例 - 双击还原窗口");

    if (!Shell_NotifyIcon(NIM_ADD, &nid))
        return FALSE;

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    return TRUE;
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.dwSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowTrayContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"还原窗口");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    SetForegroundWindow(hwnd);

    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 处理任务栏重启消息（不能用 switch）
    if (uMsg == g_uTaskbarRestart)
    {
        if (g_inTray)
            AddTrayIcon(hwnd);
        return 0;
    }

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建提示文本
        CreateWindowEx(0, L"STATIC",
            L"点击窗口关闭按钮最小化到系统托盘。\r\n\r\n"
            L"托盘图标右键菜单可还原或退出。\r\n"
            L"双击托盘图标还原窗口。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 340, 80,
            hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        return 0;
    }

    case WM_CLOSE:
        // 隐藏窗口并添加托盘图标
        if (!g_inTray)
        {
            AddTrayIcon(hwnd);
            g_inTray = TRUE;

            // 首次最小化时显示气泡提示
            NOTIFYICONDATA nid = {};
            nid.dwSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = ID_TRAY_ICON;
            nid.uFlags = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;
            wcscpy_s(nid.szInfoTitle, L"托盘示例");
            wcscpy_s(nid.szInfo, L"程序已最小化到系统托盘，双击图标可还原");
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_TRAYICON:
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case WM_RBUTTONUP:
            ShowTrayContextMenu(hwnd);
            break;
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_RESTORE:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon(hwnd);
            g_inTray = FALSE;
            break;

        case IDM_ABOUT:
            MessageBox(hwnd,
                L"系统托盘示例程序\r\n版本 1.0\r\n\r\n"
                L"演示 Shell_NotifyIcon 的用法",
                L"关于", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_EXIT:
            RemoveTrayIcon(hwnd);
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     PWSTR pCmdLine, int nCmdShow)
{
    g_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TrayDemoClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);

    g_hWnd = CreateWindowEx(
        0, L"TrayDemoClass", L"系统托盘示例",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 180,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWnd)
    {
        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

### 代码要点解析

1. **WM_CLOSE 拦截**：不调用 DefWindowProc（它会 DestroyWindow），而是隐藏窗口并添加托盘图标。首次最小化时还弹一个气泡通知。

2. **NOTIFYICON_VERSION_4**：设置后，回调消息的 lParam 低 16 位是鼠标消息，高 16 位是图标坐标（某些场景下）。推荐总是设置这个版本。

3. **SetForegroundWindow**：显示右键菜单前后各调用一次，确保菜单行为正常。

4. **WM_TASKBARCREATED**：用 RegisterWindowMessage 注册，在 WndProc 中用 if（不是 switch）检查。

5. **WM_DESTROY 兜底**：即使走了 IDM_EXIT 之外的路径（比如 Task Manager 强制关闭），WM_DESTROY 也会确保删除托盘图标。

---

## 常见陷阱

### 陷阱一：cbSize / dwSize 字段错误

NOTIFYICONDATA 结构体在不同 Windows SDK 版本中大小不同。必须用 `sizeof(NOTIFYICONDATA)` 初始化，不要手动填写数值。

### 陷阱二：程序退出未删除图标

在 WM_DESTROY 中一定要调用 `Shell_NotifyIcon(NIM_DELETE, ...)`。否则图标会留在托盘里，直到用户把鼠标移上去触发系统清理——很不专业。

### 陷阱三：hIcon 资源管理

`NOTIFYICONDATA.hIcon` 是共享引用——系统会复制图标，所以你可以在设置后安全地 `DestroyIcon`（如果你是自己 LoadImage 创建的）。但如果你传的是通过 `LoadIcon` 加载的系统图标（如 `IDI_APPLICATION`），不要 `DestroyIcon`——那些是系统资源。

### 陷阱四：64 位/32 位跨进程拖放

64 位程序的托盘图标右键菜单如果使用了 drag-drop 功能，需要额外的消息过滤处理。这是一个冷门但棘手的兼容性问题。

---

## 后续可以做什么

到这里，系统托盘的知识就讲完了。你现在应该能够添加/删除托盘图标、处理托盘回调消息（单击、双击、右键菜单）、显示气泡通知、正确处理任务栏重启、实现"最小化到托盘"的行为模式。

下一篇文章，我们会聊一个在文件操作场景中非常实用的功能——**拖放（Drag & Drop）**。你将学会如何让你的窗口接受文件拖入（WM_DROPFILES）以及如何实现完整的 OLE 拖放协议（IDropTarget）。

在此之前，建议你做一些练习巩固今天的知识：

1. **基础练习**：修改示例，给托盘图标使用自定义图标（从资源文件加载），而不是系统默认图标
2. **进阶练习**：实现一个"番茄钟"托盘程序——设置 25 分钟倒计时，时间到了弹出气泡通知，右键菜单可以暂停/重置/退出
3. **挑战练习**：让托盘图标动态变化（比如在"工作"和"休息"状态之间切换不同图标），并通过图标变化反映当前状态

---

## 相关资源

- [Shell_NotifyIcon function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw)
- [NOTIFYICONDATA structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw)
- [Taskbar Notifications - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area)
- [RegisterWindowMessage function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerwindowmessagew)
