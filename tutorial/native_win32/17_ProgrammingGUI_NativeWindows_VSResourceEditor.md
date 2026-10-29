# 通用GUI编程技术——Win32 原生编程实战（十六）——Visual Studio 资源编辑器使用指南

> 前面几篇文章我们讲了对话框、资源文件、菜单、图标这些内容，你可能已经注意到一个问题：我是让你手写那些 .rc 文件定义的。说实话，在实际开发中，我很少手写 .rc 文件，因为 Visual Studio 自带了一个非常好用的资源编辑器，可以可视化地设计对话框、菜单、图标这些资源。今天我们要聊的就是这个被很多初学者忽视的强大工具。

---

## 为什么需要资源编辑器

说实话，我刚学 Win32 的时候特别抗拒用可视化工具，总觉得手写代码才叫"硬核"。但当我真正开始做项目的时候，很快就发现自己是在自虐。想象一下这个场景：你需要设计一个有十几个控件的对话框，如果你手写 .rc 文件，你得一个个计算坐标、猜测控件大小、反复编译运行看效果，改一个位置可能要调整好几个控件的坐标。这种开发方式不仅效率低，而且很容易出错。

资源编辑器的核心价值在于它把你从繁琐的坐标计算中解放出来，让你可以专注于 UI 设计本身。你只需要拖拽控件到想要的位置、在属性窗口里调整样式和属性，.rc 文件会自动生成。更重要的是，你可以直接在编辑器里预览效果，所见即所得，不需要反复编译运行。

另一个现实原因是：现代开发基本都是团队协作，UI 设计师可能不怎么会写代码，但完全可以操作资源编辑器来设计界面。而且，资源编辑器生成的 .rc 文件格式是标准的，任何编译器都能编译，你不用担心被 Visual Studio 绑定。

但这并不意味着你可以完全不用了解 .rc 文件的格式。当你遇到资源编辑器无法满足的复杂需求时，或者需要批量修改多个资源时，直接编辑 .rc 文件仍然是最高效的方式。最好的方式是两者结合：能用资源编辑器就用，编辑器搞不定的再手动改 .rc 文件。

这篇文章会带着你从零开始，把 Visual Studio 资源编辑器彻底搞透。我们不只是学会怎么用，更重要的是理解它生成的 .rc 文件是什么样的，以及什么时候需要手动编辑。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

你需要确保在安装 Visual Studio 时勾选了"使用 C++ 的桌面开发"工作负载，这样才能使用资源编辑器。如果安装时没勾选，可以打开 Visual Studio Installer，修改安装选项，把这个工作负载加上。

---

## 第一步——认识资源视图和资源文件

当你创建一个 Win32 项目时，Visual Studio 会自动生成一些基础文件，其中包括一个 .rc 文件（通常叫作项目名.rc）和一个 resource.h 头文件。.rc 文件是资源脚本文件，包含了所有的资源定义；resource.h 里是资源 ID 的符号定义。

打开解决方案资源管理器，你会看到一个叫"资源文件"的文件夹，里面有个 .rc 文件。但这个视图并不是用来编辑资源的，你需要切换到"资源视图"。资源视图通常在解决方案资源管理器的下方或者右侧，如果找不到，可以在"视图"菜单里打开"资源视图"。

在资源视图里，你的 .rc 文件被展开成一棵资源树，根节点是项目名，下面是各种资源类型：Accelerator（加速键）、Dialog（对话框）、Icon（图标）、Menu（菜单）、Version（版本信息）等等。每种资源类型下面是具体的资源实例，比如 Dialog 下面可能有 IDD_ABOUTBOX 和 IDD_INPUTDLG 两个对话框。

⚠️ 注意

千万别直接在解决方案资源管理器里双击 .rc 文件，这样会用文本编辑器打开它。你应该在资源视图里双击对应的资源，这样才会用资源编辑器打开。如果你真的不小心用文本编辑器改了 .rc 文件，然后又用资源编辑器改了同一个资源，Visual Studio 会提示你 .rc 文件被修改了，需要重新加载。

这里有个值得提到的小知识：Visual Studio 资源编辑器不直接读取 .rc 文件，而是读取一个叫 .aps 的文件。.aps 文件是资源编译器编译 .rc 文件后生成的二进制文件，包含了符号信息，让资源编辑器能够快速加载资源。每次你用资源编辑器保存资源时，Visual Studio 会更新 .aps 文件，然后把更改写回 .rc 文件。如果 .aps 文件损坏了（这偶尔会发生），Visual Studio 会重新编译 .rc 文件来生成新的 .aps 文件，或者你可以直接删除 .aps 文件让它重新生成。

---

## 第二步——创建和编辑对话框

对话框是 Win32 程序中最常用的资源类型之一，也是资源编辑器最擅长处理的东西。我们来看怎么用资源编辑器创建一个对话框。

### 创建新对话框

在资源视图里右键点击你的 .rc 文件根节点，选择"添加资源"，会弹出一个对话框，列出了所有可以添加的资源类型。选择"Dialog"，然后点击"新建"，就会创建一个新的对话框资源，并在资源编辑器里打开。

新创建的对话框通常有两个按钮：OK 和 Cancel，这是 Visual Studio 提供的模板。你可以在右侧的属性窗口里修改对话框的属性，最常用的是：

* **ID**：对话框的资源 ID，默认是 IDD_DIALOG1，建议改成更有意义的名字
* **Caption**：对话框标题栏显示的文字
* **Font**：对话框使用的字体，默认是 MS Shell Dlg
* **Style**：对话框样式，通常包含 Popup、Caption、SysMenu 等
* **Border**：边框样式，Dialog Frame 是典型的对话框边框
* **System Menu**：是否显示系统菜单（左上角的图标和关闭、最大化、最小化按钮）

### 从工具箱拖拽控件

资源编辑器打开后，你会看到一个工具箱窗口（如果找不到，在"视图"菜单里打开"工具箱"）。工具箱里列出了所有可以拖到对话框上的控件：Button（按钮）、Edit Control（编辑框）、Static Text（静态文本）、List Box（列表框）、Combo Box（组合框）等等。

添加控件很简单，只需要从工具箱里拖拽控件到对话框上，然后调整位置和大小。选中控件后，你可以在属性窗口里修改它的属性，最常用的是：

* **ID**：控件的资源 ID，对于需要处理消息的控件，建议改成有意义的名字
* **Caption**：控件显示的文字（适用于按钮、静态文本等）
* **Visible**：控件是否可见
* **Disabled**：控件是否禁用
* **Tab Stop**：控件是否能通过 Tab 键获得焦点
* **Group**：是否开始一个新的控件组（用于单选按钮分组）
* **控件特有的样式**：比如按钮的 Type、编辑框的 Auto HScroll、多行编辑框的 Multiline 等

### 对齐和布局控件

当你把多个控件放到对话框上后，你可能会发现它们的大小和位置不太整齐。资源编辑器提供了一些对齐工具来帮你快速调整布局：

* **对齐**：在"格式"菜单里，可以选择左对齐、右对齐、顶端对齐、底端对齐、居中对齐
* **使大小相同**：可以让选中的多个控件宽度相同、高度相同、或者大小完全相同
* **排列按钮**：可以把选中的按钮横向或纵向均匀分布
* **间距**：可以调整控件之间的间距，让它相等、增加或减少

这些操作在"格式"菜单和工具栏上都有，或者你选中控件后右键，在弹出的菜单里也能找到。熟练使用这些工具可以大大提高 UI 设计效率。

### 测试对话框

设计完对话框后，你可以在不编译运行的情况下预览它的效果。在资源编辑器的工具栏上有个"测试对话框"按钮（通常是个对话框图标），或者按 Ctrl+T，就会弹出一个测试窗口。在测试窗口里，你可以像真实运行时一样操作对话框，看看 Tab 键导航是否正常、按钮点击是否响应、默认按钮是否正确等等。

测试对话框只会检查 UI 的行为，不会执行你的对话框过程代码。所以如果你的按钮点击处理逻辑有问题，测试对话框是发现不了的。但测试对话框对于验证 UI 布局和基本交互非常有用，可以帮你省下很多编译运行的时间。

---

## 第三步——创建和编辑菜单

菜单是另一个常用的资源类型，资源编辑器对菜单的支持也很好用。

### 创建菜单资源

在资源视图里右键点击你的 .rc 文件，选择"添加资源"，然后选择"Menu"，点击"新建"，就会创建一个新的菜单资源，并在菜单编辑器里打开。

菜单编辑器的界面和对话框编辑器不太一样。中间是一个菜单栏预览，下面是一个属性窗口。你可以直接点击菜单栏上的"在此处键入"来添加顶级菜单项，比如"文件"、"编辑"、"帮助"。

### 添加菜单项和子菜单

点击顶级菜单项后，会展开一个下拉菜单，你可以在里面添加子菜单项。添加菜单项有几种方式：

* **直接输入**：点击"在此处键入"，输入菜单项的文字，比如"打开..."
* **添加分隔符**：在菜单项之间右键，选择"插入分隔符"，会画一条横线
* **添加子菜单**：选中一个菜单项，在属性窗口里把"Popup"设为 TRUE，或者在右键菜单里选择"插入子菜单"

菜单项的属性窗口里，最重要的属性是：

* **ID**：菜单项的命令 ID，当用户点击这个菜单项时，会发送 WM_COMMAND 消息，wParam 的低位就是这个 ID
* **Caption**：菜单项显示的文字，可以用 & 来定义快捷键，比如"&File"会显示"File"，F 下面有下划线
* **Prompt**：状态栏提示文字，当鼠标悬停在这个菜单项上时，状态栏会显示这个文字（如果你的程序有状态栏的话）
* **Checked**：菜单项是否显示勾选标记
* **Grayed**：菜单项是否灰色显示（禁用）
* **Inactive**：菜单项是否不响应鼠标

### 弹出菜单（快捷菜单）

除了菜单栏上的菜单，Win32 程序经常还会用到弹出菜单（也叫快捷菜单或上下文菜单），就是右键点击时弹出的那种菜单。在资源编辑器里创建弹出菜单的方式和普通菜单完全一样，区别在于使用方式。

假设你创建了一个 IDR_CONTEXTMENU 菜单资源，里面有几个菜单项。在代码里，你可以这样显示它：

```cpp
case WM_CONTEXTMENU:
{
    // 加载菜单资源
    HMENU hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_CONTEXTMENU));
    HMENU hSubMenu = GetSubMenu(hMenu, 0);

    // 获取鼠标位置
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    // 如果是键盘触发的（x 和 y 都是 -1），用当前鼠标位置
    if (x == -1 && y == -1)
    {
        POINT pt;
        GetCursorPos(&pt);
        x = pt.x;
        y = pt.y;
    }

    // 显示弹出菜单
    TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                   x, y, 0, hwnd, NULL);

    // 销毁菜单
    DestroyMenu(hMenu);
    return 0;
}
```

这段代码处理 WM_CONTEXTMENU 消息，加载菜单资源，然后调用 TrackPopupMenu 显示菜单。注意我们用的是 GetSubMenu 获取第一个子菜单，因为 LoadMenu 返回的是整个菜单栏，而弹出菜单只需要其中的一个子菜单。

---

## 第四步——其他资源类型

除了对话框和菜单，资源编辑器还支持其他几种常用资源类型。

### 图标和光标

图标和光标的编辑需要专门的工具，Visual Studio 自带的图标编辑器功能比较基础，只能编辑简单的图标和光标。对于复杂的图标，你可能需要用更专业的工具，比如 GIMP、Photoshop 加上图标插件，或者专门的图标编辑器如 Axialis IconWorkshop。

在资源编辑器里添加图标或光标的方式和其他资源一样：右键 .rc 文件，选择"添加资源"，然后选择 Icon 或 Cursor，可以新建一个图标/光标，或者从现有的 .ico/.cur 文件导入。

图标有个特殊的地方：一个图标文件可以包含多个尺寸和颜色深度的版本，这样系统在不同 DPI 和显示设置下可以选择合适的版本。资源编辑器支持编辑多尺寸图标，你可以在工具栏上切换不同的尺寸来编辑。

### 字符串表

字符串表（String Table）资源在资源视图里的显示方式和其他资源不太一样，它不是一个个独立的资源，而是一个大表格。你双击 String Table 节点，会打开一个表格编辑器，每一行是一个字符串资源，包含 ID、值、和内容。

字符串表的一个好处是可以集中管理程序中所有的字符串，方便以后做国际化。在代码里，你可以用 LoadString 函数加载字符串表中的字符串：

```cpp
wchar_t msg[256];
LoadString(GetModuleHandle(NULL), IDS_HELLO, msg, 256);
MessageBox(hwnd, msg, L"", MB_OK);
```

### 版本信息

版本信息资源是用来记录程序版本号、公司信息、版权声明等元数据的。双击 Version 节点会打开一个固定格式的编辑器，你可以在里面填写各种信息。这些信息会显示在文件的属性页里，用户右键点击你的 .exe 文件，选择"属性"，在"详细信息"选项卡里就能看到这些信息。

---

## 第五步——资源编辑器和手写 .rc 的配合

资源编辑器确实很强大，但它不是万能的。有些高级需求可能需要在 .rc 文件里手动调整。

### 资源编辑器的限制

资源编辑器最大的限制是它只支持标准的资源类型和属性。如果你需要用一些高级的对话框样式、或者需要在一个对话框里放上百个控件，资源编辑器可能会变得难以使用。另外，资源编辑器生成的 .rc 文件格式是固定的，如果你有特殊的格式要求（比如为了兼容某个老代码），可能需要手动调整。

### 手动编辑 .rc 文件

你可以在资源编辑器里右键选择"查看代码"，或者直接在文本编辑器里打开 .rc 文件来手动编辑。手动编辑时要注意，.rc 文件的语法有一定的规则，如果语法错误，资源编译器会报错。

一个常见的技巧是用资源编辑器设计好界面，然后复制生成的 .rc 代码，根据需要进行批量修改。比如你需要创建 10 个类似的对话框，手动一个个设计会很慢，但你可以先设计好一个，然后复制它的代码，修改 ID 和一些关键属性，这样会快很多。

⚠️ 注意

如果你手动修改了 .rc 文件，然后又用资源编辑器打开同一个资源，Visual Studio 可能会警告你文件被外部修改。如果你在资源编辑器里保存了更改，你手动修改的内容可能会被覆盖。一个安全的做法是：手动修改后，在资源编辑器里重新加载 .rc 文件，或者在资源编辑器里不要修改你手动改过的部分。

---

## 第六步——完整示例

我们来看一个完整的例子，用资源编辑器创建一个简单的记事本程序，包含菜单栏、工具栏、状态栏和一个编辑框。

### 创建项目

首先创建一个 Win32 项目，命名为"MyNotepad"。在应用程序向导里选择"空项目"，这样我们从头开始写代码。

### 添加菜单资源

在资源视图里添加一个菜单资源，设计如下菜单结构：

```
文件(&F)
  新建(&N)...        Ctrl+N    ID_FILE_NEW
  打开(&O)...        Ctrl+O    ID_FILE_OPEN
  保存(&S)           Ctrl+S    ID_FILE_SAVE
  -
  退出(&X)                      ID_FILE_EXIT

编辑(&E)
  撤销(&U)           Ctrl+Z    ID_EDIT_UNDO
  重做(&R)           Ctrl+Y    ID_EDIT_REDO
  -
  剪切(&T)           Ctrl+X    ID_EDIT_CUT
  复制(&C)           Ctrl+C    ID_EDIT_COPY
  粘贴(&P)           Ctrl+V    ID_EDIT_PASTE
  删除(&D)           Del       ID_EDIT_DELETE
  -
  全选(&A)           Ctrl+A    ID_EDIT_SELECTALL

帮助(&H)
  关于(&A)...                   ID_HELP_ABOUT
```

### 添加工具栏资源

添加一个 ToolBar 资源，设计几个常用按钮：新建、打开、保存、剪切、复制、粘贴。

### 添加图标资源

导入或者绘制一个程序图标，设为 IDI_MAINICON。

### 添加对话框资源

添加一个"关于"对话框，包含程序名称、版本信息和一个确定按钮。

### 主程序代码

```cpp
#include <windows.h>
#include "resource.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);

HWND hEdit;
HWND hStatus;
HINSTANCE hInst;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_MYNOTEPAD);
    wcex.lpszClassName = L"MyNotepadClass";

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, L"窗口类注册失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建主窗口
    HWND hwnd = CreateWindow(
        L"MyNotepadClass",
        L"我的记事本",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, L"窗口创建失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 创建编辑框
        hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            hwnd, (HMENU)IDC_EDIT, hInst, NULL);

        // 创建状态栏
        hStatus = CreateWindowEx(0, STATUSCLASSNAME, L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0,
            hwnd, (HMENU)IDC_STATUSBAR, hInst, NULL);

        return 0;
    }

    case WM_SIZE:
    {
        // 调整编辑框和状态栏大小
        RECT rcStatus;
        GetWindowRect(hStatus, &rcStatus);
        int statusHeight = rcStatus.bottom - rcStatus.top;

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        MoveWindow(hEdit, 0, 0, rcClient.right, rcClient.bottom - statusHeight, TRUE);
        MoveWindow(hStatus, 0, rcClient.bottom - statusHeight, rcClient.right, statusHeight, TRUE);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_FILE_NEW:
            SetWindowText(hEdit, L"");
            break;

        case ID_FILE_OPEN:
            // TODO: 实现打开文件
            MessageBox(hwnd, L"打开文件功能待实现", L"提示", MB_OK);
            break;

        case ID_FILE_SAVE:
            // TODO: 实现保存文件
            MessageBox(hwnd, L"保存文件功能待实现", L"提示", MB_OK);
            break;

        case ID_FILE_EXIT:
            DestroyWindow(hwnd);
            break;

        case ID_HELP_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutDlgProc);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}
```

编译运行这个程序，你应该能看到一个带有菜单栏、状态栏的记事本程序。编辑框占据了大部分窗口区域，当窗口大小改变时，编辑框和状态栏会自动调整大小。菜单栏上的各个菜单项都可以点击，虽然很多功能还是待实现的状态。

---

## 后续可以做什么

到这里，Visual Studio 资源编辑器的基础知识就讲完了。你现在应该能够用资源编辑器设计对话框和菜单，理解 .rc 文件的结构，知道什么时候需要手动编辑。但资源的世界远不止这些，还有很多高级主题等着我们去探索。

下一篇文章，我们会进入 **GDI 绘图的世界**——学习怎么在窗口上绘制图形、文字、图片。这是实现自定义控件、图表显示、游戏开发的基础。我们会介绍设备上下文（HDC）、GDI 对象（画笔、画刷、字体）、双缓冲技术等内容。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习，巩固一下知识：

1. 用资源编辑器设计一个完整的设置对话框，包含各种控件类型
2. 创建一个多级菜单系统，包含快捷键和状态栏提示
3. 导入自定义图标和光标，让程序看起来更专业
4. 手动编辑 .rc 文件，批量创建多个类似的对话框

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。好了，今天的文章就到这里，我们下一篇再见！

---

## 相关资源

- [Resource Editors (C++) - Microsoft Learn](https://learn.microsoft.com/zh-cn/cpp/windows/resource-editors?view=msvc-170)
- [Dialog Editor (C++) - Microsoft Learn](https://learn.microsoft.com/zh-cn/cpp/windows/dialog-editor?view=msvc-170)
- [How To: Create a Dialog Box (C++) - Microsoft Learn](https://learn.microsoft.com/zh-cn/cpp/windows/creating-a-new-dialog-box?view=msvc-170)
- [Resource Files (C++) - Microsoft Learn](https://learn.microsoft.com/zh-cn/cpp/windows/resource-files-visual-studio?view=msvc-170)
- [Using the Resource Editor to Design a Dialog Box - Intel](https://www.intel.com/content/www/us/en/docs/fortran-compiler/developer-reference-build-windows-applications/15-0/using-the-resource-editor-to-design-a-dialog-box.html)
