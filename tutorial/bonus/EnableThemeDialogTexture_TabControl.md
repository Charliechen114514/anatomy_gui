# 通用GUI编程技术——Win32 原生编程实战（番外）——TabControl 的深色背景与 EnableThemeDialogTexture

> 写在最前面：非常感谢来自知乎的老师：巨大八爪鱼的私信！这位老师建有我提一下`EnableThemeDialogTexture`，并且给出了一些参考博客！这里特别鸣谢！
> 这位老师提出的博客：https://blog.csdn.net/ZLK1214/article/details/140308793

---

> 在使用 TabControl 时，如果你用对话框作为每个选项卡的内容，可能会遇到一个让人头疼的问题：子对话框有深色背景，和 TabControl 的浅色背景格格不入。这个问题困扰了无数 Win32 开发者。今天我们就来彻底解决这个问题，顺便认识一个很多人不知道但其实非常有用的函数——`EnableThemeDialogTexture`。

当你用对话框充当 TabControl 的每个选项卡内容时，通常会这样设计：

1. 创建主对话框，上面放一个 TabControl 控件
2. 为每个选项卡创建一个子对话框（`IDD_DIALOG1`、`IDD_DIALOG2`...）
3. 子对话框的 `Border` 属性设为 `None`，`Style` 属性设为 `Child`
4. 用户切换标签时显示/隐藏对应的子对话框

但运行后你会看到：

```
┌─────────────────────────────────────────┐
│ [第一个选项卡] [第二个选项卡]             │
├─────────────────────────────────────────┤
│ ┌─────────────────────────────────────┐ │
│ │ 这里是深色的对话框背景！！！          │ │  ← 烦人的深色背景
│ │                                     │ │
│ │  [控件1]  [控件2]                   │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

这个深色背景和 TabControl 的样式完全不搭，看起来非常难看。

---

## 解决方案：EnableThemeDialogTexture

要去掉这个深色对话框背景，只需要在每个选项卡对话框的 `WM_INITDIALOG` 消息处理中加一行代码：

```cpp
#include <Uxtheme.h>
#pragma comment(lib, "Uxtheme.lib")

INT_PTR CALLBACK Page1_Proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_INITDIALOG:
        // 删除选项卡的深色灰色背景
        EnableThemeDialogTexture(hdlg, ETDT_ENABLETAB);
        // ... 其他初始化代码
        break;
    }
    return 0;
}
```

**有多少个选项卡，就在多少个对话框的 WM_INITDIALOG 中加上这一行。**

运行后的效果：

```
┌─────────────────────────────────────────┐
│ [第一个选项卡] [第二个选项卡]             │
├─────────────────────────────────────────┤
│                                         │
│  这里是正常的浅色背景，和 TabControl     │  ← 完美融合！
│  的背景融为一体                          │
│                                         │
│  [控件1]  [控件2]                       │
└─────────────────────────────────────────┘
```

---

## EnableThemeDialogTexture 函数详解

### 函数原型

```cpp
HRESULT EnableThemeDialogTexture(
    HWND hwnd,
    DWORD dwFlags
);
```

### 参数说明

| 参数 | 说明 |
|------|------|
| `hwnd` | 对话框窗口句柄 |
| `dwFlags` | 标志位，指定要应用的主题纹理类型 |

### 标志位（dwFlags）

| 值 | 说明 |
|-----|------|
| `ETDT_ENABLETAB` | 启用选项卡纹理（去除深色背景） |
| `ETDT_ENABLE` | 启用主题纹理（一般用于属性表） |
| `ETDT_DISABLE` | 禁用主题纹理 |
| `ETDT_NONE` | 等同于 `ETDT_DISABLE` |

**对于 TabControl 的子对话框，你应该使用 `ETDT_ENABLETAB`。**

### 返回值

- `S_OK`：成功
- `E_NOTIMPL`：当前系统不支持（Windows XP 以下）
- 其他错误码：表示失败

---

## 前置条件

### 头文件和库

使用 `EnableThemeDialogTexture` 需要包含 `Uxtheme.h` 和链接 `Uxtheme.lib`：

```cpp
#include <Uxtheme.h>
#pragma comment(lib, "Uxtheme.lib")
```

### 系统支持

这个函数从 **Windows XP** 开始支持。在 Windows 95/98/Me/2000 上不可用，但现代开发基本不需要考虑这些系统。

---

## 完整示例

下面是一个完整的 TabControl + 子对话框的示例，展示如何正确使用 `EnableThemeDialogTexture`：

```cpp
#include <windows.h>
#include <commctrl.h>
#include <Uxtheme.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Uxtheme.lib")

// 全局变量
HINSTANCE g_hInstance;
HWND g_hMainDlg;
HWND g_hTabPages[2] = {NULL};

// 选项卡1的消息处理函数
INT_PTR CALLBACK Page1_Proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_INITDIALOG:
        // 关键代码：删除选项卡的深色灰色背景
        EnableThemeDialogTexture(hdlg, ETDT_ENABLETAB);

        // 其他初始化代码
        Button_SetCheck(GetDlgItem(hdlg, IDC_RADIO1), BST_CHECKED);
        break;

    case WM_COMMAND:
        // 处理控件命令
        break;
    }
    return 0;
}

// 选项卡2的消息处理函数
INT_PTR CALLBACK Page2_Proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_INITDIALOG:
        // 关键代码：删除选项卡的深色灰色背景
        EnableThemeDialogTexture(hdlg, ETDT_ENABLETAB);

        // 其他初始化代码
        // ...
        break;

    case WM_COMMAND:
        // 处理控件命令
        break;
    }
    return 0;
}

// 初始化选项卡
void InitTabs(HWND hDlg)
{
    HWND hTab = GetDlgItem(hDlg, IDC_TAB1);

    // 添加选项卡
    TCITEM tie = {0};
    tie.mask = TCIF_TEXT;

    tie.pszText = L"第一个选项卡";
    TabCtrl_InsertItem(hTab, 0, &tie);

    tie.pszText = L"第二个选项卡";
    TabCtrl_InsertItem(hTab, 1, &tie);

    // 计算内容区域
    RECT rcTab;
    GetWindowRect(hTab, &rcTab);
    MapWindowPoints(NULL, hDlg, (POINT*)&rcTab, 2);
    TabCtrl_AdjustRect(hTab, FALSE, &rcTab);

    // 创建子对话框
    g_hTabPages[0] = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1),
                                   hDlg, Page1_Proc);
    g_hTabPages[1] = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG2),
                                   hDlg, Page2_Proc);

    // 设置位置和大小
    for (int i = 0; i < 2; i++)
    {
        SetWindowPos(g_hTabPages[i], NULL,
                     rcTab.left, rcTab.top,
                     rcTab.right - rcTab.left,
                     rcTab.bottom - rcTab.top,
                     SWP_NOZORDER | (i == 0 ? SWP_SHOWWINDOW : 0));
    }
}

// 切换选项卡
void SwitchTab(HWND hDlg)
{
    HWND hTab = GetDlgItem(hDlg, IDC_TAB1);
    int currentIndex = TabCtrl_GetCurSel(hTab);

    for (int i = 0; i < 2; i++)
    {
        ShowWindow(g_hTabPages[i], (i == currentIndex) ? SW_SHOW : SW_HIDE);
    }

    // 重要：将焦点设回 TabControl，这样 Tab 键才能正常工作
    SetFocus(hTab);
}

// 主对话框消息处理
INT_PTR CALLBACK MainDlg_Proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_INITDIALOG:
        g_hMainDlg = hdlg;
        InitTabs(hdlg);
        return TRUE;

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == IDC_TAB1 && pnmh->code == TCN_SELCHANGE)
        {
            SwitchTab(hdlg);
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL)
        {
            EndDialog(hdlg, 0);
        }
        break;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINDLG), NULL, MainDlg_Proc);
    return 0;
}
```

---

## 资源文件示例

```rc
// resource.h
#define IDD_MAINDLG     100
#define IDD_DIALOG1     101
#define IDD_DIALOG2     102
#define IDC_TAB1        1001
#define IDC_RADIO1      1002
#define IDC_RADIO2      1003
#define IDC_LIST1       1004

// resource.rc
#include <windows.h>
#include "resource.h"

IDD_MAINDLG DIALOGEX 0, 0, 300, 200
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "EnableThemeDialogTexture 示例"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    CONTROL         "", IDC_TAB1, "SysTabControl32", WS_TABSTOP, 7, 7, 286, 186
END

IDD_DIALOG1 DIALOGEX 0, 0, 280, 160
STYLE DS_SETFONT | WS_CHILD | WS_SYSMENU
EXSTYLE WS_EX_CONTROLPARENT
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    GROUPBOX        "设置", IDC_STATIC, 7, 7, 266, 50
    CONTROL         "模式1", IDC_RADIO1, "Button", BS_AUTORADIOBUTTON | WS_TABSTOP, 20, 25, 50, 10
    CONTROL         "模式2", IDC_RADIO2, "Button", BS_AUTORADIOBUTTON | WS_TABSTOP, 80, 25, 50, 10
END

IDD_DIALOG2 DIALOGEX 0, 0, 280, 160
STYLE DS_SETFONT | WS_CHILD | WS_SYSMENU
EXSTYLE WS_EX_CONTROLPARENT
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    CONTROL         "", IDC_LIST1, "SysListView32", WS_BORDER | WS_TABSTOP, 7, 7, 266, 146
END
```

---

## 注意事项：Tab 键导航

为了让 Tab 键能够正常在 TabControl 和子页面控件间切换，需要确保以下几点：

1. **使用 `CreateDialog` 创建子页面时，父窗口必须是主对话框**，不能是 TabControl 本身
   ```cpp
   // 正确
   CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hDlg, Page1_Proc);

   // 错误 - 不要把 TabControl 作为父窗口
   CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hTab, Page1_Proc);
   ```

2. **TabControl 的 Tabstop 属性必须为 True**
   ```rc
   CONTROL "", IDC_TAB1, "SysTabControl32", WS_TABSTOP, ...
   ```

3. **各子对话框的 Control Parent 属性必须为 True**
   ```rc
   IDD_DIALOG1 DIALOGEX 0, 0, 280, 160
   STYLE DS_SETFONT | WS_CHILD
   EXSTYLE WS_EX_CONTROLPARENT  // 这个很重要！
   ```

4. **切换选项卡后，必须调用 `SetFocus` 将焦点设回 TabControl**
   ```cpp
   void SwitchTab(HWND hDlg)
   {
       // ... 显示/隐藏子对话框的代码 ...
       SetFocus(GetDlgItem(hDlg, IDC_TAB1));  // 关键！
   }
   ```

---

## 注意事项：WM_CTLCOLORSTATIC

如果程序处理了 Static 控件的 `WM_CTLCOLORSTATIC` 消息，使用 `EnableThemeDialogTexture` 后需要注意：

```cpp
case WM_CTLCOLORSTATIC:
{
    // 设置文本颜色
    HDC hdcStatic = (HDC)wparam;
    SetTextColor(hdcStatic, RGB(255, 0, 0));

    // 启用主题纹理后，直接返回 0 即可使用渐变背景
    // 不需要返回 HBRUSH 背景刷
    return 0;
}
```

---

## 为什么这个函数这么好用

`EnableThemeDialogTexture` 做的事情其实很简单：它告诉系统"这个对话框应该使用 TabControl 的主题纹理"，而不是使用默认的对话框背景。

在 Windows XP 引入主题支持后，对话框和 TabControl 使用了不同的绘制逻辑。对话框默认有自己的背景绘制，而 TabControl 的子区域期望使用 TabControl 的背景纹理。`EnableThemeDialogTexture` 就是连接两者的桥梁。

这个函数之所以很多人不知道，是因为：
1. MSDN 文档不太显眼
2. 很多教程只讲基本用法，不讲这种细节
3. 这个问题只在用对话框作为 TabControl 内容时才会出现

---

## 相关资源

- [EnableThemeDialogTexture 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/uxtheme/nf-uxtheme-enablethemedialogtexture)
- [TabControl 控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/tab-controls)
- [原文参考：去除Win32 Tab Control控件每个选项卡上的深色对话框背景 - CSDN](https://blog.csdn.net/ZLK1214/article/details/140308793)

---

## 总结

`EnableThemeDialogTexture` 是一个简单但非常有用的函数，解决了 TabControl + 子对话框的经典深色背景问题。只需要在每个子对话框的 `WM_INITDIALOG` 中调用：

```cpp
EnableThemeDialogTexture(hdlg, ETDT_ENABLETAB);
```

就能让你的 TabControl 界面看起来专业、统一，符合 Windows 原生应用的标准外观。
