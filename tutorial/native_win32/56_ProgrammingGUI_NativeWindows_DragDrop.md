# 通用GUI编程技术——Win32 原生编程实战（五十六）——拖放（Drag & Drop）

> 上一篇文章我们学会了系统托盘——让你的程序可以安静地在后台运行，用户需要时再调出来。今天我们要聊的是一个几乎每天都会用到但很少去想"怎么实现"的功能：拖放。从资源管理器拖一个文件到你的程序窗口上打开、从浏览器拖一张图片到编辑器里、拖一段文字到输入框中——这些都是拖放操作。Win32 提供了两套拖放机制：简单的 WM_DROPFILES 和完整的 OLE IDropTarget。今天我们两个都讲。

---

## 为什么需要拖放

拖放是现代 GUI 中最自然的交互方式之一。用户不需要在菜单里翻找"打开文件"命令、不需要在对话框里一层层导航——直接把文件从资源管理器拖过来就行。

如果你的程序需要接收文件或数据，支持拖放几乎是标配。不支持拖放的程序会给用户一种"不完整"的感觉——就像一个输入框不支持 Ctrl+V 粘贴一样别扭。

从技术角度说，拖放涉及到进程间通信（数据从资源管理器传递到你的程序）、COM 接口（OLE 拖放协议）、以及消息处理。理解了这套机制，你对 Windows 的进程间数据传递会有更深的认识。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道窗口过程怎么写、WM_COMMAND 和 WM_NOTIFY 怎么处理。OLE 拖放部分需要基本的 COM 知识。

---

## 第一步——两套机制对比

Win32 提供了两套拖放机制，适用于不同场景：

| 特性 | WM_DROPFILES | OLE IDropTarget |
|------|-------------|-----------------|
| 支持的数据 | 仅文件（CF_HDROP） | 任意数据格式 |
| 复杂度 | 非常简单 | 中等（需实现 COM 接口） |
| 初始化 | DragAcceptFiles | OleInitialize + RegisterDragDrop |
| 拖入高亮 | 不支持 | 支持（DragEnter/DragOver/DragLeave） |
| 跨进程位 | 自动处理 | 自动处理 |
| 适用场景 | 只需接收文件拖入 | 需要接收文字、图片等自定义数据 |
| 64/32 位兼容 | 自动处理 | 需要额外处理（ChangeWindowMessageFilterEx） |

**选择建议**：
* 只需要接收文件拖入 → **WM_DROPFILES**（5 行代码搞定）
* 需要接收非文件数据（文字、图片等）→ **OLE IDropTarget**
* 两者可以共存，先注册 OLE 拖放，再调用 DragAcceptFiles

---

## 第二步——WM_DROPFILES：简单文件拖入

### 注册窗口接受文件

只需要一行代码：

```cpp
DragAcceptFiles(hwnd, TRUE);
```

调用后，当用户从资源管理器拖文件到你的窗口时，窗口会收到 `WM_DROPFILES` 消息。

如果想禁用，传 FALSE：

```cpp
DragAcceptFiles(hwnd, FALSE);
```

### 处理 WM_DROPFILES

```cpp
case WM_DROPFILES:
{
    HDROP hDrop = (HDROP)wParam;

    // 获取拖入的文件数量
    UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    for (UINT i = 0; i < fileCount; i++)
    {
        // 获取文件路径的长度
        UINT pathLen = DragQueryFile(hDrop, i, NULL, 0);

        // 分配缓冲区并获取文件路径
        wchar_t* filePath = new wchar_t[pathLen + 1];
        DragQueryFile(hDrop, i, filePath, pathLen + 1);

        // 使用 filePath...
        // 例如添加到列表、打开文件等

        delete[] filePath;
    }

    // 获取鼠标释放位置（可选）
    POINT pt;
    DragQueryPoint(hDrop, &pt);
    // pt 是鼠标在客户区中的坐标

    // 释放 HDROP 句柄
    DragFinish(hDrop);

    return 0;
}
```

### DragQueryFile 详解

```cpp
UINT DragQueryFile(
    HDROP  hDrop,
    UINT   iFile,       // 文件索引（0xFFFFFFFF = 查询数量）
    LPTSTR lpszFile,    // 输出缓冲区（NULL = 查询长度）
    UINT   cch          // 缓冲区大小
);
```

| iFile 值 | 返回值 | 行为 |
|----------|--------|------|
| 0xFFFFFFFF | 文件数量 | 查询有多少文件被拖入 |
| 0, 1, 2... | 路径字符串长度（不含 \0） | 查询指定文件的路径 |

### 完整示例：文件拖入列表

```cpp
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#define ID_LISTVIEW 1001

HWND g_hListView = NULL;

void AddDroppedFiles(HWND hListView, HDROP hDrop)
{
    UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    for (UINT i = 0; i < fileCount; i++)
    {
        UINT len = DragQueryFile(hDrop, i, NULL, 0);
        wchar_t* path = new wchar_t[len + 1];
        DragQueryFile(hDrop, i, path, len + 1);

        // 提取文件名
        const wchar_t* fileName = wcsrchr(path, L'\\');
        fileName = fileName ? fileName + 1 : path;

        // 提取扩展名
        const wchar_t* ext = wcsrchr(path, L'.');
        ext = ext ? ext + 1 : L"";

        // 添加到 ListView
        int idx = ListView_GetItemCount(hListView);

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = (LPWSTR)fileName;
        ListView_InsertItem(hListView, &lvi);

        ListView_SetItemText(hListView, idx, 1, (LPWSTR)ext);
        ListView_SetItemText(hListView, idx, 2, path);

        delete[] path;
    }

    DragFinish(hDrop);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 初始化通用控件
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        // 创建 ListView
        g_hListView = CreateWindowEx(0, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_BORDER,
            0, 0, 0, 0,
            hwnd, (HMENU)ID_LISTVIEW, hInst, NULL);

        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // 添加列
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

        lvc.cx = 150; lvc.pszText = L"文件名"; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(g_hListView, 0, &lvc);

        lvc.cx = 80; lvc.pszText = L"类型";
        ListView_InsertColumn(g_hListView, 1, &lvc);

        lvc.cx = 350; lvc.pszText = L"完整路径";
        ListView_InsertColumn(g_hListView, 2, &lvc);

        // 注册窗口接受文件拖放
        DragAcceptFiles(hwnd, TRUE);

        return 0;
    }

    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        MoveWindow(g_hListView, 0, 0, rc.right, rc.bottom, TRUE);
        return 0;
    }

    case WM_DROPFILES:
        AddDroppedFiles(g_hListView, (HDROP)wParam);
        return 0;

    case WM_DESTROY:
        DragAcceptFiles(hwnd, FALSE);
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
    wc.lpszClassName = L"FileDropDemo";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES,  // 也可以在窗口类中设置
        L"FileDropDemo", L"文件拖放示例 - 将文件拖入此窗口",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 400,
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

1. **DragAcceptFiles**：在 WM_CREATE 中调用，注册窗口接受文件拖入。也可以在 CreateWindowEx 时使用 `WS_EX_ACCEPTFILES` 扩展样式。

2. **DragQueryFile(0xFFFFFFFF)**：先查询数量，再逐个获取路径。

3. **DragFinish**：使用完毕后必须调用，释放系统分配的资源。

---

## 第三步——OLE 拖放：IDropTarget

WM_DROPFILES 只能接收文件。如果你想接收文字、图片、自定义数据格式，就需要使用 OLE 拖放协议。

OLE 拖放涉及四个角色：

| 角色 | 说明 |
|------|------|
| IDropSource | 数据的提供方（拖出去的源） |
| IDropTarget | 数据的接收方（拖入的目标） |
| IDataObject | 数据的容器（承载数据本身） |
| IEnumFORMATETC | 枚举数据格式（列出 IDataObject 支持哪些格式） |

作为拖放接收方，你只需要实现 `IDropTarget` 接口。

### 初始化 OLE

```cpp
// 在 WinMain 的最开始
OleInitialize(NULL);

// 在程序退出前
OleUninitialize();
```

⚠️ 注意

**必须调用 OleInitialize**，不是 CoInitialize。OleInitialize 内部会调用 CoInitialize，额外初始化 OLE 子系统。如果你只调用 CoInitialize，RegisterDragDrop 会失败但不一定有错误提示。

### IDropTarget 接口

```cpp
class MyDropTarget : public IDropTarget
{
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IDropTarget
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                           POINTL pt, DWORD* pdwEffect);
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHODIMP DragLeave();
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState,
                      POINTL pt, DWORD* pdwEffect);

private:
    LONG  m_cRef = 1;
    HWND  m_hwnd = NULL;
    bool  m_isValidData = false;
};
```

### 四个方法的调用时机

```
DragEnter → 文件进入窗口区域
  ↓
DragOver → 文件在窗口内移动（频繁调用）
  ↓
DragLeave → 文件离开窗口区域（没有放下）
  或
Drop → 文件在窗口内放下
```

### DragEnter：判断数据是否可以接受

```cpp
STDMETHODIMP MyDropTarget::DragEnter(
    IDataObject* pDataObj, DWORD grfKeyState,
    POINTL pt, DWORD* pdwEffect)
{
    // 检查数据对象是否包含我们想要的格式
    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;       // 文件列表格式
    fmt.ptd = NULL;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    HRESULT hr = pDataObj->QueryGetData(&fmt);
    m_isValidData = SUCCEEDED(hr);

    if (m_isValidData)
    {
        *pdwEffect = DROPEFFECT_COPY;  // 显示"复制"光标
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;  // 显示"禁止"光标
    }

    return S_OK;
}
```

### DragOver：持续反馈

```cpp
STDMETHODIMP MyDropTarget::DragOver(
    DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    if (m_isValidData)
    {
        // 可以根据 pt 位置判断是否在有效区域
        *pdwEffect = DROPEFFECT_COPY;
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;
    }

    return S_OK;
}
```

### DragLeave：离开窗口

```cpp
STDMETHODIMP MyDropTarget::DragLeave()
{
    m_isValidData = false;
    return S_OK;
}
```

### Drop：接收数据

```cpp
STDMETHODIMP MyDropTarget::Drop(
    IDataObject* pDataObj, DWORD grfKeyState,
    POINTL pt, DWORD* pdwEffect)
{
    if (!m_isValidData)
    {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // 获取 HDROP 数据
    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;
    fmt.ptd = NULL;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM stg = {};
    HRESULT hr = pDataObj->GetData(&fmt, &stg);

    if (SUCCEEDED(hr))
    {
        HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
        if (hDrop)
        {
            UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < fileCount; i++)
            {
                UINT len = DragQueryFile(hDrop, i, NULL, 0);
                wchar_t* path = new wchar_t[len + 1];
                DragQueryFile(hDrop, i, path, len + 1);

                // 使用 path...
                // 可以通过 PostMessage 通知主窗口

                delete[] path;
            }
            GlobalUnlock(stg.hGlobal);
        }

        // 释放 STGMEDIUM
        ReleaseStgMedium(&stg);
    }

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}
```

### 注册 IDropTarget

```cpp
// 在 WM_CREATE 中
MyDropTarget* pDropTarget = new MyDropTarget(hwnd);
RegisterDragDrop(hwnd, pDropTarget);

// 在 WM_DESTROY 中
RevokeDragDrop(hwnd);
pDropTarget->Release();
```

---

## 第四步——获取其他数据格式

OLE 拖放的强大之处在于可以接收任意数据格式。除了 CF_HDROP（文件），常用的还有：

| 格式 | 常量 | 说明 |
|------|------|------|
| 文件列表 | CF_HDROP | 被拖入的文件路径 |
| Unicode 文字 | CF_UNICODETEXT | 纯文本 |
| 位图 | CF_DIB / CF_BITMAP | 图片数据 |
| 富文本 | 注册 "Rich Text Format" | RTF 格式文字 |
| HTML | 注册 "HTML Format" | HTML 内容 |
| 自定义 | RegisterClipboardFormat | 你自己定义的格式 |

### 获取文本数据的示例

```cpp
// 检查是否有文本数据
FORMATETC fmt = {};
fmt.cfFormat = CF_UNICODETEXT;
fmt.dwAspect = DVASPECT_CONTENT;
fmt.lindex = -1;
fmt.tymed = TYMED_HGLOBAL;

STGMEDIUM stg = {};
if (SUCCEEDED(pDataObj->GetData(&fmt, &stg)))
{
    wchar_t* text = (wchar_t*)GlobalLock(stg.hGlobal);
    if (text)
    {
        // 使用 text...
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
}
```

---

## 常见陷阱

### 陷阱一：忘记 OleInitialize

```cpp
// 错误！CoInitialize 不够
CoInitialize(NULL);
RegisterDragDrop(hwnd, pDropTarget);  // 静默失败

// 正确
OleInitialize(NULL);
RegisterDragDrop(hwnd, pDropTarget);  // 成功
```

### 陷阱二：STGMEDIUM 未释放

`IDataObject::GetData` 返回的 STGMEDIUM 必须通过 `ReleaseStgMedium` 释放。直接 `GlobalFree` 或 `delete` 不够——STGMEDIUM 的释放规则取决于 tymed 字段。

### 陷阱三：64 位 / 32 位拖放

64 位程序默认不能接收来自 32 位程序的拖放（反过来也一样）。如果你需要跨位数拖放，需要调用 `ChangeWindowMessageFilterEx`：

```cpp
// 允许接收来自其他位数进程的拖放消息
ChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, NULL);
ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, NULL);
ChangeWindowMessageFilterEx(hwnd, 0x0049, MSGFLT_ALLOW, NULL);
```

### 陷阱四：DragEnter 中做耗时操作

DragEnter、DragOver 在拖放过程中被频繁调用（鼠标每移动一个像素都可能调用 DragOver）。不要在这些方法中做耗时操作——应该只做快速的格式检查和光标设置。

---

## 后续可以做什么

到这里，拖放功能就讲完了。你现在应该理解了两套拖放机制的适用场景、WM_DROPFILES 的简单用法、OLE IDropTarget 的完整实现、以及如何接收不同数据格式（文件、文本等）。

下一篇文章，我们会聊一个相对轻量但非常实用的功能——**定时器**。你将学会使用 SetTimer/WM_TIMER 做定时任务和动画，以及如何使用高精度定时器。

在此之前，建议你做一些练习巩固今天的知识：

1. **基础练习**：修改 WM_DROPFILES 示例，计算并显示拖入文件的总大小
2. **进阶练习**：实现 OLE IDropTarget 版本，同时支持文件拖入和文本拖入（检查 CF_HDROP 和 CF_UNICODETEXT 两种格式）
3. **挑战练习**：实现拖入高亮效果——当文件拖到窗口上时，窗口边框变色或显示覆盖层提示"松开以打开文件"（提示：在 DragEnter/DragLeave 中控制一个状态标志，在 WM_PAINT 中绘制高亮效果）

---

## 相关资源

- [DragAcceptFiles function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragacceptfiles)
- [WM_DROPFILES message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles)
- [IDropTarget interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/oleidl/nn-oleidl-idroptarget)
- [RegisterDragDrop function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-registerdragdrop)
- [OleInitialize function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-oleinitialize)
- [Standard Clipboard Formats - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats)
