# 通用GUI编程技术——Win32 原生编程实战（十一）——资源文件完全指南

## 前言：为什么要把 UI 从代码里分离出来

回想一下我们之前创建菜单的方式，是不是觉得有点别扭？一行又一行地调用 `CreateMenu`、`AppendMenu`，菜单文本散落在代码的各个角落。当我们想调整菜单布局、添加一个分隔符或者修改某个菜单项的文字时，不得不在一堆 C++ 代码里翻来翻去。这还不是最糟糕的，如果有一天老板说「我们要做一个英文版」，那可真是要把代码改得面目全非。

资源文件（Resource File）就是为了解决这个问题而生的。它让我们把 UI 定义从业务逻辑代码中分离出来，用一种专门的语言来描述界面元素。图标、菜单、对话框、字符串表、版本信息，这些都可以在 `.rc` 文件中集中管理。编译时，资源编译器 rc.exe 会把这些文本描述编译成二进制格式的 `.res` 文件，最终链接到我们的可执行文件中。

这种分离带来的好处是显而易见的：UI 设计师可以不碰一行 C++ 代码就调整界面；本地化团队只需要翻译资源文件而不必关心代码逻辑；我们的程序代码也变得更加简洁，专注于实现功能。

先别急，我们一步步来。

## 环境说明

本文示例基于以下环境：
- Windows 11 Pro 10.0.26200
- Visual Studio 2022（MSVC 14.x）
- Windows SDK 10.0
- C++17 标准

如果你使用的是 MinGW 或其他工具链，资源编译的命令会有所不同，但 `.rc` 文件的语法是一致的。

## 第一步：认识 .rc 文件的基本结构

资源文件是一个文本文件，通常使用 `.rc` 扩展名。它的语法类似 C 语言，支持预处理器指令、注释和宏定义。一个最简单的资源文件可能长这样：

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
    POPUP "&Help"
    BEGIN
        MENUITEM "&About", IDM_HELP_ABOUT
    END
END
```

这里有几个关键点需要说明。`MENU` 关键字声明这是一个菜单资源，`IDR_MAINMENU` 是这个资源的唯一标识符。`BEGIN` 和 `END`（也可以用 `{` 和 `}`）界定资源定义的范围。`POPUP` 定义一个可以包含其他菜单项的弹出菜单，`MENUITEM` 定义具体的菜单命令。`SEPARATOR` 是一个特殊的分隔符，会在菜单中画一条水平线。

你可能注意到有些字符串前面有个 `&` 符号，这表示它后面的字符是该菜单项的快捷键。在菜单显示时，这个字符会被加上下划线，用户可以通过 Alt+那个字符来快速访问菜单项。

### 资源头文件 resource.h

资源标识符通常定义在一个单独的头文件中，约定俗成地叫做 `resource.h`：

```c
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDR_MAINMENU                   100
#define IDM_FILE_NEW                   101
#define IDM_FILE_OPEN                  102
#define IDM_FILE_EXIT                  103
#define IDM_HELP_ABOUT                 104

#endif // RESOURCE_H
```

这个头文件既被 `.rc` 文件包含，也被我们的 C++ 代码包含，这样两边就能使用相同的标识符来引用资源。通过这种约定，我们避免了硬编码数字，让代码更具可读性。

## 第二步：资源编译器 rc.exe 的工作原理

当我们在 Visual Studio 中构建项目时，IDE 会自动调用资源编译器。如果我们手动从命令行编译，命令大概是这样的：

```cmd
rc.exe /fo app.res app.rc
```

`/fo` 选项指定输出文件名。如果成功执行，会生成一个 `app.res` 文件，这是编译后的二进制资源文件。然后链接器会把 `.res` 文件的内容合并到最终的 EXE 或 DLL 中。

rc.exe 有不少命令行选项值得了解：

- `/v`：显示详细输出，编译时会打印正在处理的每个资源
- `/n`：所有字符串表中的字符串都以 null 结尾
- `/l`：指定资源语言的 ID，如 `/l 0x409` 代表英语（美国）
- `/d`：定义预处理器宏，类似于编译器的 `/D` 选项
- `/i`：添加头文件搜索路径
- `/x`：忽略 INCLUDE 环境变量

比如，如果我们要在编译时定义一个 `_DEBUG` 宏，可以这样做：

```cmd
rc.exe /d _DEBUG /v app.rc
```

### ⚠️ 注意

千万别在同一个项目中使用不同版本的 rc.exe 编译资源，否则可能导致资源 ID 冲突或加载失败。Visual Studio 通常会自动使用正确版本的 rc.exe，但如果你有自定义的构建脚本，就要确保 rc.exe 的版本与项目设置一致。

## 第三步：在代码中加载资源

编译好资源之后，我们需要在程序代码中加载它们。对于菜单资源，使用 `LoadMenu` 函数：

```cpp
#include <windows.h>
#include "resource.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // 从资源加载菜单
        HMENU hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINMENU));
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
            // 处理新建命令
            MessageBox(hwnd, L"New file", L"Menu Demo", MB_OK);
            break;

        case IDM_FILE_OPEN:
            // 处理打开命令
            MessageBox(hwnd, L"Open file", L"Menu Demo", MB_OK);
            break;

        case IDM_FILE_EXIT:
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_HELP_ABOUT:
            MessageBox(hwnd, L"Resource File Demo v1.0", L"About", MB_OK);
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

`LoadMenu` 的第一个参数是包含资源的模块句柄，通常用 `GetModuleHandle(NULL)` 获取当前程序的句柄。第二个参数是资源标识符，`MAKEINTRESOURCE` 宏把一个整数转换为资源查找所需的字符串指针类型。

调用 `SetMenu` 后，窗口就会显示我们定义的菜单。`SetMenu` 会把菜单关联到窗口，之后我们就不需要手动销毁这个菜单了——窗口销毁时会自动处理。

## 第四步：更多类型的资源

### 图标资源

图标是应用程序的"门面"，定义图标资源也很简单：

```rc
// app.rc
IDI_APP ICON "app.ico"

// resource.h
#define IDI_APP    201
```

在注册窗口类时引用图标：

```cpp
WNDCLASS wc = {0};
wc.hInstance = GetModuleHandle(NULL);
wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP));
wc.hCursor = LoadCursor(NULL, IDC_ARROW);
// ... 其他设置
```

### 字符串资源

把用户可见的字符串放在资源表中是个好习惯，这样便于本地化：

```rc
STRINGTABLE
BEGIN
    IDS_APP_NAME       "My Application"
    IDS_FILE_FILTER    "All Files (*.*)|*.*|Text Files (*.txt)|*.txt"
    IDS_ERROR_OPEN     "Failed to open file"
END
```

在代码中加载字符串：

```cpp
WCHAR szAppName[256];
LoadString(GetModuleHandle(NULL), IDS_APP_NAME, szAppName, sizeof(szAppName) / sizeof(WCHAR));
```

### 版本信息资源

Windows 资源管理器会显示可执行文件的版本信息，这同样通过资源定义：

```rc
1 VERSIONINFO
FILEVERSION 1,0,0,1
PRODUCTVERSION 1,0,0,1
FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
FILEFLAGS 0x1L
#else
FILEFLAGS 0x0L
#endif
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "My Company"
            VALUE "FileDescription", "My Application"
            VALUE "FileVersion", "1.0.0.1"
            VALUE "InternalName", "MyApp"
            VALUE "LegalCopyright", "Copyright (C) 2024"
            VALUE "OriginalFilename", "MyApp.exe"
            VALUE "ProductName", "My Product"
            VALUE "ProductVersion", "1.0.0.1"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```

这个资源定义会让你的程序在属性面板中显示完整的版本信息，看起来更专业。

## 第五步：资源 ID 的命名约定

微软有一套推荐的资源 ID 命名约定，遵循这些约定可以让代码更容易理解：

| 前缀 | 用途 | 示例 |
|------|------|------|
| `IDR_` | 多种资源类型（菜单、快捷键等共用） | `IDR_MAINMENU` |
| `IDD_` | 对话框模板 | `IDD_ABOUTBOX` |
| `IDI_` | 图标 | `IDI_APPICON` |
| `IDB_` | 位图 | `IDB_LOGO` |
| `IDC_` | 光标或控件 | `IDC_ARROW`，`IDC_EDIT` |
| `IDS_` | 字符串资源 | `IDS_APPNAME` |
| `IDM_` | 菜单项命令 | `IDM_FILE_NEW` |
| `IDP_` | 提示字符串（用于消息框） | `IDP_ERROR_OPEN` |

在数字范围上也有一些约定：
- 1-0x6FFF：标准资源（菜单、对话框、图标、位图、光标）
- 0x7000-0x7FFF：MFC 内部使用
- 0x8000-0xDFFF：命令和控件 ID
- 0xE000-0xEFFF：MFC 内部使用
- 0xF000-0xFFFF：Windows 系统命令

我们从 100 或 101 开始分配自己的 ID，可以避免与系统预定义的冲突。

## 第六步：在 Visual Studio 中配置资源编译

如果你使用 Visual Studio IDE，大部分工作都会自动完成。创建一个新的 C++ 桌面项目时，VS 会自动添加一个资源文件。不过，了解如何手动配置还是有帮助的。

### 项目配置

在项目属性页中：
1. 展开「资源配置文件」节点
2. 「常规」页可以设置输出文件名、预处理器定义等
3. 「命令行」页可以看到实际的 rc.exe 命令，也可以添加额外的选项

确保你的 `.rc` 文件被添加到项目中，并且「项目类型」设置为「自定义生成工具」或由 Visual Studio 自动识别。

### 资源视图

Visual Studio 提供了一个可视化的资源编辑器。在「资源视图」中双击一个资源，会打开对应的编辑器。对于菜单资源，这是一个所见即所得的设计器；对于图标资源，是一个简单的绘图工具。

不过说实话，对于复杂的资源，很多人还是倾向于直接编辑 `.rc` 文件文本，因为这样更精确，也更容易进行版本控制对比。

## 第七步：一个完整的可运行示例

让我们把所有内容整合起来，创建一个完整的示例程序。

**resource.h**
```c
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDR_MAINMENU                   100
#define IDM_FILE_NEW                   101
#define IDM_FILE_OPEN                  102
#define IDM_FILE_EXIT                  103
#define IDM_HELP_ABOUT                 104

#define IDI_APPICON                    201
#define IDS_APP_TITLE                  301

#endif // RESOURCE_H
```

**app.rc**
```rc
#include <windows.h>
#include "resource.h"

IDR_MAINMENU MENU
BEGIN
    POPUP "&File"
    BEGIN
        MENUITEM "&New\tCtrl+N", IDM_FILE_NEW
        MENUITEM "&Open\tCtrl+O", IDM_FILE_OPEN
        MENUITEM SEPARATOR
        MENUITEM "E&xit", IDM_FILE_EXIT
    END
    POPUP "&Help"
    BEGIN
        MENUITEM "&About", IDM_HELP_ABOUT
    END
END

IDI_APPICON ICON "app.ico"

STRINGTABLE
BEGIN
    IDS_APP_TITLE "Resource File Demo"
END
```

**main.cpp**
```cpp
#include <windows.h>
#include "resource.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // 获取应用程序标题
    WCHAR szTitle[256];
    LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ResourceDemoClass";

    if (!RegisterClassW(&wc))
    {
        MessageBox(NULL, L"Failed to register window class", szTitle, MB_ICONERROR);
        return 0;
    }

    // 创建窗口
    HWND hwnd = CreateWindowExW(
        0, L"ResourceDemoClass", szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        MessageBox(NULL, L"Failed to create window", szTitle, MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
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
            MessageBox(hwnd, L"Creating new file...", L"New", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_OPEN:
            MessageBox(hwnd, L"Opening file...", L"Open", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_FILE_EXIT:
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_HELP_ABOUT:
        {
            WCHAR szText[512];
            wsprintf(szText, L"Resource File Demo\n\nDemonstrates using .rc files for UI resources.");
            MessageBox(hwnd, szText, L"About", MB_OK | MB_ICONINFORMATION);
        }
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

### 编译和运行

如果你使用 Visual Studio，只需要把这三个文件添加到项目，直接构建即可。从命令行编译的话：

```cmd
cl.exe /EHsc /std:c++17 main.cpp /link user32.lib gdi32.lib
rc.exe app.rc
link.exe /out:app.exe main.obj app.res user32.lib gdi32.lib
```

运行程序后，你会看到一个带有菜单的窗口。菜单项完全由资源文件定义，代码中没有硬编码任何菜单文本。

## 常见问题和解决方案

### 资源找不到

如果 `LoadMenu` 返回 NULL，最可能的原因是资源没有被正确链接到可执行文件。用资源查看工具（如 Visual Studio 的「文件 → 打开 → 文件」打开 .exe，选择「以二进制方式打开」）检查一下 EXE 中是否包含预期的资源。

### 资源 ID 冲突

当多个资源文件被包含到一个项目中时，可能会发生 ID 冲突。确保你的 resource.h 被所有相关文件正确包含，并且使用了 `#ifndef` 保护宏。

### Unicode vs ANSI

如果你在代码中使用了 `LoadMenuW`（Unicode 版本），但资源文件中定义的是 ANSI 字符串，可能会出现乱码。现代 Windows 编程建议全面使用 Unicode，资源文件中的字符串会根据编译设置自动转换。

## 后续可以做什么

现在你已经掌握了 Win32 资源文件的基础，可以尝试以下练习来巩固知识：

1. 为你的程序添加一个自定义图标，并在任务栏、标题栏、关于对话框中都使用这个图标
2. 创建一个包含多级子菜单的复杂菜单结构
3. 把所有用户可见的字符串移到 STRINGTABLE 中，然后创建一个中文版本的资源
4. 使用 `#ifdef` 指令在资源文件中创建调试版本和发布版本的不同菜单
5. 探索对话框资源，尝试用资源定义一个简单的对话框

## 相关资源

- [Resource Compiler - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/resource-compiler)
- [About Resource Files - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/about-resource-files)
- [Using RC (The RC Command Line) - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/using-rc-the-rc-command-line-)
- [RC Task - MSBuild](https://learn.microsoft.com/zh-cn/visualstudio/msbuild/rc-task)
- [Win32 Tutorial - Resource file notes](https://winprog.org/tutorial/resnotes.html)
- [TN020: ID Naming and Numbering Conventions](https://learn.microsoft.com/en-us/cpp/mfc/tn020-id-naming-and-numbering-conventions)
