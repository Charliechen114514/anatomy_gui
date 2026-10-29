# 通用GUI编程技术——Win32 原生编程实战（九）——非模态对话框详解

## 前言：为什么需要非模态对话框

还记得我们之前讲过的模态对话框吗？那时候我们用 `DialogBox` 函数弹出一个对话框，整个程序的主窗口就被冻结了，用户必须先处理完这个对话框才能继续操作主窗口。这在某些场景下确实是合理的设计——比如一个"确定要删除文件吗？"的警告框，你确实希望用户专注地做出选择。

但现实中的需求往往没有这么简单。想象一下你在用 Photoshop 或者 VS Code，你想一边调整图层面板上的参数，一边在主窗口里实时看到效果。如果每次调整参数都要弹出一个模态对话框，让你点完"确定"才能继续，那体验简直是灾难。

这就是非模态对话框存在的意义——它允许对话框和主窗口同时响应用户的操作，让两个窗口能够协同工作。工具面板、属性窗口、查找替换框这些 UI 元素，本质上都是非模态对话框的典型应用场景。

我们现在要做的是深入理解 Win32 中非模态对话框的实现机制，特别是那个让很多人初学者困惑的消息循环集成问题。

## 环境说明

- 平台：Windows 10/11
- 开发工具：Visual Studio 2022 或 MinGW-w64
- 编程语言：C/C++
- Win32 API 版本：Windows 2000 Professional 及以上支持的 API

## 第一步：认识 CreateDialog 函数

在模态对话框的世界里，我们用的是 `DialogBox` 函数。这个函数有个特点：它会创建对话框，然后进入自己的消息循环，直到对话框关闭才会返回。这就是"模态"的本质——函数调用被阻塞了。

非模态对话框使用的是 `CreateDialog` 系列函数，它的行为更像 `CreateWindowEx`。让我们看看官方文档对它的定义：

> CreateDialogParam 函数从对话框模板资源创建无模式对话框。在显示对话框之前，该函数会将应用程序定义的值作为 WM_INITDIALOG 消息的 lParam 参数传递给对话框过程。

函数原型是这样的：

```cpp
HWND CreateDialogParamA(
  [in, optional] HINSTANCE hInstance,
  [in]           LPCSTR    lpTemplateName,
  [in, optional] HWND      hWndParent,
  [in, optional] DLGPROC   lpDialogFunc,
  [in]           LPARAM    dwInitParam
);
```

关键点在于：这个函数**立即返回**，返回值是对话框的窗口句柄（HWND）。这意味着对话框创建完成后，控制权马上回到你的代码，你需要自己负责消息的泵取和处理。

这里有个重要的区别需要理解：

| 特性 | DialogBox（模态） | CreateDialog（非模态） |
|------|-------------------|------------------------|
| 返回时机 | 对话框关闭时返回 | 立即返回窗口句柄 |
| 消息循环 | 系统内部提供 | 需要集成到主消息循环 |
| 阻塞性 | 阻塞父窗口 | 不阻塞父窗口 |
| 销毁方式 | EndDialog | DestroyWindow |

## 第二步：创建第一个非模态对话框

先别急着写复杂的功能，我们从最简单的例子开始。下面的代码展示了如何在主窗口创建时同时创建一个非模态工具面板。

首先我们需要一个全局变量来保存对话框的窗口句柄：

```cpp
// 全局变量：保存工具面板的窗口句柄
HWND g_hToolPanel = NULL;
```

为什么需要全局变量？因为 `DialogBox` 在关闭对话框后才返回，而 `CreateDialog` 立即返回句柄。这个句柄是我们后续与对话框交互的唯一桥梁，必须妥善保管。

接下来在主窗口的 `WM_CREATE` 消息处理中创建对话框：

```cpp
case WM_CREATE:
{
    // 创建非模态对话框
    g_hToolPanel = CreateDialog(
        GetModuleHandle(NULL),           // 应用程序实例句柄
        MAKEINTRESOURCE(IDD_TOOLPANEL),  // 对话框资源ID
        hwnd,                            // 父窗口句柄
        ToolPanelProc                    // 对话框过程
    );

    if (g_hToolPanel == NULL)
    {
        MessageBox(hwnd, L"创建工具面板失败", L"错误",
                   MB_OK | MB_ICONERROR);
        return -1;  // 创建失败，终止窗口创建
    }

    // CreateDialog 不会自动显示窗口，需要手动调用
    ShowWindow(g_hToolPanel, SW_SHOW);
}
break;
```

这里有几个新手容易犯的错误。第一，`CreateDialog` 不会自动显示窗口，即使你在资源模板里设置了 `WS_VISIBLE` 样式，最好也显式调用一下 `ShowWindow`。第二，别忘了检查返回值，如果创建失败了，句柄会是 NULL，后续所有操作都会出问题。

对话框过程和模态对话框的基本相同：

```cpp
INT_PTR CALLBACK ToolPanelProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        // 初始化对话框控件
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_APPLY:
            // 处理应用按钮
            MessageBox(hDlg, L"应用更改", L"提示", MB_OK);
            return TRUE;

        case IDC_CLOSE:
            // 关闭对话框（不是 EndDialog！）
            DestroyWindow(hDlg);
            g_hToolPanel = NULL;  // 清空句柄
            return TRUE;
        }
        break;
    }
    return FALSE;
}
```

到这里，如果你运行程序，应该能看到主窗口和工具面板同时显示出来了。你可以在两个窗口之间切换，它们都能响应鼠标操作。但是先别高兴得太早——你会发现键盘导航完全不起作用。按 Tab 键无法在控件间切换，快捷键也没反应。

这就是接下来要解决的核心问题。

## 第三步：消息循环集成——IsDialogMessage 的关键作用

为什么会这样？还记得我们说过 `DialogBox` 内部有自己的消息循环吗？那个内置的消息循环会自动处理对话框特有的键盘消息，比如 Tab 导航、方向键选择、默认按钮等。

`CreateDialog` 就没这么贴心了——它只是创建窗口，然后就把消息处理的责任完全甩给了你。如果你不特别处理对话框消息，那些对话框特有的键盘行为就都不会工作。

解决方案就是使用 `IsDialogMessage` 函数。根据官方文档：

> IsDialogMessage 函数确定消息是否适用于指定的对话框，如果是，则处理该消息。

这个函数会在消息循环中检查每条消息，如果消息是发给对话框的键盘消息，它就会进行相应的处理并返回 TRUE。

让我们修改消息循环：

```cpp
// 原始的消息循环
while (GetMessage(&msg, NULL, 0, 0))
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

// 修改后的消息循环
while (GetMessage(&msg, NULL, 0, 0))
{
    // 先检查消息是否属于对话框
    if (!IsDialogMessage(g_hToolPanel, &msg))
    {
        // 如果不是对话框消息，按常规方式处理
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

这个逻辑很简单但很重要：先让 `IsDialogMessage` 检查消息，如果它返回 TRUE，说明消息已经被处理了，就不再调用 `TranslateMessage` 和 `DispatchMessage`。如果返回 FALSE，说明消息不是给对话框的，或者不是对话框需要特殊处理的消息，就按常规流程处理。

⚠️ 注意
千万别在 IsDialogMessage 返回 TRUE 之后再调用 TranslateMessage/DispatchMessage，否则消息会被处理两次，导致奇怪的行为。

现在再运行程序，Tab 导航和快捷键应该都能正常工作了。

## 第四步：处理多个非模态对话框

在实际应用中，你可能需要同时显示多个非模态对话框。比如一个绘图程序可能有工具面板、图层面板、颜色面板等多个浮动窗口。

这种情况下，消息循环需要遍历所有对话框句柄：

```cpp
// 保存多个对话框句柄的数组
HWND g_hDialogs[3] = { NULL, NULL, NULL };

// 修改后的消息循环
while (GetMessage(&msg, NULL, 0, 0))
{
    BOOL bHandled = FALSE;

    // 遍历所有对话框，检查消息是否属于其中任何一个
    for (int i = 0; i < 3; i++)
    {
        if (g_hDialogs[i] != NULL && IsDialogMessage(g_hDialogs[i], &msg))
        {
            bHandled = TRUE;
            break;
        }
    }

    // 如果没有对话框处理这条消息，按常规方式处理
    if (!bHandled)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

这种方式的效率在对话框数量不多时是完全可以接受的。如果你有大量的对话框（几十个以上），可能需要考虑更高效的数据结构，但那种情况在实际应用中很少见。

## 第五步：非模态对话框的销毁

关于对话框的销毁，这是另一个容易混淆的地方。模态对话框用 `EndDialog` 关闭，非模态对话框则必须用 `DestroyWindow`。

这两个函数的本质区别在于：
- `EndDialog` 只是让对话框退出消息循环，窗口实际上是由系统销毁的
- `DestroyWindow` 直接销毁窗口，适用于任何窗口（包括对话框）

下面是正确关闭非模态对话框的代码：

```cpp
case IDCANCEL:  // 用户点击取消或关闭按钮
case IDCLOSE:
    DestroyWindow(hDlg);  // 销毁窗口
    g_hToolPanel = NULL;  // 清空全局句柄
    return TRUE;
```

注意这里我们把全局变量设为 NULL，这是个好习惯。这可以让其他代码通过检查 `g_hToolPanel == NULL` 来判断对话框是否还存在。

如果你想在主窗口关闭时自动关闭所有非模态对话框：

```cpp
case WM_DESTROY:
{
    // 关闭所有可能打开的非模态对话框
    if (g_hToolPanel != NULL)
    {
        DestroyWindow(g_hToolPanel);
        g_hToolPanel = NULL;
    }

    // 如果有其他对话框，也在这里关闭...

    PostQuitMessage(0);
}
break;
```

## 第六步：主窗口和对话框之间的通信

实际应用中，主窗口和对话框往往需要相互通信。比如对话框中的设置改变后，需要通知主窗口重新绘制；或者主窗口状态改变时，需要更新对话框中的控件显示。

有几种常见的通信方式：

### 方法一：使用 SendMessage/PostMessage

对话框可以向主窗口发送自定义消息：

```cpp
// 定义自定义消息
#define WM_SETTINGS_CHANGED (WM_USER + 100)

// 在对话框中，当设置改变时
case IDC_APPLY:
{
    // 通知主窗口设置已改变
    HWND hParent = GetParent(hDlg);
    if (hParent)
    {
        SendMessage(hParent, WM_SETTINGS_CHANGED, 0, 0);
    }
    return TRUE;
}
```

主窗口处理这个消息：

```cpp
case WM_SETTINGS_CHANGED:
{
    // 根据新的设置更新主窗口
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}
```

### 方法二：使用 Get/SetWindowLongPtr 保存数据

可以在对话框窗口的额外字节中保存数据指针：

```cpp
// 定义一个设置结构
typedef struct {
    COLORREF color;
    int lineWidth;
} Settings;

// 创建对话框时传入数据
Settings* pSettings = new Settings{ RGB(255, 0, 0), 2 };
g_hToolPanel = CreateDialogParam(
    GetModuleHandle(NULL),
    MAKEINTRESOURCE(IDD_TOOLPANEL),
    hwnd,
    ToolPanelProc,
    (LPARAM)pSettings  // 通过 lParam 传入
);

// 在 WM_INITDIALOG 中保存
case WM_INITDIALOG:
{
    Settings* pSettings = (Settings*)lParam;
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pSettings);

    // 初始化控件状态...
    return TRUE;
}

// 使用时获取
Settings* pSettings = (Settings*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
if (pSettings)
{
    // 使用设置...
}
```

### 方法三：直接通过句柄操作

最直接的方式就是通过保存的窗口句柄直接操作：

```cpp
// 在主窗口中更新对话框控件
void UpdateToolPanelControls()
{
    if (g_hToolPanel != NULL)
    {
        HWND hColorEdit = GetDlgItem(g_hToolPanel, IDC_COLOR_EDIT);
        SetWindowText(hColorEdit, L"#FF0000");
    }
}

// 在对话框中获取主窗口状态
void SyncWithMainWindow(HWND hDlg)
{
    HWND hParent = GetParent(hDlg);
    if (hParent)
    {
        // 从主窗口获取某些信息...
    }
}
```

## 完整示例：绘图程序与工具面板

现在让我们把所有内容整合到一个完整的示例中。这是一个简单的绘图程序，主窗口用于绘图，非模态对话框作为工具面板控制画笔颜色和线宽。

完整的资源文件：

```rc
// resource.rc
#include <windows.h>
#include "resource.h"

IDD_TOOLPANEL DIALOGEX 0, 0, 200, 120
STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION
EXSTYLE WS_EX_TOOLWINDOW
CAPTION "工具面板"
FONT 9, "Microsoft YaHei UI"
BEGIN
    LTEXT           "画笔颜色:", IDC_STATIC, 10, 10, 60, 10
    COMBOBOX        IDC_COLOR_COMBO, 10, 25, 100, 80, CBS_DROPDOWNLIST

    LTEXT           "线宽:", IDC_STATIC, 10, 50, 60, 10
    TRACKBAR        IDC_WIDTH_TRACK, 10, 65, 100, 15
    LTEXT           "1", IDC_WIDTH_LABEL, 115, 68, 20, 10

    PUSHBUTTON      "应用", IDC_APPLY, 10, 90, 50, 20
    PUSHBUTTON      "关闭", IDCANCEL, 70, 90, 50, 20
END

IDR_MENU MENU
BEGIN
    POPUP "窗口"
    BEGIN
        MENUITEM "显示工具面板", IDM_SHOW_TOOLPANEL
        MENUITEM "隐藏工具面板", IDM_HIDE_TOOLPANEL
    END
END
```

完整的头文件：

```cpp
// resource.h
#define IDD_TOOLPANEL           100
#define IDC_COLOR_COMBO         101
#define IDC_WIDTH_TRACK         102
#define IDC_WIDTH_LABEL         103
#define IDC_APPLY               104
#define IDC_STATIC              -1
#define IDR_MENU                200
#define IDM_SHOW_TOOLPANEL      201
#define IDM_HIDE_TOOLPANEL      202
```

完整的源代码：

```cpp
#include <windows.h>
#include <commctrl.h>
#include "resource.h"

// 全局变量
HWND g_hToolPanel = NULL;
COLORREF g_currentColor = RGB(0, 0, 0);
int g_currentWidth = 1;

// 自定义消息
#define WM_SETTINGS_CHANGED (WM_USER + 100)

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ToolPanelProc(HWND, UINT, WPARAM, LPARAM);
void RegisterMainWindowClass(HINSTANCE);
HWND CreateMainWindow(HINSTANCE);
void InitializeControls(HWND);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // 初始化通用控件
    INITCOMMONCONTROLSEX icc = { sizeof(icc) };
    icc.iccWin95 = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    // 注册窗口类
    RegisterMainWindowClass(hInstance);

    // 创建主窗口
    HWND hWnd = CreateMainWindow(hInstance);
    if (!hWnd)
        return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // 关键：先检查消息是否属于对话框
        if (g_hToolPanel == NULL || !IsDialogMessage(g_hToolPanel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

void RegisterMainWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
    wc.lpszClassName = L"MainWindowClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);
}

HWND CreateMainWindow(HINSTANCE hInstance)
{
    return CreateWindow(
        L"MainWindowClass",
        L"绘图程序 - 非模态对话框示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInstance, NULL
    );
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建工具面板
        g_hToolPanel = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_TOOLPANEL),
            hWnd,
            ToolPanelProc
        );

        if (g_hToolPanel)
        {
            ShowWindow(g_hToolPanel, SW_SHOW);
        }

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 创建画笔和画刷
        HPEN hPen = CreatePen(PS_SOLID, g_currentWidth, g_currentColor);
        HGDIOBJ hOldPen = SelectObject(hdc, hPen);

        // 绘制一些示例图形
        Rectangle(hdc, 100, 100, 300, 200);
        Ellipse(hdc, 350, 100, 550, 200);

        // 绘制线宽预览
        HPEN hPreviewPen = CreatePen(PS_SOLID, g_currentWidth, g_currentColor);
        SelectObject(hdc, hPreviewPen);
        MoveToEx(hdc, 100, 300, NULL);
        LineTo(hdc, 550, 300);

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
        DeleteObject(hPreviewPen);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SETTINGS_CHANGED:
    {
        // 设置改变后重绘主窗口
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_SHOW_TOOLPANEL:
            if (g_hToolPanel && !IsWindowVisible(g_hToolPanel))
            {
                ShowWindow(g_hToolPanel, SW_SHOW);
            }
            break;

        case IDM_HIDE_TOOLPANEL:
            if (g_hToolPanel && IsWindowVisible(g_hToolPanel))
            {
                ShowWindow(g_hToolPanel, SW_HIDE);
            }
            break;
        }
        return 0;
    }

    case WM_DESTROY:
    {
        // 关闭工具面板
        if (g_hToolPanel)
        {
            DestroyWindow(g_hToolPanel);
            g_hToolPanel = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

INT_PTR CALLBACK ToolPanelProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // 初始化颜色组合框
        HWND hCombo = GetDlgItem(hDlg, IDC_COLOR_COMBO);
        ComboBox_AddString(hCombo, L"黑色");
        ComboBox_AddString(hCombo, L"红色");
        ComboBox_AddString(hCombo, L"蓝色");
        ComboBox_AddString(hCombo, L"绿色");
        ComboBox_SetCurSel(hCombo, 0);

        // 初始化线宽滑块
        HWND hTrack = GetDlgItem(hDlg, IDC_WIDTH_TRACK);
        SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
        SendMessage(hTrack, TBM_SETPOS, TRUE, 1);

        return TRUE;
    }

    case WM_HSCROLL:
    {
        // 处理滑块移动
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_WIDTH_TRACK))
        {
            int pos = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

            // 更新标签显示
            wchar_t szText[10];
            swprintf_s(szText, L"%d", pos);
            SetDlgItemText(hDlg, IDC_WIDTH_LABEL, szText);
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_APPLY:
        {
            // 获取颜色选择
            HWND hCombo = GetDlgItem(hDlg, IDC_COLOR_COMBO);
            int colorSel = ComboBox_GetCurSel(hCombo);

            switch (colorSel)
            {
            case 0: g_currentColor = RGB(0, 0, 0); break;
            case 1: g_currentColor = RGB(255, 0, 0); break;
            case 2: g_currentColor = RGB(0, 0, 255); break;
            case 3: g_currentColor = RGB(0, 128, 0); break;
            }

            // 获取线宽
            HWND hTrack = GetDlgItem(hDlg, IDC_WIDTH_TRACK);
            g_currentWidth = (int)SendMessage(hTrack, TBM_GETPOS, 0, 0);

            // 通知主窗口更新
            HWND hParent = GetParent(hDlg);
            if (hParent)
            {
                SendMessage(hParent, WM_SETTINGS_CHANGED, 0, 0);
            }

            MessageBox(hDlg, L"设置已应用", L"提示", MB_OK);
            return TRUE;
        }

        case IDCANCEL:
        {
            DestroyWindow(hDlg);
            g_hToolPanel = NULL;
            return TRUE;
        }
        }
        break;
    }
    }

    return FALSE;
}
```

这个完整示例展示了：
1. 创建和显示非模态对话框
2. 在消息循环中集成 `IsDialogMessage`
3. 对话框与主窗口之间的通信
4. 对话框的正确销毁
5. 菜单控制对话框的显示/隐藏

## 踩坑点总结

在折腾非模态对话框的过程中，我总结了一些容易踩的坑，希望能帮你省点时间：

**Tab 键和快捷键不工作**

这是新手最常遇到的问题。症状是对话框显示正常，鼠标操作也正常，但键盘导航完全失效。原因就是消息循环中没有调用 `IsDialogMessage`。记住这个修改模式：

```cpp
// 错误的消息循环
while (GetMessage(&msg, NULL, 0, 0))
{
    TranslateMessage(&msg);
    DispatchMessage(&msg);
}

// 正确的消息循环
while (GetMessage(&msg, NULL, 0, 0))
{
    if (g_hDialog == NULL || !IsDialogMessage(g_hDialog, &msg))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
```

**对话框关闭后程序崩溃**

这个问题通常发生在对话框关闭后，代码还在使用已经无效的窗口句柄。解决方案是在销毁对话框时立即将全局句柄设为 NULL：

```cpp
case IDCANCEL:
    DestroyWindow(hDlg);
    g_hToolPanel = NULL;  // 不要忘记这行！
    return TRUE;
```

**重复创建对话框**

如果用户多次点击"显示工具面板"菜单，可能会创建多个对话框实例。解决方法是在创建前检查：

```cpp
case IDM_SHOW_TOOLPANEL:
{
    if (g_hToolPanel == NULL || !IsWindow(g_hToolPanel))
    {
        g_hToolPanel = CreateDialog(...);
        ShowWindow(g_hToolPanel, SW_SHOW);
    }
    else if (!IsWindowVisible(g_hToolPanel))
    {
        ShowWindow(g_hToolPanel, SW_SHOW);
    }
    break;
}
```

## 后续可以做什么

到这里就大功告成了！你已经掌握了 Win32 非模态对话框的核心知识。如果你想继续深入，可以尝试：

1. 实现可拖动和停靠的工具面板，类似 Visual Studio 的窗口布局
2. 添加工具面板的状态保存和恢复功能
3. 创建多文档界面（MDI）应用程序
4. 探索属性表（Property Sheets）和向导（Wizard）对话框
5. 研究自定义绘制，让工具面板更美观

## 相关资源

- [CreateDialog 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-createdialogparama)
- [IsDialogMessage 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-isdialogmessagea)
- [使用对话框 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/using-dialog-boxes)
- [Modeless Dialogs - winprog.org](http://winprog.org/tutorial/modeless_dialogs.html)
