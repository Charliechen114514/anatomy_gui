# 编程GUI到底什么是DPI —— Windows和Qt双平台实战指南

## 前言：为什么我们非得折腾这个

说实话，如果你今年还在做GUI开发却没考虑过DPI问题，那你迟早要踩坑。笔者之前工作的时候，就出现了，换一个显示器，窗口不居中跑到边缘的问题。这事情真的不能怪显示器，纯粹是我们当年写代码的时候太"天真"了，默认世界上所有屏幕都是96 DPI。

事情的起因是这样的：显示器厂商这些年在像素密度上卷得厉害，以前的屏幕大概每英寸96个像素就算标准配置，现在动不动就200多甚至300 DPI。但我们的GUI代码还停留在旧时代，硬编码了一堆"看起来差不多"的尺寸值，结果在高分屏上要么小到看不见，要么被系统拉伸成一团模糊。

如果你跟我一样，需要维护一些"历史悠久"的桌面应用，或者打算从零开始写一个能在各种屏幕上都清晰显示的程序，那我们就得认真对待DPI这个问题了。这篇文章会带你完整走一遍Windows和Qt两个平台的DPI处理方案，把那些官方文档里分散在各种角落的知识点串起来，顺便分享一些笔者踩过的坑。

## 环境说明：我们在什么平台上折腾

在开始之前，先明确一下我们这篇文章涉及的技术栈和测试环境：

- **Windows平台**：主要关注Win10 1703（Creators Update）及以上版本，因为这是Per-Monitor V2 DPI Awareness支持的起点
- **Qt版本**：Qt 6.x系列，默认就是Per-Monitor DPI Aware V2（是的，笔者业余的时候很想写这个）
- **测试硬件**：至少包含两个不同DPI的显示器（比如一个1080p的96 DPI，一个4K的192 DPI）
- **开发场景**：既有从零开始的新项目，也有需要改造的老项目

## 第一步 —— 搞清楚DPI到底是个什么鬼

很多人一听到DPI就觉得是"图片清晰度"相关的东西，但在GUI编程的语境下，我们需要更准确地理解它。DPI（Dots Per Inch）指的是每英寸的像素点数，96 DPI意味着屏幕上每英寸有96个像素。这是一个物理密度概念。

但真正让我们头疼的是"显示缩放比例"（Display Scale Factor）。当你在Windows的显示设置里把缩放调成150%的时候，系统会告诉应用程序"现在的DPI是144"（96 × 1.5），然后应用程序需要据此调整自己的布局和渲染。

这里有个关键点需要理解：**设备像素比**（Device Pixel Ratio，简称DPR）。Qt的文档把它解释得很清楚 —— 如果你的窗口设置为200×200"设备无关像素"，在DPI为96（DPR=1.0）的屏幕上它实际占用200×200个物理像素，但在DPI为192（DPR=2.0）的屏幕上它会占用400×400个物理像素。

这个模型的核心思想是：应用程序在一个"虚拟"的坐标系里工作，系统负责把虚拟坐标映射到真实的物理像素上。理想情况下，你只需要用"设备无关像素"来思考布局，剩下的事情让框架帮你处理。

当然，理想很丰满，现实往往很骨感 —— 尤其是当你需要处理一些底层绘图操作的时候。

## 第二步 —— Windows的DPI Awareness模式

Windows这块的设计历史包袱比较重，所以有好几种DPI Awareness模式，我们得一个个搞清楚。

### DPI Unaware：最原始的模式

DPI Unaware的应用程序基本上就是在告诉Windows："我就按96 DPI来写，别的我不管"。当这种程序跑在高DPI屏幕上时，Windows会把整个窗口的位图拉伸到预期大小，结果就是你看到的模糊界面。

说实话，2025年了如果你还在写这种程序，那真的有点说不过去。但有些legacy代码确实是这样的，我们后面会讲怎么改造。

### System DPI Awareness：稍微进步了一点

System DPI Aware的应用程序会在启动时获取主显示器的DPI，然后按照这个DPI来布局界面。听起来不错，但问题在于它只会在初始化时做一次这件事。当你把窗口拖到另一个DPI不同的显示器上时，Windows又会开始位图拉伸，于是你的界面又模糊了。

很多老程序都属于这一类，它们是"一个DPI用到死"的典型代表。

### Per-Monitor和Per-Monitor V2：正确的方向

Per-Monitor DPI Awareness是Windows 8.1引入的，它的核心思想是：当DPI变化时，系统会发送`WM_DPICHANGED`消息给你的窗口，然后你需要自己重新布局。

但Per-Monitor V1的限制比较多，基本就是个"半成品"。Windows 10 Creators Update引入了Per-Monitor V2，这个才是真正可用的版本。当你的程序声明自己是PMv2 Aware时，你会得到这些好处：

1. 窗口（包括子窗口）会在DPI变化时收到通知
2. 你能看到每个显示器的真实像素数
3. Windows不会对你的窗口进行位图拉伸
4. 非客户区（标题栏、滚动条等）会自动缩放
5. Win32对话框会自动处理DPI缩放
6. 通用控件的位图资源会自动用正确的DPI渲染

当然，天下没有免费的午餐 —— 既然你告诉Windows"我自己来处理DPI"，那当DPI变化时，如果你没有重新布局，你的界面就会变得要么太小要么太大，完全取决于新旧DPI的差异。

下面这个表格总结了各种模式的行为差异：

| DPI Awareness模式 | Windows引入版本 | 应用程序看到的DPI | DPI变化时的行为 |
|-------------------|----------------|-------------------|-----------------|
| Unaware | - | 所有显示器都是96 DPI | 位图拉伸（模糊） |
| System | Vista | 所有显示器都是主显示器的DPI | 位图拉伸（模糊） |
| Per-Monitor | 8.1 | 当前窗口所在显示器的DPI | 顶级窗口收到通知，其他不管 |
| Per-Monitor V2 | Win10 1703 | 当前窗口所在显示器的DPI | 顶级和子窗口都收到通知，非客户区、控件、对话框自动缩放 |

## 第三步 —— Qt是怎么处理这个问题的

Qt这块的设计就清爽多了，它提供了一个统一的API来屏蔽平台差异。Qt 6默认就是Per-Monitor DPI Aware V2，你不需要额外做什么就能获得基本的DPI支持。

Qt的坐标系统模型是这样的：应用程序使用"设备无关像素"，然后通过设备像素比映射到物理像素。当你创建一个200×200的QWindow或QWidget时：

- 在DPR为1.0的标准屏幕上，它占用200×200物理像素
- 在DPR为2.0的高密度屏幕上，它占用400×400物理像素

这个模型适用于Qt GUI的大部分高级API，包括Widget和Quick的几何尺寸、事件坐标、桌面和窗口几何等。

### 绘图相关的事情

如果你使用QPainter或者Qt Quick的图形绘制，Qt会自动处理高DPI渲染。你可以在统一的坐标系里工作，不需要关心显示器的实际像素密度。

但是当你用低级API（比如OpenGL）时，你就得自己处理设备像素比了。Qt提供了`QWindow::devicePixelRatio()`和`QScreen::devicePixelRatio()`来获取这个值，你需要据此调整你的渲染逻辑。

这里有个容易踩坑的地方：QImage和QPixmap代表的是原始像素，不在设备无关坐标系里。一个400×400的QImage在DPR为2.0的屏幕上只会填满200×200的窗口空间，或者如果在标准屏幕上绘制会被自动缩小到200×200。

### 图像资源怎么处理

要充分利用高DPI显示器的优势，你需要提供高分辨率的图像资源。Qt使用一个简单的命名约定：`logo@2x.png`表示2x分辨率的版本。当你把普通版本和高分辨率版本都加载到QIcon里时，Qt会自动选择最合适的版本。

具体的做法我们在后面实战部分会详细演示。

## 第四步 —— Win32实战：从System DPI Aware升级到Per-Monitor V2

假设我们有一个老的Win32程序，目前是System DPI Aware的（大部分老程序都是这样），我们想把它升级到Per-Monitor V2。这里有一系列步骤需要完成。

### 4.1 声明DPI Awareness模式

首先我们需要在application manifest里声明程序是Per-Monitor V2 DPI Aware。如果你的程序还没有manifest，那就需要创建一个；如果已经有了，就添加dpiAwareness相关的内容：

```xml
<application xmlns="urn:schemas-microsoft-com:asm.v3">
  <windowsSettings>
    <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/PM</dpiAware>
    <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
  </windowsSettings>
</application>
```

这个声明告诉Windows："嘿，我知道怎么处理DPI变化，别帮我拉伸位图了"。

### 4.2 找出所有硬编码的尺寸

这步是最烦人的，但也是必须的。你需要在代码里搜索所有假设DPI固定为96的地方。常见的罪魁祸首包括：

- 直接写在代码里的像素值（比如`CreateWindow`时的x, y, width, height）
- 缓存的字体大小
- 缓存的DPI值
- 硬编码的图标尺寸

这里有个典型的错误示例：

```cpp
case WM_CREATE:
{
    // 这段代码假设程序运行在96 DPI下
    HWND hWndChild = CreateWindow(L"BUTTON", L"Click Me",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        50,   // x
        50,   // y
        100,  // width
        50,   // height
        hWnd, (HMENU)NULL, NULL, NULL);
}
```

按钮的位置和尺寸都是硬编码的，在高DPI屏幕上会显得很小。

正确的做法是先定义一些"96 DPI下的基准值"，然后在运行时根据实际DPI进行缩放：

```cpp
#define INITIALX_96DPI 50
#define INITIALY_96DPI 50
#define INITIALWIDTH_96DPI 100
#define INITIALHEIGHT_96DPI 50

void UpdateButtonLayoutForDpi(HWND hWnd)
{
    int iDpi = GetDpiForWindow(hWnd);
    int dpiScaledX = MulDiv(INITIALX_96DPI, iDpi, USER_DEFAULT_SCREEN_DPI);
    int dpiScaledY = MulDiv(INITIALY_96DPI, iDpi, USER_DEFAULT_SCREEN_DPI);
    int dpiScaledWidth = MulDiv(INITIALWIDTH_96DPI, iDpi, USER_DEFAULT_SCREEN_DPI);
    int dpiScaledHeight = MulDiv(INITIALHEIGHT_96DPI, iDpi, USER_DEFAULT_SCREEN_DPI);
    SetWindowPos(hWnd, hWnd, dpiScaledX, dpiScaledY,
                 dpiScaledWidth, dpiScaledHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}
```

`MulDiv`这个API很有用，它执行`(a * b) / c`的操作，并且会处理中间结果的溢出问题。这里我们用它来把96 DPI下的基准值按比例缩放到实际DPI。

然后在窗口创建时调用这个函数：

```cpp
case WM_CREATE:
{
    HWND hWndChild = CreateWindow(L"BUTTON", L"Click Me",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0, 0, 0, 0,  // 尺寸由UpdateButtonLayoutForDpi设置
        hWnd, (HMENU)NULL, NULL, NULL);
    if (hWndChild != NULL)
    {
        UpdateButtonLayoutForDpi(hWndChild);
    }
}
break;
```

### 4.3 处理DPI变化消息

当我们声明为Per-Monitor DPI Aware后，当窗口被拖到不同DPI的显示器上时，会收到`WM_DPICHANGED`消息。我们需要处理这个消息，重新布局界面：

```cpp
case WM_DPICHANGED:
{
    // 找到需要调整大小的控件并更新布局
    HWND hWndButton = FindWindowEx(hWnd, NULL, NULL, NULL);
    if (hWndButton != NULL)
    {
        UpdateButtonLayoutForDpi(hWndButton);
    }
}
break;
```

⚠️ 注意：这里有个重要的坑 —— `WM_DPICHANGED`消息的`lParam`参数包含一个建议的新窗口矩形。Windows强烈建议你用这个矩形来调整窗口大小，因为这样能确保：

1. 鼠标指针在拖动窗口时保持在相对位置
2. 避免触发递归的DPI变化循环

更完整的处理应该是这样的：

```cpp
case WM_DPICHANGED:
{
    // 获取建议的窗口矩形
    LPRECT lpRect = (LPRECT)lParam;

    // 调整窗口大小
    SetWindowPos(hWnd, NULL,
                 lpRect->left, lpRect->top,
                 lpRect->right - lpRect->left,
                 lpRect->bottom - lpRect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // 更新子窗口布局
    UpdateAllChildLayouts(hWnd);
}
break;
```

### 4.4 替换不支持DPI上下文的API

很多老版本的Win32 API没有DPI或HWND上下文参数，它们返回的值可能是"虚拟化"的（即被系统按照某种DPI Awareness模式转换过）。Windows提供了带`ForDpi`后缀的新版API：

| 老API | 新API |
|-------|-------|
| GetSystemMetrics | GetSystemMetricsForDpi |
| AdjustWindowRectEx | AdjustWindowRectExForDpi |
| SystemParametersInfo | SystemParametersInfoForDpi |
| GetDpiForMonitor | GetDpiForWindow |

你需要在代码里搜索这些老API的调用，并根据具体情况替换。如果老API是在某个HWND相关的上下文中调用的，通常可以改用`GetDpiForWindow`获取DPI后再调用新API。

### 4.5 处理位图资源

当DPI变化时，你需要重新加载位图资源。如果你只提供了单分辨率的图片，可能需要手动缩放。更好的做法是提供多分辨率的资源，然后根据当前DPI选择合适的版本。

对于图标，可以使用`LoadImage`代替`LoadIcon`，因为它支持指定尺寸：

```cpp
HICON hIcon = (HICON)LoadImage(
    hInstance,
    MAKEINTRESOURCE(IDI_MYICON),
    IMAGE_ICON,
    GetSystemMetricsForDpi(SM_CXICON, GetDpiForWindow(hWnd)),
    GetSystemMetricsForDpi(SM_CYICON, GetDpiForWindow(hWnd)),
    LR_DEFAULTCOLOR
);
```

### 4.6 测试清单

改造完成后，你需要测试以下场景：

1. 在不同DPI的显示器上启动程序
2. 把窗口在不同DPI的显示器之间拖动
3. 程序运行时更改显示器的缩放设置
4. 更改主显示器，注销后重新登录测试
5. 远程桌面连接到不同DPI的机器

如果所有场景下界面都能保持清晰且尺寸合理，恭喜你，你的程序已经正确支持DPI缩放了。

## 第五步 —— Qt实战：从入门到正确

Qt的DPI处理比Win32简单很多，但仍有一些需要注意的地方。

### 5.1 确认Qt的DPI配置

Qt 6默认就是Per-Monitor DPI Aware V2，你通常不需要额外配置。但如果你想确认或修改这个行为，可以在`qt.conf`里设置：

```ini
[Platforms]
WindowsArguments = dpiawareness=0,1,2
```

这里的数字对应不同的DPI Awareness级别：0是Unaware，1是System，2是Per-Monitor。默认是2。

### 5.2 使用设备无关坐标系统

在Qt中，你用到的绝大部分坐标和尺寸都是设备无关的。比如：

```cpp
// 创建一个200x200的窗口
QWidget *window = new QWidget;
window->resize(200, 200);
```

无论在什么DPI的屏幕上，这个窗口的"视觉大小"都是差不多的，只是实际占用的物理像素不同。这就是Qt的坐标系统在起作用。

### 5.3 处理图像资源

Qt使用`@2x`的命名约定来标记高分辨率版本：

```
logo.png       // 1x版本
logo@2x.png    // 2x版本
logo@3x.png    // 3x版本（如果需要的话）
```

加载时使用QIcon：

```cpp
QIcon icon;
icon.addFile(":/images/logo.png");       // 自动加载所有匹配的@Nx版本
icon.addFile(":/images/logo@2x.png");

// 之后使用时Qt会自动选择合适的版本
button->setIcon(icon);
```

这里有个小提示：如果你使用Qt资源系统，需要把所有分辨率的文件都添加到资源文件里。Qt会根据文件名自动选择。

### 5.4 处理低级绘图

如果你使用OpenGL或其他低级绘图API，需要手动处理设备像素比：

```cpp
// 获取当前窗口的设备像素比
qreal dpr = window->devicePixelRatio();

// 调整视口大小
glViewport(0, 0, width * dpr, height * dpr);
```

对于QPainter绘制的图像，也需要注意：

```cpp
QPainter painter(this);
QImage image(":/image.png");

// 如果图像是设备像素（比如从文件加载的），需要正确设置设备像素比
image.setDevicePixelRatio(devicePixelRatio());

painter.drawImage(targetRect, image);
```

### 5.5 测试环境变量

Qt提供了一些环境变量来帮助测试DPI支持，不用真的去买多个显示器：

- `QT_SCALE_FACTOR`：设置全局缩放因子，比如`QT_SCALE_FACTOR=2 ./myapp`会让所有几何尺寸翻倍
- `QT_ENABLE_HIGHDPI_SCALING=0`：禁用高DPI缩放，回退到Qt 5的默认行为（仅用于测试）
- `QT_AUTO_SCREEN_SCALE_FACTOR`：Qt 5遗留的环境变量，Qt 6不需要

测试时可以这样：

```bash
# 模拟2x缩放
QT_SCALE_FACTOR=2 ./myapp

# 模拟1.5x缩放
QT_SCALE_FACTOR=1.5 ./myapp
```

## 第六步 —— 踩坑预警与进阶话题

这里整理一些笔者在折腾过程中遇到的坑点和进阶问题。

### DPI虚拟化的陷阱

当一个DPI Unaware或System DPI Aware的窗口被Windows拉伸时，某些API的返回值会被"虚拟化" —— 即返回的是转换后的值，而不是真实值。这可能导致混淆。

比如，一个DPI Unaware的线程在4K屏幕上查询屏幕尺寸时，Windows会返回一个"虚拟"的尺寸（好像屏幕只有96 DPI一样）。这本身没问题，但如果你混用不同DPI Awareness的代码，可能会踩坑。

解决办法：确保调用API时的线程DPI上下文是你期望的。如果需要临时改变上下文，使用`SetThreadDpiAwarenessContext`，记得恢复：

```cpp
DPI_AWARENESS_CONTEXT oldContext = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
// ... 调用需要特定上下文的API ...
SetThreadDpiAwarenessContext(oldContext);
```

### 混合模式DPI缩放

当你没法一次性更新所有窗口时，可以让部分窗口保持旧的DPI模式，只更新重要的窗口。从Windows 10 1607开始，DPI Awareness可以按顶级窗口设置：

```cpp
// 创建一个使用System DPI的窗口
SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
HWND hSecondaryWindow = CreateWindow(...);
SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

// 主窗口使用Per-Monitor V2
HWND hMainWindow = CreateWindow(...);
```

这个功能要慎用，因为它会增加程序复杂度。只应该在过渡期使用，最终目标还是让所有窗口都正确处理DPI。

### 虚拟桌面的"孤岛"问题

在Windows上，如果你有多个不同DPI的显示器，Qt报告的屏幕几何可能会出现"空隙"。这是因为Windows的屏幕布局是用物理像素定位的，而Qt会把屏幕尺寸缩小（按DPR缩放），但位置不变。

这会导致某些位置"不属于任何屏幕"。解决办法是不要假设屏幕边缘外就是相邻屏幕，而是用`QGuiApplication::screens()`获取屏幕列表来推理布局。

### 非整数缩放的问题

Windows允许以125%、150%这样的比例缩放，这会产生非整数的DPR（1.25、1.5）。有些UI框架在这种情况下可能出现渲染问题。

Qt提供了`QGuiApplication::setHighDpiScaleFactorRoundingPolicy()`来控制缩放因子的舍入策略，可以限制为只使用整数或0.25增量的值。

### Qt的DPI配置优先级（X11）

在X11平台上，DPI配置比较复杂。Qt按以下优先级读取配置：

1. Xft/DPI（来自X设置，全局逻辑DPI）
2. Xft.dpi（来自X资源，全局逻辑DPI）
3. RandR物理DPI（仅Qt 5，从屏幕物理尺寸计算）
4. 96 DPI（回退值）

可以设置`QT_USE_PHYSICAL_DPI=1`强制使用物理DPI，但通常逻辑DPI是更好的选择。

## 测试工具推荐

### DprGadget

Qt提供了一个测试工具叫DprGadget，可以用来检查DPI相关的值：

```
qtbase/tests/manual/highdpi/dprgadget
```

它会显示：
- 窗口的设备像素比
- 屏幕的逻辑DPI
- 屏幕的设备像素比

当你拖动窗口或改变DPI时，这些值应该会自动更新，窗口大小应该保持不变。如果不是这样，可能是Qt的bug。

### Windows内置工具

Windows的设置页面可以方便地调整缩放比例来测试：

1. 打开"设置 > 系统 > 显示"
2. 修改"缩放与布局"
3. 观察程序是否正确响应

## 收尾

到这里我们就把Windows和Qt两个平台的DPI处理都梳理完了。说实话，DPI这个话题确实有点枯燥，但它对于现代GUI应用来说是绕不过去的一道坎。

如果你正在维护一个老项目，希望这篇文章能给你一个清晰的升级路线。如果你是从零开始的新项目，那就更简单了 —— 选一个现代框架（比如Qt 6），默认配置通常就能覆盖大部分场景。

最后提醒一点：DPI支持最好在项目早期就考虑好，越到后面改造成本越高。不要像笔者这样，等在4K屏幕上看到模糊界面时才想起来要处理这个事情。

完结撒花，你的GUI应用终于可以在各种屏幕上都清晰显示了！
