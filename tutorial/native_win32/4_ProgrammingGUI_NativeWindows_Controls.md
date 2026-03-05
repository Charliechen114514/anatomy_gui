# 通用GUI编程技术——Win32 原生编程实战（三）——从按钮到列表框

> 上一篇文章我们搞定了键盘和鼠标输入，你的窗口已经能够响应用户的各种操作了。但说实话，光有一个空白窗口什么都干不了，就像买了一台电脑却没配键盘鼠标一样难受。今天我们要聊的是让窗口真正"好用"起来的东西——标准控件。

---

## 为什么一定要搞懂控件

说实话，我在刚学 Win32 的时候也想过：不就是放几个按钮、输入框吗，这有什么好学的。但当我真正开始做项目的时候，才发现自己想得太简单了。

首先，控件并不是什么特殊的东西，它们本质上就是预定义好的窗口类。系统已经帮你写好了处理绘制、焦点、键盘输入这些麻烦事的代码，你只需要调用 `CreateWindow` 或者 `CreateWindowEx`，填上几个参数就能用。这听起来很美好，但如果你不知道控件是怎么和父窗口通信的，不知道每个参数的含义，那你很快就会遇到各种奇怪的问题。

比如你可能遇到过这种情况：明明给按钮写了点击处理代码，但程序就是不按预期执行。或者编辑框里的文字怎么都获取不到，每次拿到的是空字符串。这些问题的根源往往不是代码写错了，而是对 Win32 的控件机制理解不够深入。

另一个现实原因是：现代框架像 Qt、WPF 把控件封装得太好了，你可能从来不需要关心 WM_COMMAND 是什么。但一旦你需要做一些底层优化，或者遇到了框架无法解决的 bug，这些知识就会成为你的救命稻草。而且，理解了 Win32 的控件机制之后，再去学其他框架的控件系统，你会发现很多东西都是相通的。

这篇文章会带着你从零开始，把按钮、编辑框、列表框这些常用控件彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* 平台：Windows 10/11（理论上 Windows Vista+ 都行，但谁还用那些老古董）
* 开发工具：Visual Studio 2019 或更高版本
* 编程语言：C++（C++17 或更新）
* 项目类型：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面两篇文章的内容——至少知道怎么创建一个基本的窗口、消息循环是怎么跑的、键盘鼠标消息怎么处理。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记，不然接下来的内容可能会让你有点头晕。

---

## 第一步——理解控件的本质

在 Win32 的世界里，控件不是什么神秘的魔法，它们就是窗口。准确地说，控件是一种特殊的子窗口，由系统预定义的窗口类创建。你可能已经知道注册窗口类时要指定 `lpszClassName`，控件也不例外，只不过这些类是系统预先注册好的。

当你创建一个按钮控件时，实际上是在告诉系统："给我创建一个使用 'BUTTON' 窗口类的子窗口"。系统会找到这个预定义的类，用它来创建窗口，然后把子窗口的句柄返回给你。这个子窗口会自己处理绘制、鼠标点击、键盘焦点这些事情，你只需要在父窗口的窗口过程里处理它发回来的消息就行。

### 创建控件的基本套路

创建控件和创建普通窗口用的是同一个 API——`CreateWindowEx`。只不过参数有些讲究：

```cpp
HWND hButton = CreateWindowEx(
    0,                          // 扩展样式，先不管它
    L"BUTTON",                  // 窗口类名，这里是按钮
    L"点击我",                  // 控件显示的文字
    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,  // 关键样式
    10, 10,                     // 位置（x, y），相对于父窗口客户区
    100, 30,                    // 大小（宽度, 高度）
    hwnd,                       // 父窗口句柄
    (HMENU)ID_BUTTON_1,         // 控件 ID，这个很重要
    hInstance,                  // 程序实例句柄
    NULL                        // 额外参数，一般填 NULL
);
```

这里有几个关键点需要解释。首先是 `WS_CHILD` 样式，这个标志告诉系统这是一个子窗口，必须有父窗口。没有这个样式，控件就无法正常工作。`WS_VISIBLE` 让控件创建后立即可见，如果不加这个，你得手动调用 `ShowWindow` 才能看到控件。

然后是 `BS_PUSHBUTTON`，这是按钮控件特有的样式。不同类型的控件有自己的样式前缀——按钮用 `BS_`，编辑框用 `ES_`，列表框用 `LBS_`，组合框用 `CBS_`。这些样式决定了控件的具体行为和外观。

### 控件 ID 的故事

你可能注意到了创建控件时的那个 `(HMENU)ID_BUTTON_1`，这里有个值得说道的设计。控件需要有一个标识符，这样父窗口收到消息时才知道是哪个控件发的。但 `CreateWindowEx` 的 `hMenu` 参数本来是用来传菜单句柄的，怎么办呢？Windows 的设计师做了一个很实用的设计：对于子窗口，这个参数被复用为控件 ID。

所以你需要定义一组常量作为控件 ID，通常放在资源头文件里：

```cpp
#define ID_BUTTON_1    1001
#define ID_BUTTON_2    1002
#define ID_EDIT_INPUT  2001
#define ID_CHECK_SAVE  3001
```

这些 ID 只要在一个父窗口内唯一就行，不同窗口的控件 ID 可以重复。建议给不同类型的控件用不同的数值范围，这样读代码的时候一眼就能看出这个 ID 属于哪种控件。

⚠️ 注意

千万别忘记那个 `(HMENU)` 强制转换。如果你直接传整数，编译器可能会给你一个警告，而且在 64 位系统上可能会出问题。因为 `HMENU` 本质上是指针大小的类型，直接传 int 可能会被截断或扩展。

### 控件的销毁

控件作为子窗口，会在父窗口销毁时自动销毁，你不需要手动调用 `DestroyWindow`。但如果你想提前移除某个控件，还是要手动调用的。销毁控件后，相关的句柄就失效了，不要再使用。

另外，如果你用 `CreateWindowEx` 创建了很多控件，但没有在窗口销毁时做任何处理，系统会自动清理。但如果你维护了一个控件句柄的列表，记得在销毁前清空或者做好标记，否则可能会误用已经失效的句柄。

---

## 第二步——按钮家族全解析

按钮是我们最常用的控件，但 Win32 里的按钮不只是那种可以点击的东西，它实际上是一个大家族。通过不同的按钮样式，你可以得到普通按钮、复选框、单选按钮、分组框等多种不同的控件。它们本质上都是同一个窗口类，只是样式不一样。

### 普通按钮

普通按钮就是最常见的点击式按钮，对应 `BS_PUSHBUTTON` 样式。用户点击后会触发一个 `BN_CLICKED` 通知，我们后面会详细讲怎么处理这个通知。

```cpp
CreateWindowEx(0, L"BUTTON", L"确定",
    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    10, 10, 80, 30,
    hwnd, (HMENU)ID_OK, hInstance, NULL);
```

默认情况下，按钮在用户按下时会有一个"按下"的视觉效果，释放后恢复原状。这个行为是系统自带的，你不需要自己处理绘制或者鼠标消息。按钮还会自动处理键盘焦点——当用户用 Tab 键切换到按钮时，按钮周围会出现一个虚线框，按下空格或回车键相当于点击。

### 默认按钮和取消按钮

在对话框里，你经常能看到一个按钮有更粗的边框，这是默认按钮（Default Button）。用户按下回车键时会触发默认按钮，不管当前焦点在哪个控件上。要创建默认按钮，需要加上 `BS_DEFPUSHBUTTON` 样式：

```cpp
CreateWindowEx(0, L"BUTTON", L"确定",
    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,  // 注意这里是 DEFPUSHBUTTON
    10, 10, 80, 30,
    hwnd, (HMENU)ID_OK, hInstance, NULL);
```

取消按钮（通常是"取消"或"关闭"）会在用户按下 Esc 键时触发。但 Win32 没有专门的样式来标记取消按钮，而是需要在消息处理里判断——如果收到 IDCANCEL 通知，就当作用户点击了取消按钮。这个我们后面会讲到。

### 复选框

复选框是一个可以独立开/关的小方块，常用于让用户选择多个选项。创建复选框用的是 `BS_CHECKBOX` 或 `BS_AUTOCHECKBOX` 样式：

```cpp
HWND hCheck = CreateWindowEx(0, L"BUTTON", L"记住密码",
    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,  // 自动切换状态
    10, 50, 120, 20,
    hwnd, (HMENU)ID_CHECK_SAVE, hInstance, NULL);
```

`BS_CHECKBOX` 和 `BS_AUTOCHECKBOX` 的区别在于状态的自动管理。如果你用的是 `BS_CHECKBOX`，每次用户点击时你需要手动发送 `BM_SETCHECK` 消息来切换复选框的状态。而 `BS_AUTOCHECKBOX` 会自动处理点击事件，你不需要写任何代码就能看到勾选/取消勾选的效果。

如果需要获取或设置复选框的状态，可以发送 `BM_GETCHECK` 和 `BM_SETCHECK` 消息：

```cpp
// 获取复选框状态
LRESULT state = SendMessage(hCheck, BM_GETCHECK, 0, 0);
if (state == BST_CHECKED)
{
    // 复选框被勾选
}

// 设置复选框状态
SendMessage(hCheck, BM_SETCHECK, BST_CHECKED, 0);  // 勾选
SendMessage(hCheck, BM_SETCHECK, BST_UNCHECKED, 0); // 取消勾选
```

这里的 `BST_CHECKED` 和 `BST_UNCHECKED` 是按钮状态常量。还有一个 `BST_INDETERMINATE` 表示"不确定"状态，通常用于三态复选框（`BS_AUTO3STATE`），比如表示"部分选中"的情况。

### 单选按钮

单选按钮是一组互斥的选项，同一时间只能选中其中一个。它们通常是圆形的，选中时中间有个实心圆点。创建单选按钮用的是 `BS_RADIOBUTTON` 样式：

```cpp
// 创建一组单选按钮
CreateWindowEx(0, L"BUTTON", L"男",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON | WS_GROUP,
    10, 80, 60, 20,
    hwnd, (HMENU)ID_RADIO_MALE, hInstance, NULL);

CreateWindowEx(0, L"BUTTON", L"女",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
    80, 80, 60, 20,
    hwnd, (HMENU)ID_RADIO_FEMALE, hInstance, NULL);
```

这里有个关键点：`WS_GROUP` 样式。第一个单选按钮需要加上这个样式，表示这是一个分组组的开始。接下来的单选按钮（直到下一个带 `WS_GROUP` 的控件）都属于同一组。这样系统就知道哪些单选按钮是互斥的——当你点击一个单选按钮时，同组的其他按钮会自动取消选中。

如果你需要创建多组单选按钮，记得给每一组的第一个按钮加上 `WS_GROUP`：

```cpp
// 第一组：性别
CreateWindowEx(0, L"BUTTON", L"男", ... | BS_RADIOBUTTON | WS_GROUP, ...);
CreateWindowEx(0, L"BUTTON", L"女", ... | BS_RADIOBUTTON, ...);

// 第二组：学历（新的 WS_GROUP 开始新的一组）
CreateWindowEx(0, L"BUTTON", L"高中", ... | BS_RADIOBUTTON | WS_GROUP, ...);
CreateWindowEx(0, L"BUTTON", L"本科", ... | BS_RADIOBUTTON, ...);
CreateWindowEx(0, L"BUTTON", L"研究生", ... | BS_RADIOBUTTON, ...);
```

获取单选按钮选中的方式和复选框类似，也是用 `BM_GETCHECK` 消息。你需要遍历整组单选按钮，检查每个按钮的状态：

```cpp
if (SendMessage(hRadioMale, BM_GETCHECK, 0, 0) == BST_CHECKED)
{
    // 选中了"男"
}
else if (SendMessage(hRadioFemale, BM_GETCHECK, 0, 0) == BST_CHECKED)
{
    // 选中了"女"
}
```

### 分组框

分组框是一个带文字边框的矩形，用于把相关控件组织在一起。它本身不参与交互，只是视觉上的分组。创建分组框用的是 `BS_GROUPBOX` 样式：

```cpp
CreateWindowEx(0, L"BUTTON", L"个人信息",
    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
    10, 70, 200, 80,
    hwnd, (HMENU)ID_GROUP_INFO, hInstance, NULL);
```

分组框里的其他控件要放在分组框的矩形范围内，但它们的父窗口仍然是主窗口，而不是分组框。分组框只是一个视觉容器，不会真正包含子窗口。

⚠️ 注意

分组框的控件 ID 通常不需要处理，因为它不会产生任何通知消息。但为了保持一致性，还是给它分配一个 ID，用不用无所谓。

---

## 第三步——编辑框详解

编辑框（Edit Control）是用来输入和显示文本的控件。它支持单行输入、多行输入、密码模式、只读模式等多种行为，是 GUI 程序中最复杂的控件之一。好消息是，大多数常见需求系统都已经帮你实现了，你只需要知道怎么触发这些功能。

### 单行编辑框

最简单的编辑框就是单行文本输入框，用户可以输入一行文字，按回车键不会换行：

```cpp
CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
    80, 10, 150, 20,
    hwnd, (HMENU)ID_EDIT_USERNAME, hInstance, NULL);
```

这里的 `WS_EX_CLIENTEDGE` 扩展样式给编辑框加了一个下沉的边框效果，让它看起来更像是传统 Windows 程序里的输入框。`ES_LEFT` 表示文字左对齐，`ES_AUTOHSCROLL` 表示当文字超过编辑框宽度时自动水平滚动。

### 获取和设置编辑框的文字

编辑框里最重要的操作当然是获取用户输入的内容，以及设置编辑框显示的文字。这些操作通过消息来完成：

```cpp
// 获取编辑框的文字长度
int len = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);

// 分配缓冲区（别忘了多留一个位置给空字符）
wchar_t* buffer = new wchar_t[len + 1];

// 获取文字
SendMessage(hEdit, WM_GETTEXT, len + 1, (LPARAM)buffer);

// 使用 buffer...
delete[] buffer;
```

`WM_GETTEXTLENGTH` 返回编辑框里文字的字符数（不包括结尾的空字符）。`WM_GETTEXT` 把文字复制到你提供的缓冲区里。一定要记得缓冲区要足够大，否则文字会被截断。

设置编辑框的文字更简单：

```cpp
// 设置文字
SendMessage(hEdit, WM_SETTEXT, 0, (LPARAM)L"默认文字");
```

如果只是想在编辑框末尾追加文字，可以用 `EM_REPLACESEL` 消息：

```cpp
// 在当前光标位置插入文字
SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)L"追加的内容");
```

这里的 `TRUE` 参数表示可以让这个操作被撤销（Undo）。如果你希望这个操作不能被撤销，传 `FALSE` 就行。

### 密码模式编辑框

密码输入框是一个很常见的需求，编辑框只需要加一个 `ES_PASSWORD` 样式就能实现：

```cpp
CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_PASSWORD,
    80, 40, 150, 20,
    hwnd, (HMENU)ID_EDIT_PASSWORD, hInstance, NULL);
```

默认情况下，密码字符是黑色的圆点（•）。如果你想换成其他字符，比如星号，可以发送 `EM_SETPASSWORDCHAR` 消息：

```cpp
// 把密码字符改成星号
SendMessage(hEdit, EM_SETPASSWORDCHAR, (WPARAM)L'*', 0);
```

恢复为正常编辑框（不隐藏输入）的话，把参数设为 0：

```cpp
SendMessage(hEdit, EM_SETPASSWORDCHAR, 0, 0);
```

这个功能在做"显示密码"功能时很有用——用户点击一个小眼睛图标，密码就会显示出来。

### 多行编辑框

多行编辑框允许用户输入多行文字，常用于日志显示、大段文本编辑等场景。创建时需要加上 `ES_MULTILINE` 样式：

```cpp
CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
    WS_VSCROLL | ES_WANTRETURN,
    10, 100, 300, 100,
    hwnd, (HMENU)ID_EDIT_LOG, hInstance, NULL);
```

这里的 `ES_AUTOVSCROLL` 和 `WS_VSCROLL` 让编辑框在文字超过高度时自动出现垂直滚动条。`ES_WANTRETURN` 是个很重要的样式——如果不加这个，用户在多行编辑框里按回车键会触发默认按钮，而不是换行。加上后，回车键就会正常换行。

多行编辑框还有一个常用的操作——设置文字选择和光标位置：

```cpp
// 选择第 10 到第 20 个字符
SendMessage(hEdit, EM_SETSEL, 10, 20);

// 把光标移到末尾
SendMessage(hEdit, EM_SETSEL, -1, -1);

// 全选
SendMessage(hEdit, EM_SETSEL, 0, -1);
```

### 只读编辑框

只读编辑框用于显示不能被用户修改的文字，比如显示程序输出、日志等。加上 `ES_READONLY` 样式即可：

```cpp
CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"这是只读内容",
    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY,
    10, 100, 300, 20,
    hwnd, (HMENU)ID_EDIT_READONLY, hInstance, NULL);
```

只读编辑框仍然允许用户选择文字和复制，但不能修改内容。如果你想要一个完全不能交互的文本显示，可能需要考虑用静态文本控件。

---

## 第四步——WM_COMMAND 机制深入

前面我们讲了怎么创建各种控件，但控件创建出来只是摆设，你得知道用户什么时候点了按钮、什么时候改了编辑框的内容。这就是 `WM_COMMAND` 消息的作用——它是子控件和父窗口之间的通信桥梁。

### WM_COMMAND 消息的结构

当用户和控件交互时，父窗口的窗口过程会收到 `WM_COMMAND` 消息。这个消息携带了三个信息：控件 ID、通知码、控件句柄。它们被打包在 `wParam` 和 `lParam` 里：

```cpp
case WM_COMMAND:
{
    // 提取消息内容
    WORD controlId = LOWORD(wParam);    // 控件 ID
    WORD notificationCode = HIWORD(wParam);  // 通知码
    HWND hControl = (HWND)lParam;       // 控件句柄

    // 根据 ID 和通知码处理
    switch (controlId)
    {
    case ID_OK:
        if (notificationCode == BN_CLICKED)
        {
            // 用户点击了确定按钮
        }
        break;

    case ID_EDIT_USERNAME:
        if (notificationCode == EN_CHANGE)
        {
            // 用户名编辑框的内容改变了
        }
        break;
    }
    break;
}
```

`LOWORD(wParam)` 是我们创建控件时指定的那个 ID，用 `HMENU` 转换传过去的。`HIWORD(wParam)` 是通知码，告诉你具体发生了什么事件。`lParam` 是控件的窗口句柄，有些操作可能需要直接操作控件。

### 按钮的通知码

按钮控件的通知码比较简单，最常用的是 `BN_CLICKED`——用户点击了按钮：

```cpp
case ID_BUTTON_1:
    if (HIWORD(wParam) == BN_CLICKED)
    {
        MessageBox(hwnd, L"你点击了按钮 1", L"提示", MB_OK);
    }
    break;
```

还有其他一些按钮通知码，比如 `BN_DOUBLECLICKED`（双击），但普通按钮默认不发双击通知。如果你需要处理双击，创建按钮时要加上 `BS_NOTIFY` 样式。

### 编辑框的通知码

编辑框的通知码就丰富多了，最常用的是：

* `EN_CHANGE`：编辑框的内容改变了（不管是用户输入还是程序设置的）
* `EN_SETFOCUS`：编辑框获得键盘焦点
* `EN_KILLFOCUS`：编辑框失去焦点
* `EN_MAXTEXT`：用户试图输入超过最大长度的文字

```cpp
case ID_EDIT_USERNAME:
    switch (HIWORD(wParam))
    {
    case EN_CHANGE:
        // 内容变化了，可以在这里做实时验证
        break;

    case EN_SETFOCUS:
        // 获得焦点，比如显示提示信息
        break;

    case EN_KILLFOCUS:
        // 失去焦点，比如做格式验证
        break;
    }
    break;
```

⚠️ 注意

`EN_CHANGE` 通知在编辑框内容每次变化时都会触发，包括你用 `WM_SETTEXT` 设置文字的时候。如果你在 `EN_CHANGE` 处理里又去设置编辑框的内容，可能会陷入无限循环。一个常见的做法是用一个标志位来避免递归：

```cpp
static BOOL isUpdating = FALSE;

case ID_EDIT_SOME:
    if (HIWORD(wParam) == EN_CHANGE && !isUpdating)
    {
        isUpdating = TRUE;
        // 做一些操作...
        SendMessage(hEdit, WM_SETTEXT, 0, (LPARAM)newText);
        isUpdating = FALSE;
    }
    break;
```

### 菜单与控件的区分

`WM_COMMAND` 不仅用于控件，还用于菜单。当用户点击菜单项时，你也会收到 `WM_COMMAND` 消息。那怎么区分消息是来自控件还是菜单呢？答案是看 `lParam`：

```cpp
case WM_COMMAND:
    if (lParam == 0)
    {
        // 来自菜单、加速键或系统命令
        switch (LOWORD(wParam))
        {
        case ID_FILE_NEW:
            // 新建文件
            break;
        }
    }
    else
    {
        // 来自控件，lParam 是控件句柄
        HWND hControl = (HWND)lParam;
        // ...
    }
    break;
```

如果你需要更精确地区分，可以检查 `HIWORD(wParam)` 的值。菜单的通知码通常是 0 或 1，而控件的通知码有各种不同的值。

### 向控件发送消息

我们已经见过用 `SendMessage` 向控件发送消息来获取或设置内容。除了 `WM_GETTEXT` 和 `WM_SETTEXT`，每个控件都有自己的专用消息。按钮控件的消息以 `BM_` 开头，编辑框以 `EM_` 开头，列表框以 `LB_` 开头，组合框以 `CB_` 开头。

比如要让编辑框获得焦点，可以发送 `EM_SETSEL` 消息并选择一个范围：

```cpp
// 设置焦点到编辑框并选中所有文字
SetFocus(hEdit);
SendMessage(hEdit, EM_SETSEL, 0, -1);
```

再比如清空编辑框：

```cpp
SendMessage(hEdit, WM_SETTEXT, 0, (LPARAM)L"");
```

或者用专门的 `EM_SETSEL` 把选择范围设为 0 到 0，然后替换：

```cpp
SendMessage(hEdit, EM_SETSEL, 0, 0);
SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)L"");
```

这两种方法都能清空编辑框，第一种更简洁。

---

## 第五步——列表框与组合框

列表框和组合框都是用来显示一个列表供用户选择的控件。列表框是一目了然的选项列表，组合框则是编辑框和列表框的结合体——平时看起来像个编辑框，点击后会弹出一个下拉列表。

### 列表框基础

创建列表框用的是 `LISTBOX` 窗口类：

```cpp
HWND hList = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
    WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_USETABSTOPS,
    10, 10, 200, 150,
    hwnd, (HMENU)ID_LIST_CHOICES, hInstance, NULL);
```

这里的 `LBS_NOTIFY` 样式很重要，它让列表框在用户选择改变时向父窗口发送通知消息。如果不加这个，你收不到 `LBN_SELCHANGE` 通知。`WS_VSCROLL` 在选项太多时显示垂直滚动条。

列表框创建后是空的，你需要往里面添加选项。添加选项用的是 `LB_ADDSTRING` 消息：

```cpp
// 添加选项
SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"选项一");
SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"选项二");
SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)L"选项三");
```

如果你需要在指定位置插入选项，用 `LB_INSERTSTRING`：

```cpp
// 在索引 1 的位置插入（索引从 0 开始）
SendMessage(hList, LB_INSERTSTRING, 1, (LPARAM)L"插入的选项");
```

删除选项用 `LB_DELETESTRING`，清空所有选项用 `LB_RESETCONTENT`：

```cpp
// 删除索引 2 的选项
SendMessage(hList, LB_DELETESTRING, 2, 0);

// 清空所有选项
SendMessage(hList, LB_RESETCONTENT, 0, 0);
```

### 获取列表框的选择

获取用户当前选中的选项，需要先获取选中索引，再获取对应的文字：

```cpp
// 获取当前选中项的索引（-1 表示没有选中）
int selectedIndex = SendMessage(hList, LB_GETCURSEL, 0, 0);

if (selectedIndex != LB_ERR)
{
    // 获取选项文字的长度
    int len = SendMessage(hList, LB_GETTEXTLEN, selectedIndex, 0);

    // 分配缓冲区并获取文字
    wchar_t* buffer = new wchar_t[len + 1];
    SendMessage(hList, LB_GETTEXT, selectedIndex, (LPARAM)buffer);

    // 使用 buffer...
    delete[] buffer;
}
```

`LB_GETCURSEL` 返回当前选中项的索引。如果没有选中项（或者列表框是多选模式），返回可能是 `LB_ERR`（-1）。多选列表框用 `LBS_EXTENDEDSEL` 样式创建，获取选择需要用 `LB_GETSELITEMS` 消息，这里就不展开了。

### 列表框的通知码

列表框最常用的通知码是 `LBN_SELCHANGE`，表示用户的选择改变了：

```cpp
case ID_LIST_CHOICES:
    if (HIWORD(wParam) == LBN_SELCHANGE)
    {
        // 用户改变了选择，获取新的选择
        HWND hList = (HWND)lParam;
        int index = SendMessage(hList, LB_GETCURSEL, 0, 0);
        // ...
    }
    break;
```

还有 `LBN_DBLCLK` 通知码，表示用户双击了某个选项。但要在创建列表框时加上 `LBS_NOTIFY` 样式才能收到这个通知。

### 组合框基础

组合框（ComboBox）是编辑框和列表框的结合体，平时显示一个编辑框（显示当前选择），点击后会展开一个列表。创建组合框用的是 `COMBOBOX` 窗口类：

```cpp
HWND hCombo = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",
    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
    10, 10, 200, 200,
    hwnd, (HMENU)ID_COMBO_STYLE, hInstance, NULL);
```

组合框有三种主要样式：

* `CBS_DROPDOWN`：可编辑的组合框，用户可以输入文字也可以从列表选择
* `CBS_DROPDOWNLIST`：只读组合框，只能从列表选择，不能自己输入
* `CBS_SIMPLE`：列表始终显示的组合框，不太常用

这里用的是 `CBS_DROPDOWNLIST`，最接近我们平时理解的"下拉选择框"。

### 操作组合框

组合框的列表部分操作和列表框类似，但消息前缀是 `CB_`：

```cpp
// 添加选项
SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"小号字体");
SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"中号字体");
SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"大号字体");

// 设置默认选中第一项
SendMessage(hCombo, CB_SETCURSEL, 0, 0);

// 获取当前选中项
int selectedIndex = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
```

获取选项文字的方式和列表框一样：

```cpp
int len = SendMessage(hCombo, CB_GETLBTEXTLEN, selectedIndex, 0);
wchar_t* buffer = new wchar_t[len + 1];
SendMessage(hCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)buffer);
// 使用 buffer...
delete[] buffer;
```

### 组合框的通知码

组合框的通知码也是 `WM_COMMAND`，常见的有：

* `CBN_SELCHANGE`：用户选择了不同的选项
* `CBN_EDITCHANGE`：用户修改了编辑框里的文字（仅 `CBS_DROPDOWN`）
* `CBN_DROPDOWN`：列表即将展开
* `CBN_CLOSEUP`：列表即将收起

```cpp
case ID_COMBO_STYLE:
    switch (HIWORD(wParam))
    {
    case CBN_SELCHANGE:
    {
        HWND hCombo = (HWND)lParam;
        int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        // 根据选择做相应的操作
        break;
    }
    case CBN_EDITCHANGE:
        // 用户修改了文字
        break;
    }
    break;
```

⚠️ 注意

`CBN_SELCHANGE` 通知在用户选择改变时会触发，但如果你用代码调用 `CB_SETCURSEL` 改变选择，也会触发这个通知。如果你只想响应用户操作，可能需要加一个标志位来区分。

---

## 第六步——实战：登录对话框

好了，理论讲得差不多了，现在我们来做点有实际意义的东西。我们来写一个完整的登录对话框，把今天学到的知识都串起来。这个登录框会有用户名输入框、密码输入框、记住密码复选框、登录和取消按钮。

### 界面布局

我们先规划一下控件的布局：

```
+----------------------------------+
| 用户名：[__________]             |
| 密　码：[__________]             |
|          [√] 记住密码            |
|                                   |
|          [登录]  [取消]           |
+----------------------------------+
```

坐标大概是这样（假设对话框大小是 260×140）：

| 控件 | X | Y | 宽度 | 高度 |
|------|---|---|------|------|
| 用户名标签 | 10 | 10 | 60 | 20 |
| 用户名输入框 | 80 | 10 | 150 | 20 |
| 密码标签 | 10 | 40 | 60 | 20 |
| 密码输入框 | 80 | 40 | 150 | 20 |
| 记住密码复选框 | 80 | 70 | 100 | 20 |
| 登录按钮 | 80 | 100 | 80 | 25 |
| 取消按钮 | 170 | 100 | 80 | 25 |

### 控件 ID 定义

首先定义控件 ID 和命令 ID：

```cpp
// 控件 ID
#define ID_EDIT_USERNAME    1001
#define ID_EDIT_PASSWORD    1002
#define ID_CHECK_REMEMBER   1003
#define ID_BTN_LOGIN        1004
#define ID_BTN_CANCEL       1005

// 命令 ID（用于 WM_COMMAND 处理）
#define IDM_LOGIN           2001
#define IDM_CANCEL          2002
```

### 创建控件

我们在 `WM_CREATE` 消息处理里创建所有控件：

```cpp
case WM_CREATE:
{
    // 创建用户名标签（用静态文本）
    CreateWindowEx(0, L"STATIC", L"用户名：",
        WS_CHILD | WS_VISIBLE,
        10, 10, 60, 20,
        hwnd, NULL, hInst, NULL);

    // 创建用户名输入框
    HWND hEditUser = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        80, 10, 150, 20,
        hwnd, (HMENU)ID_EDIT_USERNAME, hInst, NULL);

    // 创建密码标签
    CreateWindowEx(0, L"STATIC", L"密　码：",
        WS_CHILD | WS_VISIBLE,
        10, 40, 60, 20,
        hwnd, NULL, hInst, NULL);

    // 创建密码输入框
    HWND hEditPass = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_PASSWORD | ES_AUTOHSCROLL,
        80, 40, 150, 20,
        hwnd, (HMENU)ID_EDIT_PASSWORD, hInst, NULL);

    // 创建记住密码复选框
    CreateWindowEx(0, L"BUTTON", L"记住密码",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        80, 70, 100, 20,
        hwnd, (HMENU)ID_CHECK_REMEMBER, hInst, NULL);

    // 创建登录按钮
    CreateWindowEx(0, L"BUTTON", L"登录",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        80, 100, 80, 25,
        hwnd, (HMENU)ID_BTN_LOGIN, hInst, NULL);

    // 创建取消按钮
    CreateWindowEx(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        170, 100, 80, 25,
        hwnd, (HMENU)ID_BTN_CANCEL, hInst, NULL);

    return 0;
}
```

### 处理控件消息

接下来是 `WM_COMMAND` 消息处理，这里我们需要处理按钮点击和可能的输入验证：

```cpp
case WM_COMMAND:
{
    WORD controlId = LOWORD(wParam);
    WORD notificationCode = HIWORD(wParam);

    switch (controlId)
    {
    case ID_BTN_LOGIN:
        if (notificationCode == BN_CLICKED)
        {
            // 获取用户名和密码
            HWND hEditUser = GetDlgItem(hwnd, ID_EDIT_USERNAME);
            HWND hEditPass = GetDlgItem(hwnd, ID_EDIT_PASSWORD);

            int userLen = SendMessage(hEditUser, WM_GETTEXTLENGTH, 0, 0);
            int passLen = SendMessage(hEditPass, WM_GETTEXTLENGTH, 0, 0);

            if (userLen == 0 || passLen == 0)
            {
                MessageBox(hwnd, L"用户名和密码不能为空！", L"登录失败",
                    MB_OK | MB_ICONWARNING);
                return 0;
            }

            wchar_t* username = new wchar_t[userLen + 1];
            wchar_t* password = new wchar_t[passLen + 1];

            SendMessage(hEditUser, WM_GETTEXT, userLen + 1, (LPARAM)username);
            SendMessage(hEditPass, WM_GETTEXT, passLen + 1, (LPARAM)password);

            // 检查是否记住密码
            HWND hCheckRemember = GetDlgItem(hwnd, ID_CHECK_REMEMBER);
            BOOL remember = SendMessage(hCheckRemember, BM_GETCHECK, 0, 0) == BST_CHECKED;

            // 这里应该验证用户名和密码
            // 为了演示，我们只是显示一下
            wchar_t msg[256];
            swprintf_s(msg, 256, L"登录信息\n用户名：%s\n密码：%s\n记住密码：%s",
                username, password, remember ? L"是" : L"否");
            MessageBox(hwnd, msg, L"登录信息", MB_OK);

            delete[] username;
            delete[] password;

            // 登录成功，关闭窗口
            DestroyWindow(hwnd);
        }
        break;

    case ID_BTN_CANCEL:
        if (notificationCode == BN_CLICKED)
        {
            // 用户取消，直接关闭窗口
            DestroyWindow(hwnd);
        }
        break;

    case ID_EDIT_PASSWORD:
        if (notificationCode == EN_SETFOCUS)
        {
            // 密码框获得焦点，全选内容方便修改
            HWND hEditPass = (HWND)lParam;
            SendMessage(hEditPass, EM_SETSEL, 0, -1);
        }
        break;
    }
    break;
}
```

这里用到了 `GetDlgItem` 函数，它可以根据控件 ID 获取控件句柄。这个函数在你知道控件 ID 但没有保存句柄的时候特别有用。

### 完整代码

为了让你能够完整运行这个例子，我把窗口过程的完整代码写出来。假设你的程序入口点（`WinMain`）和前面文章里的一样，只是注册窗口类名换成了 `LOGIN_DIALOG_CLASS`：

```cpp
#define ID_EDIT_USERNAME    1001
#define ID_EDIT_PASSWORD    1002
#define ID_CHECK_REMEMBER   1003
#define ID_BTN_LOGIN        1004
#define ID_BTN_CANCEL       1005

LRESULT CALLBACK LoginDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // 静态文本：用户名
        CreateWindowEx(0, L"STATIC", L"用户名：",
            WS_CHILD | WS_VISIBLE,
            10, 10, 60, 20,
            hwnd, NULL, hInst, NULL);

        // 编辑框：用户名
        CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
            80, 10, 150, 20,
            hwnd, (HMENU)ID_EDIT_USERNAME, hInst, NULL);

        // 静态文本：密码
        CreateWindowEx(0, L"STATIC", L"密　码：",
            WS_CHILD | WS_VISIBLE,
            10, 40, 60, 20,
            hwnd, NULL, hInst, NULL);

        // 编辑框：密码
        CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_PASSWORD | ES_AUTOHSCROLL,
            80, 40, 150, 20,
            hwnd, (HMENU)ID_EDIT_PASSWORD, hInst, NULL);

        // 复选框：记住密码
        CreateWindowEx(0, L"BUTTON", L"记住密码",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            80, 70, 100, 20,
            hwnd, (HMENU)ID_CHECK_REMEMBER, hInst, NULL);

        // 按钮：登录
        CreateWindowEx(0, L"BUTTON", L"登录",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            80, 100, 80, 25,
            hwnd, (HMENU)ID_BTN_LOGIN, hInst, NULL);

        // 按钮：取消
        CreateWindowEx(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            170, 100, 80, 25,
            hwnd, (HMENU)ID_BTN_CANCEL, hInst, NULL);

        return 0;
    }

    case WM_COMMAND:
    {
        WORD controlId = LOWORD(wParam);
        WORD notificationCode = HIWORD(wParam);

        switch (controlId)
        {
        case ID_BTN_LOGIN:
            if (notificationCode == BN_CLICKED)
            {
                HWND hEditUser = GetDlgItem(hwnd, ID_EDIT_USERNAME);
                HWND hEditPass = GetDlgItem(hwnd, ID_EDIT_PASSWORD);

                int userLen = SendMessage(hEditUser, WM_GETTEXTLENGTH, 0, 0);
                int passLen = SendMessage(hEditPass, WM_GETTEXTLENGTH, 0, 0);

                if (userLen == 0 || passLen == 0)
                {
                    MessageBox(hwnd, L"用户名和密码不能为空！", L"登录失败",
                        MB_OK | MB_ICONWARNING);
                    return 0;
                }

                wchar_t* username = new wchar_t[userLen + 1];
                wchar_t* password = new wchar_t[passLen + 1];

                SendMessage(hEditUser, WM_GETTEXT, userLen + 1, (LPARAM)username);
                SendMessage(hEditPass, WM_GETTEXT, passLen + 1, (LPARAM)password);

                HWND hCheckRemember = GetDlgItem(hwnd, ID_CHECK_REMEMBER);
                BOOL remember = SendMessage(hCheckRemember, BM_GETCHECK, 0, 0) == BST_CHECKED;

                wchar_t msg[256];
                swprintf_s(msg, 256, L"登录信息\n用户名：%s\n密码：%s\n记住密码：%s",
                    username, password, remember ? L"是" : L"否");
                MessageBox(hwnd, msg, L"登录信息", MB_OK);

                delete[] username;
                delete[] password;

                DestroyWindow(hwnd);
            }
            break;

        case ID_BTN_CANCEL:
            if (notificationCode == BN_CLICKED)
            {
                DestroyWindow(hwnd);
            }
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
```

编译运行这个程序，你应该能看到一个完整的登录对话框。输入用户名和密码，点击登录按钮，程序会弹出一个消息框显示你输入的信息。如果用户名或密码为空，会有警告提示。

---

## 后续可以做什么

到这里，Win32 标准控件的基础知识就讲完了。你现在应该能够创建包含按钮、输入框、列表框的完整界面了。但 Win32 的控件世界远不止这些，还有很多高级控件和机制等着我们去探索。

下一篇文章，我们会聊一聊 **WM_NOTIFY 机制和更复杂的控件**——比如列表视图（ListView）、树视图（TreeView）、标签页（Tab Control）这些。这些控件不再是简单的 `WM_COMMAND` 就能搞定的，它们需要更复杂的 `WM_NOTIFY` 通知系统。而且，我们还会学习如何用资源文件来定义界面，这样你就不用在代码里硬编码一堆坐标了。

在此之前，建议你先把今天的内容消化一下。试着做一些小练习，巩固一下知识：

1. 写一个简单的计算器界面，包含数字按钮、运算符按钮和显示结果的编辑框
2. 做一个"选项设置"对话框，包含各种类型的控件
3. 实现一个下拉字体选择器，选择不同字体后，编辑框里的文字用对应字体显示

这些练习看似简单，但能帮你把今天学到的知识真正变成自己的东西。好了，今天的文章就到这里，我们下一篇再见！

---

**相关资源**

- [按钮控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/button-styles)
- [编辑框控件 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/about-edit-controls)
- [列表框 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/list-boxes)
- [组合框 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/controls/combo-boxes)
- [WM_COMMAND 消息 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/wm-command)
