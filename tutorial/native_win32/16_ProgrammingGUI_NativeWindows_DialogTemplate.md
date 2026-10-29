# 通用GUI编程技术——Win32 原生编程实战（十五）——对话框模板深入

> 之前我们聊过模态对话框的基本使用，知道怎么用资源文件定义对话框，怎么处理 `WM_INITDIALOG` 消息，怎么从对话框获取数据。但如果你仔细看过 Visual Studio 生成的 .rc 文件，你会发现对话框定义用的是 `DIALOGEX` 而不是 `DIALOG`。这两个有什么区别？对话框模板里那些 `DS_` 开头的样式是做什么的？控件的样式和窗口的样式有什么不同？今天我们就把对话框模板彻底讲清楚。

---

## 为什么要专门聊对话框模板

说实话，在我刚开始学 Win32 的时候，对对话框模板这种东西基本是"能用就行"的态度。Visual Studio 的资源编辑器不是很好用吗？拖拖控件就能设计对话框，为什么要关心底层的 .rc 文件内容？

但这种态度很快就遇到了问题。当你需要做一些稍微高级一点的操作时，比如：
- 创建一个运行时动态生成的对话框
- 理解为什么某些对话框样式组合不工作
- 手写 .rc 文件来实现某些资源编辑器做不到的事情
- 调试对话框相关的问题

这时候如果你不懂对话框模板的底层机制，就会发现自己寸步难行。

另一个现实原因是：`DIALOG` 已经被标记为过时了。现代 Win32 开发应该使用 `DIALOGEX`。但很多老教程甚至 Visual Studio 的某些版本还在用 `DIALOG`。如果你不理解两者的区别，可能会在迁移代码时遇到各种奇怪的问题。

还有一个重要的话题是 DPI 适配。现代的高 DPI 显示器要求对话框能够正确缩放，而这需要在对话框模板中正确设置字体信息。如果你不理解对话框模板的字体处理机制，你的对话框在高 DPI 显示器上可能会变得很难看。

这篇文章会带着你从零开始，把对话框模板的方方面面彻底搞透。我们不只是学会怎么写，更重要的是理解"为什么要这么写"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows 95+ 都支持 DIALOGEX）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的对话框、怎么处理 `WM_INITDIALOG` 和 `WM_COMMAND` 消息。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——DIALOG vs DIALOGEX 的区别

Win32 有两种对话框资源类型：`DIALOG` 和 `DIALOGEX`。`DIALOG` 是老式的对话框模板，已经基本过时；`DIALOGEX` 是扩展版本，提供了更多功能。

### DIALOG 的限制

`DIALOG` 资源类型有以下限制：
1. 不支持控件的扩展样式（`WS_EX_` 开头的样式）
2. 不支持特定控件的高级特性（如编辑框的 `ES_NUMBER` 等需要用特殊方式指定）
3. 字体信息处理方式较简单
4. 对某些新控件的支持不完善

### DIALOGEX 的优势

`DIALOGEX` 资源类型提供了以下额外功能：
1. 支持控件的扩展窗口样式
2. 支持更完整的控件样式标志
3. 更好的字体处理，包括 `DS_SHELLFONT` 样式
4. 支持帮助 ID
5. 更好的 DPI 适配支持

⚠️ 注意

千万别在新代码中使用 `DIALOG`。微软已经明确推荐使用 `DIALOGEX`，而且 `DIALOG` 的一些行为在现代 Windows 版本中可能不完全符合预期。如果你在维护老代码，应该考虑把 `DIALOG` 迁移到 `DIALOGEX`。

### 语法对比

```rc
// 老式的 DIALOG（不推荐）
MYDIALOG DIALOG 10, 10, 200, 100
STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "我的对话框"
FONT 9, "MS Shell Dlg"
BEGIN
    PUSHBUTTON      "确定", IDOK, 10, 80, 50, 14
    PUSHBUTTON      "取消", IDCANCEL, 70, 80, 50, 14
END

// 推荐的 DIALOGEX
MYDIALOG DIALOGEX 10, 10, 200, 100
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "我的对话框"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    PUSHBUTTON      "确定", IDOK, 10, 80, 50, 14, WS_TABSTOP
    PUSHBUTTON      "取消", IDCANCEL, 70, 80, 50, 14, WS_TABSTOP
END
```

---

## 第二步——对话框样式详解

对话框模板中的样式可以分为三类：对话框专用样式（`DS_`）、窗口样式（`WS_`）和控件样式。

### DS_ 系列对话框样式

这些样式是对话框特有的，用于控制对话框的行为和外观：

| 样式 | 说明 |
|------|------|
| `DS_MODALFRAME` | 创建模态对话框的边框样式，通常和 `WS_POPUP` 一起使用 |
| `DS_SHELLFONT` | 使用系统 shell 字体，推荐用于现代对话框 |
| `DS_SETFONT` | 对话框使用指定字体 |
| `DS_FIXEDSYS` | 使用 `SYSTEM_FIXED` 字体（已过时，用 `DS_SHELLFONT` 替代） |
| `DS_ABSALIGN` | 对话框坐标是屏幕坐标而非父窗口坐标 |
| `DS_CENTER` | 对话框在屏幕上居中 |
| `DS_CENTERMOUSE` | 对话框在鼠标位置居中 |
| `DS_CONTEXTHELP` | 在标题栏添加帮助按钮 |
| `DS_CONTROL` | 对话框可以作为另一个对话框的子控件 |
| `DS_LOCALEDIT` | 编辑控件使用本地内存 |
| `DS_NOIDLEMSG` | 防止对话框向父窗口发送 `WM_ENTERIDLE` 消息 |
| `DS_3DLOOK` | 使用 3D 边框（在现代 Windows 中效果不明显） |
| `DS_NOMODALFRAME` | 创建无模态框架的对话框（不常用） |

最常用的样式组合：

```rc
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
```

这个组合创建一个标准的模态对话框：
- `DS_MODALFRAME`：模态框架
- `DS_SHELLFONT`：使用系统字体（DPI 感知）
- `WS_POPUP`：弹出窗口
- `WS_CAPTION`：有标题栏
- `WS_SYSMENU`：有系统菜单（关闭按钮）

### 窗口样式（WS_）

对话框也是窗口，所以可以使用标准的窗口样式：

```rc
STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME
```

`WS_THICKFRAME` 让对话框可以调整大小。

---

## 第三步——字体和 DPI 处理

字体处理是现代对话框设计中的一个重要话题。如果你的对话框不能正确适配不同的 DPI 设置，在高分辨率显示器上会变得很小或者模糊。

### DPI 问题

假设你在 96 DPI（100% 缩放）的显示器上设计了一个 200×100 的对话框：
- 在 96 DPI（100%）下：200×100 像素
- 在 144 DPI（150%）下：系统会把对话框放大到 300×150 像素
- 字体也会相应放大

但如果你的对话框没有正确设置字体信息，系统可能无法正确缩放。

### 设置对话框字体

在 `DIALOGEX` 中设置字体：

```rc
MYDIALOG DIALOGEX 0, 0, 200, 100
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION
CAPTION "我的对话框"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    // ...
END
```

`FONT` 语句的参数：
1. `9`：字体大小（单位是 1/8 点）
2. `"MS Shell Dlg"`：字体名称
3. `400`：字体粗细（400 是正常，700 是粗体）
4. `0`： italic 标志（0 = 非斜体）
5. `0x1`：字符集（0x1 = DEFAULT_CHARSET）

### DS_SHELLFONT 样式

`DS_SHELLFONT` 是现代对话框应该使用的字体样式。它会：
- 使用系统配置的 Shell 字体（默认是 Segoe UI 或 MS Shell Dlg）
- 正确响应 DPI 缩放
- 在不同版本的 Windows 上保持一致的外观

⚠️ 注意

千万别忘记设置 `DS_SHELLFONT` 样式和正确的 `FONT` 语句。如果你使用默认字体设置，对话框在不同 DPI 设置下的显示效果可能会非常糟糕。

### 手写 DPI 感知的对话框模板

```rc
// 高 DPI 感知的对话框模板
MYDIALOG DIALOGEX 0, 0, 260, 140
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "高 DPI 对话框示例"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    LTEXT           "用户名：", IDC_STATIC, 10, 15, 50, 10
    EDITTEXT        IDC_EDIT_USERNAME, 70, 13, 180, 14, ES_AUTOHSCROLL | WS_TABSTOP

    LTEXT           "密码：", IDC_STATIC, 10, 40, 50, 10
    EDITTEXT        IDC_EDIT_PASSWORD, 70, 38, 180, 14, ES_AUTOHSCROLL | ES_PASSWORD | WS_TABSTOP

    DEFPUSHBUTTON   "确定", IDOK, 100, 70, 50, 14, WS_TABSTOP
    PUSHBUTTON      "取消", IDCANCEL, 160, 70, 50, 14, WS_TABSTOP
END
```

---

## 第四步——控件定义语法

在对话框模板中定义控件需要了解控件的类名、样式和坐标系统。

### 预定义控件类

对话框模板中可以使用以下预定义的控件类：

| 类名 | 宏 | 用途 |
|------|-----|------|
| `"BUTTON"` | `BUTTON` | 按钮、复选框、单选按钮、组框 |
| `"EDIT"` | `EDIT` 或 `EDITTEXT` | 编辑框 |
| `"STATIC"` | `LTEXT`、`CTEXT`、`RTEXT`、`GROUPBOX` | 静态文本、矩形、图标 |
| `"LISTBOX"` | `LISTBOX` | 列表框 |
| `"COMBOBOX"` | `COMBOBOX` | 组合框 |
| `"SCROLLBAR"` | `SCROLLBAR` | 滚动条 |

### 控件定义的通用语法

```rc
control-class text, id, x, y, width, height [, style [, exstyle [, help-id]]]
```

或者使用简化的宏：

```rc
EDITTEXT    id, x, y, width, height [, style]
LTEXT       text, id, x, y, width, height [, style]
PUSHBUTTON  text, id, x, y, width, height [, style]
```

### 常用控件的样式

#### 按钮样式

```rc
// 普通按钮
PUSHBUTTON      "确定", IDOK, 10, 10, 50, 14

// 默认按钮（Enter 键触发）
DEFPUSHBUTTON   "确定", IDOK, 10, 10, 50, 14

// 复选框
AUTOCHECKBOX    "记住密码", IDC_CHECK_REMEMBER, 10, 40, 80, 10

// 自动三态复选框
AUTO3STATE      "灰选框", IDC_CHECK_3STATE, 10, 55, 80, 10

// 单选按钮
AUTORADIOBUTTON "选项1", IDC_RADIO_OPT1, 10, 70, 60, 10, WS_GROUP | WS_TABSTOP
AUTORADIOBUTTON "选项2", IDC_RADIO_OPT2, 10, 85, 60, 10

// 组框
GROUPBOX        "设置", IDC_STATIC, 5, 5, 240, 100
```

按钮样式标志：
- `BS_PUSHBUTTON`：普通按钮
- `BS_DEFPUSHBUTTON`：默认按钮
- `BS_CHECKBOX`：复选框
- `BS_AUTOCHECKBOX`：自动切换状态的复选框
- `BS_RADIOBUTTON`：单选按钮
- `BS_AUTORADIOBUTTON`：自动管理的单选按钮
- `BS_GROUPBOX`：组框
- `BS_LEFT`、`BS_RIGHT`、`BS_CENTER`：文本对齐

#### 编辑框样式

```rc
// 单行编辑框
EDITTEXT    IDC_EDIT_NAME, 10, 10, 200, 14, ES_AUTOHSCROLL

// 密码编辑框
EDITTEXT    IDC_EDIT_PASSWORD, 10, 30, 200, 14, ES_PASSWORD | ES_AUTOHSCROLL

// 只读编辑框
EDITTEXT    IDC_EDIT_READONLY, 10, 50, 200, 14, ES_READONLY | ES_AUTOHSCROLL

// 多行编辑框
EDITTEXT    IDC_EDIT_MULTILINE, 10, 70, 200, 60, ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL

// 数字编辑框（只允许输入数字）
EDITTEXT    IDC_EDIT_NUMBER, 10, 140, 100, 14, ES_NUMBER | ES_AUTOHSCROLL
```

编辑框样式标志：
- `ES_AUTOHSCROLL`：自动水平滚动
- `ES_AUTOVSCROLL`：自动垂直滚动
- `ES_MULTILINE`：多行编辑
- `ES_READONLY`：只读
- `ES_PASSWORD`：密码模式
- `ES_NUMBER`：只允许数字
- `ES_LEFT`、`ES_CENTER`、`ES_RIGHT`：文本对齐

#### 静态控件样式

```rc
// 左对齐文本
LTEXT       "标签：", IDC_STATIC, 10, 10, 50, 10

// 居中文本
CTEXT       "标题", IDC_STATIC, 100, 10, 100, 10

// 右对齐文本
RTEXT       "右对齐", IDC_STATIC, 200, 10, 50, 10

// 矩形框架
CONTROL     "", IDC_STATIC, "STATIC", SS_BLACKFRAME, 10, 30, 200, 50

// 图标
ICON        IDI_ICON1, IDC_STATIC, 10, 90, 32, 32

// 位图
CONTROL     IDB_BITMAP1, IDC_STATIC, "STATIC", SS_BITMAP, 50, 90, 64, 32
```

静态控件样式标志：
- `SS_LEFT`、`SS_CENTER`、`SS_RIGHT`：文本对齐
- `SS_NOPREFIX`：不处理 `&` 快捷键
- `SS_BITMAP`：显示位图
- `SS_ICON`：显示图标
- `SS_BLACKRECT`、`SS_GRAYRECT`、`SS_WHITERECT`：填充矩形
- `SS_BLACKFRAME`、`SS_GRAYFRAME`、`SS_WHITEFRAME`：边框

---

## 第五步——控件坐标和布局

对话框中的坐标使用"对话框单位"（Dialog Units），这是一种与设备无关的单位系统。

### 对话框单位

对话框单位基于当前对话框字体的大小：
- 水平单位：字体平均字符宽度的 1/4
- 垂直单位：字体高度的 1/8

这意昧着：
- 如果字体变大，对话框会相应缩放
- 在不同 DPI 下，对话框会保持相对比例

⚠️ 注意

千万别使用像素单位来手动计算对话框位置。对话框单位的设计目的就是为了自动适配不同的字体大小和 DPI 设置。如果你需要精确控制位置，可以使用 `MapDialogRect` 函数在运行时转换。

### MapDialogRect 函数

```cpp
// 将对话框单位转换为像素
void ConvertDialogUnitsToPixels(HWND hDlg, int dlgX, int dlgY, int* pixX, int* pixY)
{
    RECT rect = { 0, 0, dlgX, dlgY };
    MapDialogRect(hDlg, &rect);
    *pixX = rect.right;
    *pixY = rect.bottom;
}

// 使用示例
void PositionControlDynamically(HWND hDlg)
{
    int dlgX = 100;  // 对话框单位
    int dlgY = 50;

    int pixX, pixY;
    ConvertDialogUnitsToPixels(hDlg, dlgX, dlgY, &pixX, &pixY);

    // 现在可以用像素位置了
    SetWindowPos(GetDlgItem(hDlg, IDC_SOME_CONTROL),
        NULL, pixX, pixY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}
```

### 布局技巧

由于对话框是绝对定位布局，没有现代 UI 框架的自动布局功能，你需要手动计算控件位置。一些实用的技巧：

```rc
// 使用组框组织相关控件
GROUPBOX        "登录信息", IDC_STATIC, 5, 5, 240, 60
    LTEXT           "用户名：", IDC_STATIC, 15, 20, 50, 10
    EDITTEXT        IDC_EDIT_USERNAME, 75, 18, 160, 14, ES_AUTOHSCROLL

    LTEXT           "密码：", IDC_STATIC, 15, 40, 50, 10
    EDITTEXT        IDC_EDIT_PASSWORD, 75, 38, 160, 14, ES_PASSWORD | ES_AUTOHSCROLL

GROUPBOX        "选项", IDC_STATIC, 5, 70, 240, 45
    AUTOCHECKBOX    "记住密码", IDC_CHECK_REMEMBER, 15, 85, 80, 10
    AUTOCHECKBOX    "自动登录", IDC_CHECK_AUTOLOGIN, 15, 100, 80, 10

// 按钮通常放在底部右对齐
DEFPUSHBUTTON   "确定", IDOK, 130, 120, 50, 14, WS_TABSTOP
PUSHBUTTON      "取消", IDCANCEL, 185, 120, 50, 14, WS_TABSTOP
```

---

## 第六步——完整对话框模板示例

下面是一个完整的、功能丰富的对话框模板示例：

```rc
#include <windows.h>
#include "resource.h"

// 主对话框
IDD_LOGIN_DIALOG DIALOGEX 0, 0, 280, 180
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "用户登录"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    // 登录信息组
    GROUPBOX        "登录信息", IDC_STATIC, 10, 10, 260, 70

    LTEXT           "用户名(&U)：", IDC_STATIC, 25, 30, 50, 10
    EDITTEXT        IDC_EDIT_USERNAME, 85, 28, 170, 14, ES_AUTOHSCROLL | WS_TABSTOP

    LTEXT           "密码(&P)：", IDC_STATIC, 25, 50, 50, 10
    EDITTEXT        IDC_EDIT_PASSWORD, 85, 48, 170, 14, ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP

    // 选项组
    GROUPBOX        "登录选项", IDC_STATIC, 10, 85, 260, 50

    AUTOCHECKBOX    "记住密码(&R)", IDC_CHECK_REMEMBER, 25, 100, 100, 10, WS_TABSTOP
    AUTOCHECKBOX    "自动登录(&A)", IDC_CHECK_AUTOLOGIN, 25, 118, 100, 10, WS_TABSTOP

    // 按钮
    DEFPUSHBUTTON   "登录(&L)", IDOK, 100, 145, 50, 14, WS_TABSTOP
    PUSHBUTTON      "取消(&C)", IDCANCEL, 160, 145, 50, 14, WS_TABSTOP
    PUSHBUTTON      "帮助(&H)", IDHELP, 220, 145, 50, 14, WS_TABSTOP
END

// 设置对话框
IDD_SETTINGS_DIALOG DIALOGEX 0, 0, 300, 220
STYLE DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "设置"
FONT 9, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    GROUPBOX        "外观设置", IDC_STATIC, 10, 10, 280, 70

    LTEXT           "主题：", IDC_STATIC, 25, 30, 50, 10
    COMBOBOX        IDC_COMBO_THEME, 85, 28, 195, 100, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP

    AUTOCHECKBOX    "使用大图标", IDC_CHECK_LARGE_ICONS, 25, 50, 100, 10, WS_TABSTOP
    AUTOCHECKBOX    "启用动画", IDC_CHECK_ANIMATIONS, 140, 50, 100, 10, WS_TABSTOP

    GROUPBOX        "高级选项", IDC_STATIC, 10, 85, 280, 85

    AUTOCHECKBOX    "启用调试日志", IDC_CHECK_DEBUG_LOG, 25, 100, 140, 10, WS_TABSTOP
    AUTOCHECKBOX    "启动时最小化到托盘", IDC_CHECK_MINIMIZE, 25, 118, 180, 10, WS_TABSTOP
    AUTOCHECKBOX    "关闭时最小化而不是退出", IDC_CHECK_MINIMIZE_ON_CLOSE, 25, 136, 220, 10, WS_TABSTOP
    AUTOCHECKBOX    "允许程序运行多个实例", IDC_CHECK_MULTIPLE_INSTANCES, 25, 154, 220, 10, WS_TABSTOP

    // 按钮
    DEFPUSHBUTTON   "确定", IDOK, 130, 180, 50, 14, WS_TABSTOP
    PUSHBUTTON      "取消", IDCANCEL, 190, 180, 50, 14, WS_TABSTOP
    PUSHBUTTON      "应用", IDAPPLY, 250, 180, 50, 14, WS_TABSTOP
END
```

### resource.h 定义

```cpp
// 对话框 ID
#define IDD_LOGIN_DIALOG        100
#define IDD_SETTINGS_DIALOG     101

// 控件 ID - 登录对话框
#define IDC_EDIT_USERNAME       200
#define IDC_EDIT_PASSWORD       201
#define IDC_CHECK_REMEMBER      202
#define IDC_CHECK_AUTOLOGIN     203

// 控件 ID - 设置对话框
#define IDC_COMBO_THEME         210
#define IDC_CHECK_LARGE_ICONS   211
#define IDC_CHECK_ANIMATIONS    212
#define IDC_CHECK_DEBUG_LOG     213
#define IDC_CHECK_MINIMIZE      214
#define IDC_CHECK_MINIMIZE_ON_CLOSE  215
#define IDC_CHECK_MULTIPLE_INSTANCES 216

// 静态控件占位符
#define IDC_STATIC              (-1)
```

---

## 第七步——手写 .rc 模板 vs 资源编辑器

Visual Studio 的资源编辑器虽然方便，但了解如何手写 .rc 文件也有很多好处。

### 资源编辑器的优势
- 可视化设计，所见即所得
- 自动生成代码，减少出错
- 支持拖放操作，设计效率高
- 内置测试功能

### 手写 .rc 的优势
- 完全控制，可以实现编辑器不支持的功能
- 便于版本控制和代码审查
- 可以用脚本自动生成
- 更容易理解和调试

### 实用建议

对于大多数情况，建议使用资源编辑器进行初步设计，然后根据需要手动调整 .rc 文件。特别适合手写的场景：
- 需要大量相似的对话框（可以用模板生成）
- 需要运行时动态创建对话框
- 调试对话框相关的问题

### 运行时创建对话框模板

有时候你需要在运行时动态创建对话框，这可以通过手动构建对话框模板内存结构来实现：

```cpp
// 创建简单的对话框模板
LPDLGTEMPLATE CreateDialogTemplateInMemory()
{
    // 计算所需大小
    DWORD dwSize = sizeof(DLGTEMPLATE) + 2 * sizeof(WORD);  // 菜单和类名称（0）
    dwSize += 50 * sizeof(WCHAR);  // 标题
    dwSize += sizeof(WORD);  // 字体大小
    dwSize += 20 * sizeof(WCHAR);  // 字体名称

    // 分配内存
    LPDLGTEMPLATE pTemplate = (LPDLGTEMPLATE)LocalAlloc(LPTR, dwSize);
    if (pTemplate == NULL)
        return NULL;

    // 填充对话框模板
    pTemplate->style = DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pTemplate->dwExtendedStyle = 0;
    pTemplate->cdit = 0;  // 控件数量
    pTemplate->x = 0;
    pTemplate->y = 0;
    pTemplate->cx = 200;
    pTemplate->cy = 100;

    // 填充菜单、类、标题（简化示例）
    LPWORD pw = (LPWORD)(pTemplate + 1);
    *pw++ = 0;  // 无菜单
    *pw++ = 0;  // 预定义窗口类

    // 标题
    wcscpy((LPWSTR)pw, L"动态对话框");
    pw += wcslen((LPWSTR)pw) + 1;

    return pTemplate;
}

// 使用动态模板创建对话框
void ShowDynamicDialog(HWND hwndParent)
{
    LPDLGTEMPLATE pTemplate = CreateDialogTemplateInMemory();
    if (pTemplate)
    {
        DialogBoxIndirect(GetModuleHandle(NULL), pTemplate, hwndParent, DlgProc);
        LocalFree(pTemplate);
    }
}
```

---

## 第八步——调试对话框模板

在开发对话框时，你可能会遇到各种问题。这里分享一些调试技巧。

### 常见问题

1. **对话框不显示**：检查样式是否包含 `WS_POPUP` 或 `WS_CHILD`
2. **控件不显示**：检查控件坐标是否在对话框范围内
3. **Tab 键不工作**：检查控件是否设置了 `WS_TABSTOP` 样式
4. **Enter/Esc 键不工作**：确保有 `IDOK` 和 `IDCANCEL` 按钮
5. **字体显示不正确**：检查是否设置了 `DS_SHELLFONT` 和正确的字体信息

### 调试技巧

```cpp
// 在 WM_INITDIALOG 中输出对话框信息
case WM_INITDIALOG:
{
    // 输出对话框和控件的矩形信息
    RECT rcDlg, rcCtrl;
    GetWindowRect(hDlg, &rcDlg);
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_USERNAME);
    GetWindowRect(hEdit, &rcCtrl);

    wchar_t szDebug[256];
    swprintf_s(szDebug, 256, L"对话框: %d×%d, 控件: %d×%d",
        rcDlg.right - rcDlg.left, rcDlg.bottom - rcDlg.top,
        rcCtrl.right - rcCtrl.left, rcCtrl.bottom - rcCtrl.top);
    OutputDebugString(szDebug);

    return TRUE;
}
```

---

## 后续可以做什么

到这里，对话框模板的深入知识就讲完了。你现在应该能够：
- 理解 `DIALOG` 和 `DIALOGEX` 的区别
- 正确设置对话框样式和字体
- 在模板中定义各种控件
- 理解对话框单位和坐标系统
- 手写或修改 .rc 文件

但对话框世界还有更多内容值得探索：

1. **属性表**：使用 `PropertySheet` API 创建多页设置对话框
2. **向导对话框**：创建步骤化的向导界面
3. **无模式对话框**：创建不阻塞父窗口的对话框
4. **自定义绘制控件**：在对话框中实现 Owner-Draw 控件
5. **对话框消息管理器**：深入理解对话框内部的实现机制

建议你先做一些练习巩固一下：
1. 手写一个完整的登录对话框模板
2. 实现一个带有多组单选按钮的设置对话框
3. 尝试在运行时动态创建一个简单的对话框
4. 测试你的对话框在不同 DPI 设置下的显示效果

下一步，我们可以探讨 Win32 的其他高级话题，如 GDI 绘图、自定义控件或者更复杂的资源管理。

---

## 相关资源

- [DIALOGEX 资源 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/dialogex-resource)
- [DIALOG 资源 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/dialog-resource)
- [对话框样式 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/dlgbox/dialog-box-styles)
- [按钮样式 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/button-styles)
- [编辑控件样式 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/edit-control-styles)
- [静态控件样式 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/static-control-styles)
- [MapDialogRect 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-mapdialogrect)
- [WS_GROUP and WS_TABSTOP Flags in Controls - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/com/ws-group-and-ws-tabstop-flags-in-controls)
- [The Old New Thing - The evolution of dialog templates](https://devblogs.microsoft.com/oldnewthing/20040622-00/?p=38773)
