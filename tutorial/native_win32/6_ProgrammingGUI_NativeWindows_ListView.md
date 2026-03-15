# 通用GUI编程技术——Win32 原生编程实战（五）——ListView 控件详解

> 上一篇文章我们搞定了 WM_NOTIFY 消息机制，你现在应该知道怎么处理复杂控件的通知了。说实话，我当时学完 WM_NOTIFY 之后特别兴奋，觉得终于可以搞定那些高级控件了。但当我第一次尝试创建 ListView 时，还是被它的复杂性吓到了——光是初始化就有好几个步骤，还要分别处理列、项、子项这些东西。但别担心，今天我会把这些内容拆解开来，带着你一步步掌握 ListView 控件。

---

## 为什么要学习 ListView

说实话，ListView 是 Win32 里最常用的控件之一。你每天用的 Windows 资源管理器、任务管理器、各种软件里的数据列表，本质上都是 ListView。掌握这个控件，你就能做出专业级别的数据展示界面。

ListView 的强大之处在于它支持四种视图模式：大图标、小图标、列表、报表（详情）。这意味着同一个控件可以适应不同的展示需求，而且切换视图模式只需要改变样式就行。这种灵活性在现代 GUI 框架里也能看到——Qt 的 QListView、WPF 的 ListView、GTK 的 TreeView，它们的设计都深受 Win32 ListView 的影响。

另一个现实原因是：很多 Windows 程序的自动化测试、数据提取都需要和 ListView 打交道。如果你不理解 ListView 的工作原理，处理这些问题就会非常困难。

这篇文章会带着你从零开始，把 ListView 控件彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么设计"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建窗口、WM_NOTIFY 是怎么工作的、基础控件怎么用。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一章：ListView 是什么

在正式开始写代码之前，我们先来聊聊 ListView 到底是什么，它长什么样，用来做什么。

### 它长什么样

如果你用过 Windows 资源管理器的"详细信息"视图，那你已经见过 ListView 了。它看起来就是一个表格：

```
+------------------+------------+------------+
| 文件名           | 大小       | 修改日期   |
+------------------+------------+------------+
| Documents        |            | 2024-01-15 |
|   report.docx    | 15 KB      | 2024-01-15 |
|   budget.xlsx    | 8 KB       | 2024-01-14 |
| Pictures         |            | 2024-01-10 |
|   photo.jpg      | 2.5 MB     | 2024-01-10 |
+------------------+------------+------------+
```

但 ListView 不只是表格，它还可以显示成大图标网格（像桌面图标那样）、小图标列表（像控制面板那样）、或者简单的列表（像文件名列表）。

### 用来做什么

ListView 主要用于展示结构化的数据，特别是需要多列显示的情况。典型的应用场景包括：

1. **文件管理器**：显示文件名、大小、类型、修改日期等多列信息
2. **任务管理器**：显示进程名、PID、CPU 使用率、内存占用
3. **音乐播放器**：显示歌曲名、歌手、专辑、时长
4. **下载管理器**：显示文件名、大小、进度、速度
5. **数据库管理工具**：显示表数据、查询结果

基本上，任何需要"列表+详情"的场景，都是 ListView 的用武之地。

### ListView 的数据结构

在理解 ListView 之前，先了解一下它的数据结构。ListView 的数据是分层组织的：

```
ListView
    ├─ 列 (Column)
    │   ├─ 列 0 (文件名)
    │   ├─ 列 1 (大小)
    │   └─ 列 2 (修改日期)
    │
    └─ 项 (Item)
        ├─ 项 0 (Documents)
        │   ├─ 子项 0 (文本：Documents)
        │   ├─ 子项 1 (文本：)
        │   └─ 子项 2 (文本：2024-01-15)
        │
        ├─ 项 1 (report.docx)
        │   ├─ 子项 0 (文本：report.docx)
        │   ├─ 子项 1 (文本：15 KB)
        │   └─ 子项 2 (文本：2024-01-15)
        │
        └─ 项 2 (budget.xlsx)
            ├─ 子项 0 (文本：budget.xlsx)
            ├─ 子项 1 (文本：8 KB)
            └─ 子项 2 (文本：2024-01-14)
```

记住这个结构：
* 每一行是一个"项"（Item）
* 项的第一列是主项，其他列是"子项"（SubItem）
* 列（Column）定义了表头和每一列的属性

---

## 第二章：创建 ListView

好了，理论讲够了，让我们开始写代码。首先来看看怎么创建一个 ListView 控件。

### 初始化通用控件

ListView 属于通用控件（Common Controls），使用前必须初始化。这个步骤经常被忘记，导致控件创建失败。

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// 在 WM_CREATE 或 WinMain 中初始化
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_LISTVIEW_CLASSES;  // 指定要初始化 ListView 类
InitCommonControlsEx(&icex);
```

⚠️ 注意

**初始化必须且仅需一次**：最好在 `WinMain` 的最开始就调用 `InitCommonControlsEx`。如果你在多个地方调用，也不会有问题，但没必要。记住，不初始化的话，创建 ListView 会失败，返回 NULL。

### WC_LISTVIEW 窗口类

ListView 的窗口类名是 `WC_LISTVIEW`（定义在 `<commctrl.h>` 中，值为 `"SysListView32"`）。你可以直接用这个常量，也可以用字符串 `"SysListView32"`。

```cpp
HWND hListView = CreateWindowEx(
    0,                          // 扩展样式（后面会讲）
    WC_LISTVIEW,                // 窗口类名
    L"",                        // 标题（ListView 不需要标题）
    WS_CHILD | WS_VISIBLE |     // 基本样式
    LVS_REPORT |                // 报表视图（多列显示）
    WS_VSCROLL | WS_HSCROLL,    // 滚动条
    10, 10,                     // 位置
    400, 300,                   // 大小
    hwnd,                       // 父窗口
    (HMENU)ID_LISTVIEW,         // 控件 ID
    hInstance,                  // 实例句柄
    NULL                        // 无额外参数
);
```

### LVS_ 样式详解

ListView 有一系列专用样式，它们决定了控件的外观和行为。最重要的是视图模式样式：

| 样式 | 说明 | 典型用途 |
|------|------|----------|
| `LVS_ICON` | 大图标视图 | 桌面图标、图片库 |
| `LVS_SMALLICON` | 小图标视图 | 文件列表（带图标） |
| `LVS_LIST` | 列表视图 | 简单的文件名列表 |
| `LVS_REPORT` | 报表视图 | 数据表格、详情列表 |

除了视图模式，还有一些常用的扩展样式：

| 样式 | 说明 |
|------|------|
| `LVS_SINGLESEL` | 单选模式（默认多选） |
| `LVS_SHOWSELALWAYS` | 失去焦点时仍显示选择 |
| `LVS_SORTASCENDING` | 自动升序排序 |
| `LVS_SORTDESCENDING` | 自动降序排序 |
| `LVS_EDITLABELS` | 允许编辑项标签 |
| `LVS_NOCOLUMNHEADER` | 不显示列标题 |

### 扩展样式

ListView 还支持扩展样式（Extended Styles），通过 `ListView_SetExtendedListViewStyle` 宏设置：

```cpp
// 设置扩展样式
ListView_SetExtendedListViewStyle(hListView,
    LVS_EX_FULLROWSELECT |    // 整行选择（而不是只选第一列）
    LVS_EX_GRIDLINES |        // 显示网格线
    LVS_EX_CHECKBOXES |       // 显示复选框
    LVS_EX_DOUBLEBUFFER       // 双缓冲，减少闪烁
);
```

常用的扩展样式：

| 样式 | 说明 |
|------|------|
| `LVS_EX_FULLROWSELECT` | 点击整行选中（推荐！） |
| `LVS_EX_GRIDLINES` | 显示网格线 |
| `LVS_EX_CHECKBOXES` | 每行前面显示复选框 |
| `LVS_EX_DOUBLEBUFFER` | 双缓冲绘制，减少闪烁 |
| `LVS_EX_HEADERDRAGDROP` | 允许拖动列标题改变列顺序 |
| `LVS_EX_TRACKSELECT` | 鼠标悬停高亮（类似热跟踪） |

### 完整的创建代码

让我们把创建 ListView 的代码整合起来：

```cpp
#define ID_LISTVIEW  1001

case WM_CREATE:
{
    // 步骤 1：初始化通用控件（如果在 WinMain 已经初始化过，可以跳过）
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // 步骤 2：创建 ListView
    HWND hListView = CreateWindowEx(
        0,
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_VSCROLL | WS_HSCROLL,
        10, 10, 500, 300,
        hwnd,
        (HMENU)ID_LISTVIEW,
        ((LPCREATESTRUCT)lParam)->hInstance,
        NULL
    );

    if (hListView == NULL)
    {
        MessageBox(hwnd, L"创建 ListView 失败", L"错误", MB_OK | MB_ICONERROR);
        return -1;
    }

    // 步骤 3：设置扩展样式
    ListView_SetExtendedListViewStyle(hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES
    );

    return 0;
}
```

编译运行这段代码，你会看到一个空的 ListView，还没有列也没有项。接下来我们就来添加它们。

⚠️ 注意

**扩展样式必须在创建后立即设置**：某些扩展样式（如 `LVS_EX_FULLROWSELECT`）需要在添加项之前设置，否则可能不生效。保险的做法是创建 ListView 后立即设置扩展样式。

---

## 第三章：列的操作

在报表视图下，ListView 需要先定义列才能显示数据。每列有一个标题、宽度和对齐方式。

### LVCOLUMN 结构

`LVCOLUMN` 结构用于定义或获取列的属性：

```cpp
typedef struct _LVCOLUMN {
    UINT mask;           // 指定哪些成员有效
    int fmt;             // 列标题的对齐方式
    int cx;              // 列宽（像素）
    LPTSTR pszText;      // 列标题文字
    int cchTextMax;      // pszText 缓冲区大小（获取信息时用）
    int iSubItem;        // 子项索引
    #if (_WIN32_IE >= 0x0300)
    int iImage;          // 图标索引
    int iOrder;          // 列顺序
    #endif
} LVCOLUMN, *LPLVCOLUMN;
```

最常用的是这几个字段：

* `mask`：指定你要设置/获取哪些属性
* `fmt`：对齐方式（`LVCFMT_LEFT`、`LVCFMT_CENTER`、`LVCFMT_RIGHT`）
* `cx`：列宽
* `pszText`：列标题文字

`mask` 可以是以下值的组合：

| 值 | 说明 |
|----|------|
| `LVCF_FMT` | `fmt` 成员有效 |
| `LVCF_WIDTH` | `cx` 成员有效 |
| `LVCF_TEXT` | `pszText` 成员有效 |
| `LVCF_SUBITEM` | `iSubItem` 成员有效 |

### 插入列

使用 `ListView_InsertColumn` 宏插入列：

```cpp
// 插入第一列：文件名
LVCOLUMN lvc = {0};
lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
lvc.fmt = LVCFMT_LEFT;        // 左对齐
lvc.cx = 200;                 // 列宽 200 像素
lvc.pszText = L"文件名";      // 列标题
lvc.iSubItem = 0;             // 子项索引 0

ListView_InsertColumn(hListView, 0, &lvc);

// 插入第二列：大小
lvc.cx = 100;
lvc.pszText = L"大小";
lvc.iSubItem = 1;
ListView_InsertColumn(hListView, 1, &lvc);

// 插入第三列：修改日期
lvc.cx = 150;
lvc.pszText = L"修改日期";
lvc.iSubItem = 2;
ListView_InsertColumn(hListView, 2, &lvc);
```

⚠️ 注意

**iSubItem 必须连续**：子项索引必须从 0 开始连续递增。如果你跳过了某个索引，后面的列会显示不正确。建议按顺序插入列，这样 `iSubItem` 就是插入索引。

### 设置列宽

除了在插入时指定宽度，你还可以动态调整列宽：

```cpp
// 设置列宽为固定值
ListView_SetColumnWidth(hListView, 0, 250);  // 第 0 列设为 250 像素

// 自动调整列宽以适应内容
ListView_SetColumnWidth(hListView, 0, LVSCW_AUTOSIZE);

// 自动调整列宽以适应标题
ListView_SetColumnWidth(hListView, 0, LVSCW_AUTOSIZE_USEHEADER);
```

| 常量 | 说明 |
|------|------|
| `LVSCW_AUTOSIZE` | 根据所有项的内容自动调整宽度 |
| `LVSCW_AUTOSIZE_USEHEADER` | 根据列标题文字调整宽度（内容可能被截断） |

一个常见的做法是先用 `LVSCW_AUTOSIZE_USEHEADER` 确保标题完整显示，如果内容更长，再用 `LVSCW_AUTOSIZE`：

```cpp
// 先按标题调整
ListView_SetColumnWidth(hListView, col, LVSCW_AUTOSIZE_USEHEADER);
// 获取当前宽度
int headerWidth = ListView_GetColumnWidth(hListView, col);
// 按内容调整
ListView_SetColumnWidth(hListView, col, LVSCW_AUTOSIZE);
// 如果内容更宽，就用内容宽度；否则保持标题宽度
int contentWidth = ListView_GetColumnWidth(hListView, col);
ListView_SetColumnWidth(hListView, col, max(headerWidth, contentWidth));
```

### 获取和修改列

获取列信息用 `ListView_GetColumn`，修改列用 `ListView_SetColumn`：

```cpp
// 获取第一列的信息
LVCOLUMN lvc = {0};
lvc.mask = LVCF_TEXT | LVCF_WIDTH;
wchar_t buf[256];
lvc.pszText = buf;
lvc.cchTextMax = 256;

if (ListView_GetColumn(hListView, 0, &lvc))
{
    // 现在 buf 里是列标题，lvc.cx 是列宽
}

// 修改列标题
lvc.mask = LVCF_TEXT;
lvc.pszText = L"新标题";
ListView_SetColumn(hListView, 0, &lvc);
```

### 删除列

删除整个列用 `ListView_DeleteColumn`：

```cpp
// 删除第 1 列（注意：删除后后面的列索引会变化）
ListView_DeleteColumn(hListView, 1);
```

⚠️ 注意

**删除列会影响子项索引**：如果你删除了中间的某一列，后面列的子项索引都会减少 1。删除列后，你可能需要重新设置所有项的数据。所以删除列是一个比较危险的操作，建议谨慎使用。

---

## 第四章：项的操作

列定义好了，现在让我们来添加数据。ListView 的数据是按"项"（Item）组织的，每个项可以有多个子项（SubItem）。

### LVITEM 结构

`LVITEM` 结构用于定义或获取项的属性：

```cpp
typedef struct _LVITEM {
    UINT mask;           // 指定哪些成员有效
    int iItem;           // 项索引
    int iSubItem;        // 子项索引（0 表示主项）
    UINT state;          // 当前状态
    UINT stateMask;      // 有效的状态位
    LPTSTR pszText;      // 项文字
    int cchTextMax;      // pszText 缓冲区大小
    int iImage;          // 图标索引
    LPARAM lParam;       // 用户定义数据
} LVITEM, *LPLVITEM;
```

最常用的字段：

* `mask`：指定哪些成员有效
* `iItem`：项索引（从 0 开始）
* `iSubItem`：子项索引（0 表示主项，1+ 表示子项）
* `pszText`：显示文字

`mask` 的常用值：

| 值 | 说明 |
|----|------|
| `LVIF_TEXT` | `pszText` 有效 |
| `LVIF_IMAGE` | `iImage` 有效 |
| `LVIF_PARAM` | `lParam` 有效 |
| `LVIF_STATE` | `state` 和 `stateMask` 有效 |

### 插入项

插入主项（第一列）用 `ListView_InsertItem`：

```cpp
// 插入第一行
LVITEM lvi = {0};
lvi.mask = LVIF_TEXT;
lvi.iItem = 0;           // 插入位置（0 = 最前面）
lvi.iSubItem = 0;        // 主项
lvi.pszText = L"Documents";

ListView_InsertItem(hListView, &lvi);

// 插入第二行
lvi.iItem = 1;
lvi.pszText = L"report.docx";
ListView_InsertItem(hListView, &lvi);

// 插入第三行
lvi.iItem = 2;
lvi.pszText = L"budget.xlsx";
ListView_InsertItem(hListView, &lvi);
```

⚠️ 注意

**iItem 的含义**：在 `ListView_InsertItem` 中，`iItem` 是插入位置。如果你插入到位置 2，原来的 2 及之后的项会向后移动。如果想添加到末尾，可以用 `ListView_GetItemCount()` 获取当前项数作为插入位置。

### 设置子项

主项插入后，需要用 `ListView_SetItem` 设置子项（其他列的值）：

```cpp
// 设置第 0 行的第 1 列（大小）
LVITEM lvi = {0};
lvi.mask = LVIF_TEXT;
lvi.iItem = 0;           // 第 0 行
lvi.iSubItem = 1;        // 第 1 列
lvi.pszText = L"";       // 文件夹不显示大小
ListView_SetItem(hListView, &lvi);

// 设置第 0 行的第 2 列（修改日期）
lvi.iSubItem = 2;
lvi.pszText = L"2024-01-15";
ListView_SetItem(hListView, &lvi);

// 设置第 1 行的第 1 列
lvi.iItem = 1;
lvi.iSubItem = 1;
lvi.pszText = L"15 KB";
ListView_SetItem(hListView, &lvi);

// 设置第 1 行的第 2 列
lvi.iSubItem = 2;
lvi.pszText = L"2024-01-15";
ListView_SetItem(hListView, &lvi);
```

### 便捷宏：设置项文本

有一个专门的宏 `ListView_SetItemText` 可以更简洁地设置项文本：

```cpp
// 设置主项（第 0 列）
ListView_SetItemText(hListView, 0, 0, L"Documents");

// 设置子项（第 1 列）
ListView_SetItemText(hListView, 0, 1, L"");

// 设置子项（第 2 列）
ListView_SetItemText(hListView, 0, 2, L"2024-01-15");
```

这个宏内部其实还是调用 `ListView_SetItem`，但代码更简洁。

### 获取项文本

获取项的文本稍微麻烦一点，因为需要提供缓冲区：

```cpp
// 获取第 0 行第 0 列的文字
wchar_t buf[256];
ListView_GetItemText(hListView, 0, 0, buf, 256);
// 现在 buf 就是 "Documents"
```

如果用 `LVITEM` 结构：

```cpp
LVITEM lvi = {0};
lvi.mask = LVIF_TEXT;
lvi.iItem = 0;
lvi.iSubItem = 0;
wchar_t buf[256];
lvi.pszText = buf;
lvi.cchTextMax = 256;

ListView_GetItem(hListView, &lvi);
// 现在 buf 就是项文本
```

### 删除项

删除单个项用 `ListView_DeleteItem`，清空所有项用 `ListView_DeleteAllItems`：

```cpp
// 删除第 0 项
ListView_DeleteItem(hListView, 0);

// 清空所有项
ListView_DeleteAllItems(hListView);
```

### 附加用户数据

每个项可以附带一个 `LPARAM` 类型的用户数据，这对于关联额外信息很有用：

```cpp
// 假设我们有一个文件信息结构
struct FileInfo {
    DWORD fileSize;
    FILETIME modifiedTime;
    BOOL isDirectory;
};

// 创建数据
FileInfo* pInfo = new FileInfo;
pInfo->fileSize = 15360;
pInfo->isDirectory = FALSE;

// 插入项并附加数据
LVITEM lvi = {0};
lvi.mask = LVIF_TEXT | LVIF_PARAM;
lvi.iItem = 0;
lvi.iSubItem = 0;
lvi.pszText = L"report.docx";
lvi.lParam = (LPARAM)pInfo;  // 附加指针

ListView_InsertItem(hListView, &lvi);

// 后续获取数据
LVITEM getItem = {0};
getItem.mask = LVIF_PARAM;
getItem.iItem = 0;
ListView_GetItem(hListView, &getItem);

FileInfo* pStoredInfo = (FileInfo*)getItem.lParam;
// 使用 pStoredInfo...

// 记得在删除项时释放内存
delete pStoredInfo;
```

⚠️ 注意

**内存管理责任**：ListView 不会自动释放你附加的 `lParam` 数据。当你删除项或清空列表时，需要先遍历所有项，释放 `lParam` 指向的内存。否则会内存泄漏。

### 完整示例：添加数据

让我们把前面的代码整合成一个完整的例子：

```cpp
// 添加示例数据到 ListView
void AddSampleData(HWND hListView)
{
    // 定义数据结构
    struct RowData {
        const wchar_t* name;
        const wchar_t* size;
        const wchar_t* date;
    };

    RowData data[] = {
        { L"Documents", L"", L"2024-01-15" },
        { L"report.docx", L"15 KB", L"2024-01-15" },
        { L"budget.xlsx", L"8 KB", L"2024-01-14" },
        { L"Pictures", L"", L"2024-01-10" },
        { L"photo.jpg", L"2.5 MB", L"2024-01-10" },
    };

    // 添加数据
    for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++)
    {
        // 插入主项
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)data[i].name;
        ListView_InsertItem(hListView, &lvi);

        // 设置子项
        ListView_SetItemText(hListView, i, 1, (LPWSTR)data[i].size);
        ListView_SetItemText(hListView, i, 2, (LPWSTR)data[i].date);
    }
}
```

---

## 第五章：视图模式切换

ListView 支持四种视图模式，可以在运行时切换。

### 四种视图模式

```
LVS_ICON (大图标)
+-------+  +-------+
| ICON  |  | ICON  |
| Name  |  | Name  |
+-------+  +-------+

LVS_SMALLICON (小图标)
+--+  +--+  +--+
|Icon| |Icon| |Icon|
+--+  +--+  |--+

LVS_LIST (列表)
+--------+
| Item 1 |
| Item 2 |
| Item 3 |
+--------+

LVS_REPORT (报表/详情)
+-----+-----+-----+
| Col1| Col2| Col3|
+-----+-----+-----+
| D11 | D12 | D13 |
| D21 | D22 | D23 |
+-----+-----+-----+
```

### 切换视图模式

有几种方法可以切换视图模式：

**方法一：重新创建窗口**（最可靠）

```cpp
void SwitchViewMode(HWND hParent, int nId, DWORD newViewStyle)
{
    HWND hListView = GetDlgItem(hParent, nId);
    if (hListView)
    {
        // 获取当前样式
        DWORD style = GetWindowLong(hListView, GWL_STYLE);

        // 清除旧的视图样式
        style &= ~(LVS_ICON | LVS_SMALLICON | LVS_LIST | LVS_REPORT);

        // 设置新样式
        style |= newViewStyle;

        // 应用新样式
        SetWindowLong(hListView, GWL_STYLE, style);

        // 强制重绘
        InvalidateRect(hListView, NULL, TRUE);
    }
}
```

**方法二：使用 SetWindowPos**（触发样式更新）

```cpp
// 切换到大图标视图
SetWindowPos(hListView, NULL, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
    SWP_FRAMECHANGED | SWP_DRAWFRAME);
```

**使用示例**：

```cpp
// 切换到大图标
SwitchViewMode(hwnd, ID_LISTVIEW, LVS_ICON);

// 切换到报表视图
SwitchViewMode(hwnd, ID_LISTVIEW, LVS_REPORT);
```

### 各视图的典型用途

| 视图 | 典型用途 | 需要图标 |
|------|----------|----------|
| `LVS_ICON` | 图片库、桌面、文件缩略图 | 是 |
| `LVS_SMALLICON` | 文件列表、控制面板 | 是 |
| `LVS_LIST` | 简单列表、选项列表 | 可选 |
| `LVS_REPORT` | 数据表格、详情列表 | 可选 |

### 图标列表

如果使用图标视图，需要设置图标列表（ImageList）：

```cpp
// 创建图标列表（这里只是示例，实际需要加载图标）
HIMAGELIST hImageList = ImageList_Create(
    32, 32,      // 图标宽高
    ILC_COLOR32, // 颜色深度
    10, 10       // 初始和增长大小
);

// 假设 hIcon 是加载的图标句柄
ImageList_AddIcon(hImageList, hIcon);

// 设置给 ListView
ListView_SetImageList(hListView, hImageList, LVSIL_NORMAL);

// 小图标用另一个列表
HIMAGELIST hSmallList = ImageList_Create(16, 16, ILC_COLOR32, 10, 10);
// ... 添加小图标
ListView_SetImageList(hListView, hSmallList, LVSIL_SMALL);
```

图标列表的管理比较复杂，涉及到资源加载和管理，这里不展开。如果只是做数据表格（报表视图），不需要图标。

---

## 第六章：ListView 通知处理

ListView 通过 `WM_NOTIFY` 消息发送通知。我们在上一篇文章已经讲过 WM_NOTIFY 的机制，现在来看看 ListView 特有的通知码。

### 常用通知码

ListView 的通知码以 `LVN_` 开头：

| 通知码 | 说明 | 何时触发 |
|--------|------|----------|
| `LVN_ITEMCHANGED` | 项状态改变 | 选择改变、状态改变 |
| `LVN_ITEMACTIVATE` | 项被激活 | 双击或按回车 |
| `NM_CLICK` | 单击 | 鼠标单击 |
| `NM_DBLCLK` | 双击 | 鼠标双击 |
| `LVN_BEGINLABELEDIT` | 开始编辑标签 | 用户开始编辑 |
| `LVN_ENDLABELEDIT` | 结束编辑标签 | 编辑完成 |
| `LVN_COLUMNCLICK` | 点击列标题 | 点击列头（可用于排序） |

### NMLISTVIEW 结构

ListView 的通知通常使用 `NMLISTVIEW` 结构：

```cpp
typedef struct tagNMLISTVIEW {
    NMHDR hdr;       // 通用通知头
    int iItem;       // 项索引
    int iSubItem;    // 子项索引
    UINT uNewState;  // 新状态
    UINT uOldState;  // 旧状态
    UINT uChanged;   // 改变的状态位
    POINT ptAction;  // 操作位置
} NMLISTVIEW, *LPNMLISTVIEW;
```

### 处理 LVN_ITEMCHANGED

`LVN_ITEMCHANGED` 是最常用的通知，它在项的选择状态改变时触发：

```cpp
case WM_NOTIFY:
{
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (pnmh->idFrom == ID_LISTVIEW)
    {
        switch (pnmh->code)
        {
        case LVN_ITEMCHANGED:
        {
            LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)lParam;

            // 检查是否是选择状态改变
            if (pnmlv->uChanged & LVIF_STATE)
            {
                BOOL isSelected = (pnmlv->uNewState & LVIS_SELECTED);
                BOOL wasSelected = (pnmlv->uOldState & LVIS_SELECTED);

                if (isSelected && !wasSelected)
                {
                    // 项被选中
                    wchar_t buf[256];
                    ListView_GetItemText(hListView, pnmlv->iItem, 0, buf, 256);
                    wchar_t msg[512];
                    swprintf_s(msg, 512, L"选中了：%s", buf);
                    SetWindowText(hStatus, msg);  // 假设有状态栏
                }
            }
            break;
        }
        }
    }
    break;
}
```

⚠️ 注意

**LVN_ITEMCHANGED 触发频繁**：这个通知在各种情况下都会触发，不只是选择改变。一定要检查 `uChanged` 和状态位，否则可能会处理不需要的事件。

### 处理 LVN_ITEMACTIVATE

`LVN_ITEMACTIVATE` 在用户"激活"项时触发（通常是双击或按回车）：

```cpp
case LVN_ITEMACTIVATE:
{
    LPNMITEMACTIVATE pnia = (LPNMITEMACTIVATE)lParam;

    // 获取被激活的项
    wchar_t buf[256];
    ListView_GetItemText(hListView, pnia->iItem, 0, buf, 256);

    wchar_t msg[512];
    swprintf_s(msg, 512, L"激活了：%s (列 %d)", buf, pnia->iSubItem);
    MessageBox(hwnd, msg, L"提示", MB_OK);
    break;
}
```

### 处理列点击（排序）

点击列标题时触发 `LVN_COLUMNCLICK`，常用于实现列排序：

```cpp
case LVN_COLUMNCLICK:
{
    LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)lParam;
    int clickedColumn = pnmlv->iSubItem;

    wchar_t buf[100];
    swprintf_s(buf, 100, L"点击了第 %d 列", clickedColumn);
    MessageBox(hwnd, buf, L"提示", MB_OK);

    // 这里可以实现排序逻辑
    break;
}
```

### 获取选中项

除了处理通知，你还可以主动获取当前选中的项：

```cpp
// 获取第一个选中项的索引
int selectedIdx = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);

if (selectedIdx != -1)
{
    // 有项被选中
    wchar_t buf[256];
    ListView_GetItemText(hListView, selectedIdx, 0, buf, 256);
    // 使用 buf...
}

// 获取所有选中项（多选模式）
int idx = -1;
while ((idx = ListView_GetNextItem(hListView, idx, LVNI_SELECTED)) != -1)
{
    // 处理每个选中项
    wchar_t buf[256];
    ListView_GetItemText(hListView, idx, 0, buf, 256);
    // ...
}
```

`ListView_GetNextItem` 的搜索标志：

| 标志 | 说明 |
|------|------|
| `LVNI_SELECTED` | 搜索选中项 |
| `LVNI_CUT` | 搜索被标记为剪切的项 |
| `LVNI_DROPHILITED` | 搜索拖放目标项 |
| `LVNI_FOCUSED` | 搜索有焦点的项 |
| `LVNI_ALL` | 搜索所有项（用于遍历） |
| `LVNI_ABOVE` | 搜索指定项上方的项 |
| `LVNI_BELOW` | 搜索指定项下方的项 |
| `LVNI_TOLEFT` | 搜索指定项左边的项 |
| `LVNI_TORIGHT` | 搜索指定项右边的项 |

---

## 第七章：实战：文件列表浏览器

让我们用学到的知识做一个完整的例子——一个简单的文件列表浏览器，显示当前目录的文件信息。

### 功能需求

1. 显示当前目录的所有文件和子目录
2. 分列显示文件名、大小、修改日期
3. 单击选择时在状态栏显示信息
4. 双击时弹出详细信息

### 完整代码

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_LISTVIEW   1001
#define ID_STATUSBAR  1002

// 全局变量
HWND g_hListView = NULL;
HWND g_hStatusBar = NULL;

// 获取文件大小的可读字符串
void GetFileSizeString(DWORD fileSize, wchar_t* buf, size_t bufSize)
{
    if (fileSize < 1024)
    {
        swprintf_s(buf, bufSize, L"%d B", fileSize);
    }
    else if (fileSize < 1024 * 1024)
    {
        swprintf_s(buf, bufSize, L"%.1f KB", fileSize / 1024.0);
    }
    else if (fileSize < 1024 * 1024 * 1024)
    {
        swprintf_s(buf, bufSize, L"%.1f MB", fileSize / (1024.0 * 1024));
    }
    else
    {
        swprintf_s(buf, bufSize, L"%.1f GB", fileSize / (1024.0 * 1024 * 1024));
    }
}

// 获取文件时间字符串
void GetFileTimeString(const FILETIME* ft, wchar_t* buf, size_t bufSize)
{
    FILETIME localTime;
    FileTimeToLocalFileTime(ft, &localTime);

    SYSTEMTIME sysTime;
    FileTimeToSystemTime(&localTime, &sysTime);

    swprintf_s(buf, bufSize, L"%04d-%02d-%02d %02d:%02d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute);
}

// 填充文件列表
void PopulateFileList(HWND hListView)
{
    // 清空现有内容
    ListView_DeleteAllItems(hListView);

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(L"*.*", &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    int itemIndex = 0;

    do
    {
        // 跳过 . 和 ..
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0)
        {
            continue;
        }

        // 插入主项（文件名）
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = itemIndex;
        lvi.iSubItem = 0;
        lvi.pszText = findData.cFileName;
        ListView_InsertItem(hListView, &lvi);

        // 设置大小列
        wchar_t sizeStr[64];
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            wcscpy_s(sizeStr, L"<DIR>");
        }
        else
        {
            LARGE_INTEGER fileSize;
            fileSize.HighPart = findData.nFileSizeHigh;
            fileSize.LowPart = findData.nFileSizeLow;
            GetFileSizeString(fileSize.QuadPart, sizeStr, 64);
        }
        ListView_SetItemText(hListView, itemIndex, 1, sizeStr);

        // 设置修改日期列
        wchar_t dateStr[64];
        GetFileTimeString(&findData.ftLastWriteTime, dateStr, 64);
        ListView_SetItemText(hListView, itemIndex, 2, dateStr);

        itemIndex++;

    } while (FindNextFile(hFind, &findData));

    FindClose(hFind);
}

// 初始化 ListView 列
void SetupListViewColumns(HWND hListView)
{
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // 文件名列
    lvc.cx = 250;
    lvc.pszText = L"文件名";
    lvc.iSubItem = 0;
    ListView_InsertColumn(hListView, 0, &lvc);

    // 大小列
    lvc.cx = 100;
    lvc.pszText = L"大小";
    lvc.iSubItem = 1;
    ListView_InsertColumn(hListView, 1, &lvc);

    // 修改日期列
    lvc.cx = 150;
    lvc.pszText = L"修改日期";
    lvc.iSubItem = 2;
    ListView_InsertColumn(hListView, 2, &lvc);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建 ListView
        g_hListView = CreateWindowEx(
            0, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_VSCROLL | WS_HSCROLL,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_LISTVIEW,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );

        // 设置扩展样式
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // 设置列
        SetupListViewColumns(g_hListView);

        // 创建状态栏
        g_hStatusBar = CreateWindowEx(
            0, STATUSCLASSNAME, L"",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_STATUSBAR,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );

        // 填充文件列表
        PopulateFileList(g_hListView);

        return 0;
    }

    case WM_SIZE:
    {
        // 调整 ListView 大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        if (g_hStatusBar)
        {
            // 让状态栏自己调整大小
            SendMessage(g_hStatusBar, WM_SIZE, 0, 0);

            // 获取状态栏高度
            RECT rcStatus;
            GetWindowRect(g_hStatusBar, &rcStatus);
            int statusHeight = rcStatus.bottom - rcStatus.top;

            // ListView 占据除状态栏外的所有空间
            if (g_hListView)
            {
                SetWindowPos(g_hListView, NULL, 0, 0,
                    rcClient.right, rcClient.bottom - statusHeight,
                    SWP_NOZORDER);
            }
        }

        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        if (pnmh->idFrom == ID_LISTVIEW)
        {
            switch (pnmh->code)
            {
            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)lParam;

                // 检查是否是选择状态改变
                if (pnmlv->uChanged & LVIF_STATE)
                {
                    BOOL isSelected = (pnmlv->uNewState & LVIS_SELECTED);
                    BOOL wasSelected = (pnmlv->uOldState & LVIS_SELECTED);

                    if (isSelected && !wasSelected)
                    {
                        // 项被选中，在状态栏显示信息
                        wchar_t name[MAX_PATH];
                        ListView_GetItemText(g_hListView, pnmlv->iItem, 0, name, MAX_PATH);

                        wchar_t size[64];
                        ListView_GetItemText(g_hListView, pnmlv->iItem, 1, size, 64);

                        wchar_t msg[MAX_PATH + 100];
                        swprintf_s(msg, L"%s - %s", name, size);
                        SendMessage(g_hStatusBar, WM_SETTEXT, 0, (LPARAM)msg);
                    }
                }
                break;
            }

            case LVN_ITEMACTIVATE:
            {
                LPNMITEMACTIVATE pnia = (LPNMITEMACTIVATE)lParam;

                wchar_t name[MAX_PATH];
                wchar_t size[64];
                wchar_t date[64];

                ListView_GetItemText(g_hListView, pnia->iItem, 0, name, MAX_PATH);
                ListView_GetItemText(g_hListView, pnia->iItem, 1, size, 64);
                ListView_GetItemText(g_hListView, pnia->iItem, 2, date, 64);

                wchar_t msg[MAX_PATH + 256];
                swprintf_s(msg, L"文件： %s\n大小： %s\n日期： %s",
                    name, size, date);
                MessageBox(hwnd, msg, L"文件信息", MB_OK);
                break;
            }
            }
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
    const wchar_t CLASS_NAME[] = L"FileBrowserWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"文件列表浏览器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
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

### 代码要点解析

1. **通用控件初始化**：`ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES` 同时初始化 ListView 和状态栏。

2. **列的设置**：`SetupListViewColumns` 函数创建三列：文件名、大小、修改日期。

3. **文件枚举**：使用 `FindFirstFile` / `FindNextFile` 枚举当前目录文件。

4. **状态显示**：`LVN_ITEMCHANGED` 通知处理在状态栏显示选中文件信息。

5. **双击激活**：`LVN_ITEMACTIVATE` 处理双击事件，显示完整文件信息。

6. **布局响应**：`WM_SIZE` 处理确保 ListView 和状态栏正确调整大小。

---

## 后续可以做什么

到这里，ListView 控件的核心内容就讲完了。你现在应该能够创建多列列表、添加/删除项、处理用户交互了。但 ListView 还有很多高级功能我们没有涉及，比如：

* 虚拟列表（Virtual ListView）—— 处理大量数据
* 自绘（Owner Draw）—— 自定义项的外观
* 拖放（Drag & Drop）—— 实现项的拖拽排序
* 排序（Sorting）—— 点击列标题排序
* 分组（Groups）—— 将项分组显示

下一篇文章，我们会聊一聊 **TreeView 控件**—— 另一个常用的复杂控件。你会学到如何创建树形结构、如何展开/折叠节点、如何处理节点选择，以及如何实现一个像资源管理器左侧那样的目录树。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习：

### 练习题

**基础练习**：
1. 修改文件列表浏览器，添加"类型"列，显示文件扩展名
2. 实现一个"刷新"按钮，点击后重新扫描当前目录

**进阶练习**：
1. 实现点击列标题排序功能（按文件名、大小或日期排序）
2. 添加右键菜单，当选中文件时显示"打开"、"删除"等选项

**挑战练习**：
1. 实现虚拟列表模式，显示 100,000 项数据而不卡顿
2. 添加图标显示，不同文件类型显示不同图标

好了，今天的文章就到这里。ListView 这关过了，下一篇的 TreeView 就不在话下了。我们下一篇再见！

---

**相关资源**

- [ListView 控件概述 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/list-view-control-reference)
- [LVCOLUMN 结构 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/commctrl/ns-commctrl-lvcolumn)
- [LVITEM 结构 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/commctrl/ns-commctrl-lvitem)
- [ListView 通知码 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/bumper-list-view-control-reference-notifications)
