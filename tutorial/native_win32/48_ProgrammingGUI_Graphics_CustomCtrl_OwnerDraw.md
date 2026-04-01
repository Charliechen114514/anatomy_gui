# 通用GUI编程技术——图形渲染实战（四十八）——Owner-Draw控件：让标准控件焕然一新

> 上一篇我们聊了 D3D12 与 D3D11 的互操作——通过 D3D11On12 桥接层在同一个渲染管线中混用两种 API。到此为止，我们的 GPU 加速渲染之旅已经覆盖了 GDI、GDI+、Direct2D、DirectWrite、D3D11、HLSL 和 D3D12，从软件光栅化一路走到了现代图形 API 的核心机制。
>
> 现在我们把视线拉回到 Win32 控件的世界。Windows 提供了一套丰富的标准控件——Button、ListBox、ComboBox、Menu。它们开箱即用，外观由系统主题决定。但有时候你需要更多——比如一个 ListBox 的列表项显示图标和描述文字、一个 ComboBox 的下拉项显示颜色块、一个按钮用自定义渐变替代系统默认的平面填充。这就是 Owner-Draw 的用武之地。

## 前言：Owner-Draw 是什么

Owner-Draw 的核心理念很简单：控件的行为（消息处理、选中、焦点管理）仍然由系统负责，但外观绘制由你——控件的"所有者"（Owner，通常是父窗口）来接管。Windows 在需要绘制控件的时候，不再用默认的系统渲染逻辑，而是向父窗口发送一条 `WM_DRAWITEM` 消息，把绘制所需的全部信息打包在 `DRAWITEMSTRUCT` 结构体中交给你。你在消息处理函数中用 GDI 或其他图形 API 自行绘制控件的外观。

这样做的好处是显而易见的：你不需要从头实现一个控件的完整行为（鼠标交互、键盘导航、无障碍接口），只需要关注"怎么画"这个层面。控件的数据管理、选中状态、焦点切换这些复杂逻辑仍然由系统维护，你的工作量大幅减少。

## 环境说明

- **操作系统**: Windows 10/11
- **编译器**: MSVC (Visual Studio 2022)
- **图形API**: GDI（本章使用 GDI 绘制，与系统控件绘制模型一致）
- **前置知识**: 文章 4-8（标准控件使用），文章 18-21（GDI 基础）

## 哪些控件支持 Owner-Draw

并不是所有控件都支持 Owner-Draw。最常见的支持 Owner-Draw 的控件包括：

- **Button**：设置 `BS_OWNERDRAW` 样式后，按钮的外观完全由你绘制。
- **ListBox**：设置 `LBS_OWNERDRAWFIXED`（固定高度项）或 `LBS_OWNERDRAWVARIABLE`（可变高度项）样式后，每个列表项的外观由你绘制。
- **ComboBox**：设置 `CBS_OWNERDRAWFIXED` 或 `CBS_OWNERDRAWVARIABLE` 样式，下拉列表项的外观由你绘制。
- **Menu**：菜单项没有单独的样式标志，但可以通过 `SetMenuItemInfo` 设置 `MIIM_FTYPE` 中的 `MFT_OWNERDRAW` 标志来启用。
- **TabControl**：设置 `TCS_OWNERDRAWFIXED` 样式，标签页的外观由你绘制。

其中 ListBox 和 ComboBox 是最常用的 Owner-Draw 场景——系统默认的 ListBox 只能显示纯文本，但通过 Owner-Draw，你可以让每个列表项显示图标+文字+描述，甚至实现多列布局。

## DRAWITEMSTRUCT 详解

`DRAWITEMSTRUCT` 是 Owner-Draw 的核心数据结构。每当 Windows 需要绘制一个 Owner-Draw 控件的某个元素时，它会填充这个结构体并发送给父窗口。根据 [Microsoft Learn - DRAWITEMSTRUCT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-drawitemstruct) 的文档，这个结构体的字段如下：

```cpp
typedef struct tagDRAWITEMSTRUCT {
    UINT      CtlType;      // 控件类型（ODT_LISTBOX, ODT_COMBOBOX 等）
    UINT      CtlID;        // 控件 ID
    UINT      itemID;       // 当前绘制的项索引（菜单为 0）
    UINT      itemAction;   // 绘制动作（ODA_DRAWENTIRE, ODA_SELECT, ODA_FOCUS）
    UINT      itemState;    // 项状态（ODS_SELECTED, ODS_FOCUS, ODS_DISABLED 等）
    HWND      hwndItem;     // 控件窗口句柄
    HDC       hDC;          // 用于绘制的设备上下文
    RECT      rcItem;       // 绘制区域的矩形
    ULONG_PTR itemData;     // 项的关联数据（来自 ListBox/ComboBox 的 itemData）
} DRAWITEMSTRUCT;
```

我们来逐个理解关键字段。

`CtlType` 告诉你这个消息来自哪种类型的控件。常见值有 `ODT_LISTBOX`（列表框）、`ODT_COMBOBOX`（组合框）、`ODT_BUTTON`（按钮）和 `ODT_MENU`（菜单）。如果你有多个 Owner-Draw 控件，可以通过这个字段区分它们。

`itemID` 是当前需要绘制的项的索引。对于 ListBox 来说，这是列表项的序号（从 0 开始）。如果是空列表或者新项被添加时的特殊绘制，`itemID` 可能为 -1。

`itemAction` 描述了需要执行什么绘制动作。`ODA_DRAWENTIRE` 表示需要完整绘制整个项，`ODA_SELECT` 表示项的选中状态发生了变化需要更新显示，`ODA_FOCUS` 表示项的焦点状态发生了变化。在实际处理中，我们通常不管 `itemAction` 是什么，都完整绘制——因为判断"哪些部分需要更新"比"全部重绘"更复杂，而且现代 GDI 的性能完全扛得住。

`itemState` 是最重要的字段之一。它用位标志描述了项的当前状态：

- `ODS_SELECTED`：项被选中（高亮显示）
- `ODS_FOCUS`：项有输入焦点（通常显示虚线边框）
- `ODS_DISABLED`：项被禁用
- `ODS_CHECKED`：菜单项被勾选
- `ODS_COMBOBOXEDIT`：ComboBox 的编辑框中的项
- `ODS_DEFAULT`：默认按钮

你可以通过位与操作检查这些状态：`if (dis->itemState & ODS_SELECTED)` 表示项被选中。

`hDC` 是 Windows 为你准备好的设备上下文。你所有的 GDI 绘制操作都要用这个 DC——不要创建新的 DC，也不要用 `GetDC` 获取别的 DC。`rcItem` 是你需要绘制的矩形区域——超出这个区域的绘制会被裁剪，不会影响其他列表项。

`itemData` 是你在添加列表项时通过 `SetItemData` 关联的自定义数据。这是一个 `ULONG_PTR`（指针大小的整数），通常用来存储一个指向自定义数据结构的指针。

## 第一步——Owner-Draw ListBox 完整示例

现在我们来实现一个完整的 Owner-Draw ListBox 示例。每个列表项显示一个彩色方块和一段描述文字，选中项有高亮背景。

### 定义数据结构

```cpp
// 每个列表项的自定义数据
struct ListItemData
{
    COLORREF color;      // 颜色方块的颜色
    wchar_t  name[64];   // 名称
    wchar_t  desc[128];  // 描述
};

// 预设数据
ListItemData items[] = {
    { RGB(220, 50, 50),   L"红色",   L"热情与活力" },
    { RGB(50, 150, 50),   L"绿色",   L"自然与和平" },
    { RGB(50, 80, 200),   L"蓝色",   L"冷静与信任" },
    { RGB(220, 180, 50),  L"黄色",   L"温暖与乐观" },
    { RGB(150, 50, 180),  L"紫色",   L"神秘与优雅" },
    { RGB(50, 180, 180),  L"青色",   L"清新与创意" },
    { RGB(220, 130, 50),  L"橙色",   L"活力与热情" },
    { RGB(128, 128, 128), L"灰色",   L"沉稳与内敛" },
};
```

### 创建 ListBox 并添加数据

```cpp
#define IDC_COLOR_LIST 1001

HWND CreateColorListBox(HWND hParent)
{
    // 创建 Owner-Draw ListBox
    // 关键样式：LBS_OWNERDRAWFIXED
    HWND hList = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        LBS_OWNERDRAWFIXED |    // Owner-Draw 固定高度
        LBS_NOTIFY |            // 发送通知消息
        WS_VSCROLL,             // 垂直滚动条
        20, 20, 300, 400,
        hParent,
        (HMENU)IDC_COLOR_LIST,
        (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE),
        nullptr
    );

    // 添加数据项
    for (int i = 0; i < _countof(items); i++)
    {
        // 添加字符串项（用于显示）
        int idx = (int)SendMessage(hList, LB_ADDSTRING, 0,
            (LPARAM)items[i].name);

        // 关联自定义数据
        SendMessage(hList, LB_SETITEMDATA, idx, (LPARAM)&items[i]);
    }

    return hList;
}
```

`LBS_OWNERDRAWFIXED` 是启用 Owner-Draw 的关键样式。`FIXED` 意味着所有列表项的高度相同——你需要在父窗口的 `WM_MEASUREITEM` 消息中指定这个高度。`LBS_OWNERDRAWVARIABLE` 则允许每个列表项有不同的高度，适用于更复杂的场景。

### 处理 WM_MEASUREITEM

⚠️ 这是 Owner-Draw 初学者最容易踩的坑——忘记处理 `WM_MEASUREITEM`。如果你不处理这个消息，系统会使用默认的列表项高度（通常是一行文字的高度，大约 16 像素）。对于我们的场景，每个列表项需要显示颜色方块和两行文字，需要更大的高度。

```cpp
case WM_MEASUREITEM:
{
    MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;

    if (mis->CtlType == ODT_LISTBOX && mis->CtlID == IDC_COLOR_LIST)
    {
        mis->itemHeight = 50;  // 每个列表项高度 50 像素
    }
    return TRUE;
}
```

`WM_MEASUREITEM` 在控件创建时发送一次。对于 `LBS_OWNERDRAWFIXED`，`itemHeight` 对所有列表项统一生效。如果你用了 `LBS_OWNERDRAWVARIABLE`，系统会为每个列表项分别发送 `WM_MEASUREITEM`，你可以根据 `itemID` 返回不同的高度。

### 处理 WM_DRAWITEM

核心绘制逻辑：

```cpp
case WM_DRAWITEM:
{
    DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;

    if (dis->CtlID != IDC_COLOR_LIST)
        break;

    // 获取列表项的自定义数据
    ListItemData* data = (ListItemData*)dis->itemData;

    if (!data)
    {
        // 空列表或无效项
        return TRUE;
    }

    // 第一步：绘制背景
    if (dis->itemState & ODS_SELECTED)
    {
        // 选中项：用系统高亮色填充
        FillRect(dis->hDC, &dis->rcItem,
            GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    else
    {
        // 非选中项：用系统窗口背景色填充
        FillRect(dis->hDC, &dis->rcItem,
            GetSysColorBrush(COLOR_WINDOW));
    }

    // 第二步：绘制颜色方块
    RECT colorRect = dis->rcItem;
    colorRect.left += 8;
    colorRect.top += 8;
    colorRect.right = colorRect.left + 34;
    colorRect.bottom = colorRect.top + 34;

    HBRUSH hColorBrush = CreateSolidBrush(data->color);
    FillRect(dis->hDC, &colorRect, hColorBrush);
    DeleteObject(hColorBrush);

    // 绘制颜色方块的边框
    FrameRect(dis->hDC, &colorRect,
        GetSysColorBrush(COLOR_WINDOWTEXT));

    // 第三步：绘制文字
    // 文字颜色根据选中状态切换
    COLORREF textColor = (dis->itemState & ODS_SELECTED)
        ? GetSysColor(COLOR_HIGHLIGHTTEXT)
        : GetSysColor(COLOR_WINDOWTEXT);

    SetTextColor(dis->hDC, textColor);
    SetBkMode(dis->hDC, TRANSPARENT);

    // 名称（较大字体）
    HFONT hNameFont = CreateFontW(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hNameFont);

    RECT textRect = dis->rcItem;
    textRect.left = colorRect.right + 12;
    textRect.top += 6;

    DrawText(dis->hDC, data->name, -1, &textRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // 描述文字（较小字体）
    HFONT hDescFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SelectObject(dis->hDC, hDescFont);

    textRect.top += 22;
    SetTextColor(dis->hDC,
        (dis->itemState & ODS_SELECTED)
            ? RGB(200, 220, 255)
            : RGB(100, 100, 100));

    DrawText(dis->hDC, data->desc, -1, &textRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // 清理字体
    SelectObject(dis->hDC, hOldFont);
    DeleteObject(hNameFont);
    DeleteObject(hDescFont);

    // 第四步：焦点指示器（虚线边框）
    if (dis->itemState & ODS_FOCUS)
    {
        DrawFocusRect(dis->hDC, &dis->rcItem);
    }

    return TRUE;
}
```

我们来逐段理解这段绘制代码。

**背景绘制**是第一步——你必须先清除列表项的背景，否则会看到上一帧的残留内容。对于选中的项，用 `COLOR_HIGHLIGHT` 系统颜色（通常是深蓝色）填充；对于非选中的项，用 `COLOR_WINDOW` 系统颜色（通常是白色）填充。使用系统颜色而不是硬编码 RGB 值的好处是：你的控件会自动适应用户选择的主题和暗色模式。

**颜色方块**是一个简单的 `FillRect` + `FrameRect` 组合——先用 `CreateSolidBrush` 创建一个指定颜色的画刷填充矩形，然后用系统文字颜色画一个边框。记得 `DeleteObject` 释放画刷——GDI 对象泄漏是 Windows GUI 编程中非常常见的问题。

**文字绘制**分两行：名称用较大加粗的字体，描述用较小的灰色字体。`SetBkMode(TRANSPARENT)` 确保文字背景不会覆盖之前绘制的颜色方块。文字颜色根据选中状态切换——选中时用白色文字，非选中时用深色文字。

**焦点指示器**是 Windows 无障碍的标准要求。`DrawFocusRect` 会画一个虚线边框表示当前有焦点的列表项。这个函数使用 XOR 模式绘制——调用两次就能擦除之前的虚线框。它是 Windows 标准控件惯用的焦点提示方式。

⚠️ 注意一个常见的遗漏：如果你只检查了 `ODS_SELECTED` 但忘了画焦点指示器（`ODS_FOCUS`），你的控件在只用键盘操作时会有问题——用户按 Tab 键切换焦点后，看不到哪个列表项有焦点。

## 第二步——Owner-Draw ComboBox 示例

ComboBox 的 Owner-Draw 和 ListBox 非常类似，但有一个额外的绘制区域——ComboBox 的编辑框部分（非下拉列表部分）也需要绘制。

```cpp
case WM_DRAWITEM:
{
    DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;

    if (dis->CtlID != IDC_COLOR_COMBO)
        break;

    ListItemData* data = (ListItemData*)dis->itemData;
    if (!data) return TRUE;

    // 检查是否是 ComboBox 编辑框区域
    bool isEditArea = (dis->itemState & ODS_COMBOBOXEDIT) != 0;

    // 绘制背景
    if (dis->itemState & ODS_SELECTED)
    {
        FillRect(dis->hDC, &dis->rcItem,
            GetSysColorBrush(COLOR_HIGHLIGHT));
        SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    }
    else
    {
        FillRect(dis->hDC, &dis->rcItem,
            GetSysColorBrush(COLOR_WINDOW));
        SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
    }

    SetBkMode(dis->hDC, TRANSPARENT);

    if (isEditArea)
    {
        // 编辑框区域：只画颜色方块和名称
        RECT colorRect = dis->rcItem;
        colorRect.left += 4;
        colorRect.top += 4;
        int size = dis->rcItem.bottom - dis->rcItem.top - 8;
        colorRect.right = colorRect.left + size;
        colorRect.bottom = colorRect.top + size;

        HBRUSH hBrush = CreateSolidBrush(data->color);
        FillRect(dis->hDC, &colorRect, hBrush);
        DeleteObject(hBrush);

        RECT textRect = dis->rcItem;
        textRect.left = colorRect.right + 8;
        DrawText(dis->hDC, data->name, -1, &textRect,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    else
    {
        // 下拉列表区域：画完整项（颜色方块 + 名称 + 描述）
        // ... 和 ListBox 的绘制逻辑相同 ...
    }

    if (dis->itemState & ODS_FOCUS)
    {
        DrawFocusRect(dis->hDC, &dis->rcItem);
    }

    return TRUE;
}
```

`ODS_COMBOBOXEDIT` 标志帮助我们区分当前需要绘制的是 ComboBox 的编辑框区域还是下拉列表区域。编辑框区域通常空间有限，所以只画颜色方块和名称就够了；下拉列表区域空间更大，可以画完整的描述。

## ⚠️ 踩坑预警

**坑点一：未处理 WM_MEASUREITEM 导致列表项不可见**

这是最经典的新手坑。`WM_MEASUREITEM` 在控件创建时发送，如果你当时还没有写好处理逻辑，或者消息被 `DefWindowProc` 默认处理了，列表项的高度就会是默认值。如果你之后在 `WM_DRAWITEM` 中绘制了超出默认高度的内容，超出部分会被裁剪——看起来就是"内容不完整"或者干脆"列表项是空的"。

如果你发现列表项高度不正确，首先检查 `WM_MEASUREITEM` 是否被正确处理。你可以在处理函数中打个断点，确认 `mis->itemHeight` 被设置为了你期望的值。

**坑点二：选中状态下忘记绘制高亮背景**

如果你在 `WM_DRAWITEM` 中没有检查 `ODS_SELECTED` 状态来切换背景色和文字色，选中效果就完全不存在——用户点击列表项后什么反馈都没有。更糟糕的是，如果你把文字颜色硬编码为深色，选中项的背景还是系统默认的深蓝色，深色文字在深蓝背景上几乎看不见。

确保你的绘制代码在 `ODS_SELECTED` 状态下切换背景色和文字色。

**坑点三：GDI 对象泄漏**

`CreateSolidBrush`、`CreateFont`、`CreatePen` 这些 GDI 创建函数返回的对象必须在使用后通过 `DeleteObject` 释放。如果你在每次 `WM_DRAWITEM` 处理中都创建新字体但不释放，GDI 对象数量会持续增长，最终导致系统资源耗尽——症状是文字突然变成系统默认字体、颜色显示异常、甚至其他应用的窗口开始渲染错误。

建议使用 RAII 封装或者在绘制函数末尾统一清理所有创建的 GDI 对象。

**坑点四：设备上下文的状态未恢复**

`WM_DRAWITEM` 提供的 `hDC` 是系统共用的设备上下文——不是每次调用都给你一个新的。如果你在绘制过程中修改了字体、文字颜色、背景模式等 DC 状态但没有恢复，下一次绘制就会继承这些修改后的状态，导致渲染错误。养成习惯：在绘制函数开头保存旧状态，末尾恢复。

## 常见问题

### Q: Owner-Draw 能不能用 Direct2D 绘制？

可以。`DRAWITEMSTRUCT` 中的 `hDC` 是一个标准的 GDI 设备上下文。你可以通过 `CreateDCRenderTarget` 创建一个 D2D 的 DC 渲染目标，然后在这个渲染目标上用 D2D API 绘制。不过要注意性能——每次 `WM_DRAWITEM` 都创建渲染目标开销不小，建议缓存渲染目标对象。

### Q: WM_DRAWITEM 和 WM_PAINT 有什么区别？

`WM_PAINT` 是窗口级别的绘制消息，你需要自己计算需要绘制的内容。`WM_DRAWITEM` 是控件级别的，Windows 已经帮你确定了需要绘制的是哪个控件的哪个元素，并且准备好了正确的 DC 和裁剪区域。Owner-Draw 的绘制逻辑通常比 `WM_PAINT` 简单得多。

### Q: 如何在 Owner-Draw 控件上实现悬停效果？

标准 Owner-Draw 没有内置的悬停（Hover）支持——`itemState` 中没有 `ODS_HOT` 这样的标志。如果你需要悬停效果，需要通过 `WM_MOUSEMOVE` 消息手动追踪鼠标位置，判断鼠标在哪个列表项上，然后用 `InvalidateRect` 触发该列表项的重绘。这在 ListBox 中可以通过 `LB_ITEMFROMPOINT` 消息来获取鼠标所在的列表项索引。

## 总结

这篇我们完整地拆解了 Owner-Draw 控件的工作机制。

核心概念是 `WM_DRAWITEM` 和 `DRAWITEMSTRUCT`——Windows 在需要绘制 Owner-Draw 控件时，把绘制所需的全部信息（哪个控件、哪个列表项、什么状态、绘制区域、设备上下文）打包在 `DRAWITEMSTRUCT` 中发送给父窗口。你在消息处理函数中根据这些信息自行绘制控件的外观。

我们实现了一个完整的 Owner-Draw ListBox 示例——每个列表项显示彩色方块和描述文字，选中项有系统风格的高亮效果，焦点指示器用虚线边框表示。关键技术包括 `LBS_OWNERDRAWFIXED` 样式的设置、`WM_MEASUREITEM` 中指定列表项高度、`WM_DRAWITEM` 中的分层绘制（背景→颜色方块→文字→焦点指示器），以及 GDI 对象的正确管理。

Owner-Draw 让你在不重写控件行为的前提下获得完全的外观控制权。但如果你需要更进一步——不仅定制外观，还要定制交互行为（比如 Hover 动画、自定义的鼠标响应逻辑），那就需要跳出 Owner-Draw 的框架，构建完全自绘控件了。这是下一篇的主题。

---

## 练习

1. 实现 Owner-Draw ComboBox：创建一个下拉颜色选择器，每个下拉项显示颜色方块 + 颜色名称 + 颜色 HEX 值。选中某个颜色后，ComboBox 的编辑框区域显示对应的颜色方块。

2. 在 Owner-Draw ListBox 上实现悬停效果：追踪 `WM_MOUSEMOVE` 消息，判断鼠标在哪个列表项上，为悬停项绘制浅灰色背景（非选中状态下的悬停提示）。

3. 为 Owner-Draw ListBox 实现交替行背景色——奇数行白色背景，偶数行浅灰色背景（类似 Excel 的斑马条纹效果），提升大量列表项时的可读性。

4. 尝试在 Owner-Draw 的绘制中使用 Direct2D 替代 GDI：通过 `CreateDCRenderTarget` 从 `DRAWITEMSTRUCT` 的 `hDC` 创建 D2D 渲染目标，用 D2D API 绘制圆角矩形、渐变填充和高质量抗锯齿文字。对比 GDI 和 D2D 绘制的视觉差异。

---

**参考资料**:
- [WM_DRAWITEM message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/wm-drawitem)
- [DRAWITEMSTRUCT structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-drawitemstruct)
- [WM_MEASUREITEM message - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/wm-measureitem)
- [MEASUREITEMSTRUCT structure - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-measureitemstruct)
- [Owner-Drawn Controls - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/owner-drawn-controls)
- [Button Styles - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/button-styles)
- [ListBox Styles - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/controls/list-box-styles)
