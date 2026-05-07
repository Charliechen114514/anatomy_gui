# 通用GUI编程技术——Win32 原生编程实战（五十三）——子类化与超类化

> 上一篇文章我们补充了高级输入消息——触控（`WM_TOUCH`、`WM_POINTER`）、Raw Input（`WM_INPUT`）、窗口位置管理（`WM_WINDOWPOSCHANGING`）等。这些消息让你对现代输入设备和窗口行为有了更精细的控制。但到目前为止，我们创建的所有控件都是"系统原样"的——按钮就是按钮，编辑框就是编辑框，它们的行为完全由系统内置的窗口过程决定。如果你想让一个 Edit 控件在按下 Enter 时自动跳到下一个控件，或者让一个 ListBox 的奇偶行显示不同颜色，怎么办？这就需要今天的主角——子类化（Subclassing）和超类化（Superclassing）。

---

## 为什么需要子类化和超类化

Win32 控件的行为是由它们的窗口过程（WndProc）定义的。当你创建一个 `"EDIT"` 控件时，系统会调用 Edit 控件的内部窗口过程来处理所有消息——按键输入、文字渲染、光标移动，全都是由系统代码完成的。

但有时候，系统默认的行为不完全满足你的需求。常见的场景包括：

* **Edit 控件按 Enter 不发出通知**——标准的 Edit 控件在按下 Enter 键时，只是插入一个换行符（如果是多行），并不通知父窗口。如果你想实现"按 Enter 提交"的功能，需要拦截 WM_KEYDOWN。
* **限制输入类型**——你想让一个 Edit 控件只接受数字输入，或者限制最大长度。
* **自定义绘制**——你想让 ListBox 的奇偶行显示不同的背景色，或者在 ComboBox 的下拉列表中显示图标。
* **统一修改同类控件**——你有 20 个 Edit 控件，想让它们都具备某种特殊行为（比如鼠标悬停时改变边框颜色）。

子类化和超类化就是解决这些问题的两种方案：

* **子类化**：修改**一个已有的控件实例**的窗口过程，只影响那一个控件。
* **超类化**：基于现有控件类**注册一个全新的窗口类**，之后创建的所有该类控件都具备新的行为。

打个比方：子类化像是给一辆现成的汽车换了一套方向盘和仪表盘；超类化像是根据现有车型的图纸，设计了一款新车型，以后量产的都是新版。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道窗口过程怎么写、WM_COMMAND 和 WM_NOTIFY 怎么处理、基本的控件使用。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——子类化的原理

子类化的核心思想很简单：**替换窗口的窗口过程指针**。

每个窗口在内部都有一个字段记录"谁负责处理我的消息"——也就是窗口过程函数指针。正常情况下，Edit 控件的这个指针指向系统内部的 Edit 窗口过程。子类化就是把这个指针替换成你自己的函数，让你的函数先看到所有消息。

```
正常情况：
消息 → 系统的 EditWndProc → 处理

子类化后：
消息 → 你的 SubclassProc → （可以拦截/修改）→ 原来的 EditWndProc → 处理
```

你的子类过程可以选择：
* 完全处理某条消息（不让原来的窗口过程看到）
* 修改消息参数后再传给原来的窗口过程
* 先让原来的窗口过程处理，再对结果做修改
* 对大部分消息不做任何处理，直接传给原来的窗口过程

---

## 第二步——SetWindowSubclass：推荐的子类化方式

Windows 提供了两种子类化方式。老式的方式用 `SetWindowLongPtr(GWLP_WNDPROC)`，新式的方式用 `SetWindowSubclass`。我们强烈推荐使用新式，原因后面会讲。

### 函数签名

```cpp
BOOL SetWindowSubclass(
    HWND      hWnd,           // 要子类化的窗口
    SUBCLASSPROC pfnSubclass, // 子类过程函数
    UINT_PTR  uIdSubclass,    // 子类 ID（用于标识）
    DWORD_PTR dwRefData       // 传给子类过程的用户数据
);
```

子类过程的原型：

```cpp
LRESULT CALLBACK SubclassProc(
    HWND      hWnd,          // 窗口句柄
    UINT      uMsg,          // 消息 ID
    WPARAM    wParam,        // 参数 1
    LPARAM    lParam,        // 参数 2
    UINT_PTR  uIdSubclass,   // 子类 ID（你设的那个）
    DWORD_PTR dwRefData      // 用户数据（你设的那个）
);
```

注意：子类过程比普通窗口过程多了两个参数——`uIdSubclass` 和 `dwRefData`。这意味着你不能直接把它当普通 WndProc 用，必须遵循这个签名。

### 核心函数：DefSubclassProc

在子类过程中，你应该调用 `DefSubclassProc` 来把消息传给"下一个处理者"（可能是另一个子类过程，也可能是原来的窗口过程）：

```cpp
LRESULT DefSubclassProc(
    HWND   hWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam
);
```

这和普通的 `DefWindowProc` 类似，但它不是调用默认窗口过程，而是调用子类链中的下一个处理器。

### 移除子类化

```cpp
BOOL RemoveWindowSubclass(
    HWND      hWnd,
    SUBCLASSPROC pfnSubclass,
    UINT_PTR  uIdSubclass
);
```

⚠️ 注意

虽然 `SetWindowSubclass` 会自动在窗口销毁时清理子类数据，但最好还是养成在不需要时调用 `RemoveWindowSubclass` 的习惯。特别是如果你在 `dwRefData` 中传递了动态分配的指针，必须确保在窗口销毁前释放它。

---

## 第三步——实战：拦截 Edit 控件的 Enter 键

这是一个经典的子类化场景：你想让用户在 Edit 控件中按 Enter 时，不是换行而是执行某个操作（比如提交搜索、发送消息）。

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define IDC_EDIT       1001
#define IDC_SUBMIT_BTN 1002
#define IDC_RESULT     1003

// 自定义消息：Enter 键被按下
#define WM_ENTER_PRESSED (WM_APP + 10)

HWND g_hEdit = NULL;
HWND g_hResult = NULL;

// 子类过程
LRESULT CALLBACK EditSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            // 拦截 Enter 键，通知父窗口
            HWND hParent = GetParent(hWnd);
            SendMessage(hParent, WM_ENTER_PRESSED, 0, 0);
            return 0;  // 不传给原始 Edit 过程
        }
        break;

    case WM_CHAR:
        // 拦截 Enter 的字符（换行符），防止多行 Edit 插入换行
        if (wParam == '\r' || wParam == '\n')
            return 0;
        break;

    case WM_DESTROY:
        // 窗口销毁时移除子类化
        RemoveWindowSubclass(hWnd, EditSubclassProc, 0);
        break;
    }

    // 其他消息交给原始窗口过程处理
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void SubmitText(HWND hwnd)
{
    wchar_t buf[256];
    GetWindowText(g_hEdit, buf, 256);

    if (wcslen(buf) == 0)
    {
        MessageBox(hwnd, L"请输入一些文字", L"提示", MB_OK);
        return;
    }

    // 在结果区域显示
    wchar_t result[512];
    swprintf_s(result, L"已提交：%s", buf);
    SetWindowText(g_hResult, result);

    // 清空输入框
    SetWindowText(g_hEdit, L"");
    SetFocus(g_hEdit);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 创建标签
        CreateWindowEx(0, L"STATIC", L"输入文字，按 Enter 提交：",
            WS_CHILD | WS_VISIBLE,
            20, 20, 300, 20,
            hwnd, NULL, hInst, NULL);

        // 创建 Edit 控件
        g_hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 50, 350, 28,
            hwnd, (HMENU)IDC_EDIT, hInst, NULL);

        // 子类化 Edit 控件
        SetWindowSubclass(g_hEdit, EditSubclassProc, 0, 0);

        // 创建提交按钮
        CreateWindowEx(0, L"BUTTON", L"提交",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 50, 80, 28,
            hwnd, (HMENU)IDC_SUBMIT_BTN, hInst, NULL);

        // 创建结果显示
        g_hResult = CreateWindowEx(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 100, 440, 30,
            hwnd, (HMENU)IDC_RESULT, hInst, NULL);

        SetFocus(g_hEdit);
        return 0;
    }

    case WM_ENTER_PRESSED:
        SubmitText(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SUBMIT_BTN)
            SubmitText(hwnd);
        return 0;

    case WM_DESTROY:
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
    wc.lpszClassName = L"SubclassDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"SubclassDemo", L"子类化示例 - Enter 提交",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 200,
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
```

### 代码要点解析

1. **SetWindowSubclass**：在 WM_CREATE 中，创建 Edit 控件后立即子类化。`uIdSubclass` 设为 0（因为我们只有一个子类），`dwRefData` 也设为 0（不需要额外数据）。

2. **WM_KEYDOWN 拦截 VK_RETURN**：检测到 Enter 键后，发送自定义消息 `WM_ENTER_PRESSED` 给父窗口，然后返回 0 阻止原始 Edit 过程处理这条消息。

3. **WM_CHAR 拦截换行符**：即使拦截了 WM_KEYDOWN，TranslateMessage 还会产生 WM_CHAR 消息。所以也要拦截 '\r' 和 '\n'。

4. **DefSubclassProc**：所有不需要拦截的消息都交给原始窗口过程处理，保证 Edit 控件的正常功能（输入、选择、复制粘贴等）不受影响。

---

## 第四步——多级子类化

`SetWindowSubclass` 的一个强大特性是支持多级子类化——你可以对同一个窗口多次调用 `SetWindowSubclass`，注册不同的子类过程。它们会形成一个调用链。

```cpp
// 子类 A：限制只能输入数字
LRESULT CALLBACK NumericOnlyProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_CHAR)
    {
        if (wParam < '0' || wParam > '9')
            return 0;  // 非数字，拦截
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// 子类 B：限制最大长度为 10
LRESULT CALLBACK MaxLengthProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_CHAR && wParam >= ' ')
    {
        int len = GetWindowTextLength(hWnd);
        if (len >= 10)
            return 0;  // 超长，拦截
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// 注册多个子类（注意注册顺序：后注册的先执行）
SetWindowSubclass(hEdit, NumericOnlyProc, 1, 0);
SetWindowSubclass(hEdit, MaxLengthProc, 2, 0);
```

调用链：消息 → MaxLengthProc → NumericOnlyProc → 原始 Edit 过程。

`DefSubclassProc` 不是调用原始窗口过程，而是调用子类链中的**下一个**子类过程。只有最后一个子类过程的 `DefSubclassProc` 才会调用原始窗口过程。

### 为什么推荐 SetWindowSubclass 而不是 SetWindowLongPtr

老式子类化用 `SetWindowLongPtr(hWnd, GWLP_WNDPROC, newProc)` 替换窗口过程，然后用 `CallWindowProc(oldProc, ...)` 调用原来的。这种方式有几个严重问题：

| 问题 | SetWindowLongPtr | SetWindowSubclass |
|------|-----------------|-------------------|
| 多级子类化 | 需要手动管理链表 | 自动管理调用链 |
| 清理 | 必须在窗口销毁前手动恢复 | 自动清理 |
| 数据传递 | 需要用全局变量或窗口属性 | 内置 dwRefData 参数 |
| 线程安全 | 不保证 | 保证 |

`SetWindowSubclass` 内部使用了一个引用计数机制。如果多段代码都对同一个窗口做了子类化，每个都能安全地移除自己的子类，不会影响其他代码。

---

## 第五步——超类化（Superclassing）

超类化和子类化的目的类似，但方式不同：子类化修改的是**一个已有的窗口实例**，超类化是创建一个**新的窗口类**。

### 基本步骤

```cpp
// 第 1 步：获取原始类的信息
WNDCLASSEX wc = {};
wc.cbSize = sizeof(WNDCLASSEX);
GetClassInfoEx(hInstance, L"EDIT", &wc);

// 第 2 步：保存原始窗口过程
WNDPROC pfnOriginal = wc.lpfnWndProc;

// 第 3 步：修改窗口过程为你自己的
wc.lpfnWndProc = MyEditProc;
wc.lpszClassName = L"MyNumericEdit";  // 新类名
wc.hInstance = hInstance;

// 第 4 步：注册新类
ATOM atom = RegisterClassEx(&wc);

// 第 5 步：用新类名创建控件
HWND hEdit = CreateWindowEx(
    0, L"MyNumericEdit", L"",
    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
    10, 10, 200, 25,
    hwndParent, (HMENU)IDC_EDIT, hInstance, NULL
);
```

### 超类化的窗口过程

```cpp
// 全局保存原始窗口过程
WNDPROC g_pfnOriginalEditProc = NULL;

LRESULT CALLBACK MyEditSuperclassProc(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CHAR:
        // 只允许数字和退格
        if (!((wParam >= '0' && wParam <= '9') || wParam == VK_BACK))
        {
            MessageBeep(MB_ICONEXCLAMATION);
            return 0;
        }
        break;

    case WM_PASTE:
    {
        // 拦截粘贴，只粘贴数字部分
        if (OpenClipboard(hwnd))
        {
            HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
            if (hClip)
            {
                wchar_t* text = (wchar_t*)GlobalLock(hClip);
                if (text)
                {
                    std::wstring digits;
                    for (int i = 0; text[i]; i++)
                    {
                        if (text[i] >= L'0' && text[i] <= L'9')
                            digits += text[i];
                    }
                    GlobalUnlock(hClip);

                    // 把过滤后的数字发回给编辑框
                    for (wchar_t c : digits)
                    {
                        SendMessage(hwnd, WM_CHAR, c, 0);
                    }
                }
            }
            CloseClipboard();
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        // 超类化不需要手动清理
        break;
    }

    // 调用原始的 Edit 窗口过程
    return CallWindowProc(g_pfnOriginalEditProc, hwnd, uMsg, wParam, lParam);
}
```

### 子类化 vs 超类化对比

| 特性 | 子类化 | 超类化 |
|------|--------|--------|
| 作用对象 | 已有的窗口实例 | 窗口类 |
| 影响范围 | 只影响被子类化的那个控件 | 之后创建的所有该类控件 |
| 时机 | 控件创建之后 | 注册新类，然后创建控件 |
| 适合场景 | 修改一两个控件 | 批量创建同类自定义控件 |
| 实现复杂度 | 简单 | 稍复杂 |
| 灵活性 | 可动态添加/移除 | 创建后不可修改 |
| 多实例 | 每个实例都要单独子类化 | 创建时自动生效 |

### 选择建议

* 如果只是修改一两个控件的行为 → **子类化**
* 如果需要创建大量相同行为的自定义控件 → **超类化**
* 如果不确定 → 先用子类化，后面有需要再重构为超类化

---

## 第六步——利用 dwRefData 传递实例数据

子类过程的一个常见需求是访问与特定控件关联的数据。`dwRefData` 参数就是为了这个目的。但由于子类过程中不能直接访问父窗口的成员变量，你需要通过 `dwRefData` 传递。

### 示例：带标签的 Edit 控件

```cpp
struct EditContext
{
    HWND hParent;
    int  controlId;
    int  maxLength;
};

LRESULT CALLBACK ContextEditProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    EditContext* ctx = (EditContext*)dwRefData;

    switch (uMsg)
    {
    case WM_CHAR:
        if (wParam >= ' ' && GetWindowTextLength(hWnd) >= ctx->maxLength)
        {
            MessageBeep(MB_ICONEXCLAMATION);
            return 0;
        }
        break;

    case WM_NCDESTROY:
        // 窗口销毁时释放上下文数据
        delete ctx;
        RemoveWindowSubclass(hWnd, ContextEditProc, uIdSubclass);
        return 0;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// 使用方式
void SubclassEditWithLimit(HWND hEdit, HWND hParent, int id, int maxLen)
{
    EditContext* ctx = new EditContext{ hParent, id, maxLen };
    SetWindowSubclass(hEdit, ContextEditProc, id, (DWORD_PTR)ctx);
}
```

⚠️ 注意

**生命周期管理**：`dwRefData` 中的指针必须在使用期间保持有效。如果传的是栈变量指针，函数返回后就成了野指针。推荐用 `new` 分配，在 WM_NCDESTROY 中 `delete`。

---

## 常见陷阱

### 陷阱一：调用 CallWindowProc 而不是 DefSubclassProc

```cpp
// 错误！在子类过程中调用 CallWindowProc 会跳过其他子类层
LRESULT CALLBACK BadSubclassProc(...)
{
    return CallWindowProc(g_oldProc, hWnd, uMsg, wParam, lParam);
}

// 正确：使用 DefSubclassProc
LRESULT CALLBACK GoodSubclassProc(...)
{
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
```

### 陷阱二：忘记在 WM_NCDESTROY 中清理

虽然 `SetWindowSubclass` 会在窗口销毁时自动移除子类，但如果你在 `dwRefData` 中传了 `new` 出来的对象，必须在 WM_NCDESTROY 中手动 `delete`，否则内存泄漏。

### 陷阱三：对系统控件做过多拦截

子类化的初衷是"微调"，不是"重写"。如果你拦截了太多消息，可能破坏控件的正常功能。比如拦截 WM_GETDLGCODE 可能导致对话框导航异常。只拦截你确实需要修改的消息，其他的都交给 `DefSubclassProc`。

---

## 后续可以做什么

到这里，子类化和超类化的知识就讲完了。你现在应该理解了子类化的原理、`SetWindowSubclass` 的用法、多级子类化的调用链机制、超类化的适用场景，以及如何通过 dwRefData 传递实例数据。

下一篇文章，我们会聊一个与子类化有些关联的话题——**Hook 机制**。子类化只能拦截发往特定窗口的消息，而 Hook 可以拦截发往整个系统（或整个进程）的消息。两者经常配合使用。

在此之前，建议你做一些练习巩固今天的知识：

1. **基础练习**：子类化一个 Edit 控件，实现只允许输入十六进制字符（0-9, A-F, a-f），并自动将小写字母转换为大写
2. **进阶练习**：子类化一个 ListBox 控件，实现鼠标悬停时高亮当前项（提示：拦截 WM_MOUSEMOVE，用 `LB_ITEMFROMPOINT` 获取当前项）
3. **挑战练习**：用超类化创建一个 `MyHyperlink` 控件类——基于 STATIC 控件，鼠标悬停时文字变蓝色并显示下划线，点击时用 ShellExecute 打开链接

---

## 相关资源

- [SetWindowSubclass function (Commctrl.h) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-setwindowsubclass)
- [RemoveWindowSubclass function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-removewindowsubclass)
- [DefSubclassProc function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-defsubclassproc)
- [Subclassing Controls - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/subclassing-overview)
- [GetClassInfoEx function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclassinfoexw)
- [Using Window Classes - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-window-classes)
