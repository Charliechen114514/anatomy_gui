# 通用GUI编程技术——Win32 原生编程实战（十二）——菜单资源实战

## 前言：从代码创建菜单到资源定义

在上一篇文章中，我们学习了 Win32 资源文件的基础知识。现在我们要深入探讨一个特别实用的主题：菜单资源。为什么菜单资源值得单独拿出一篇来讲呢？因为菜单是 Windows 应用程序中最常见、最重要的 UI 元素之一，而使用资源定义菜单不仅能大大简化代码，还能让我们的程序更易于维护和本地化。

回想一下用代码创建菜单的日子。我们需要调用 `CreateMenu` 创建菜单栏，然后为每个下拉菜单再调用一次 `CreateMenu`，接着用一堆 `AppendMenu` 把菜单项一个个加进去。代码写起来像叠罗汉，维护起来更是痛苦。更糟糕的是，所有的菜单文本都硬编码在 C++ 源文件里，想要调整布局或者翻译成其他语言简直是场灾难。

资源文件给了我们一种更好的方式。我们可以用一种接近自然语言的语法来描述菜单结构，把 UI 定义从代码中彻底分离出来。你会发现，一旦习惯了这种方式，再也回不去那个纯代码创建菜单的旧时代了。

本文我们将系统地学习如何定义和使用菜单资源，包括弹出菜单、子菜单、快捷菜单的各种技巧。

## 环境说明

本文示例基于以下环境：
- Windows 11 Pro 10.0.26200
- Visual Studio 2022（MSVC 14.x）
- Windows SDK 10.0
- C++17 标准

## 第一步：MENU 资源的基本语法

让我们从最基本的菜单资源定义开始。一个菜单资源由 `MENU` 关键字声明，包含若干个 `POPUP` 和 `MENUITEM` 语句。

```rc
#include <windows.h>
#include "resource.h"

IDR_MAINMENU MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "&New", IDM_FILE_NEW
        MENUITEM "&Open", IDM_FILE_OPEN
        MENUITEM SEPARATOR
        MENUITEM "E&xit", IDM_FILE_EXIT
    END
    POPUP "&Edit"
    BEGIN
        MENUITEM "&Undo", IDM_EDIT_UNDO
        MENUITEM SEPARATOR
        MENUITEM "Cu&t", IDM_EDIT_CUT
        MENUITEM "&Copy", IDM_EDIT_COPY
        MENUITEM "&Paste", IDM_EDIT_PASTE
    END
    POPUP "&Help"
    BEGIN
        MENUITEM "&About", IDM_HELP_ABOUT
    END
END
```

这个结构看起来很直观。`IDR_MAINMENU` 是整个菜单资源的标识符，`MENU` 关键字告诉资源编译器这是一个菜单定义。`POPUP` 定义一个弹出菜单（即下拉菜单），`MENUITEM` 定义具体的菜单项。

关于菜单文本中的 `&` 符号：它指定了快捷键字符。当用户按下 Alt 键时，菜单栏上会出现带下划线的字母，用户可以通过按 Alt+那个字母来快速打开对应的菜单。同样，在下拉菜单中也可以直接按那个字母键来选择菜单项。

`SEPARATOR` 是一个特殊的菜单项，它不会响应用户操作，只是在菜单中画一条水平线，用于对菜单项进行逻辑分组。

### BEGIN/END vs 大括号

你可以用 `BEGIN` 和 `END` 来界定代码块，也可以像 C 语言那样用 `{` 和 `}`。两种写法完全等价，选择哪种纯粹是个人偏好。我个人更喜欢 `BEGIN/END`，因为这样在资源文件中一眼就能看出这不是 C++ 代码。

## 第二步：加载和使用菜单资源

定义好了菜单资源，接下来就是在代码中加载它。这比用代码创建菜单简单太多了：

```cpp
#include <windows.h>
#include "resource.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MenuResourceDemo";

    RegisterClassW(&wc);

    // 创建窗口（注意：这里还没有设置菜单）
    HWND hwnd = CreateWindowExW(
        0, L"MenuResourceDemo", L"Menu Resource Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 从资源加载菜单
        HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU));

        if (hMenu)
        {
            // 将菜单设置到窗口
            SetMenu(hwnd, hMenu);
        }
    }
    break;

    case WM_COMMAND:
    {
        // 菜单项的 ID 存储在 wParam 的低位
        switch (LOWORD(wParam))
        {
        case IDM_FILE_NEW:
            MessageBox(hwnd, L"Creating new file...", L"New", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_OPEN:
            MessageBox(hwnd, L"Opening file...", L"Open", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_EXIT:
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_EDIT_UNDO:
        case IDM_EDIT_CUT:
        case IDM_EDIT_COPY:
        case IDM_EDIT_PASTE:
            MessageBox(hwnd, L"Edit command", L"Edit", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_HELP_ABOUT:
            MessageBox(hwnd, L"Menu Resource Demo v1.0", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
```

`LoadMenu` 函数从模块资源中加载菜单。第一个参数是模块句柄，通常是我们程序的实例句柄。第二个参数是菜单资源的标识符，`MAKEINTRESOURCE` 宏将整数 ID 转换为资源查找所需的格式。

`SetMenu` 将菜单关联到窗口。一个有趣的事实是，一旦菜单被设置到窗口，它就由窗口管理了。当窗口销毁时，关联的菜单也会被自动销毁。所以这里不需要调用 `DestroyMenu`。

## 第三步：创建子菜单

子菜单（submenu）是嵌套在另一个菜单中的菜单。在资源文件中，子菜单就是一个嵌套的 `POPUP`：

```rc
IDR_MAINMENU MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "&New", IDM_FILE_NEW
        MENUITEM "&Open", IDM_FILE_OPEN
        MENUITEM SEPARATOR
        POPUP "&Recent Files"
        BEGIN
            MENUITEM "file1.txt", IDM_FILE_RECENT_1
            MENUITEM "file2.txt", IDM_FILE_RECENT_2
            MENUITEM "file3.txt", IDM_FILE_RECENT_3
        END
        MENUITEM SEPARATOR
        MENUITEM "E&xit", IDM_FILE_EXIT
    END
END
```

这里我们定义了一个「Recent Files」子菜单，它嵌套在「File」菜单中。用户打开「File」菜单后，会看到一个带右箭头的「Recent Files」项，鼠标悬停时会自动展开子菜单。

子菜单的嵌套可以有多层，不过从用户体验角度考虑，最好不要超过三层。太深的菜单结构会让用户迷失方向。

## 第四步：菜单项的状态和选项

菜单资源支持多种状态和显示选项，可以控制菜单项的外观和行为。

### CHECKED - 复选标记

```rc
MENUITEM "Show Status Bar", IDM_VIEW_STATUSBAR, CHECKED
```

这会在菜单项前面显示一个复选标记，表示该选项当前处于启用状态。在代码中可以用 `CheckMenuItem` 动态切换这个状态。

### GRAYED - 灰色显示

```rc
MENUITEM "Paste", IDM_EDIT_PASTE, GRAYED
```

菜单项会以灰色显示，表示当前不可用。代码中可以用 `EnableMenuItem` 动态启用或禁用菜单项。

### INACTIVE - 非活动

```rc
MENUITEM "Special Command", IDM_SPECIAL, INACTIVE
```

菜单项会正常显示，但不响应用户点击。

### MENUBARBREAK 和 MENUBREAK

这两个选项用于控制多列菜单布局：

```rc
MENUITEM "Item 1", IDM_ITEM_1
MENUITEM "Item 2", IDM_ITEM_2
MENUITEM "Item 3", IDM_ITEM_3, MENUBARBREAK
MENUITEM "Item 4", IDM_ITEM_4
```

`MENUBREAK` 会让后续菜单项在新的一列显示（对于菜单栏是在新的一行）。`MENUBARBREAK` 类似，但会在列之间画一条垂直分隔线。

## 第五步：快捷菜单（上下文菜单）

快捷菜单（也叫上下文菜单或右键菜单）是用户右键点击时弹出的菜单。实现快捷菜单有两种主要方式：从资源加载和动态创建。

### 从资源加载快捷菜单

这是一个常见的陷阱，所以我们要仔细讲解。很多人第一次尝试从资源加载快捷菜单时，会遇到菜单显示但文字不出来的问题。问题出在 `TrackPopupMenu` 需要的是一个弹出菜单（popup menu）的句柄，而不是顶层菜单的句柄。

正确的做法是在资源中定义一个包含 `POPUP` 的菜单：

```rc
IDR_CONTEXTMENU MENU
BEGIN
    POPUP "Context"
    BEGIN
        MENUITEM "&Copy", IDM_CTX_COPY
        MENUITEM "&Paste", IDM_CTX_PASTE
        MENUITEM SEPARATOR
        MENUITEM "&Delete", IDM_CTX_DELETE
    END
END
```

然后在代码中这样使用：

```cpp
case WM_CONTEXTMENU:
{
    // 获取鼠标位置
    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);

    // 如果是从键盘触发的（Shift+F10），坐标是 -1, -1
    if (xPos == -1 && yPos == -1)
    {
        // 获取当前窗口的矩形，在中心显示菜单
        RECT rect;
        GetClientRect(hwnd, &rect);
        xPos = rect.left + (rect.right - rect.left) / 2;
        yPos = rect.top + (rect.top - rect.bottom) / 2;
        ClientToScreen(hwnd, &(POINT){xPos, yPos});
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);

    // 加载菜单
    HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU));

    if (hMenu)
    {
        // 获取第一个（也是唯一一个）弹出菜单
        HMENU hSubMenu = GetSubMenu(hMenu, 0);

        if (hSubMenu)
        {
            // 设置前景窗口，确保菜单能正常接收输入
            SetForegroundWindow(hwnd);

            // 显示快捷菜单
            TrackPopupMenu(
                hSubMenu,
                TPM_LEFTBUTTON | TPM_RIGHTALIGN,  // 标志
                xPos, yPos,                        // 位置（屏幕坐标）
                0,                                 // 保留参数
                hwnd,                              // 拥有窗口
                NULL                               // 保留区域
            );

            // 发送一个空消息，防止菜单问题
            PostMessage(hwnd, WM_NULL, 0, 0);
        }

        // 销毁菜单（因为我们把它从窗口中分离了）
        DestroyMenu(hMenu);
    }
}
break;
```

### ⚠️ 注意

千万别直接用 `LoadMenu` 返回的菜单句柄调用 `TrackPopupMenu`，否则菜单会显示但文字不会出现。必须先用 `GetSubMenu` 获取第一个弹出菜单的句柄。

另外，显示快捷菜单前调用 `SetForegroundWindow`，显示后发送 `WM_NULL` 消息，这是为了确保菜单能正常工作，避免某些情况下菜单显示后立即消失的怪异行为。

### 动态创建快捷菜单

如果菜单内容需要根据上下文动态变化，我们可以用 `CreatePopupMenu` 在运行时创建：

```cpp
case WM_CONTEXTMENU:
{
    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);

    if (xPos == -1 && yPos == -1)
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        xPos = (rect.right - rect.left) / 2;
        yPos = (rect.top - rect.bottom) / 2;
        ClientToScreen(hwnd, &(POINT){xPos, yPos});
    }

    // 创建一个空的弹出菜单
    HMENU hMenu = CreatePopupMenu();

    if (hMenu)
    {
        // 动态添加菜单项
        AppendMenu(hMenu, MF_STRING, IDM_CTX_COPY, L"&Copy");
        AppendMenu(hMenu, MF_STRING, IDM_CTX_PASTE, L"&Paste");
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

        // 根据剪贴板状态决定是否启用粘贴
        BOOL canPaste = IsClipboardFormatAvailable(CF_TEXT);
        EnableMenuItem(hMenu, IDM_CTX_PASTE,
            canPaste ? MF_ENABLED : MF_GRAYED);

        AppendMenu(hMenu, MF_STRING, IDM_CTX_DELETE, L"&Delete");

        SetForegroundWindow(hwnd);

        TrackPopupMenu(
            hMenu,
            TPM_LEFTBUTTON | TPM_RIGHTALIGN,
            xPos, yPos,
            0,
            hwnd,
            NULL
        );

        PostMessage(hwnd, WM_NULL, 0, 0);

        // 销毁动态创建的菜单
        DestroyMenu(hMenu);
    }
}
break;
```

动态创建的优势是灵活性极高，你可以根据当前的程序状态来决定显示哪些菜单项、启用哪些选项。缺点是代码会变得比较复杂，维护成本也更高。

## 第六步：WM_RBUTTONUP vs WM_CONTEXTMENU

你可能会问，处理右键菜单是应该用 `WM_RBUTTONUP` 还是 `WM_CONTEXTMENU`？这两个消息确实都可以用，但 `WM_CONTEXTMENU` 是更现代、更推荐的方式。

`WM_RBUTTONUP` 只在用户释放鼠标右键时触发，而 `WM_CONTEXTMENU` 是一个更通用的消息，它可能在多种情况下触发：
- 用户右键点击
- 用户按下 Shift+F10
- 用户按下键盘上的应用键（那个带菜单图标的键）

从兼容性和可访问性的角度，应该优先处理 `WM_CONTEXTMENU`。如果你只需要支持鼠标右键，处理 `WM_RBUTTONUP` 也可以，但要记得手动把客户区坐标转换为屏幕坐标：

```cpp
case WM_RBUTTONUP:
{
    // lParam 包含客户区坐标
    int xPos = LOWORD(lParam);
    int yPos = HIWORD(lParam);

    // 转换为屏幕坐标
    POINT pt = {xPos, yPos};
    ClientToScreen(hwnd, &pt);

    // 显示快捷菜单
    HMENU hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_CONTEXTMENU));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);
    }
}
break;
```

## 第七步：完整示例代码

让我们创建一个完整的示例程序，展示菜单资源的各种用法。

**resource.h**
```c
#ifndef RESOURCE_H
#define RESOURCE_H

// 主菜单资源
#define IDR_MAINMENU                   100

// 文件菜单
#define IDM_FILE_NEW                   101
#define IDM_FILE_OPEN                  102
#define IDM_FILE_SAVE                  103
#define IDM_FILE_EXIT                  104

// 编辑菜单
#define IDM_EDIT_UNDO                  201
#define IDM_EDIT_CUT                   202
#define IDM_EDIT_COPY                  203
#define IDM_EDIT_PASTE                 204

// 视图菜单
#define IDM_VIEW_STATUSBAR             301
#define IDM_VIEW_TOOLBAR               302

// 帮助菜单
#define IDM_HELP_ABOUT                 401

// 快捷菜单
#define IDR_CONTEXTMENU                500
#define IDM_CTX_COPY                   501
#define IDM_CTX_PASTE                  502
#define IDM_CTX_DELETE                 503

// 字符串资源
#define IDS_APP_TITLE                  601
#define IDS_ABOUT_TEXT                 602

#endif // RESOURCE_H
```

**menu.rc**
```rc
#include <windows.h>
#include "resource.h"

IDR_MAINMENU MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "&New\tCtrl+N", IDM_FILE_NEW
        MENUITEM "&Open\tCtrl+O", IDM_FILE_OPEN
        MENUITEM "&Save\tCtrl+S", IDM_FILE_SAVE
        MENUITEM SEPARATOR
        MENUITEM "E&xit", IDM_FILE_EXIT
    END

    POPUP "&Edit"
    BEGIN
        MENUITEM "&Undo\tCtrl+Z", IDM_EDIT_UNDO
        MENUITEM SEPARATOR
        MENUITEM "Cu&t\tCtrl+X", IDM_EDIT_CUT
        MENUITEM "&Copy\tCtrl+C", IDM_EDIT_COPY
        MENUITEM "&Paste\tCtrl+V", IDM_EDIT_PASTE
    END

    POPUP "&View"
    BEGIN
        MENUITEM "Status Bar", IDM_VIEW_STATUSBAR, CHECKED
        MENUITEM "Tool Bar", IDM_VIEW_TOOLBAR
    END

    POPUP "&Help"
    BEGIN
        MENUITEM "&About", IDM_HELP_ABOUT
    END
END

IDR_CONTEXTMENU MENU
BEGIN
    POPUP "Dummy"
    BEGIN
        MENUITEM "&Copy", IDM_CTX_COPY
        MENUITEM "&Paste", IDM_CTX_PASTE
        MENUITEM SEPARATOR
        MENUITEM "&Delete", IDM_CTX_DELETE
    END
END

STRINGTABLE
BEGIN
    IDS_APP_TITLE "Menu Resource Demo"
    IDS_ABOUT_TEXT "Menu Resource Demo\n\nDemonstrates various menu resource features including:\n- Main menu from resource\n- Context menu\n- Dynamic menu item state management"
END
```

**main.cpp**
```cpp
#include <windows.h>
#include "resource.h"

// 全局变量用于存储菜单状态
static BOOL g_statusBarVisible = TRUE;
static BOOL g_toolBarVisible = FALSE;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    WCHAR szTitle[256];
    LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MenuResourceDemo";

    if (!RegisterClassW(&wc))
    {
        MessageBox(NULL, L"Failed to register window class", L"Error", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        0, L"MenuResourceDemo", szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 加载主菜单
        HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU));
        if (hMenu)
        {
            SetMenu(hwnd, hMenu);
        }
    }
    break;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_FILE_NEW:
            MessageBox(hwnd, L"New file command", L"File", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_OPEN:
            MessageBox(hwnd, L"Open file command", L"File", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_SAVE:
            MessageBox(hwnd, L"Save file command", L"File", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_EXIT:
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_EDIT_UNDO:
        case IDM_EDIT_CUT:
        case IDM_EDIT_COPY:
        case IDM_EDIT_PASTE:
            MessageBox(hwnd, L"Edit command", L"Edit", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_VIEW_STATUSBAR:
        {
            g_statusBarVisible = !g_statusBarVisible;
            HMENU hMenu = GetMenu(hwnd);
            CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR,
                g_statusBarVisible ? MF_CHECKED : MF_UNCHECKED);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case IDM_VIEW_TOOLBAR:
        {
            g_toolBarVisible = !g_toolBarVisible;
            HMENU hMenu = GetMenu(hwnd);
            CheckMenuItem(hMenu, IDM_VIEW_TOOLBAR,
                g_toolBarVisible ? MF_CHECKED : MF_UNCHECKED);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case IDM_HELP_ABOUT:
        {
            WCHAR szAboutText[512];
            LoadString(GetModuleHandle(NULL), IDS_ABOUT_TEXT,
                szAboutText, ARRAYSIZE(szAboutText));
            MessageBox(hwnd, szAboutText, L"About", MB_OK | MB_ICONINFORMATION);
        }
        break;

        case IDM_CTX_COPY:
            MessageBox(hwnd, L"Copy selected", L"Context Menu", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_CTX_PASTE:
            MessageBox(hwnd, L"Paste from clipboard", L"Context Menu", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_CTX_DELETE:
            MessageBox(hwnd, L"Delete selected", L"Context Menu", MB_OK | MB_ICONINFORMATION);
            break;
        }
    }
    break;

    case WM_CONTEXTMENU:
    {
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        // 处理键盘触发的情况
        if (xPos == -1 && yPos == -1)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            xPos = (rect.right - rect.left) / 2;
            yPos = (rect.top + rect.bottom) / 2;
            POINT pt = {xPos, yPos};
            ClientToScreen(hwnd, &pt);
            xPos = pt.x;
            yPos = pt.y;
        }

        HINSTANCE hInstance = GetModuleHandle(NULL);
        HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU));

        if (hMenu)
        {
            HMENU hSubMenu = GetSubMenu(hMenu, 0);
            if (hSubMenu)
            {
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON | TPM_RIGHTALIGN,
                    xPos, yPos, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0);
            }
            DestroyMenu(hMenu);
        }
    }
    return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // 显示当前视图状态
        WCHAR szStatus[256];
        wsprintf(szStatus,
            L"Status Bar: %s | Tool Bar: %s\n\nRight-click for context menu",
            g_statusBarVisible ? L"Visible" : L"Hidden",
            g_toolBarVisible ? L"Visible" : L"Hidden");

        DrawText(hdc, szStatus, -1, &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
```

### 编译和运行

使用 Visual Studio 构建项目，或者在命令行执行：

```cmd
cl.exe /EHsc /std:c++17 main.cpp /c /EHsc
rc.exe menu.rc
link.exe main.obj menu.res user32.lib gdi32.lib /out:menudemo.exe
```

运行程序后，你会看到一个完整的菜单栏，包括 File、Edit、View、Help 菜单。在 View 菜单中可以切换状态栏和工具栏的显示状态，菜单项前面的复选标记会相应更新。右键点击窗口会显示快捷菜单。

## 第八步：TrackPopupMenu 的显示位置计算

`TrackPopupMenu` 需要屏幕坐标，这在处理 `WM_CONTEXTMENU` 时很方便（因为它本身就是屏幕坐标），但处理 `WM_RBUTTONUP` 时需要转换坐标。

有几个常用的定位标志：

- `TPM_LEFTALIGN`：菜单左边缘对齐到 x 坐标（默认）
- `TPM_CENTERALIGN`：菜单居中于 x 坐标
- `TPM_RIGHTALIGN`：菜单右边缘对齐到 x 坐标
- `TPM_TOPALIGN`：菜单上边缘对齐到 y 坐标（默认）
- `TPM_VCENTERALIGN`：菜单垂直居中于 y 坐标
- `TPM_BOTTOMALIGN`：菜单下边缘对齐到 y 坐标
- `TPM_LEFTBUTTON`：只能用左键选择菜单项
- `TPM_RIGHTBUTTON`：左键和右键都可以选择菜单项

组合使用这些标志可以精确控制菜单的显示位置。例如，`TPM_RIGHTALIGN | TPM_BOTTOMALIGN` 会让菜单的右下角对齐到指定坐标点。

## 常见问题总结

### 菜单文字不显示

这是从资源加载快捷菜单时最常见的错误。记住：`LoadMenu` 返回的是顶层菜单句柄，必须用 `GetSubMenu` 获取实际的弹出菜单。

### 菜单显示后立即消失

确保在 `TrackPopupMenu` 之前调用 `SetForegroundWindow`，之后发送 `WM_NULL` 消息。这是 Windows 的一个历史遗留问题。

### 坐标不对

`WM_CONTEXTMENU` 的坐标是屏幕坐标，而 `WM_RBUTTONUP` 的坐标是客户区坐标。使用 `ClientToScreen` 进行转换。

## 后续可以做什么

掌握了菜单资源后，你可以继续探索：

1. 为菜单项添加图标，这需要使用 `owner-draw` 菜单或自定义绘制
2. 创建动态菜单，根据程序状态实时更新菜单内容
3. 实现菜单的键盘快捷键（加速键表）
4. 探索 `MENUEX` 扩展格式，它提供了更多的自定义选项
5. 学习对话框资源，掌握更复杂的 UI 设计

## 相关资源

- [Using Menus - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/using-menus)
- [MENU resource - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/menurc/menu-resource)
- [POPUP resource - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/menurc/popup-resource)
- [CreatePopupMenu - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-createpopupmenu)
- [TrackPopupMenu - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu)
- [WM_CONTEXTMENU message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu)
- [Windows API menus - ZetCode](https://zetcode.com/gui/winapi/menus/)
- [Win32 Tutorial - Resource file notes](https://winprog.org/tutorial/resnotes.html)
