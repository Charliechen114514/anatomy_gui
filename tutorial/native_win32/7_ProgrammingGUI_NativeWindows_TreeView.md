# 通用GUI编程技术——Win32 原生编程实战（六）——TreeView 控件详解

> 上一篇文章我们完成了 ListView 控件的学习，你已经能够创建多列列表、处理用户选择、实现懒加载了。说实话，ListView 确实很强大，但当你需要展示层级数据的时候——比如文件系统的目录结构、公司的组织架构图——ListView 就显得力不从心了。今天我们来聊一个专门处理层级数据的控件：TreeView。

---

## 为什么一定要搞懂 TreeView

说实话，我在刚开始做 Win32 开发的时候，对 TreeView 是有些畏惧的。光是那些带着 "TV" 前缀的结构体和宏就让人眼花缭乱：TVITEM、TVINSERTSTRUCT、TreeView_GetItem、TreeView_SetItem... 一开始我总是在 MSDN 和代码之间来回切换，才能找到正确的用法。

但当你真正掌握了 TreeView 之后，你会发现它是处理层级数据最优雅的方式。想一想资源管理器左侧的目录树——用户可以展开、折叠、选择不同的文件夹，这种交互模式是任何线性列表都无法替代的。

另一个现实原因是：TreeView 的使用模式在各个 GUI 框架中都是相通的。不管你以后用 Qt、WPF 还是 Web 前端，理解了 TreeView 的核心概念——节点、父节点、子节点、展开/折叠——你就能快速上手任何框架的树形控件。

这篇文章会带着你从零开始，彻底搞透 TreeView 控件。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建窗口、WM_NOTIFY 是怎么工作的、基本的控件操作。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一章：TreeView 是什么

### 长什么样

打开 Windows 资源管理器，看看左侧那个面板——这就是典型的 TreeView 控件。你会看到：

```
? 这台电脑
  ? 本地磁盘 (C:)
    ? Program Files
      ? Adobe
      ? Internet Explorer
    ? Windows
      ? System32
  ? 本地磁盘 (D:)
    ? 文档
    ? 图片
```

每个可以展开的项前面有一个小加号框（+）或向右箭头，点击后展开显示子项，加号变成减号（-）或向下箭头。被选中的项会有高亮背景色。

### 用来做什么

TreeView 专门用于展示**层级数据**——就是那种有父子关系的数据。与 ListView 不同，TreeView 的每一项（节点）都可以有自己的子节点，子节点还可以有子节点，理论上可以无限嵌套。

### 典型应用场景

1. **资源管理器的目录树**——最经典的应用，展示文件系统的层级结构
2. **注册表编辑器**——注册表的键值本身就是树形结构
3. **组织架构图**——公司 CEO 下面有多个副总裁，副总裁下面有部门经理
4. **文档大纲视图**——Word 或 IDE 里的文档结构导航，章节下面有小节，小节下面有段落

---

## 第二章：创建 TreeView

### WC_TREEVIEW 窗口类

TreeView 的窗口类名是 `WC_TREEVIEW`，定义在 `<commctrl.h>` 中。和其他通用控件一样，你需要先初始化通用控件库。

### InitCommonControlsEx 初始化

```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// 初始化通用控件
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
icex.dwICC = ICC_TREEVIEW_CLASSES;  // 关键：指定 TreeView 类
InitCommonControlsEx(&icex);
```

⚠️ 注意

**千万别忘记初始化**！如果你忘记调用 `InitCommonControlsEx`，TreeView 创建会失败，`CreateWindowEx` 会返回 NULL。更糟糕的是，它不会给你任何错误提示，让你半天找不到原因。

### TVS_ 样式详解

TreeView 有一组专用样式，以 `TVS_` 开头：

| 样式 | 说明 |
|------|------|
| TVS_HASLINES | 在父节点和子节点之间画连接线 |
| TVS_HASBUTTONS | 在节点前面显示 +/- 按钮 |
| TVS_LINESATROOT | 在根节点级别也画线 |
| TVS_SHOWSELALWAYS | 即使 TreeView 失去焦点，也保持选中高亮 |
| TVS_EDITLABELS | 允许用户编辑节点文字（需处理 TVN_BEGINLABELEDIT） |
| TVS_DISABLEDRAGDROP | 禁止拖放操作 |
| TVS_CHECKBOXES | 在每个节点前显示复选框（Comctl32.dll 5.80+） |

### 完整创建代码

```cpp
#define ID_TREEVIEW 1001

case WM_CREATE:
{
    HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

    // 初始化通用控件（只需调用一次）
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // 创建 TreeView
    HWND hTree = CreateWindowEx(
        0,                          // 扩展样式
        WC_TREEVIEW,                // 窗口类名
        L"",                        // 标题（TreeView 不用）
        WS_CHILD | WS_VISIBLE |     // 基本样式
        TVS_HASLINES |              // 画连接线
        TVS_HASBUTTONS |            // 显示 +/- 按钮
        TVS_LINESATROOT |           // 根节点也画线
        TVS_SHOWSELALWAYS,          // 始终显示选中状态
        10, 10,                     // 位置
        300, 400,                   // 大小
        hwnd,                       // 父窗口
        (HMENU)ID_TREEVIEW,         // 控件 ID
        hInst,                      // 实例句柄
        NULL                        // 无额外参数
    );

    // 保存句柄到窗口用户数据（可选）
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hTree);

    return 0;
}
```

### 样式效果示意

```
TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT:
?
+-? 根节点1
| +-? 子节点1.1
| +-? 子节点1.2
+-? 根节点2
  +- 子节点2.1

仅 TVS_HASBUTTONS:
? 根节点1
  ? 子节点1.1
  ? 子节点1.2
? 根节点2
  ? 子节点2.1
```

---

## 第三章：树节点操作

### TVITEM 结构

`TVITEM` 结构描述了一个节点的属性：

```cpp
typedef struct tagTVITEM {
    UINT      mask;               // 指定哪些成员有效
    HTREEITEM hItem;              // 节点句柄
    UINT      state;              // 节点状态
    UINT      stateMask;          // 状态掩码
    LPTSTR    pszText;            // 节点文字
    int       cchTextMax;         // 文字缓冲区大小
    int       iImage;             // 未选中时的图标索引
    int       iSelectedImage;     // 选中时的图标索引
    int       cChildren;          // 子节点数量
    LPARAM    lParam;             // 应用程序定义的数据
} TVITEM, *LPTVITEM;
```

最常用的是 `mask` 和 `pszText`：

```cpp
TVITEM tvi = {0};
tvi.mask = TVIF_TEXT;                    // 指定 pszText 有效
tvi.pszText = L"我是节点";                // 节点文字
```

`mask` 可以是以下值的组合：

| 值 | 说明 |
|----|------|
| TVIF_TEXT | pszText 有效 |
| TVIF_IMAGE | iImage 有效 |
| TVIF_SELECTEDIMAGE | iSelectedImage 有效 |
| TVIF_PARAM | lParam 有效 |
| TVIF_STATE | state/stateMask 有效 |
| TVIF_CHILDREN | cChildren 有效 |

### TVINSERTSTRUCT 结构

`TVINSERTSTRUCT` 用于插入新节点：

```cpp
typedef struct tagTVINSERTSTRUCT {
    HTREEITEM hParent;              // 父节点句柄
    HTREEITEM hInsertAfter;         // 插入在哪个节点之后
    TVITEM item;                    // 节点属性
} TVINSERTSTRUCT, *LPTVINSERTSTRUCT;
```

### HTREEITEM 句柄类型

`HTREEITEM` 是 TreeView 节点的句柄类型，它是一个句柄（本质上是个指针），用于唯一标识 TreeView 中的某个节点。你需要保存这个句柄，以便后续操作这个节点。

### 特殊句柄值

| 值 | 说明 |
|----|------|
| TVI_ROOT | 作为最顶层节点（根节点） |
| TVI_FIRST | 插入在父节点的第一个子节点位置 |
| TVI_LAST | 插入在父节点的最后一个子节点位置 |
| TVI_SORT | 按字母顺序自动排序插入 |

### TreeView_InsertItem 宏

```cpp
// 插入根节点
TVINSERTSTRUCT tvins = {0};
tvins.hParent = TVI_ROOT;           // 作为根节点
tvins.hInsertAfter = TVI_LAST;      // 插入到最后
tvins.item.mask = TVIF_TEXT;        // 指定文字有效
tvins.item.pszText = L"根节点";

HTREEITEM hRoot = TreeView_InsertItem(hTree, &tvins);

// 插入子节点
tvins.hParent = hRoot;              // 父节点是刚才创建的根节点
tvins.item.pszText = L"子节点1";
HTREEITEM hChild1 = TreeView_InsertItem(hTree, &tvins);

tvins.item.pszText = L"子节点2";
HTREEITEM hChild2 = TreeView_InsertItem(hTree, &tvins);
```

### TreeView_DeleteItem 宏

```cpp
// 删除单个节点
TreeView_DeleteItem(hTree, hChild1);

// 删除所有节点
TreeView_DeleteAllItems(hTree);
```

⚠️ 注意

**删除节点后，该节点的句柄立即失效**。不要保存和使用已删除节点的句柄。删除父节点会自动删除所有子节点，不需要手动递归删除。

### 完整节点操作示例

```cpp
// 创建一个简单的树结构
HTREEITEM CreateSampleTree(HWND hTree)
{
    // 根节点
    TVINSERTSTRUCT tvins = {0};
    tvins.hParent = TVI_ROOT;
    tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT;

    tvins.item.pszText = L"我的电脑";
    HTREEITEM hRoot = TreeView_InsertItem(hTree, &tvins);

    // C: 盘
    tvins.hParent = hRoot;
    tvins.item.pszText = L"本地磁盘 (C:)";
    HTREEITEM hDriveC = TreeView_InsertItem(hTree, &tvins);

    // C: 下的文件夹
    tvins.hParent = hDriveC;
    tvins.item.pszText = L"Program Files";
    TreeView_InsertItem(hTree, &tvins);

    tvins.item.pszText = L"Windows";
    TreeView_InsertItem(hTree, &tvins);

    // D: 盘
    tvins.hParent = hRoot;
    tvins.item.pszText = L"本地磁盘 (D:)";
    HTREEITEM hDriveD = TreeView_InsertItem(hTree, &tvins);

    // D: 下的文件夹
    tvins.hParent = hDriveD;
    tvins.item.pszText = L"文档";
    TreeView_InsertItem(hTree, &tvins);

    tvins.item.pszText = L"图片";
    TreeView_InsertItem(hTree, &tvins);

    // 展开根节点
    TreeView_Expand(hTree, hRoot, TVE_EXPAND);

    return hRoot;
}
```

---

## 第四章：展开与折叠

### TreeView_Expand 宏

```cpp
BOOL TreeView_Expand(
    HWND hwnd,        // TreeView 句柄
    HTREEITEM hitem,  // 要展开/折叠的节点
    UINT code         // 操作标志
);
```

`code` 参数可以是：

| 值 | 说明 |
|----|------|
| TVE_EXPAND | 展开节点 |
| TVE_COLLAPSE | 折叠节点 |
| TVE_TOGGLE | 切换展开/折叠状态 |
| TVE_EXPANDPARTIAL | 部分展开（只显示一级子节点） |

```cpp
// 展开节点
TreeView_Expand(hTree, hRoot, TVE_EXPAND);

// 折叠节点
TreeView_Expand(hTree, hRoot, TVE_COLLAPSE);

// 切换状态
TreeView_Expand(hTree, hRoot, TVE_TOGGLE);
```

### 遍历节点宏

TreeView 提供了一组宏用于遍历节点：

| 宏 | 说明 |
|----|------|
| TreeView_GetChild | 获取第一个子节点 |
| TreeView_GetParent | 获取父节点 |
| TreeView_GetNextSibling | 获取下一个兄弟节点 |
| TreeView_GetPrevSibling | 获取上一个兄弟节点 |
| TreeView_GetRoot | 获取根节点 |
| TreeView_GetSelection | 获取当前选中的节点 |

### 遍历示例

```cpp
// 递归遍历所有节点
void EnumTreeNodes(HWND hTree, HTREEITEM hItem)
{
    while (hItem != NULL)
    {
        // 获取节点文字
        TVITEM tvi = {0};
        wchar_t szText[256];
        tvi.mask = TVIF_TEXT;
        tvi.hItem = hItem;
        tvi.pszText = szText;
        tvi.cchTextMax = 256;
        TreeView_GetItem(hTree, &tvi);

        // 输出或处理节点
        OutputDebugString(szText);
        OutputDebugString(L"\n");

        // 递归处理子节点
        HTREEITEM hChild = TreeView_GetChild(hTree, hItem);
        if (hChild != NULL)
        {
            EnumTreeNodes(hTree, hChild);
        }

        // 移动到下一个兄弟节点
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

// 使用：从根节点开始遍历
HTREEITEM hRoot = TreeView_GetRoot(hTree);
EnumTreeNodes(hTree, hRoot);
```

### 展开所有节点

```cpp
// 递归展开所有节点
void ExpandAll(HWND hTree, HTREEITEM hItem)
{
    while (hItem != NULL)
    {
        // 展开当前节点
        TreeView_Expand(hTree, hItem, TVE_EXPAND);

        // 递归展开子节点
        HTREEITEM hChild = TreeView_GetChild(hTree, hItem);
        if (hChild != NULL)
        {
            ExpandAll(hTree, hChild);
        }

        // 移动到下一个兄弟节点
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

// 使用
HTREEITEM hRoot = TreeView_GetRoot(hTree);
ExpandAll(hTree, hRoot);
```

---

## 第五章：TreeView 通知处理

TreeView 通过 WM_NOTIFY 发送各种通知，通知码以 `TVN_` 开头。

### 常用通知码

| 通知码 | 说明 |
|--------|------|
| TVN_ITEMEXPANDING | 节点即将展开或折叠（可阻止） |
| TVN_ITEMEXPANDED | 节点已经展开或折叠 |
| TVN_SELCHANGING | 选择即将改变（可阻止） |
| TVN_SELCHANGED | 选择已经改变 |
| TVN_GETDISPINFO | 需要显示信息（用于懒加载） |
| TVN_SETDISPINFO | 用户修改了显示信息 |
| TVN_BEGINDRAG | 开始拖拽操作 |
| TVN_BEGINLABELEDIT | 开始编辑标签 |
| TVN_ENDLABELEDIT | 结束编辑标签 |
| TVN_KEYDOWN | 按下了键盘键 |

### NMTREEVIEW 结构

大多数 TreeView 通知使用 `NMTREEVIEW` 结构：

```cpp
typedef struct tagNMTREEVIEW {
    NMHDR hdr;              // 标准通知头
    UINT action;            // 操作类型（展开/折叠等）
    TVITEM itemOld;         // 旧的节点信息
    TVITEM itemNew;         // 新的节点信息
    POINT ptDrag;           // 拖拽位置
} NMTREEVIEW, *LPNMTREEVIEW;
```

### TVN_SELCHANGED 处理

```cpp
case WM_NOTIFY:
{
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (pnmh->idFrom == ID_TREEVIEW)
    {
        switch (pnmh->code)
        {
        case TVN_SELCHANGED:
        {
            // 选择改变了
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;

            // 获取新选中的节点
            HTREEITEM hNewItem = pnmtv->itemNew.hItem;

            // 获取节点文字
            TVITEM tvi = {0};
            wchar_t szText[256];
            tvi.mask = TVIF_TEXT;
            tvi.hItem = hNewItem;
            tvi.pszText = szText;
            tvi.cchTextMax = 256;
            TreeView_GetItem(hTree, &tvi);

            // 显示选择信息
            wchar_t msg[256];
            swprintf_s(msg, 256, L"你选择了：%s", szText);
            SetWindowText(hwndStatus, msg);

            break;
        }
        }
    }
    break;
}
```

### TVN_ITEMEXPANDING 处理（阻止展开）

```cpp
case TVN_ITEMEXPANDING:
{
    LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;

    // pnmtv->action 告诉我们是展开还是折叠
    if (pnmtv->action == TVE_EXPAND)
    {
        // 获取要展开的节点
        HTREEITEM hItem = pnmtv->itemNew.hItem;

        // 可以在这里检查是否允许展开
        // 比如：如果节点文字包含 "锁定"，则不允许展开

        TVITEM tvi = {0};
        wchar_t szText[256];
        tvi.mask = TVIF_TEXT;
        tvi.hItem = hItem;
        tvi.pszText = szText;
        tvi.cchTextMax = 256;
        TreeView_GetItem(hTree, &tvi);

        if (wcscmp(szText, L"锁定节点") == 0)
        {
            // 不允许展开
            MessageBox(hwnd, L"此节点已被锁定，无法展开", L"提示", MB_OK);
            return TRUE;  // 返回 TRUE 阻止操作
        }
    }

    return FALSE;  // 允许操作
}
```

### TVN_GETDISPINFO 处理（懒加载）

```cpp
case TVN_GETDISPINFO:
{
    LPNMTVDISPINFO pnmtdi = (LPNMTVDISPINFO)lParam;

    if (pnmtdi->item.mask & TVIF_TEXT)
    {
        // 动态提供文字
        // 假设我们用 lParam 存储字符串
        wchar_t* pszText = (wchar_t*)pnmtdi->item.lParam;
        if (pszText != NULL)
        {
            wcscpy_s(pnmtdi->item.pszText, pnmtdi->item.cchTextMax, pszText);
        }
    }

    if (pnmtdi->item.mask & TVIF_IMAGE)
    {
        // 动态提供图标索引
        pnmtdi->item.iImage = 0;  // 根据需要设置
        pnmtdi->item.iSelectedImage = 1;
    }

    return 0;
}
```

⚠️ 注意

**TVN_ITEMEXPANDING vs TVN_ITEMEXPANDED**：`TVN_ITEMEXPANDING` 在操作**发生前**触发，你可以返回 TRUE 来阻止操作。`TVN_ITEMEXPANDED` 在操作**发生后**触发，此时操作已完成，无法阻止。如果你需要在展开时动态加载子节点，应该使用 `TVN_ITEMEXPANDING`。

---

## 第六章：图标和状态

### ImageList 简介

`ImageList` 是一个图像列表容器，可以存储多个图标或位图。TreeView 使用 ImageList 来管理节点的图标。

### 创建 ImageList

```cpp
// 创建图标列表（16x16，使用系统颜色）
HIMAGELIST hImageList = ImageList_Create(
    16, 16,           // 宽度、高度
    ILC_COLOR32 | ILC_MASK,  // 颜色格式
    10,               // 初始容量
    10                // 增长容量
);

// 加载图标并添加到列表
HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_FOLDER));
int folderIndex = ImageList_AddIcon(hImageList, hIcon);

HICON hIconOpen = LoadIcon(hInst, MAKEINTRESOURCE(IDI_FOLDER_OPEN));
int folderOpenIndex = ImageList_AddIcon(hImageList, hIconOpen);

// 关联到 TreeView
TreeView_SetImageList(hTree, hImageList, TVSIL_NORMAL);
```

`TreeView_SetImageList` 的第三个参数：

| 值 | 说明 |
|----|------|
| TVSIL_NORMAL | 普通图标列表 |
| TVSIL_STATE | 状态图标列表 |

### 设置节点图标

```cpp
TVINSERTSTRUCT tvins = {0};
tvins.hParent = TVI_ROOT;
tvins.hInsertAfter = TVI_LAST;
tvins.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
tvins.item.pszText = L"文件夹";
tvins.item.iImage = 0;              // 未选中时的图标索引
tvins.item.iSelectedImage = 1;      // 选中时的图标索引
TreeView_InsertItem(hTree, &tvins);
```

### 修改节点图标

```cpp
TVITEM tvi = {0};
tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
tvi.hItem = hItem;
tvi.iImage = 2;          // 新图标
tvi.iSelectedImage = 3;  // 新选中图标
TreeView_SetItem(hTree, &tvi);
```

### 状态图标

TreeView 还可以为节点设置状态图标（显示在普通图标旁边的小图标）：

```cpp
TVITEM tvi = {0};
tvi.mask = TVIF_STATE;
tvi.hItem = hItem;
tvi.stateMask = TVIS_STATEIMAGEMASK;
tvi.state = INDEXTOSTATEIMAGEMASK(1);  // 状态图标索引 1-15
TreeView_SetItem(hTree, &tvi);
```

`INDEXTOSTATEIMAGEMASK(index)` 宏将 1-15 的索引转换为状态值。

---

## 第七章：实战：目录树展示器

好了，理论讲得差不多了。现在让我们做一个完整的项目：一个模拟资源管理器目录树的程序。这个程序会：

1. 显示所有驱动器
2. 展开时动态加载子目录（懒加载）
3. 显示文件夹图标
4. 双击节点打开对应的文件夹

### 完整代码

```cpp
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

#define ID_TREEVIEW    1001
#define ID_STATUSBAR   1002

// 全局变量
HWND g_hTree = NULL;
HIMAGELIST g_hImageList = NULL;
int g_iIconFolder = 0;
int g_iIconFolderOpen = 1;
int g_iIconDrive = 2;

// 图标资源 ID（在实际项目中需要添加资源）
#define IDI_FOLDER        101
#define IDI_FOLDER_OPEN   102
#define IDI_DRIVE         103

// 初始化图标列表
void InitImageList(HINSTANCE hInst)
{
    g_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 10, 10);

    // 在实际项目中，从资源加载图标
    // HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_FOLDER));
    // g_iIconFolder = ImageList_AddIcon(g_hImageList, hIcon);

    // 这里使用系统图标作为示例
    SHFILEINFO sfi;
    HIMAGELIST hSysImageList = (HIMAGELIST)SHGetFileInfo(L"C:\\", 0, &sfi,
        sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

    // 复制系统图标到我们的列表
    if (hSysImageList)
    {
        ICONINFO ii;
        HICON hIcon;

        // 文件夹图标
        hIcon = ImageList_GetIcon(hSysImageList, sfi.iIcon, ILD_NORMAL);
        if (hIcon)
        {
            g_iIconFolder = ImageList_AddIcon(g_hImageList, hIcon);
            DestroyIcon(hIcon);
        }

        g_iIconFolderOpen = g_iIconFolder;  // 简化处理
        g_iIconDrive = g_iIconFolder;
    }

    TreeView_SetImageList(g_hTree, g_hImageList, TVSIL_NORMAL);
}

// 添加驱动器节点
void AddDrives()
{
    wchar_t drives[256];
    GetLogicalDriveStrings(256, drives);

    wchar_t* p = drives;
    while (*p)
    {
        DWORD type = GetDriveType(p);

        // 只添加固定驱动器、可移动驱动器和网络驱动器
        if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_REMOTE)
        {
            TVINSERTSTRUCT tvins = {0};
            tvins.hParent = TVI_ROOT;
            tvins.hInsertAfter = TVI_LAST;
            tvins.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
            tvins.item.pszText = p;
            tvins.item.iImage = g_iIconDrive;
            tvins.item.iSelectedImage = g_iIconDrive;

            // 保存驱动器路径
            wchar_t* path = new wchar_t[4];
            wcscpy_s(path, 4, p);
            tvins.item.lParam = (LPARAM)path;

            HTREEITEM hItem = TreeView_InsertItem(g_hTree, &tvins);

            // 检查是否有子目录，如果有就添加一个占位节点
            wchar_t searchPath[MAX_PATH];
            wcscpy_s(searchPath, MAX_PATH, p);
            wcscat_s(searchPath, MAX_PATH, L"*");

            WIN32_FIND_DATA fd;
            HANDLE hFind = FindFirstFile(searchPath, &fd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                BOOL hasDir = FALSE;
                do
                {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        if (wcscmp(fd.cFileName, L".") != 0 &&
                            wcscmp(fd.cFileName, L"..") != 0)
                        {
                            hasDir = TRUE;
                            break;
                        }
                    }
                } while (FindNextFile(hFind, &fd));
                FindClose(hFind);

                if (hasDir)
                {
                    // 添加占位子节点
                    TVINSERTSTRUCT tvinsChild = {0};
                    tvinsChild.hParent = hItem;
                    tvinsChild.hInsertAfter = TVI_LAST;
                    tvinsChild.item.mask = TVIF_TEXT | TVIF_PARAM;
                    tvinsChild.item.pszText = L"";
                    tvinsChild.item.lParam = 0;  // 0 表示占位节点
                    TreeView_InsertItem(g_hTree, &tvinsChild);
                }
            }
        }

        p += wcslen(p) + 1;
    }
}

// 加载子目录
void LoadSubDirectories(HTREEITEM hParent, const wchar_t* path)
{
    // 先删除占位节点
    HTREEITEM hChild = TreeView_GetChild(g_hTree, hParent);
    while (hChild != NULL)
    {
        HTREEITEM hNext = TreeView_GetNextSibling(g_hTree, hChild);
        TVITEM tvi = {0};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hChild;
        TreeView_GetItem(g_hTree, &tvi);

        if (tvi.lParam == 0)  // 占位节点
        {
            TreeView_DeleteItem(g_hTree, hChild);
            break;
        }
        hChild = hNext;
    }

    // 搜索子目录
    wchar_t searchPath[MAX_PATH];
    wcscpy_s(searchPath, MAX_PATH, path);
    wcscat_s(searchPath, MAX_PATH, L"*");

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (wcscmp(fd.cFileName, L".") != 0 &&
                    wcscmp(fd.cFileName, L"..") != 0)
                {
                    // 构造完整路径
                    wchar_t fullPath[MAX_PATH];
                    wcscpy_s(fullPath, MAX_PATH, path);
                    wcscat_s(fullPath, MAX_PATH, fd.cFileName);
                    wcscat_s(fullPath, MAX_PATH, L"\\");

                    // 保存路径
                    wchar_t* savedPath = new wchar_t[wcslen(fullPath) + 1];
                    wcscpy_s(savedPath, wcslen(fullPath) + 1, fullPath);

                    // 添加节点
                    TVINSERTSTRUCT tvins = {0};
                    tvins.hParent = hParent;
                    tvins.hInsertAfter = TVI_LAST;
                    tvins.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
                    tvins.item.pszText = fd.cFileName;
                    tvins.item.iImage = g_iIconFolder;
                    tvins.item.iSelectedImage = g_iIconFolderOpen;
                    tvins.item.lParam = (LPARAM)savedPath;

                    HTREEITEM hItem = TreeView_InsertItem(g_hTree, &tvins);

                    // 检查是否有子目录
                    wcscpy_s(searchPath, MAX_PATH, fullPath);
                    wcscat_s(searchPath, MAX_PATH, L"*");

                    WIN32_FIND_DATA fd2;
                    HANDLE hFind2 = FindFirstFile(searchPath, &fd2);
                    if (hFind2 != INVALID_HANDLE_VALUE)
                    {
                        BOOL hasSubDir = FALSE;
                        do
                        {
                            if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                            {
                                if (wcscmp(fd2.cFileName, L".") != 0 &&
                                    wcscmp(fd2.cFileName, L"..") != 0)
                                {
                                    hasSubDir = TRUE;
                                    break;
                                }
                            }
                        } while (FindNextFile(hFind2, &fd2));
                        FindClose(hFind2);

                        if (hasSubDir)
                        {
                            // 添加占位节点
                            TVINSERTSTRUCT tvinsChild = {0};
                            tvinsChild.hParent = hItem;
                            tvinsChild.hInsertAfter = TVI_LAST;
                            tvinsChild.item.mask = TVIF_TEXT | TVIF_PARAM;
                            tvinsChild.item.pszText = L"";
                            tvinsChild.item.lParam = 0;
                            TreeView_InsertItem(g_hTree, &tvinsChild);
                        }
                    }
                }
            }
        } while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
}

// 递归释放路径字符串
void FreeTreeData(HTREEITEM hItem)
{
    while (hItem != NULL)
    {
        // 释放当前节点的数据
        TVITEM tvi = {0};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hItem;
        TreeView_GetItem(g_hTree, &tvi);

        if (tvi.lParam != 0)
        {
            wchar_t* path = (wchar_t*)tvi.lParam;
            delete[] path;
        }

        // 递归处理子节点
        HTREEITEM hChild = TreeView_GetChild(g_hTree, hItem);
        if (hChild != NULL)
        {
            FreeTreeData(hChild);
        }

        // 移动到下一个兄弟节点
        HTREEITEM hNext = TreeView_GetNextSibling(g_hTree, hItem);
        hItem = hNext;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建 TreeView
        g_hTree = CreateWindowEx(
            0, WC_TREEVIEW, L"",
            WS_CHILD | WS_VISIBLE |
            TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
            TVS_SHOWSELALWAYS,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_TREEVIEW, hInst, NULL
        );

        // 初始化图标列表
        InitImageList(hInst);

        // 添加驱动器
        AddDrives();

        // 创建状态栏
        CreateWindowEx(
            0, STATUSCLASSNAME, L"",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_STATUSBAR, hInst, NULL
        );

        return 0;
    }

    case WM_SIZE:
    {
        // 调整 TreeView 和状态栏大小
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        HWND hStatus = GetDlgItem(hwnd, ID_STATUSBAR);
        if (hStatus)
        {
            SendMessage(hStatus, WM_SIZE, 0, 0);

            RECT rcStatus;
            GetWindowRect(hStatus, &rcStatus);
            int statusHeight = rcStatus.bottom - rcStatus.top;

            SetWindowPos(g_hTree, NULL, 0, 0,
                rcClient.right, rcClient.bottom - statusHeight,
                SWP_NOZORDER);
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        if (pnmh->idFrom == ID_TREEVIEW)
        {
            switch (pnmh->code)
            {
            case TVN_ITEMEXPANDING:
            {
                // 节点即将展开，加载子目录
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;

                if (pnmtv->action == TVE_EXPAND)
                {
                    HTREEITEM hItem = pnmtv->itemNew.hItem;

                    // 获取节点路径
                    TVITEM tvi = {0};
                    tvi.mask = TVIF_PARAM;
                    tvi.hItem = hItem;
                    TreeView_GetItem(g_hTree, &tvi);

                    if (tvi.lParam != 0)
                    {
                        wchar_t* path = (wchar_t*)tvi.lParam;
                        LoadSubDirectories(hItem, path);
                    }
                }
                break;
            }

            case TVN_SELCHANGED:
            {
                // 选择改变，更新状态栏
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                HTREEITEM hItem = pnmtv->itemNew.hItem;

                if (hItem != NULL)
                {
                    TVITEM tvi = {0};
                    wchar_t szText[MAX_PATH];
                    tvi.mask = TVIF_TEXT | TVIF_PARAM;
                    tvi.hItem = hItem;
                    tvi.pszText = szText;
                    tvi.cchTextMax = MAX_PATH;
                    TreeView_GetItem(g_hTree, &tvi);

                    HWND hStatus = GetDlgItem(hwnd, ID_STATUSBAR);
                    if (hStatus)
                    {
                        if (tvi.lParam != 0)
                        {
                            wchar_t* path = (wchar_t*)tvi.lParam;
                            SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)path);
                        }
                        else
                        {
                            SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)szText);
                        }
                    }
                }
                break;
            }

            case NM_DBLCLK:
            {
                // 双击节点
                HTREEITEM hItem = TreeView_GetSelection(g_hTree);
                if (hItem != NULL)
                {
                    TVITEM tvi = {0};
                    tvi.mask = TVIF_PARAM;
                    tvi.hItem = hItem;
                    TreeView_GetItem(g_hTree, &tvi);

                    if (tvi.lParam != 0)
                    {
                        wchar_t* path = (wchar_t*)tvi.lParam;

                        // 打开文件夹
                        ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOW);
                    }
                }
                break;
            }
            }
        }
        return 0;
    }

    case WM_DESTROY:
    {
        // 清理分配的内存
        HTREEITEM hRoot = TreeView_GetRoot(g_hTree);
        if (hRoot != NULL)
        {
            FreeTreeData(hRoot);
        }

        // 销毁图标列表
        if (g_hImageList != NULL)
        {
            ImageList_Destroy(g_hImageList);
        }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DirTreeWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"目录树展示器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 600,
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

⚠️ 注意

**内存管理**：这个例子中，我们为每个节点分配了内存来存储路径。在 WM_DESTROY 时必须释放这些内存，否则会内存泄漏。生产环境中，你可能还需要处理 WM_DELETEITEM 通知来释放单个被删除节点的数据。

**占位节点技巧**：为了显示展开按钮，我们给可能有子目录的节点添加了一个空的占位子节点。当用户展开时，我们删除占位节点并加载真正的子目录。这是 TreeView 懒加载的标准做法。

---

## 后续可以做什么

到这里，TreeView 控件的核心知识就讲完了。你现在应该能够创建功能完整的树形控件，处理各种用户交互，实现懒加载优化性能。

但 Win32 的控件世界远不止这些。接下来还有更多高级控件等着我们：

* **TabControl**：标签页控件，用于分组显示内容
* **ProgressBar**：进度条，显示任务完成进度
* **TrackBar**：滑块控件，用于调节数值
* **UpDown**：上下箭头控件，配合编辑框使用
* **ComboBoxEx**：扩展组合框，支持图标

在此之前，建议你先把今天的内容消化一下。试着做一些小练习：

1. **基础练习**：修改目录树程序，添加删除节点的功能（右键菜单）
2. **进阶练习**：实现节点的拖放功能，允许用户将文件夹拖到其他位置
3. **挑战练习**：使用 TVN_BEGINLABELEDIT 和 TVN_ENDLABELEDIT 实现节点重命名功能

好了，今天的文章就到这里。掌握了 TreeView，你的 Win32 工具箱里又多了一件利器。我们下一篇再见！

---

**相关资源**

- [TreeView 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/tree-view-controls)
- [TVITEM 结构 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/commctrl/ns-commctrl-tvitemw)
- [TVINSERTSTRUCT 结构 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/commctrl/ns-commctrl-tvinsertstructw)
- [TreeView 通知码 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/tree-view-control-reference)
