# 通用GUI编程技术——Win32 原生编程实战（四）——WM_NOTIFY 消息机制

> 上一篇文章我们搞定了基础控件——按钮、编辑框、列表框这些都能用了。你可能觉得自己已经掌握了 Win32 控件的核心，说实话，我当时也是这么想的。但当我开始尝试用 ListView 做一个文件列表的时候，突然发现：怎么消息处理和之前教的不一样？通知码长得完全没见过，lParam 也不再是一个简单的句柄了。今天我们就来聊聊这个让很多人踩坑的东西——WM_NOTIFY 消息机制。

---

## 为什么一定要搞懂 WM_NOTIFY

说实话，我在刚接触这个机制的时候也有些抵触。前面刚学会了 WM_COMMAND，不是挺好用的吗？为什么要搞一套新的？但当我真正理解了它的设计之后，才发现这东西其实很有必要。

首先，WM_COMMAND 有个硬伤：通知码只有 16 位。你可能觉得 16 位已经很多了（0-65535），但当你需要处理各种复杂控件的通知时，这个空间很快就不够用了。ListView 控件就有几十种不同的通知码，更别提 TreeView、TabControl 这些了。

另一个问题是 lParam。在 WM_COMMAND 里，lParam 只能传递一个控件句柄。但想想看，当用户双击 ListView 里的某一项时，你可能需要知道：是哪一项？是哪一列？点击的位置在哪里？键盘状态怎么样？这些信息一个句柄根本装不下。

Windows 的设计师选择了一个聪明的方案：让 lParam 指向一个结构体，结构体里可以包含任意多的信息。这就是 WM_NOTIFY 的核心思想。

还有一个现实原因：现代 GUI 框架（WPF、GTK）都有自己的通知系统，但它们的设计思路都可以追溯到 WM_NOTIFY。理解了这套机制，你会发现其他框架的事件系统其实也没什么神秘的。

这篇文章会带着你彻底搞懂 WM_NOTIFY，从为什么要设计它，到怎么正确处理各种通知。掌握了这个，下一篇我们就能愉快地学习 ListView 和 TreeView 了。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建窗口、WM_COMMAND 是怎么工作的、基本的控件通信。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——回顾 WM_COMMAND 的局限

在讲 WM_NOTIFY 之前，我们先来复习一下 WM_COMMAND 的消息结构：

```cpp
case WM_COMMAND:
{
    WORD controlId = LOWORD(wParam);    // 控件 ID
    WORD notificationCode = HIWORD(wParam);  // 通知码
    HWND hControl = (HWND)lParam;       // 控件句柄
    ...
}
```

这个设计在当时（Windows 2.0 时代）是挺不错的，简单高效。但随着控件越来越复杂，问题开始暴露了。

### 痛点一：通知码空间不够

WM_COMMAND 的通知码只有 16 位，范围是 0-65535。听起来很多，但实际上：

* 按钮控件的通知码大约有 10 个（BN_CLICKED、BN_PAINT 等）
* 编辑框的通知码大约有 20 个（EN_CHANGE、EN_SETFOCUS 等）
* 列表框的通知码大约有 10 个（LBN_SELCHANGE、LBN_DBLCLK 等）

这些简单的控件还好，但到了复杂控件就完全不够看了。ListView 的通知码有上百种，TreeView 也有几十种。如果用 16 位来表示，很快就用完了。

⚠️ 注意

通知码冲突是个大问题。如果两个控件恰好用了同一个通知码值，你就无法区分是哪个控件发的。Windows 设计师为了避免这个问题，不得不给每类控件预留独立的号码段，但 16 位空间根本不够分配。

### 痛点二：lParam 传递的信息太少

在 WM_COMMAND 里，lParam 只能传递一个控件句柄。这对于简单通知（比如"按钮被点击了"）足够了，但对于复杂通知就不够了。

举个例子：用户双击了 ListView 里的某一项，你至少需要知道：

| 信息 | 说明 |
|------|------|
| 哪个控件发的 | 控件句柄或 ID |
| 是哪个通知 | 通知码 |
| 哪一项被点击 | 项的索引 |
| 哪一列被点击 | 子项索引 |
| 点击的具体位置 | 屏幕坐标或客户区坐标 |
| 键盘状态 | 是否按下了 Ctrl、Shift 等 |

用 WM_COMMAND 的结构，你只能拿到前三个信息，后面的全都没有。

### 痛点三：结构不统一

不同控件的通知码格式不一样，有的用 LOWORD，有的用 HIWORD，有的还要看 lParam。处理起来很混乱，容易出错。

---

## 第二步——WM_NOTIFY 的设计思路

为了解决这些问题，Windows 引入了 WM_NOTIFY 消息。它的核心思想很简单：**让 lParam 指向一个结构体**，结构体里可以包含任意多的信息。

### WM_NOTIFY 的消息结构

```cpp
case WM_NOTIFY:
{
    // lParam 指向一个 NMHDR 结构体（或其扩展结构）
    LPNMHDR pnmh = (LPNMHDR)lParam;

    // 根据 code 判断是什么通知
    switch (pnmh->code)
    {
    case NM_CLICK:
        // 处理点击
        break;
    }
    ...
}
```

这里 wParam 基本不用了（保留给将来可能的扩展），lParam 承载了所有信息。

### NMHDR 结构体

所有 WM_NOTIFY 通知都至少包含一个 `NMHDR` 结构体：

```cpp
typedef struct tagNMHDR {
    HWND hwndFrom;     // 发送通知的控件句柄
    UINT_PTR idFrom;   // 控件 ID
    UINT code;         // 通知码
} NMHDR;
```

这三个字段是所有通知的"最小公倍数"——任何通知都会告诉你谁发的、ID 是什么、是什么通知。

### 扩展结构体

对于需要更多信息的通知，结构体会在 NMHDR 的基础上扩展：

```cpp
// 点击通知的扩展结构
typedef struct tagNMITEMACTIVATE {
    NMHDR hdr;              // 基础信息（必须是第一个成员）
    int iItem;              // 项的索引
    int iSubItem;           // 子项索引
    UINT uNewState;         // 新状态
    UINT uOldState;         // 旧状态
    UINT uChanged;          // 改变的状态位
    POINT ptAction;         // 操作位置
    LPARAM lParam;          // 应用程序定义的数据
    UINT uKeyFlags;         // 键盘状态标志
} NMITEMACTIVATE, *LPNMITEMACTIVATE;
```

这种设计的好处是：你可以用 `LPNMHDR` 指针接收任何通知，然后根据 code 字段决定要不要转换成更具体的结构体。

---

## 第三步——通用通知码

WM_NOTIFY 定义了一些所有控件都可能发送的通用通知码。这些通知码以 `NM_` 开头：

| 通知码 | 说明 |
|--------|------|
| NM_CLICK | 用户单击了控件 |
| NM_DBLCLK | 用户双击了控件 |
| NM_RCLICK | 用户右键单击了控件 |
| NM_RDBLCLK | 用户右键双击了控件 |
| NM_RETURN | 用户在控件上按下了回车键 |
| NM_SETFOCUS | 控件获得了键盘焦点 |
| NM_KILLFOCUS | 控件失去了键盘焦点 |
| NM_CUSTOMDRAW | 控件需要自绘（用于自定义外观） |
| NM_HOVER | 鼠标悬停在控件上（需要样式支持） |

这些通用通知码的好处是：你可以用一套代码处理所有控件的同类事件。比如你想在任何控件被点击时播放一个音效，只需要处理 NM_click 就行。

---

## 第四步——WM_NOTIFY 处理模板

现在让我们来看一个完整的 WM_NOTIFY 处理模板。这个模板可以作为你以后处理 WM_NOTIFY 的起点：

```cpp
case WM_NOTIFY:
{
    // 先转换成基础结构体
    LPNMHDR pnmh = (LPNMHDR)lParam;

    // 方式一：根据控件 ID 处理（适合多个同类控件）
    switch (pnmh->idFrom)
    {
    case ID_MY_CONTROL:
        switch (pnmh->code)
        {
        case NM_CLICK:
            // 处理点击
            break;
        case NM_DBLCLK:
            // 处理双击
            break;
        }
        break;
    }

    // 方式二：直接根据通知码处理（适合单个控件或通用处理）
    switch (pnmh->code)
    {
    case NM_CLICK:
    {
        // 如果需要更多信息，转换成具体结构体
        LPNMITEMACTIVATE pnia = (LPNMITEMACTIVATE)lParam;
        // 现在可以访问 pnia->iItem、pnia->iSubItem 等
        break;
    }

    case NM_DBLCLK:
        // 处理双击
        break;

    case NM_SETFOCUS:
        // 处理获得焦点
        break;

    case NM_KILLFOCUS:
        // 处理失去焦点
        break;
    }
    break;
}
```

⚠️ 注意

**类型转换的安全性**：当你把 `LPNMHDR` 转换成更具体的结构体时，要确保通知码和结构体类型匹配。比如处理 NM_CLICK 时转成 `LPNMITEMACTIVATE` 是安全的，但如果转成 `LPNMTREEVIEW` 就会访问到错误的内存。

一个安全的做法是先检查通知码：

```cpp
switch (pnmh->code)
{
case LVN_ITEMACTIVATE:  // ListView 特定的激活通知
    {
        // 现在可以安全转换
        LPNMITEMACTIVATE pnia = (LPNMITEMACTIVATE)lParam;
        // ...
    }
    break;
}
```

---

## 第五步——实战：一个支持 WM_NOTIFY 的示例控件

让我们用一个实际的例子来巩固今天的知识。我们创建一个简单的程序，演示 WM_NOTIFY 的处理。虽然真正的 ListView 要等到下一篇才讲，但我们可以先用 Status Bar（状态栏）来练习，因为它也使用 WM_NOTIFY。

### 示例：状态栏点击检测

```cpp
#include <windows.h>
#include <commctrl.h>  // 必须包含，用于通用控件
#pragma comment(lib, "comctl32.lib")  // 链接 comctl32.lib

#define ID_STATUSBAR  1001

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 初始化通用控件（必须！）
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_BAR_CLASSES;  // 状态栏属于 BAR_CLASSES
        InitCommonControlsEx(&icex);

        // 创建状态栏
        HWND hStatus = CreateWindowEx(
            0, STATUSCLASSNAME, L"",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_STATUSBAR,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );

        // 设置状态栏文字
        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)L"试着点击状态栏");

        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        // 检查是否是状态栏发的通知
        if (pnmh->idFrom == ID_STATUSBAR)
        {
            switch (pnmh->code)
            {
            case NM_CLICK:
                MessageBox(hwnd, L"你单击了状态栏", L"WM_NOTIFY 测试", MB_OK);
                break;

            case NM_DBLCLK:
                MessageBox(hwnd, L"你双击了状态栏", L"WM_NOTIFY 测试", MB_OK);
                break;

            case NM_RCLICK:
                MessageBox(hwnd, L"你右键单击了状态栏", L"WM_NOTIFY 测试", MB_OK);
                break;
            }
        }
        return 0;
    }

    case WM_SIZE:
    {
        // 窗口大小改变时，重新定位状态栏
        HWND hStatus = GetDlgItem(hwnd, ID_STATUSBAR);
        if (hStatus)
        {
            SendMessage(hStatus, WM_SIZE, 0, 0);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"NotifyTestWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"WM_NOTIFY 测试",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg = {0};
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
```

编译运行这个程序，你会看到窗口底部有一个状态栏。试着单击、双击、右键点击它，你会看到相应的消息框。

### 代码要点解析

1. **InitCommonControlsEx**：这是使用任何高级控件前必须调用的初始化函数。如果不调用，状态栏创建会失败。

2. **STATUSCLASSNAME**：这是状态栏的窗口类名，定义在 `<commctrl.h>` 中。

3. **WM_NOTIFY 处理**：我们通过 `idFrom` 判断是否是状态栏发的通知，然后根据 `code` 判断具体是什么通知。

4. **WM_SIZE 处理**：窗口大小改变时，需要通知状态栏重新调整自己的大小。

---

## 第六步——WM_COMMAND vs WM_NOTIFY 对照表

为了帮你更好地理解两种消息机制的区别，这里有一个对照表：

| 特性 | WM_COMMAND | WM_NOTIFY |
|------|------------|-----------|
| 出现版本 | Windows 1.0 | Windows 95（comctl32.dll 4.0） |
| wParam | LOWORD=ID, HIWORD=通知码 | 控件 ID（通常不用） |
| lParam | 控件句柄 | 指向 NMHDR（或扩展结构）的指针 |
| 通知码空间 | 16 位（0-65535） | 32 位（完整 UINT 范围） |
| 信息传递量 | 有限（句柄+通知码） | 丰富（可扩展结构体） |
| 典型使用 | 按钮、编辑框、列表框 | ListView、TreeView、TabControl、状态栏 |
| 通知码前缀 | BN_、EN_、LBN_、CBN_ | NM_、LVN_、TVN_、TCN_、HDN_ 等 |

⚠️ 注意

**不要混淆**：WM_COMMAND 仍然是基础控件的主要通知方式，它并没有被 WM_NOTIFY 取代。两种消息机制会长期并存。判断用哪种消息很简单：如果控件文档说用 WM_NOTIFY，就用 WM_NOTIFY；否则用 WM_COMMAND。

---

## 后续可以做什么

到这里，WM_NOTIFY 消息机制就讲完了。你现在应该理解了为什么要设计 WM_NOTIFY，以及如何正确处理它。虽然今天的例子比较简单（只用了状态栏），但这套机制是通用的——下一篇讲 ListView 时，你会发现今天学的知识完全可以复用。

下一篇文章，我们会正式进入 **ListView 控件的世界**——这是 Windows 程序中最常用的复杂控件之一。你会学到如何创建多列列表、如何添加/删除项、如何处理用户的选择，以及如何实现一个像文件管理器那样的列表视图。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习：

1. **基础练习**：修改上面的示例程序，让状态栏显示你点击的是左键、右键还是中键
2. **进阶练习**：在窗口中添加多个控件（比如两个状态栏），在同一个 WM_NOTIFY 处理中区分它们
3. **挑战练习**：查阅 MSDN，了解 Toolbar 控件的 WM_NOTIFY 通知，实现一个点击工具栏按钮就显示对应通知码的程序

好了，今天的文章就到这里。WM_NOTIFY 这关过了，下一篇的 ListView 就不在话下了。我们下一篇再见！

---

**相关资源**

- [WM_NOTIFY 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/wm-notify)
- [NMHDR 结构 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/ns-winuser-nmhdr)
- [通用控件简介 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/common-controls-intro)
