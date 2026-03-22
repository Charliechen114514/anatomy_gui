# 通用GUI编程技术——Win32 原生编程实战（十）——对话框过程与窗口过程

> 在之前的文章里，我们已经聊过模态对话框的基本使用，但你可能有一个疑问在心里盘旋了很久：为什么对话框要有自己专门的"过程"函数？它和我们前面学的窗口过程到底有什么区别？说实话，这个问题困扰了我相当长的一段时间。当你习惯了用 `DefWindowProc` 处理默认消息，突然看到一个返回 `TRUE/FALSE` 的对话框过程，难免会感到困惑。今天我们就把这个话题彻底搞清楚。

---

## 为什么要专门聊这个区别

先说一件有点尴尬的事。在我刚开始学 Win32 的时候，我以为对话框过程就是窗口过程的一个别名，函数签名稍微不同而已。结果可想而知——我的对话框要么根本弹不出来，要么点了按钮没反应，要么焦点到处乱跑。后来查了半天才明白，这两个过程的设计思路完全是两回事。

很多教程会告诉你"对话框过程返回 TRUE/FALSE，窗口过程返回 LRESULT"，然后就跳过去了。但这个区别背后的设计哲学其实非常有趣。理解了这些，你写对话框的时候就会少踩很多坑，而且能明白为什么某些行为在对话框里是"自动"的，而在普通窗口里需要自己实现。

另一个现实原因是：当你需要用普通窗口模拟对话框行为的时候，或者需要在一个窗口里混用对话框和普通窗口的特性时，这些知识就会变得非常重要。比如你可能想要一个非模态的对话框，但又想保留 Tab 键导航和默认按钮处理。这时候你就得知道哪些特性是对话框管理器提供的，哪些需要自己实现。

这篇文章会从底层机制讲起，带着你理解对话框过程的"顾问"角色，以及它和窗口过程的本质区别。

---

## 环境说明

在正式开始之前，先说明一下我们这次实践的环境：

* **平台**：Windows 10/11（理论上 Windows Vista+ 都支持）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经对 Win32 编程有基本了解——知道什么是窗口过程、消息循环是怎么工作的、怎么处理基本的窗口消息。如果这些概念对你来说还比较陌生，建议先去翻翻前面的文章。

---

## 第一步——返回值的语义差异

让我们从最直观的区别开始：返回值。这是对话框过程和窗口过程最容易被混淆的地方，也是最容易出 bug 的地方。

### 窗口过程的返回值

普通窗口过程的函数签名是这样的：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        // 处理绘制
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

窗口过程返回 `LRESULT` 类型，本质上就是一个指针大小的整数（`LONG_PTR`）。这个返回值的含义取决于具体的消息类型：

* 对于大多数消息，返回 0 表示你已经处理了
* 有些消息需要返回特定的值（比如 `WM_GETTEXTLENGTH` 返回文本长度）
* 对于你不处理的消息，必须返回 `DefWindowProc` 的结果

关键点是：窗口过程是一个"处理链"的终点。你处理了就返回你的结果，没处理就交给 `DefWindowProc`，最终一定会有一个返回值。

### 对话框过程的返回值

对话框过程的签名看起来很像，但返回值类型不同：

```cpp
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        // 初始化对话框
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

对话框过程返回 `INT_PTR`（也是指针大小的整数），但这里的语义完全不同：

* 返回 `TRUE` 表示你**已经处理**了这个消息
* 返回 `FALSE` 表示你**没有处理**这个消息
* 不需要调用 `DefDlgProc`，因为对话框管理器会根据返回值决定是否调用

这是一个重要的设计差异：窗口过程是"必须返回一个结果"，而对话框过程是"告诉系统你处理了没有"。

⚠️ 注意

千万别在对话框过程的最后返回 `DefDlgProc` 的结果，或者忘记处理某些消息后返回 `FALSE`。这会导致对话框的行为完全异常。记住：对话框过程只负责说"我处理了"（TRUE）或"没处理"（FALSE），默认处理由对话框管理器接管。

### 为什么会有这个差异

这个设计差异源于对话框的历史。对话框最早的设计目的是简化常见 UI 任务的实现——比如属性页、输入框、确认对话框等等。这些场景有一些共同的模式：

* 需要处理一组特定的消息（初始化、按钮点击、输入验证）
* 大部分消息用默认行为就行
* 开发者只关心"我需要特殊处理什么"

所以对话框过程采用了一种"顾问"模式：你给对话框管理器提供建议（返回 TRUE 表示处理了，返回 FALSE 表示没意见），最终的决策由管理器来做。而窗口过程是"全权负责"模式：你要么处理所有消息，要么明确地委托给 `DefWindowProc`。

---

## 第二步——WM_INITDIALOG 与 WM_CREATE

对话框和窗口的初始化消息也不一样，这反映了它们不同的生命周期。

### WM_CREATE：窗口创建时

普通窗口在创建过程中会收到 `WM_CREATE` 消息：

```cpp
case WM_CREATE:
{
    CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
    // lParam 可以访问 CreateWindowEx 的最后一个参数
    // 这里可以做初始化，但窗口还没显示

    // 创建子控件、分配内存等等
    return 0;  // 返回 0 表示继续创建窗口
}
```

`WM_CREATE` 在 `CreateWindowEx` 返回之前发送，这时候窗口已经创建了但还没显示。如果返回 -1，窗口创建会失败。

### WM_INITDIALOG：对话框初始化时

对话框收到的是 `WM_INITDIALOG` 消息：

```cpp
case WM_INITDIALOG:
{
    // lParam 是 DialogBoxParam 的最后一个参数
    // 可以在这里初始化控件、设置默认值

    // 返回 TRUE：系统自动设置焦点到第一个 WS_TABSTOP 控件
    // 返回 FALSE：你需要自己用 SetFocus 设置焦点
    return TRUE;
}
```

`WM_INITDIALOG` 在对话框显示之前发送，但所有子控件都已经创建好了。这时候你可以安全地操作控件。

### 返回值的特殊约定

`WM_INITDIALOG` 的返回值有个特殊的约定，这可能是整个 Win32 API 里最让人困惑的细节之一：

* 返回 `TRUE`：系统会把键盘焦点设置到对话框中第一个具有 `WS_TABSTOP` 样式的控件
* 返回 `FALSE`：系统不会设置焦点，你需要自己调用 `SetFocus`

如果你想在初始化时把焦点设置到特定控件，应该这样做：

```cpp
case WM_INITDIALOG:
{
    // 设置焦点到某个特定控件
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_NAME);
    SetFocus(hEdit);

    // 必须返回 FALSE，告诉系统"我已经处理了焦点"
    return FALSE;
}
```

如果这时候返回 `TRUE`，系统会再次设置焦点，可能会覆盖你的设置。

### 为什么需要不同的初始化消息

一个原因是历史遗留：对话框比 `WM_CREATE` 更早出现。但更深层的原因是对话框的创建过程和普通窗口不同。对话框是从资源模板创建的，系统会解析资源文件、创建所有子控件、然后才发送 `WM_INITDIALOG`。这时候所有控件都已经存在，你可以直接操作它们。

而普通窗口创建时，子控件通常是在 `WM_CREATE` 里手动创建的。所以 `WM_CREATE` 更像是"给你一个机会在窗口完全创建之前做些准备工作"。

---

## 第三步——DefDlgProc 与 DefWindowProc

现在我们来聊聊默认处理函数。这是理解对话框和窗口区别的关键。

### DefWindowProc：窗口的默认行为

对于普通窗口，你不处理的消息要交给 `DefWindowProc`：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        // 处理绘制
        return 0;
    }
    // 必须调用 DefWindowProc
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

`DefWindowProc` 会执行所有默认的窗口行为：移动窗口、绘制边框、响应菜单点击等等。如果没有它，你的窗口基本不能用。

### DefDlgProc：对话框的默认行为

对话框也有一个类似的函数 `DefDlgProc`，但你通常不需要显式调用它：

```cpp
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
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
    // 对话框过程通常直接返回 FALSE
    // 对话框管理器会根据返回值决定是否调用 DefDlgProc
    return FALSE;
}
```

当你返回 `FALSE` 时，对话框管理器会自动调用 `DefDlgProc`。所以你不需要显式调用它。

### DefDlgProc 内部做了什么

`DefDlgProc` 做的事情比 `DefWindowProc` 更"智能"：

1. **Tab 键导航**：自动处理 Tab、Shift+Tab，在控件之间移动焦点
2. **默认按钮**：Enter 键触发默认按钮（`DEFPUSHBUTTON`）
3. **取消按钮**：Esc 键触发 ID 为 `IDCANCEL` 的按钮
4. **控件消息转发**：把来自控件的通知消息转发给对话框过程
5. **内部调用 DefWindowProc**：`DefDlgProc` 内部最终会调用 `DefWindowProc` 处理基础窗口消息

这也是为什么对话框会自动"有"这些行为，而普通窗口需要自己实现。这些功能都是 `DefDlgProc` 提供的。

### 对话框过程的"顾问"角色

现在我们可以理解对话框过程的设计哲学了。对话框过程实际上是一个"顾问"，而不是"决策者"：

* 对话框过程告诉系统"我处理了这个消息"（返回 TRUE）
* 或者"我没意见，你看着办"（返回 FALSE）
* 对话框管理器根据返回值决定是否调用 `DefDlgProc`
* `DefDlgProc` 才是真正的窗口过程，负责所有默认行为

这种设计简化了对话框的代码。大多数情况下，你只需要处理几个关键消息（初始化、按钮点击），剩下的交给系统。

---

## 第四步——Tab 键导航的内部机制

Tab 键导航是对话框的一个核心特性，但很多人不知道它具体是怎么工作的。让我们深入了解一下。

### WS_TABSTOP 样式

要让控件参与 Tab 导航，必须设置 `WS_TABSTOP` 样式：

```rc
EDITTEXT        IDC_EDIT_NAME, 55, 13, 150, 14, WS_TABSTOP
DEFPUSHBUTTON   "确定", IDOK, 110, 75, 50, 14, WS_TABSTOP
PUSHBUTTON      "取消", IDCANCEL, 165, 75, 50, 14, WS_TABSTOP
```

资源编辑器通常默认会给按钮和编辑框添加这个样式。

### Tab 导航是如何工作的

当用户在对话框里按下 Tab 键时，会发生以下过程：

1. 对话框收到 `WM_GETDLGCODE` 消息
2. 当前焦点控件返回 `DLGC_WANTTAB` 表示它不处理 Tab 键
3. 对话框管理器查找下一个具有 `WS_TABSTOP` 的控件
4. 调用 `SetFocus` 把焦点移到那个控件
5. 新焦点控件收到 `WM_SETFOCUS` 消息

这一切都是 `DefDlgProc` 自动处理的，你不需要写任何代码。

### WS_GROUP 样式

`WS_GROUP` 样式用于控制单选按钮组：

```rc
CONTROL         "男", IDC_RADIO_MALE, "Button", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 20, 30, 40, 10
CONTROL         "女", IDC_RADIO_FEMALE, "Button", BS_AUTORADIOBUTTON, 70, 30, 40, 10
CONTROL         "其他", IDC_RADIO_OTHER, "Button", BS_AUTORADIOBUTTON, 120, 30, 40, 10
```

`WS_GROUP` 标记了一个控件组的开始。当用户用方向键在单选按钮之间导航时，焦点会停留在同一个组内。Tab 键会跳到下一组的第一个控件。

### 在普通窗口里实现 Tab 导航

如果你想在普通窗口里实现类似的功能，需要自己处理：

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTabStops[10];  // 存储 WS_TABSTOP 控件的句柄
    static int nTabStopCount = 0;
    static int nCurrentFocus = 0;

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 创建控件时，记录哪些控件支持 Tab 导航
        hTabStops[nTabStopCount++] = CreateWindowEx(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            10, 10, 200, 20, hwnd, (HMENU)IDC_EDIT1, hInstance, NULL);

        hTabStops[nTabStopCount++] = CreateWindowEx(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            10, 40, 80, 30, hwnd, (HMENU)IDOK, hInstance, NULL);
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_TAB)
        {
            // 计算下一个焦点控件
            BOOL bShift = (GetKeyState(VK_SHIFT) & 0x8000);
            if (bShift)
            {
                nCurrentFocus = (nCurrentFocus - 1 + nTabStopCount) % nTabStopCount;
            }
            else
            {
                nCurrentFocus = (nCurrentFocus + 1) % nTabStopCount;
            }
            SetFocus(hTabStops[nCurrentFocus]);
            return 0;
        }
        break;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

这就解释了为什么对话框方便——你不需要自己实现这些逻辑。

---

## 第五步——默认按钮和取消按钮处理

对话框的另一个自动特性是默认按钮和取消按钮处理。这是很多开发者会忽略的细节。

### 默认按钮

资源文件里用 `DEFPUSHBUTTON` 定义的按钮是默认按钮：

```rc
DEFPUSHBUTTON   "确定", IDOK, 110, 75, 50, 14
```

当用户在对话框里按下 Enter 键时（焦点不在其他按钮或编辑框上），系统会自动点击默认按钮。这个过程由对话框管理器处理，你不需要写任何代码。

你可以在运行时更改默认按钮：

```cpp
// 把 IDOK 设置为默认按钮
SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDOK), TRUE);
```

### 取消按钮

ID 为 `IDCANCEL` 的按钮是取消按钮：

```rc
PUSHBUTTON      "取消", IDCANCEL, 165, 75, 50, 14
```

当用户按下 Esc 键时，系统会自动点击取消按钮。同样，这个行为是自动的。

### 编辑框里的 Enter 键

如果焦点在多行编辑框（`ES_MULTILINE`）上，Enter 键不会触发默认按钮，而是正常换行。这是对话框管理器的智能行为。

但如果焦点在单行编辑框上，Enter 键会触发默认按钮。如果你想在单行编辑框里"吃掉" Enter 键，可以处理 `WM_GETDLGCODE`：

```cpp
case WM_GETDLGCODE:
{
    // 告诉对话框管理器我们想要处理 Enter 键
    if (lParam && ((MSG*)lParam)->message == WM_KEYDOWN &&
        ((MSG*)lParam)->wParam == VK_RETURN)
    {
        return DLGC_WANTALLKEYS;
    }
    return DefDlgProc(hDlg, message, wParam, lParam);
}
```

不过这种方法比较复杂，更常见的做法是处理 `EN_CHANGE` 通知或子类化编辑框。

### 在普通窗口里实现这些行为

普通窗口需要自己处理 Enter 和 Esc 键：

```cpp
case WM_KEYDOWN:
{
    if (wParam == VK_RETURN)
    {
        // 模拟点击默认按钮
        SendMessage(hwnd, WM_COMMAND, MAKELONG(IDOK, BN_CLICKED), 0);
        return 0;
    }
    if (wParam == VK_ESCAPE)
    {
        // 模拟点击取消按钮
        SendMessage(hwnd, WM_COMMAND, MAKELONG(IDCANCEL, BN_CLICKED), 0);
        return 0;
    }
    break;
}
```

或者你可以用 `WM_NEXTDLGCTL` 消息，它实际上是对话框管理器使用的内部消息：

```cpp
// 把焦点移到特定控件
SendMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)hButton, TRUE);

// 把焦点移到下一个/上一个 WS_TABSTOP 控件
SendMessage(hwnd, WM_NEXTDLGCTL, 0, FALSE);  // 下一个
SendMessage(hwnd, WM_NEXTDLGCTL, 1, TRUE);   // 上一个
```

有趣的是，普通窗口也可以发送 `WM_NEXTDLGCTL` 来实现类似的导航行为。

---

## 第六步——什么时候可以用普通窗口模拟对话框

理解了这些区别后，你可能会问：既然对话框这么方便，为什么不一直用对话框？什么时候应该用普通窗口？

### 对话框的限制

对话框有一些限制，可能会影响你的设计选择：

1. **窗口样式受限**：对话框必须有一些特定的样式（`DS_MODALFRAME`、`WS_POPUP` 等），不能像普通窗口那样自由配置
2. **菜单栏不常见**：虽然有方法给对话框添加菜单，但不如普通窗口方便
3. **非模态对话框需要特殊处理**：非模态对话框的消息循环集成比模态对话框复杂
4. **自定义绘制受限**：虽然可以做，但对话框的默认绘制逻辑可能干扰你的自定义绘制

### 普通窗口的优势

普通窗口更灵活：

1. **完全控制窗口样式**：可以创建任何类型的窗口
2. **更容易实现复杂 UI**：比如工具栏、状态栏、分割窗口等
3. **更自然的 OOP 封装**：普通窗口更容易和 C++ 类结合
4. **自定义绘制更自由**：没有对话框管理器的默认行为干扰

### 混合方案

你也可以创建一个"看起来像对话框"的普通窗口：

```cpp
HWND hwnd = CreateWindowEx(
    0,
    L"MYWNDCLASS",
    L"设置",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
    CW_USEDEFAULT, CW_USEDEFAULT,
    400, 300,
    hwndParent,
    NULL,
    hInstance,
    NULL
);

// 禁止最大化/最小化
LONG style = GetWindowLong(hwnd, GWL_STYLE);
style &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
SetWindowLong(hwnd, GWL_STYLE, style);
SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

// 居中显示
HWND hParent = GetParent(hwnd);
if (hParent)
{
    RECT rcParent, rcDlg;
    GetWindowRect(hParent, &rcParent);
    GetWindowRect(hwnd, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left - rcDlg.right + rcDlg.left) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - rcDlg.bottom + rcDlg.top) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}
```

这样你就得到了一个"看起来像对话框"的普通窗口，可以享受普通窗口的灵活性。

---

## 第七步——完整示例对比

为了更直观地理解区别，我们来写一个完整的例子。我们会实现两个版本的"登录"界面：一个用对话框，一个用普通窗口。

### 资源文件（对话框版）

```rc
#include <windows.h>
#include "resource.h"

IDD_LOGIN_DLG DIALOGEX 0, 0, 250, 120
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "用户登录"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    LTEXT           "用户名：",IDC_STATIC,10,15,50,10
    EDITTEXT        IDC_EDIT_USERNAME,65,13,160,14,ES_AUTOHSCROLL
    LTEXT           "密码：",IDC_STATIC,10,40,50,10
    EDITTEXT        IDC_EDIT_PASSWORD,65,38,160,14,ES_AUTOHSCROLL | ES_PASSWORD
    DEFPUSHBUTTON   "登录",IDOK,65,75,80,14
    PUSHBUTTON      "取消",IDCANCEL,155,75,80,14
END
```

### 对话框过程

```cpp
#include <windows.h>
#include <string>
#include "resource.h"

struct LoginData
{
    std::wstring username;
    std::wstring password;
};

INT_PTR CALLBACK LoginDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // 设置焦点到用户名编辑框，并选中文本
        HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_USERNAME);
        SetFocus(hEdit);
        SendDlgItemMessage(hDlg, IDC_EDIT_USERNAME, EM_SETSEL, 0, -1);
        return FALSE;  // 我们手动设置了焦点，所以返回 FALSE
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            wchar_t username[256], password[256];

            // 获取用户输入
            GetDlgItemText(hDlg, IDC_EDIT_USERNAME, username, 256);
            GetDlgItemText(hDlg, IDC_EDIT_PASSWORD, password, 256);

            // 简单验证
            if (wcslen(username) == 0)
            {
                MessageBox(hDlg, L"请输入用户名", L"登录失败", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hDlg, IDC_EDIT_USERNAME));
                SendDlgItemMessage(hDlg, IDC_EDIT_USERNAME, EM_SETSEL, 0, -1);
                return TRUE;  // 已处理，不关闭对话框
            }

            if (wcslen(password) == 0)
            {
                MessageBox(hDlg, L"请输入密码", L"登录失败", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hDlg, IDC_EDIT_PASSWORD));
                SendDlgItemMessage(hDlg, IDC_EDIT_PASSWORD, EM_SETSEL, 0, -1);
                return TRUE;
            }

            // 保存数据
            LoginData* pData = (LoginData*)GetWindowLongPtr(hDlg, DWLP_USER);
            if (pData)
            {
                pData->username = username;
                pData->password = password;
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

// 调用对话框
BOOL ShowLoginDialog(HWND hwndParent, LoginData* pData)
{
    SetWindowLongPtr(hwndParent, DWLP_USER, (LONG_PTR)pData);

    INT_PTR result = DialogBoxParam(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_LOGIN_DLG),
        hwndParent,
        LoginDlgProc,
        (LPARAM)pData
    );

    return (result == IDOK);
}
```

### 普通窗口版本

```cpp
#include <windows.h>
#include <string>
#include "resource.h"

struct LoginData
{
    std::wstring username;
    std::wstring password;
};

// 全局数据（实际项目中应该用更好的方式管理）
static LoginData* g_pLoginData = nullptr;
static HWND g_hLoginDlg = NULL;

LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建控件
        CreateWindowEx(0, L"STATIC", L"用户名：",
            WS_CHILD | WS_VISIBLE,
            10, 15, 50, 14,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            65, 13, 160, 20,
            hwnd, (HMENU)IDC_EDIT_USERNAME, GetModuleHandle(NULL), NULL);

        CreateWindowEx(0, L"STATIC", L"密码：",
            WS_CHILD | WS_VISIBLE,
            10, 40, 50, 14,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_PASSWORD | WS_TABSTOP,
            65, 38, 160, 20,
            hwnd, (HMENU)IDC_EDIT_PASSWORD, GetModuleHandle(NULL), NULL);

        CreateWindowEx(0, L"BUTTON", L"登录",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            65, 75, 80, 24,
            hwnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

        CreateWindowEx(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            155, 75, 80, 24,
            hwnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

        // 设置初始焦点
        SetFocus(GetDlgItem(hwnd, IDC_EDIT_USERNAME));
        return 0;  // WM_CREATE 返回 0 表示成功
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            wchar_t username[256], password[256];

            GetDlgItemText(hwnd, IDC_EDIT_USERNAME, username, 256);
            GetDlgItemText(hwnd, IDC_EDIT_PASSWORD, password, 256);

            if (wcslen(username) == 0)
            {
                MessageBox(hwnd, L"请输入用户名", L"登录失败", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_USERNAME));
                SendDlgItemMessage(hwnd, IDC_EDIT_USERNAME, EM_SETSEL, 0, -1);
                return 0;
            }

            if (wcslen(password) == 0)
            {
                MessageBox(hwnd, L"请输入密码", L"登录失败", MB_OK | MB_ICONWARNING);
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_PASSWORD));
                SendDlgItemMessage(hwnd, IDC_EDIT_PASSWORD, EM_SETSEL, 0, -1);
                return 0;
            }

            if (g_pLoginData)
            {
                g_pLoginData->username = username;
                g_pLoginData->password = password;
            }

            DestroyWindow(hwnd);
            return 0;
        }

        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        // 手动处理 Enter 和 Esc 键
        if (wParam == VK_RETURN)
        {
            // 检查焦点是否在编辑框
            HWND hFocus = GetFocus();
            if (hFocus == GetDlgItem(hwnd, IDC_EDIT_USERNAME) ||
                hFocus == GetDlgItem(hwnd, IDC_EDIT_PASSWORD))
            {
                // 如果是单行编辑框，触发确定按钮
                SendMessage(hwnd, WM_COMMAND, MAKELONG(IDOK, BN_CLICKED), 0);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            SendMessage(hwnd, WM_COMMAND, MAKELONG(IDCANCEL, BN_CLICKED), 0);
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        // 通知父窗口对话框已关闭
        if (GetParent(hwnd))
        {
            PostMessage(GetParent(hwnd), WM_USER + 1, 0, 0);
        }
        g_hLoginDlg = NULL;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// 创建登录窗口（非模态）
HWND CreateLoginWindow(HWND hwndParent, LoginData* pData)
{
    g_pLoginData = pData;

    // 注册窗口类
    static BOOL bRegistered = FALSE;
    if (!bRegistered)
    {
        WNDCLASS wc = {};
        wc.lpfnWndProc = LoginWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"LOGINWNDCLASS";

        RegisterClass(&wc);
        bRegistered = TRUE;
    }

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0,
        L"LOGINWNDCLASS",
        L"用户登录",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        270, 180,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (hwnd)
    {
        // 居中显示
        if (hwndParent)
        {
            RECT rcParent, rcDlg;
            GetWindowRect(hwndParent, &rcParent);
            GetWindowRect(hwnd, &rcDlg);
            int x = rcParent.left + (rcParent.right - rcParent.left - rcDlg.right + rcDlg.left) / 2;
            int y = rcParent.top + (rcParent.bottom - rcParent.top - rcDlg.bottom + rcDlg.top) / 2;
            SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        ShowWindow(hwnd, SW_SHOW);
    }

    return hwnd;
}
```

### 对比总结

对比两个版本，你会发现：

1. **对话框版代码更简洁**：不需要手动创建控件、不需要处理 Tab 键、Enter/Esc 键是自动的
2. **普通窗口版更灵活**：可以完全控制窗口样式、可以是非模态的、更容易自定义
3. **对话框版的返回值处理更简单**：`DialogBoxParam` 直接返回结果，普通窗口需要用消息通信

选择哪个版本取决于你的具体需求。对于简单的输入对话框，对话框版本更合适。对于复杂的设置界面或需要自定义行为的场景，普通窗口更灵活。

---

## 后续可以做什么

到这里，对话框过程和窗口过程的区别应该讲清楚了。你现在应该理解了：

* 为什么对话框过程返回 TRUE/FALSE
* `WM_INITDIALOG` 和 `WM_CREATE` 的区别
* `DefDlgProc` 内部做了什么
* Tab 导航和默认按钮是如何工作的
* 什么时候应该用普通窗口模拟对话框

在继续深入之前，建议你做些练习巩固一下：

1. **创建一个自定义对话框**，处理 `WM_CTLCOLORSTATIC` 来改变静态文本的颜色
2. **实现一个普通窗口**，手动处理 Tab 键导航，体验一下对话框管理器帮你省了多少事
3. **子类化对话框里的编辑框**，拦截某些按键行为
4. **研究 `WM_NEXTDLGCTRL` 消息**，看看如何在普通窗口里复用对话框的导航逻辑

下一步，我们可以探讨更高级的主题：属性表、向导对话框、或者无模式对话框的消息循环集成。这些都是建立在今天所学基础之上的扩展。

---

## 相关资源

- [DefDlgProc 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-defdlgproca)
- [DefWindowProc 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-defwindowproca)
- [WM_INITDIALOG 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/wm-initdialog)
- [DialogBox 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-dialogboxa)
- [使用对话框 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/using-dialog-boxes)
- [WM_GETDLGCODE 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/wm-getdlgcode)
- [WM_NEXTDLGCTL 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/wm-nextdlgctl)
