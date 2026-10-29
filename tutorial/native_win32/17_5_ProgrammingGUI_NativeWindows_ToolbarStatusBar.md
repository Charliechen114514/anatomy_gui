# 通用GUI编程技术——Win32 原生编程实战（十七）——工具栏与状态栏

> 上一篇文章我们学会了用 Visual Studio 资源编辑器可视化地设计界面，菜单、对话框、图标这些资源都能方便地拖拽编辑了。但你可能注意到了一个细节：我在最后那个记事本示例里用了一个 Status Bar（状态栏），代码里直接写了个 `STATUSCLASSNAME` 创建控件，但压根没解释这东西是怎么工作的。而且如果你做过一些桌面应用，你会发现几乎每个正经的 Windows 程序在菜单栏下面都有一排工具栏按钮，在窗口底部都有一行状态栏。今天我们就把这两个"看起来简单、实际坑不少"的控件彻底讲清楚。

---

## 为什么需要工具栏和状态栏

说实话，我刚开始写 Win32 程序的时候，觉得菜单就够了，工具栏和状态栏都是锦上添花的东西。但很快就发现不是这么回事。

工具栏解决了菜单的"操作距离"问题。假设你的程序有十几个常用功能，用户每次都要点菜单 → 找子菜单 → 点击菜单项，三级操作才能完成一个动作。工具栏把这些高频操作直接放在手边，一次点击就行。更重要的是，工具栏上的图标比菜单文字更直观，用户扫一眼就知道每个按钮是干什么的。

状态栏解决的是"信息反馈"问题。用户选中了一段文字，多长？当前光标在第几行第几列？文件有没有未保存的修改？这些信息如果用弹窗提示会很烦人，如果用对话框又太重了。状态栏就是在窗口底部默默展示这些信息的地方，不打扰用户工作，但需要的时候一眼就能看到。

从技术角度说，工具栏和状态栏都属于 Windows 通用控件（Common Controls），需要使用 `InitCommonControlsEx` 初始化。它们虽然不像 ListView、TreeView 那么复杂，但有很多细节值得深入了解。而且它们经常配合菜单、快捷键一起使用，是构建完整 Windows 应用界面的关键组成部分。

这篇文章会带你从零开始，把工具栏和状态栏的创建、使用、自定义彻底搞透。我们不只是知道"怎么创建"，更重要的是理解"怎么配合其他 UI 元素协同工作"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（需要 Common Controls 版本 6.0+）
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）
* **额外依赖**：需要链接 `Comctl32.lib`

代码假设你已经熟悉前面文章的内容——至少知道怎么创建窗口、怎么处理 WM_COMMAND 和 WM_NOTIFY 消息、怎么用资源编辑器。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

⚠️ 注意

使用通用控件需要在代码里调用 `InitCommonControlsEx`，或者在资源文件里包含一个 manifest 指定使用 6.0 版本的 Common Controls。如果你不这样做，工具栏和状态栏可能能创建成功，但样式会是很老的 Windows 2000 风格，而不是现代的视觉效果。我们后面会详细讲怎么配置。

---

## 第一步——通用控件库的初始化

在创建工具栏和状态栏之前，我们需要先解决一个前置问题：通用控件库的初始化。这个问题让很多新手踩过坑，值得专门拿出来说。

### InitCommonControlsEx

Windows 的通用控件（包括工具栏、状态栏、ListView、TreeView、ProgressBar 等）都实现在 `Comctl32.dll` 里。但这个 DLL 默认不会被加载到你的进程中，你需要显式地告诉系统"我要用这些控件"。

方法就是调用 `InitCommonControlsEx`：

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// 在 WinMain 的最开始调用
INITCOMMONCONTROLSEX icc = {};
icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
icc.dwICC = ICC_BAR_CLASSES;  // 初始化工具栏和状态栏类
InitCommonControlsEx(&icc);
```

`dwICC` 参数指定你要初始化哪些控件类。常用的值有：

| 值 | 包含的控件 |
|---|---|
| `ICC_BAR_CLASSES` | Toolbar、StatusBar、ToolTip |
| `ICC_LISTVIEW_CLASSES` | ListView、Header |
| `ICC_TREEVIEW_CLASSES` | TreeView、ToolTip |
| `ICC_TAB_CLASSES` | TabControl |
| `ICC_PROGRESS_CLASS` | ProgressBar |
| `ICC_WIN95_CLASSES` | 以上所有控件 |

你可以组合多个值，用位或运算符 `|` 连接。如果你不确定需要哪些，直接用 `ICC_WIN95_CLASSES` 就行了，它会初始化所有标准通用控件。

⚠️ 注意

还有一个老版本的函数叫 `InitCommonControls`（没有 `Ex` 后缀），它等价于 `InitCommonControlsEx` 并传入 `ICC_WIN95_CLASSES`。虽然这个函数还能用，但微软已经标记为废弃了，建议使用 `InitCommonControlsEx`。

### Visual Styles（视觉样式）

如果你想让工具栏和状态栏看起来是现代的 Windows 风格（圆角按钮、半透明效果等），而不像 Windows 2000 那样方正和素朴，你需要启用 Visual Styles。

最简单的方式是在你的代码里加一行 pragma：

```cpp
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
```

这行 pragma 会让链接器在你的 exe 中嵌入一个 manifest，告诉系统使用 6.0 版本的 Common Controls。没有这行的话，工具栏按钮会是经典的凸起 3D 边框样式；有了这行，按钮会是现代的扁平风格，鼠标悬停时有微妙的高亮效果。

如果你不想用 pragma，也可以手动创建一个 manifest 文件（.manifest 后缀），然后在资源文件里包含它：

```
// 在 .rc 文件里
CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "YourApp.manifest"
```

manifest 文件的内容是一段 XML，定义了你的程序依赖的组件版本。具体的 XML 内容可以在微软文档里找到模板，这里就不展开了。

---

## 第二步——创建工具栏

工具栏（Toolbar）是一组按钮的容器，通常放在窗口顶部、菜单栏下方。用户点击工具栏按钮就相当于点击对应的菜单项。

### 基本创建方式

创建工具栏最简单的方式是使用 `CreateToolbarEx` 函数，但微软已经标记为废弃了。推荐的方式是先创建一个空的工具栏窗口，然后逐步添加按钮。

```cpp
#include <commctrl.h>

HWND hToolbar = CreateWindowEx(
    0,                          // 扩展样式
    TOOLBARCLASSNAME,           // 工具栏窗口类名
    NULL,                       // 标题（工具栏不用）
    WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS,  // 样式
    0, 0,                       // 位置（自动调整）
    0, 0,                       // 大小（自动调整）
    hwndParent,                 // 父窗口
    (HMENU)IDC_TOOLBAR,         // 控件 ID
    hInstance,                  // 实例句柄
    NULL                        // 创建参数
);
```

几个重要的样式标志：

* **WS_CHILD | WS_VISIBLE**：子窗口且可见，必须有
* **TBSTYLE_TOOLTIPS**：启用工具提示（鼠标悬停时显示文字提示）
* **TBSTYLE_FLAT**：扁平风格按钮（推荐，看起来更现代）
* **TBSTYLE_LIST**：按钮文字显示在图标右侧（默认文字在图标下方）
* **CCS_NODIVIDER**：去掉工具栏顶部的分隔线
* **CCS_NORESIZE**：禁止系统自动调整工具栏大小

创建后，你需要发送一个 `TB_BUTTONSTRUCTSIZE` 消息，告诉工具栏 `TBBUTTON` 结构体的大小。这是一个"仪式性"的操作，文档里说"确保系统使用正确的结构体版本"：

```cpp
SendMessage(hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
```

### 添加按钮

工具栏的按钮用 `TBBUTTON` 结构体来描述：

```cpp
typedef struct {
    int       iBitmap;    // 按钮图像的索引
    int       idCommand;  // 点击时发送的命令 ID
    BYTE      fsState;    // 按钮状态
    BYTE      fsStyle;    // 按钮样式
    DWORD_PTR dwData;     // 自定义数据
    INT_PTR   iString;    // 按钮文字的索引或字符串指针
} TBBUTTON;
```

最关键的三个字段：

* **iBitmap**：按钮图像在图像列表中的索引。如果你没有图像列表，可以用系统预定义的位图（后面会讲）。
* **idCommand**：当用户点击这个按钮时，工具栏会向父窗口发送 `WM_COMMAND` 消息，`wParam` 的低位就是这个 ID。你可以直接复用菜单项的 ID，这样点击工具栏按钮和点击菜单项效果完全一样。
* **fsStyle**：按钮的样式，常见的有：
  * `TBSTYLE_BUTTON`：普通按钮
  * `TBSTYLE_SEP`：分隔符（一条竖线或空白间隔）
  * `TBSTYLE_CHECK`：切换按钮（按下后保持按下状态）
  * `TBSTYLE_DROPDOWN`：下拉按钮（点击显示菜单）
  * `TBSTYLE_AUTOSIZE`：按钮宽度根据文字自动调整

* **fsState**：初始状态，通常设为 `TBSTATE_ENABLED` 表示启用。

添加一组按钮的代码：

```cpp
// 定义按钮数组
TBBUTTON buttons[] = {
    { 0, ID_FILE_NEW,    TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"新建" },
    { 1, ID_FILE_OPEN,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"打开" },
    { 2, ID_FILE_SAVE,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"保存" },
    { 0, 0,              TBSTATE_ENABLED, TBSTYLE_SEP,    {0}, 0, 0 },  // 分隔符
    { 3, ID_EDIT_CUT,    TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"剪切" },
    { 4, ID_EDIT_COPY,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"复制" },
    { 5, ID_EDIT_PASTE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"粘贴" },
};

// 添加按钮
SendMessage(hToolbar, TB_ADDBUTTONS, _countof(buttons), (LPARAM)buttons);
```

### 使用系统标准位图

如果你不想自己画图标，Windows 提供了一套标准位图可以直接用。通过发送 `TB_LOADIMAGES` 消息加载：

```cpp
// 加载标准的小尺寸位图（16x16）
SendMessage(hToolbar, TB_LOADIMAGES, IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);

// 加载标准的大尺寸位图（24x24）
SendMessage(hToolbar, TB_LOADIMAGES, IDB_STD_LARGE_COLOR, (LPARAM)HINST_COMMCTRL);
```

系统标准位图包含以下图像，对应的索引是预定义的常量：

| 索引常量 | 图像 |
|---|---|
| `STD_CUT` | 剪切 |
| `STD_COPY` | 复制 |
| `STD_PASTE` | 粘贴 |
| `STD_UNDO` | 撤销 |
| `STD_REDOW` | 重做 |
| `STD_DELETE` | 删除 |
| `STD_FILENEW` | 新建文件 |
| `STD_FILEOPEN` | 打开文件 |
| `STD_FILESAVE` | 保存文件 |
| `STD_PRINT` | 打印 |

使用系统标准位图时，`TBBUTTON` 的 `iBitmap` 字段直接用这些常量：

```cpp
TBBUTTON buttons[] = {
    { STD_FILENEW,  ID_FILE_NEW,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"新建" },
    { STD_FILEOPEN, ID_FILE_OPEN,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"打开" },
    { STD_FILESAVE, ID_FILE_SAVE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"保存" },
    { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0 },
    { STD_CUT,   ID_EDIT_CUT,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"剪切" },
    { STD_COPY,  ID_EDIT_COPY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"复制" },
    { STD_PASTE, ID_EDIT_PASTE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"粘贴" },
};
```

这样你不用准备任何图标资源，就能得到一套标准的工具栏按钮。适合快速出原型或者做内部工具。

### 使用自定义图像列表

当你需要更专业的图标时，可以创建自己的图像列表（ImageList），然后关联到工具栏：

```cpp
// 创建图像列表（16x16 像素，32 位色深）
HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_COLOR32, 8, 4);

// 加载图标并添加到图像列表
HICON hIcon1 = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_NEW),
                                 IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
HICON hIcon2 = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_OPEN),
                                 IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
ImageList_AddIcon(hImageList, hIcon1);
ImageList_AddIcon(hImageList, hIcon2);
DestroyIcon(hIcon1);
DestroyIcon(hIcon2);

// 关联图像列表到工具栏
SendMessage(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
```

⚠️ 注意

`ImageList_AddIcon` 会复制图标数据，所以添加完后你可以安全地 `DestroyIcon`。但图像列表本身需要在工具栏销毁时（或程序退出前）用 `ImageList_Destroy` 释放。

### 自适应大小

工具栏创建后，需要告诉它根据按钮数量自动调整大小。发送 `TB_AUTOSIZE` 消息即可：

```cpp
SendMessage(hToolbar, TB_AUTOSIZE, 0, 0);
```

这个消息应该在你添加完所有按钮后发送。如果你后来又动态添加或删除了按钮，也需要再发一次。

更关键的问题是：工具栏占据了窗口顶部的一部分空间，你的主内容区域需要相应地缩小。标准的做法是在 `WM_SIZE` 消息里获取工具栏的矩形区域，然后调整主内容区域的位置和大小。我们会在完整示例里展示这个模式。

---

## 第三步——创建状态栏

状态栏（Status Bar）通常放在窗口底部，用来显示程序状态信息。和工具栏一样，它也是通用控件的一部分。

### 基本创建方式

```cpp
HWND hStatus = CreateWindowEx(
    0,                          // 扩展样式
    STATUSCLASSNAME,            // 状态栏窗口类名
    NULL,                       // 标题
    WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,  // 样式
    0, 0, 0, 0,                 // 位置和大小（自动调整）
    hwndParent,                 // 父窗口
    (HMENU)IDC_STATUSBAR,       // 控件 ID
    hInstance,                  // 实例句柄
    NULL                        // 创建参数
);
```

重要的样式标志：

* **SBARS_SIZEGRIP**：在状态栏右下角显示一个大小调整手柄（如果窗口可调整大小的话）
* **SBARS_TOOLTIPS**：启用工具提示

创建状态栏时不需要指定位置和大小，系统会自动把它放到窗口底部并铺满宽度。当窗口大小改变时，状态栏也会自动调整宽度（但你需要处理高度相关的布局）。

### 设置文本

状态栏最基本的操作就是显示文字：

```cpp
// 设置整个状态栏的文字
SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"就绪");
```

但更有用的功能是把状态栏分成多个区域（part），每个区域显示不同的信息。

### 分区显示

你可以用 `SB_SETPARTS` 消息把状态栏分成多个区域：

```cpp
// 定义每个区域右边缘的 x 坐标
// -1 表示最后一个区域延伸到状态栏右边缘
int parts[] = { 200, 350, -1 };
SendMessage(hStatus, SB_SETPARTS, _countof(parts), (LPARAM)parts);
```

这段代码把状态栏分成了三个区域：第一个区域宽 200 像素，第二个从 200 到 350，第三个占据剩余空间（-1 表示延伸到右边缘）。

设置好分区后，用 `SB_SETTEXT` 指定区域索引来设置文字：

```cpp
// 区域索引从 0 开始
SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"就绪");
SendMessage(hStatus, SB_SETTEXT, 1, (LPARAM)L"第 1 行，第 1 列");
SendMessage(hStatus, SB_SETTEXT, 2, (LPARAM)L"UTF-8");
```

⚠️ 注意

`SB_SETTEXT` 的 `wParam` 低位是区域索引，高位可以附加绘制类型标志。最常用的是 `SBT_NOBORDERS`（去掉区域的 3D 边框）和 `SBT_OWNERDRAW`（自绘模式）。如果你不需要这些，直接传索引就行。

### 响应窗口大小变化

当窗口大小改变时，你需要重新计算状态栏各区域的宽度并更新：

```cpp
case WM_SIZE:
{
    // 让状态栏自适应宽度
    SendMessage(hStatus, WM_SIZE, 0, 0);

    // 获取状态栏高度
    RECT rcStatus;
    GetWindowRect(hStatus, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;

    // 获取工具栏高度
    RECT rcToolbar;
    GetWindowRect(hToolbar, &rcToolbar);
    int toolbarHeight = rcToolbar.bottom - rcToolbar.top;

    // 调整主内容区域
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    MoveWindow(hEdit,
               0, toolbarHeight,
               rcClient.right,
               rcClient.bottom - toolbarHeight - statusHeight,
               TRUE);

    return 0;
}
```

这段代码的思路很清晰：先让状态栏自己处理 WM_SIZE（它会自动调整宽度），然后计算工具栏和状态栏的高度，最后把主内容区域夹在两者之间。

如果你希望状态栏的分区域宽度也跟着窗口宽度自适应（比如第一个区域占 30%，第二个占 20%，第三个占剩余），你需要在 WM_SIZE 里重新计算坐标并发送 `SB_SETPARTS`：

```cpp
case WM_SIZE:
{
    SendMessage(hStatus, WM_SIZE, 0, 0);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int width = rcClient.right;

    // 自适应分区宽度
    int parts[] = { width / 3, width * 2 / 3, -1 };
    SendMessage(hStatus, SB_SETPARTS, _countof(parts), (LPARAM)parts);

    return 0;
}
```

### Simple 模式

有时候你不需要分区域，只想显示一行文字。可以用 `SB_SIMPLE` 消息切换到简单模式：

```cpp
// 进入简单模式
SendMessage(hStatus, SB_SIMPLE, TRUE, 0);
SendMessage(hStatus, SB_SETTEXT, 255 | SBT_NOBORDERS, (LPARAM)L"处理中，请稍候...");

// 退出简单模式
SendMessage(hStatus, SB_SIMPLE, FALSE, 0);
```

简单模式下，状态栏只显示一个区域，索引固定是 255。简单模式常用于显示"正在处理..."之类的临时信息，处理完后退出简单模式恢复分区显示。

---

## 第四步——处理工具栏通知

工具栏不只是发 `WM_COMMAND`，它还会发 `WM_NOTIFY` 通知消息来告诉你一些事件。

### 工具提示（Tooltip）

如果在创建工具栏时加了 `TBSTYLE_TOOLTIPS` 样式，系统会自动创建一个 Tooltip 控件。当鼠标悬停在按钮上时，工具栏会向父窗口发送 `TTN_GETDISPINFO` 通知，让你提供提示文字：

```cpp
case WM_NOTIFY:
{
    NMHDR* pnmh = (NMHDR*)lParam;

    if (pnmh->code == TTN_GETDISPINFO)
    {
        NMTTDISPINFO* pttdi = (NMTTDISPINFO*)lParam;

        // 根据命令 ID 返回对应的提示文字
        switch (pttdi->hdr.idFrom)
        {
        case ID_FILE_NEW:
            pttdi->lpszText = (LPWSTR)L"新建文件 (Ctrl+N)";
            break;
        case ID_FILE_OPEN:
            pttdi->lpszText = (LPWSTR)L"打开文件 (Ctrl+O)";
            break;
        case ID_FILE_SAVE:
            pttdi->lpszText = (LPWSTR)L"保存文件 (Ctrl+S)";
            break;
        case ID_EDIT_CUT:
            pttdi->lpszText = (LPWSTR)L"剪切 (Ctrl+X)";
            break;
        case ID_EDIT_COPY:
            pttdi->lpszText = (LPWSTR)L"复制 (Ctrl+C)";
            break;
        case ID_EDIT_PASTE:
            pttdi->lpszText = (LPWSTR)L"粘贴 (Ctrl+V)";
            break;
        }
        return 0;
    }

    break;
}
```

这个通知机制的工作原理是：鼠标进入工具栏按钮区域时，Tooltip 控件向父窗口请求文字，你通过设置 `lpszText` 字段提供文字内容，Tooltip 控件会自动显示。你不需要手动管理 Tooltip 的显示和隐藏。

### 下拉按钮通知

如果你用了 `TBSTYLE_DROPDOWN` 样式的按钮，当用户点击按钮旁边的下拉箭头时，工具栏会发送 `TBN_DROPDOWN` 通知：

```cpp
case WM_NOTIFY:
{
    NMHDR* pnmh = (NMHDR*)lParam;

    if (pnmh->hwndFrom == hToolbar && pnmh->code == TBN_DROPDOWN)
    {
        NMTOOLBAR* pnmtb = (NMTOOLBAR*)lParam;

        if (pnmtb->iItem == ID_FILE_OPEN)
        {
            // 在按钮下方显示一个弹出菜单
            RECT rc;
            SendMessage(hToolbar, TB_GETRECT, ID_FILE_OPEN, (LPARAM)&rc);
            MapWindowPoints(hToolbar, NULL, (LPPOINT)&rc, 2);

            HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_RECENT_MENU));
            HMENU hSubMenu = GetSubMenu(hMenu, 0);
            TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
                          rc.left, rc.bottom, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            return TBDDRET_DEFAULT;
        }
    }

    break;
}
```

这段代码展示了如何实现"最近打开的文件"下拉列表。先通过 `TB_GETRECT` 获取按钮在屏幕上的位置，然后在按钮下方弹出菜单。

### 自定义绘制通知

如果你需要对工具栏按钮进行自定义绘制，可以处理 `NM_CUSTOMDRAW` 通知。这个通知提供了绘制各个阶段的机会，让你可以精细控制按钮的外观：

```cpp
case WM_NOTIFY:
{
    NMHDR* pnmh = (NMHDR*)lParam;

    if (pnmh->code == NM_CUSTOMDRAW && pnmh->hwndFrom == hToolbar)
    {
        NMCUSTOMDRAW* pcd = (NMCUSTOMDRAW*)lParam;

        switch (pcd->dwDrawStage)
        {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
        {
            // 可以在这里修改按钮的绘制参数
            // 比如改变文字颜色、背景色等
            return CDRF_DODEFAULT;
        }
        }

        return CDRF_DODEFAULT;
    }

    break;
}
```

自定义绘制是一个比较大的话题，这里只是简单提一下。等你学会了 GDI 绘图（后面几篇文章），你就能做更多事情了。

---

## 第五步——菜单、工具栏与命令 ID 的统一

到这里你可能已经注意到一个关键模式：工具栏按钮和菜单项使用相同的命令 ID。这不是巧合，而是 Win32 程序设计的标准模式。

### 统一命令处理

当用户点击工具栏按钮时，工具栏会发送 `WM_COMMAND` 消息，`wParam` 的低位是按钮的 `idCommand`。当用户点击菜单项时，系统也会发送 `WM_COMMAND` 消息，`wParam` 的低位是菜单项的 ID。

所以如果你让工具栏按钮和菜单项使用相同的 ID，你的命令处理代码只需要写一份：

```cpp
case WM_COMMAND:
{
    switch (LOWORD(wParam))
    {
    case ID_FILE_NEW:
        // 不管用户是点菜单、点工具栏还是按快捷键，都执行同一份代码
        OnFileNew();
        break;
    case ID_FILE_OPEN:
        OnFileOpen();
        break;
    // ... 其他命令
    }
    return 0;
}
```

这是 Win32 程序设计的一个重要原则：**界面元素通过命令 ID 解耦**。菜单、工具栏、快捷键、甚至右键菜单，它们都是"命令的触发方式"，而命令的处理逻辑是统一的。

### 同步启用/禁用状态

当你需要禁用某个功能时，你需要同时更新菜单项和工具栏按钮的状态：

```cpp
void EnableFileSave(BOOL enable)
{
    // 更新菜单
    EnableMenuItem(hMenu, ID_FILE_SAVE,
                   enable ? MF_ENABLED : MF_GRAYED);

    // 更新工具栏按钮
    SendMessage(hToolbar, TB_ENABLEBUTTON, ID_FILE_SAVE, enable);

    // 更新状态栏提示
    if (!enable)
    {
        SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"无文件打开");
    }
}
```

如果你有较多的命令需要管理，可以考虑写一个辅助函数或者维护一个命令状态表，集中管理所有 UI 元素的启用/禁用状态。

---

## 第六步——完整示例

我们来看一个完整的例子，把今天讲的所有知识都用上。这个程序是一个简单的文本编辑器，有菜单栏、工具栏和状态栏。

### 资源定义

首先，在 resource.h 里定义 ID：

```cpp
// resource.h
#pragma once

#define IDR_MENU_MAIN       101
#define IDR_TOOLBAR_MAIN    102
#define IDI_NEW             201
#define IDI_OPEN            202
#define IDI_SAVE            203
#define IDI_CUT             204
#define IDI_COPY            205
#define IDI_PASTE           206

#define IDC_EDIT            301
#define IDC_TOOLBAR         302
#define IDC_STATUSBAR       303

// 菜单命令
#define ID_FILE_NEW         1001
#define ID_FILE_OPEN        1002
#define ID_FILE_SAVE        1003
#define ID_FILE_EXIT        1004
#define ID_EDIT_CUT         1005
#define ID_EDIT_COPY        1006
#define ID_EDIT_PASTE       1007
#define ID_EDIT_UNDO        1008
#define ID_HELP_ABOUT       1009
```

### 完整代码

```cpp
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateStatusBar(HWND hStatus, HWND hEdit);
void UpdateLineInfo(HWND hStatus, HWND hEdit);

HWND hToolbar = NULL;
HWND hStatus = NULL;
HWND hEdit = NULL;
HINSTANCE g_hInst = NULL;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    g_hInst = hInstance;

    // 初始化通用控件
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ToolbarDemoClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_MAIN);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"ToolbarDemoClass", L"工具栏与状态栏示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // ---- 创建工具栏 ----
        hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER,
            0, 0, 0, 0, hwnd, (HMENU)IDC_TOOLBAR, g_hInst, NULL);

        SendMessage(hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

        // 使用系统标准位图
        SendMessage(hToolbar, TB_LOADIMAGES, IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);

        // 添加按钮
        TBBUTTON buttons[] = {
            { STD_FILENEW,  ID_FILE_NEW,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { STD_FILEOPEN, ID_FILE_OPEN,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { STD_FILESAVE, ID_FILE_SAVE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0 },
            { STD_CUT,   ID_EDIT_CUT,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { STD_COPY,  ID_EDIT_COPY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { STD_PASTE, ID_EDIT_PASTE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
            { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0 },
            { STD_UNDO,  ID_EDIT_UNDO,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 },
        };
        SendMessage(hToolbar, TB_ADDBUTTONS, _countof(buttons), (LPARAM)buttons);
        SendMessage(hToolbar, TB_AUTOSIZE, 0, 0);

        // ---- 创建编辑框 ----
        hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_EDIT, g_hInst, NULL);

        // ---- 创建状态栏 ----
        hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, g_hInst, NULL);

        // 设置状态栏分区
        int parts[] = { 200, 400, -1 };
        SendMessage(hStatus, SB_SETPARTS, _countof(parts), (LPARAM)parts);

        // 初始状态
        SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"就绪");
        SendMessage(hStatus, SB_SETTEXT, 1, (LPARAM)L"第 1 行，第 1 列");
        SendMessage(hStatus, SB_SETTEXT, 2, (LPARAM)L"UTF-8");

        return 0;
    }

    case WM_SIZE:
    {
        // 让工具栏和状态栏自适应
        SendMessage(hToolbar, TB_AUTOSIZE, 0, 0);
        SendMessage(hStatus, WM_SIZE, 0, 0);

        // 获取工具栏高度
        RECT rcToolbar;
        GetWindowRect(hToolbar, &rcToolbar);
        int toolbarHeight = rcToolbar.bottom - rcToolbar.top;

        // 获取状态栏高度
        RECT rcStatus;
        GetWindowRect(hStatus, &rcStatus);
        int statusHeight = rcStatus.bottom - rcStatus.top;

        // 调整编辑框位置和大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        MoveWindow(hEdit, 0, toolbarHeight,
                   rcClient.right,
                   rcClient.bottom - toolbarHeight - statusHeight,
                   TRUE);

        // 重新计算状态栏分区宽度
        int width = rcClient.right;
        int parts[] = { width / 4, width / 2, -1 };
        SendMessage(hStatus, SB_SETPARTS, _countof(parts), (LPARAM)parts);

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_FILE_NEW:
            SetWindowText(hEdit, L"");
            SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"新建文件");
            break;

        case ID_FILE_OPEN:
            MessageBox(hwnd, L"打开文件功能待实现", L"提示", MB_OK);
            break;

        case ID_FILE_SAVE:
            MessageBox(hwnd, L"保存文件功能待实现", L"提示", MB_OK);
            break;

        case ID_FILE_EXIT:
            DestroyWindow(hwnd);
            break;

        case ID_EDIT_CUT:
            SendMessage(hEdit, WM_CUT, 0, 0);
            break;

        case ID_EDIT_COPY:
            SendMessage(hEdit, WM_COPY, 0, 0);
            break;

        case ID_EDIT_PASTE:
            SendMessage(hEdit, WM_PASTE, 0, 0);
            break;

        case ID_EDIT_UNDO:
            SendMessage(hEdit, WM_UNDO, 0, 0);
            break;

        case ID_HELP_ABOUT:
            MessageBox(hwnd, L"工具栏与状态栏示例程序\n版本 1.0",
                       L"关于", MB_OK | MB_ICONINFORMATION);
            break;
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        NMHDR* pnmh = (NMHDR*)lParam;

        // 工具提示
        if (pnmh->code == TTN_GETDISPINFO)
        {
            NMTTDISPINFO* pttdi = (NMTTDISPINFO*)lParam;
            switch (pttdi->hdr.idFrom)
            {
            case ID_FILE_NEW:   pttdi->lpszText = (LPWSTR)L"新建 (Ctrl+N)"; break;
            case ID_FILE_OPEN:  pttdi->lpszText = (LPWSTR)L"打开 (Ctrl+O)"; break;
            case ID_FILE_SAVE:  pttdi->lpszText = (LPWSTR)L"保存 (Ctrl+S)"; break;
            case ID_EDIT_CUT:   pttdi->lpszText = (LPWSTR)L"剪切 (Ctrl+X)"; break;
            case ID_EDIT_COPY:  pttdi->lpszText = (LPWSTR)L"复制 (Ctrl+C)"; break;
            case ID_EDIT_PASTE: pttdi->lpszText = (LPWSTR)L"粘贴 (Ctrl+V)"; break;
            case ID_EDIT_UNDO:  pttdi->lpszText = (LPWSTR)L"撤销 (Ctrl+Z)"; break;
            }
            return 0;
        }

        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

编译运行这个程序，你会看到一个带有菜单栏、工具栏和状态栏的文本编辑器窗口。工具栏使用了系统标准位图，按钮排列整齐；状态栏分成了三个区域，分别显示"操作状态"、"光标位置"和"编码格式"。编辑框夹在工具栏和状态栏之间，窗口大小改变时三者会自动调整布局。

这个示例展示了几个关键的设计模式：
- 工具栏按钮和菜单项共享相同的命令 ID，命令处理只写一份
- WM_SIZE 里统一处理工具栏、状态栏、主内容的布局
- 状态栏分区宽度随窗口宽度自适应
- 通过 WM_NOTIFY 处理工具提示

---

## 后续可以做什么

到这里，工具栏和状态栏的基础知识就讲完了。你现在应该能够创建带工具提示的工具栏、创建分区域的状态栏、理解命令 ID 统一的设计模式、正确处理窗口布局。但 Windows 的 UI 世界远不止这些，还有很多高级主题等着我们去探索。

下一篇文章，我们会进入 **GDI 绘图的世界**——学习怎么在窗口上绘制图形、文字、图片。这是实现自定义控件、图表显示、游戏开发的基础。我们会介绍设备上下文（HDC）、GDI 对象（画笔、画刷、字体）、双缓冲技术等内容。有了 GDI 知识，你就能进一步自定义工具栏和状态栏的绘制效果了。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习，巩固一下知识：

1. 给示例程序添加更多工具栏按钮，实现下拉按钮（TBSTYLE_DROPDOWN）和弹出菜单
2. 修改状态栏，在某个区域中显示一个小图标（提示：SB_SETTEXT 配合 SBT_OWNERDRAW）
3. 尝试使用自定义图像列表替换系统标准位图，让工具栏更美观
4. 实现一个简单的快捷键系统，让菜单、工具栏、快捷键三者联动

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。好了，今天的文章就到这里，我们下一篇再见！

---

## 相关资源

- [Toolbar Controls - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/toolbar-controls-overview)
- [Status Bars - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/status-bars)
- [Using Toolbar Controls - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/using-toolbar-controls)
- [Using Status Bars - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/using-status-bars)
- [InitCommonControlsEx function (Commctrl.h) | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-initcommoncontrolsex)
- [Enabling Visual Styles - Win32 apps | Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/cookbook-overview)
