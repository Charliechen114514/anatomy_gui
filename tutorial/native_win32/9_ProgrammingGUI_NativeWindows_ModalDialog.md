# 通用GUI编程技术——Win32 原生编程实战（八）——模态对话框实战指南

> 上一篇文章我们搞定了一堆标准控件，你的窗口现在应该能响应用户的各种操作了。但说实话，光有一个填满控件的主窗口还不够用。实际做项目的时候，你经常需要弹出一个对话框让用户确认操作、输入一些配置信息，或者显示一个"关于"对话框。今天我们要聊的就是这种最常见的对话框类型——模态对话框。

---

## 为什么一定要搞懂模态对话框

说实话，我在刚学 Win32 的时候对对话框是有点抗拒的。一方面是因为前面学窗口创建的时候已经折腾够多了，另一方面是对话框的 API 看起来和普通窗口不太一样，总觉得是一套需要单独记忆的东西。但当你真正开始做项目的时候，会发现对话框其实是你用得最多的 UI 组件之一。

首先，模态对话框最核心的特点就是它会在显示时"阻塞"父窗口。这是什么意思呢？简单来说，当模态对话框弹出后，用户必须先处理完对话框里的操作（点确定或取消），才能回到主窗口继续操作。这个行为模式非常适合需要用户确认或者集中输入某些信息的场景。

比如你想让用户确认是否删除一个重要文件，这时候用模态对话框就非常合适——用户不能假装没看见直接去点别的地方，必须明确做出选择。再比如设置页面，让用户在一个专门的窗口里集中配置各种选项，配置完成后再回到主程序。这些场景都是模态对话框的典型用例。

另一个现实原因是：现代框架像 Qt、WPF、WinUI 3 把对话框封装得太好了，你可能从来不需要关心 DialogBox 和 EndDialog 这些 API。但一旦你需要做一些底层优化，或者遇到了框架无法解决的 bug，或者只是想用纯 Win32 写一个小工具，这些知识就会成为你的救命稻草。

而且，理解了 Win32 的对话框机制之后，你会发现它实际上并不复杂。对话框本质上就是一种特殊的窗口，系统帮你做了很多默认处理。你只需要知道怎么创建它、怎么处理它的消息、怎么正确地关闭它，就能应付绝大多数场景。

这篇文章会带着你从零开始，把模态对话框彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的窗口、消息循环是怎么跑的、WM_COMMAND 消息怎么处理。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记，不然接下来的内容可能会让你有点头晕。

---

## 第一步——理解对话框的本质

在 Win32 的世界里，对话框不是什么神秘的魔法，它们本质上就是窗口。准确地说，对话框是一种由系统管理的窗口，系统已经帮你写好了很多默认处理逻辑。比如 Tab 键在控件之间切换焦点、Enter 键触发默认按钮、Esc 键触发取消按钮，这些东西都是对话框管理器自动处理的，你不需要自己写代码。

但你可能会问：既然对话框就是窗口，为什么不直接用 CreateWindow 创建呢？这就是对话框和普通窗口的第一个区别——创建方式。对话框不是用 CreateWindow 创建的，而是用专门的 API 函数创建的，比如我们今天要讲的 DialogBox 和 DialogBoxParam。

这两个函数的区别其实不大，DialogBoxParam 只是多了一个参数可以让你在创建对话框时传递一些自定义数据。如果你需要给对话框传递初始化参数，比如要显示的文件名、要编辑的内容等等，用 DialogBoxParam 会更方便。如果不需要传任何数据，用 DialogBox 就够了。

另一个重要的区别是消息处理。对话框用的是"对话框过程"而不是"窗口过程"，这两个函数的签名和返回值都不一样。窗口过程返回 LRESULT，而对话框过程返回 BOOL（TRUE 或 FALSE）。这个区别很容易踩坑，我们后面会详细讲。

⚠️ 注意

千万别把窗口过程和对话框过程混用，它们的返回值语义完全不同。如果你在对话框过程里返回 DefWindowProc 的结果，或者忘记返回 TRUE/FALSE，你的对话框大概率不会按预期工作。

---

## 第二步——最简单的模态对话框

我们先从最简单的例子开始，创建一个只显示一行文字的"关于"对话框。这个例子足够简单，让你能专注理解对话框的基本流程，而不是被各种控件的消息处理分散注意力。

### 用资源定义对话框

在现代的 Win32 开发中，对话框通常是在资源文件（.rc 文件）中定义的，而不是在代码里用一堆 CreateWindow 调用来硬编码。这种方式有很多好处：UI 布局和业务逻辑分离、更容易维护、可以用 Visual Studio 的资源编辑器可视化设计等等。

假设我们有一个简单的资源文件：

```rc
#include <windows.h>
#include "resource.h"

IDD_ABOUTBOX DIALOGEX 0, 0, 200, 80
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "关于本程序"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    LTEXT           "这是一个用纯 Win32 编写的示例程序",IDC_STATIC,10,20,180,20
    DEFPUSHBUTTON   "确定",IDOK,143,59,50,14
END
```

这个资源定义描述了一个 200×80 像素的对话框，包含一段静态文本和一个确定按钮。其中 IDD_ABOUTBOX 是这个对话框资源的 ID，我们在代码里需要用这个 ID 来引用这个对话框。resource.h 文件里会定义这些 ID 常量：

```cpp
#define IDD_ABOUTBOX                100
#define IDC_STATIC                  -1
```

IDC_STATIC 是一个特殊值，表示这个控件不需要处理消息，只是用来显示内容。

### 创建和显示对话框

有了资源定义之后，在代码里创建对话框就非常简单了：

```cpp
// 在菜单或者按钮的点击处理里
case IDM_HELP_ABOUT:
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutDlgProc);
    break;
}
```

`DialogBox` 函数的第一个参数是程序实例句柄，第二个参数是对话框资源的 ID，第三个参数是父窗口句柄，第四个参数是对话框过程函数的指针。

这里有个值得注意的点：`DialogBox` 是一个"阻塞"函数。这意味着当你调用这个函数后，代码会停在这里，直到对话框被关闭才会继续执行后面的代码。这就是"模态"的由来——在对话框显示期间，父窗口的消息循环实际上被暂停了，系统运行的是对话框自己的内部消息循环。

### 对话框过程

对话框过程和窗口过程类似，也是用来处理消息的，但它的函数签名不太一样：

```cpp
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
```

这个函数的返回类型是 `INT_PTR`，而不是 `LRESULT`。`INT_PTR` 本质上就是一个能容纳指针大小的整数类型，在 32 位系统上是 32 位，在 64 位系统上是 64 位。但这里的关键是返回值的意义：`TRUE` 表示你已经处理了这个消息，`FALSE` 表示你没有处理。

`WM_INITDIALOG` 是对话框收到的第一个消息，它在对话框显示之前发送，作用类似于普通窗口的 `WM_CREATE`。你可以在这里做初始化操作，比如设置控件的初始状态、从 `lParam` 参数获取传递过来的数据等等。

`WM_INITDIALOG` 的返回值有个特殊的约定：如果你返回 `TRUE`，系统会自动把键盘焦点设置到对话框中第一个具有 `WS_TABSTOP` 样式的控件上。如果你返回 `FALSE`，系统不会设置焦点，你需要自己用 `SetFocus` 来指定焦点控件。

大多数情况下，你只需要返回 `TRUE` 就行了，让系统自动处理焦点。但如果你确实需要在初始化时把焦点设置到某个特定的控件，可以这样做：

```cpp
case WM_INITDIALOG:
{
    // 手动设置焦点到某个控件
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_INPUT);
    SetFocus(hEdit);
    return FALSE;  // 注意这里返回 FALSE
}
```

`EndDialog` 函数用于关闭对话框。这里的"关闭"不是说立即销毁窗口，而是告诉系统"这个对话框可以结束了"。系统会做一些清理工作，然后 `DialogBox` 函数才会返回。`EndDialog` 的第二个参数会作为 `DialogBox` 的返回值，这样调用者就能知道用户是怎么关闭对话框的。

---

## 第三步——从对话框获取数据

上面的"关于"对话框太简单了，它只是显示一些信息，不需要从用户那里获取任何输入。但在实际使用中，对话框的主要目的是让用户输入一些数据，然后把这些数据返回给主程序。我们来看一个更实用的例子：一个让用户输入姓名和年龄的对话框。

### 资源定义

首先定义对话框资源：

```rc
IDD_INPUTDLG DIALOGEX 0, 0, 220, 100
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "输入信息"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    LTEXT           "姓名：",IDC_STATIC,10,15,40,10
    EDITTEXT        IDC_EDIT_NAME,55,13,150,14,ES_AUTOHSCROLL
    LTEXT           "年龄：",IDC_STATIC,10,40,40,10
    EDITTEXT        IDC_EDIT_AGE,55,38,150,14,ES_AUTOHSCROLL | ES_NUMBER
    DEFPUSHBUTTON   "确定",IDOK,110,75,50,14
    PUSHBUTTON      "取消",IDCANCEL,165,75,50,14
END
```

这里我们用了 `EDITTEXT` 来定义编辑框控件，`IDC_EDIT_NAME` 和 `IDC_EDIT_AGE` 是我们需要在代码里引用这两个编辑框的 ID。`ES_NUMBER` 样式让年龄编辑框只能输入数字。

### 对话框过程

```cpp
struct InputData
{
    wchar_t name[64];
    int age;
};

INT_PTR CALLBACK InputDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    InputData* pData = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        // lParam 传递过来的数据指针保存在窗口的长指针里
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);

        // 如果有初始数据，设置到编辑框里
        pData = (InputData*)lParam;
        if (pData != nullptr)
        {
            SetDlgItemText(hDlg, IDC_EDIT_NAME, pData->name);
            if (pData->age > 0)
            {
                wchar_t ageStr[16];
                _itow_s(pData->age, ageStr, 10);
                SetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            // 获取保存的数据指针
            pData = (InputData*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (pData != nullptr)
            {
                // 从编辑框获取数据
                GetDlgItemText(hDlg, IDC_EDIT_NAME, pData->name, 64);

                wchar_t ageStr[16];
                GetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr, 16);
                pData->age = _wtoi(ageStr);
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}
```

这里有个值得讲一下的技巧：我们用 `SetWindowLongPtr` 把 `lParam` 传递过来的数据指针保存在对话框的 `DWLP_USER` 位置。这样在后续的消息处理里，我们就能通过 `GetWindowLongPtr` 把这个指针取出来，而不需要用全局变量或者其他蹩脚的方式。

`SetDlgItemText` 和 `GetDlgItemText` 是两个非常方便的辅助函数，它们可以根据控件 ID 直接设置或获取控件的文本内容，不需要先获取控件句柄。如果你需要频繁操作一个控件，可以先获取句柄缓存起来，用 `SendMessage` 直接操作会更高效一些。但对于对话框这种偶尔才操作一次的场景，用这两个函数就足够了。

### 调用对话框

```cpp
void ShowInputDialog(HWND hwndParent)
{
    InputData data = {};
    wcscpy_s(data.name, L"张三");
    data.age = 25;

    // DialogBoxParam 可以传递自定义数据
    INT_PTR result = DialogBoxParam(
        hInst,
        MAKEINTRESOURCE(IDD_INPUTDLG),
        hwndParent,
        InputDlgProc,
        (LPARAM)&data
    );

    if (result == IDOK)
    {
        // 用户点击了确定，data 里保存了输入的内容
        wchar_t msg[256];
        swprintf_s(msg, 256, L"姓名：%s\n年龄：%d", data.name, data.age);
        MessageBox(hwndParent, msg, L"输入结果", MB_OK);
    }
    else
    {
        // 用户点击了取消或者关闭了对话框
        MessageBox(hwndParent, L"用户取消了输入", L"提示", MB_OK);
    }
}
```

`DialogBoxParam` 的最后一个参数 `lParam` 会作为 `WM_INITDIALOG` 消息的 `lParam` 传递给对话框过程。这就是我们传递初始化数据的途径。

⚠️ 注意

`lParam` 传递的是一个指针，所以你需要确保这个指针指向的数据在对话框显示期间一直有效。如果你传递了一个局部变量的指针，然后对话框还在显示时这个变量已经被销毁了，就会出现悬空指针的问题。最安全的做法是使用静态变量、堆内存，或者像上面例子一样在调用函数的栈帧内。

---

## 第四步——处理输入验证

对话框的一个重要职责是验证用户输入。你肯定不希望用户在年龄编辑框里输入"abc"，或者在姓名编辑框里什么都不填就点确定。这些验证逻辑应该放在哪里呢？

最直接的做法是在 `IDOK` 的处理里做验证：

```cpp
if (LOWORD(wParam) == IDOK)
{
    // 获取输入内容
    wchar_t name[64];
    GetDlgItemText(hDlg, IDC_EDIT_NAME, name, 64);

    wchar_t ageStr[16];
    GetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr, 16);

    // 验证输入
    if (wcslen(name) == 0)
    {
        MessageBox(hDlg, L"请输入姓名", L"输入错误", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
        return TRUE;  // 不关闭对话框
    }

    int age = _wtoi(ageStr);
    if (age <= 0 || age > 150)
    {
        MessageBox(hDlg, L"请输入有效的年龄（1-150）", L"输入错误", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(hDlg, IDC_EDIT_AGE));
        // 选中编辑框里的文字，方便用户直接修改
        SendDlgItemMessage(hDlg, IDC_EDIT_AGE, EM_SETSEL, 0, -1);
        return TRUE;  // 不关闭对话框
    }

    // 验证通过，保存数据
    pData = (InputData*)GetWindowLongPtr(hDlg, DWLP_USER);
    if (pData != nullptr)
    {
        wcscpy_s(pData->name, name);
        pData->age = age;
    }

    EndDialog(hDlg, IDOK);
    return TRUE;
}
```

这里的关键点是：如果验证失败，不要调用 `EndDialog`，而是返回 `TRUE` 表示你已经处理了这个消息。对话框会继续显示，用户可以修正输入后再试一次。我们还用 `SetFocus` 把焦点设置到有问题的控件上，用 `EM_SETSEL` 选中编辑框里的文字，让用户能直接输入新的内容，这些都是提升用户体验的小细节。

更好的做法是使用实时验证，在用户输入的时候就给出反馈，而不是等到点确定才报错。这可以通过处理编辑框的 `EN_CHANGE` 通知来实现：

```cpp
case WM_COMMAND:
{
    switch (LOWORD(wParam))
    {
    case IDC_EDIT_AGE:
        if (HIWORD(wParam) == EN_CHANGE)
        {
            // 实时验证年龄输入
            wchar_t ageStr[16];
            GetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr, 16);
            int age = _wtoi(ageStr);

            HWND hHint = GetDlgItem(hDlg, IDC_STATIC_HINT);
            if (age < 0 || age > 150)
            {
                SetWindowText(hHint, L"年龄必须在 1-150 之间");
            }
            else
            {
                SetWindowText(hHint, L"");
            }
        }
        break;

    case IDOK:
        // ... 确定按钮处理
        break;
    }
    break;
}
```

这样用户在输入的时候就能立即看到提示，而不是等到点确定才发现有问题。

---

## 第五步——完整示例

为了让你能够完整运行这个例子，我把完整的代码写出来。假设你的程序入口点（`WinMain`）和前面文章里的一样，只是注册窗口类名换成了 `MYWNDCLASS`：

```cpp
#include <windows.h>
#include "resource.h"

// 资源 ID 定义
#define IDD_ABOUTBOX    100
#define IDD_INPUTDLG    101
#define IDC_EDIT_NAME   200
#define IDC_EDIT_AGE    201
#define IDC_STATIC_HINT 202
#define IDM_HELP_ABOUT  300
#define IDM_INPUT       301
#define IDM_EXIT        302

// 输入数据结构
struct InputData
{
    wchar_t name[64];
    int age;
};

// 前向声明
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK InputDlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 对话框过程：关于对话框
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// 对话框过程：输入对话框
INT_PTR CALLBACK InputDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    InputData* pData = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);

        pData = (InputData*)lParam;
        if (pData != nullptr)
        {
            SetDlgItemText(hDlg, IDC_EDIT_NAME, pData->name);
            if (pData->age > 0)
            {
                wchar_t ageStr[16];
                _itow_s(pData->age, ageStr, 10);
                SetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_EDIT_AGE:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                wchar_t ageStr[16];
                GetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr, 16);
                int age = _wtoi(ageStr);

                HWND hHint = GetDlgItem(hDlg, IDC_STATIC_HINT);
                if (age < 0 || age > 150)
                {
                    SetWindowText(hHint, L"年龄必须在 1-150 之间");
                }
                else
                {
                    SetWindowText(hHint, L"");
                }
            }
            break;

        case IDOK:
        {
            wchar_t name[64];
            GetDlgItemText(hDlg, IDC_EDIT_NAME, name, 64);

            if (wcslen(name) == 0)
            {
                MessageBox(hDlg, L"请输入姓名", L"输入错误", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
                return TRUE;
            }

            wchar_t ageStr[16];
            GetDlgItemText(hDlg, IDC_EDIT_AGE, ageStr, 16);
            int age = _wtoi(ageStr);

            if (age <= 0 || age > 150)
            {
                MessageBox(hDlg, L"请输入有效的年龄（1-150）", L"输入错误", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hDlg, IDC_EDIT_AGE));
                SendDlgItemMessage(hDlg, IDC_EDIT_AGE, EM_SETSEL, 0, -1);
                return TRUE;
            }

            pData = (InputData*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (pData != nullptr)
            {
                wcscpy_s(pData->name, name);
                pData->age = age;
            }

            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// 主窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建菜单
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreatePopupMenu();
        AppendMenuA(hFileMenu, MF_STRING, IDM_INPUT, "输入信息...");
        AppendMenuA(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hFileMenu, MF_STRING, IDM_EXIT, "退出");
        HMENU hHelpMenu = CreatePopupMenu();
        AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, "关于...");
        InsertMenuA(hMenu, 0, MF_POPUP, (UINT_PTR)hFileMenu, "文件");
        InsertMenuA(hMenu, 1, MF_POPUP, (UINT_PTR)hHelpMenu, "帮助");
        SetMenu(hwnd, hMenu);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_INPUT:
        {
            InputData data = {};
            wcscopy_s(data.name, L"张三");
            data.age = 25;

            INT_PTR result = DialogBoxParam(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDD_INPUTDLG),
                hwnd,
                InputDlgProc,
                (LPARAM)&data
            );

            if (result == IDOK)
            {
                wchar_t msg[256];
                swprintf_s(msg, 256, L"姓名：%s\n年龄：%d", data.name, data.age);
                MessageBox(hwnd, msg, L"输入结果", MB_OK);
            }
            break;
        }

        case IDM_HELP_ABOUT:
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutDlgProc);
            break;

        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MYWNDCLASS";

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindow(
        L"MYWNDCLASS",
        L"模态对话框示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, L"窗口创建失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
```

编译运行这个程序，你应该能看到一个带有菜单栏的窗口。点击"文件→输入信息"会弹出输入对话框，输入姓名和年龄后点确定，主窗口会显示你输入的内容。点击"帮助→关于"会显示关于对话框。

---

## 后续可以做什么

到这里，Win32 模态对话框的基础知识就讲完了。你现在应该能够创建和运行模态对话框，理解对话框过程和窗口过程的区别，知道怎么传递数据、怎么验证输入。但模态对话框的世界远不止这些，还有很多高级主题等着我们去探索。

下一篇文章，我们会聊一聊 **非模态对话框**——那种不会阻塞父窗口、可以一直显示在屏幕上的对话框。非模态对话框在很多场景下比模态对话框更实用，比如工具面板、查找替换对话框等等。而且，非模态对话框的创建方式、消息循环集成方式都和模态对话框有所不同，需要特别注意。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习，巩固一下知识：

1. 创建一个设置对话框，包含多种类型的控件（复选框、单选按钮、组合框等）
2. 实现输入验证，在用户输入错误的时候给出清晰的提示
3. 使用 `DialogBoxParam` 传递初始化数据，让对话框打开时显示上次输入的内容
4. 创建一个多页面的属性表（Property Sheet），用多个对话框组合成一个设置向导

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。好了，今天的文章就到这里，我们下一篇再见！

---

## 相关资源

- [WM_INITDIALOG 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/wm-initdialog)
- [DialogBoxParam 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-dialogboxparama)
- [EndDialog 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-enddialog)
- [使用对话框 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/using-dialog-boxes)
- [WinProg Tutorial: Dialogs](http://winprog.org/tutorial/dialogs.html)
- [Win32: Modal dialog not returning focus - Stack Overflow](https://stackoverflow.com/questions/923631/win32-modal-dialog-not-returning-focus)
