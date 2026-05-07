# 通用GUI编程技术——Win32 原生编程实战（五十二）——高级输入消息：触控、Raw Input 与窗口管理

> 上一篇文章我们把视野拓展到了 OpenGL——在 Win32 窗口中嵌入 GPU 渲染的另一种方式。到目前为止，我们对 Win32 的输入处理主要停留在鼠标和键盘的层面——`WM_LBUTTONDOWN`、`WM_KEYDOWN`、`WM_CHAR` 这些经典消息。但在现代 Windows 系统上，输入远不止于此：触控屏上的多点触控、高精度的游戏控制器、手写笔的压力感应，这些都需要更高级的消息来处理。此外，窗口在系统中的位置变化、显示器热插拔等场景也有对应的消息通知。今天我们就来把这些"进阶级"的系统消息一次性搞清楚。

---

## 为什么需要了解这些消息

说实话，如果你的程序只需要处理鼠标和键盘，前面那几篇文章的内容已经完全够用了。但如果你要做以下任何一件事，就需要今天的知识：

* **触控屏支持**：Windows 平板和触控笔记本越来越普遍，`WM_TOUCH` 和 `WM_POINTER` 是处理触控输入的两种方式。
* **高精度输入**：游戏手柄、绘图板、飞行摇杆——`WM_INPUT`（Raw Input）让你能拿到最原始的设备数据，绕过系统的消息翻译和合并。
* **窗口位置感知**：`WM_WINDOWPOSCHANGING` 让你在窗口位置即将改变时拦截并修改，`WM_WINDOWPOSCHANGED` 则是位置已经改变后的通知。
* **打印和截图场景**：`WM_PRINTCLIENT` 让系统或别的程序在需要你的窗口内容时，能正确获取客户区的绘制结果。
* **多显示器热插拔**：`WM_DISPLAYCHANGE` 通知你显示器分辨率或数量发生了变化。

这些消息在日常开发中不一定天天用，但一旦遇到相关需求，如果你不知道它们的存在，就会走很多弯路。

---

## 环境说明

* **平台**：Windows 10/11（部分 API 需要 Windows 8+）
* **开发工具**：Visual Studio 2019 或更高版本（Community 版本就行）
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道窗口过程怎么写、`WM_MOUSEMOVE` 和 `WM_KEYDOWN` 怎么处理。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——WM_TOUCH：多点触控

### 基本概念

`WM_TOUCH` 是 Windows 7 引入的触控消息。当用户在触控屏上按下、移动或抬起手指时，系统会发送这个消息。与 `WM_LBUTTONDOWN` 等鼠标消息不同，`WM_TOUCH` 一次消息可以包含多个触控点的信息——这就是"多点触控"的含义。

### 注册触控窗口

在接收 `WM_TOUCH` 之前，你必须调用 `RegisterTouchWindow` 告诉系统这个窗口需要触控输入：

```cpp
// 在窗口创建后调用
RegisterTouchWindow(hWnd, TWF_FINETOUCH | TWF_WANTPALM);
```

* `TWF_FINETOUCH`：请求精细触控输入（不与鼠标消息合并）。
* `TWF_WANTPALM`：请求手掌拒绝（palm rejection），系统会尽量过滤掉手掌意外触碰。

不调用这个函数，触控操作会被系统翻译成鼠标消息（`WM_LBUTTONDOWN` 等），你不会收到 `WM_TOUCH`。

### 处理 WM_TOUCH

```cpp
case WM_TOUCH: {
    // 获取输入句柄
    HTOUCHINPUT hTouchInput = (HTOUCHINPUT)lParam;
    UINT nInputs = LOWORD(wParam); // 触控点数量

    // 分配缓冲区并获取触控点数据
    std::vector<TOUCHINPUT> inputs(nInputs);
    if (GetTouchInputInfo(hTouchInput, nInputs, inputs.data(),
                          sizeof(TOUCHINPUT))) {

        for (UINT i = 0; i < nInputs; i++) {
            const TOUCHINPUT& ti = inputs[i];

            // 触控点坐标（百分之一像素为单位，需要除以 100）
            POINT pt;
            pt.x = ti.x / 100;
            pt.y = ti.y / 100;

            // 转换为窗口客户区坐标
            ScreenToClient(hWnd, &pt);

            // 判断触控事件类型
            if (ti.dwFlags & TOUCHEVENTF_DOWN) {
                // 手指按下，ti.dwID 是这个触控点的唯一标识
            }
            if (ti.dwFlags & TOUCHEVENTF_MOVE) {
                // 手指移动
            }
            if (ti.dwFlags & TOUCHEVENTF_UP) {
                // 手指抬起
            }
        }
    }

    // 必须关闭触控输入句柄
    CloseTouchInputHandle(hTouchInput);
    return 0;
}
```

### 关键要点

* **坐标单位**：`TOUCHINPUT` 的 `x` 和 `y` 是百分之一像素（centipixel），需要除以 100 才能得到像素坐标。
* **触控点 ID**：`dwID` 在一次触控序列（按下→移动→抬起）中保持不变，用来追踪同一个手指。
* **必须调用 `CloseTouchInputHandle`**：每个 `WM_TOUCH` 消息处理完后都要关闭句柄，否则系统会卡住。
* **与鼠标消息共存**：如果不调用 `RegisterTouchWindow`，触控会被翻译成鼠标消息。如果调用了，你就只收到 `WM_TOUCH`，不会同时收到鼠标消息。

---

## 第二步——WM_POINTER：统一指针模型（Windows 8+）

### 为什么有 WM_POINTER

`WM_TOUCH` 有一个设计上的限制：它只处理触控输入。如果你想让程序同时支持鼠标、触控笔和触控屏，你需要分别处理三套消息（鼠标消息、`WM_TOUCH`、`WM_TABLET` 等）。Windows 8 引入了 `WM_POINTER` 系列消息，把所有指针输入统一到了一套消息里。

### 指针消息家族

| 消息 | 触发时机 |
|------|----------|
| WM_POINTERDOWN | 指针按下（触控/笔/鼠标） |
| WM_POINTERUPDATE | 指针移动或属性变化 |
| WM_POINTERUP | 指针抬起 |
| WM_POINTERENTER | 指针进入窗口范围 |
| WM_POINTERLEAVE | 指针离开窗口范围 |
| WM_POINTERCAPTURECHANGED | 指针捕获变化 |

### 基本用法

```cpp
case WM_POINTERDOWN:
case WM_POINTERUPDATE:
case WM_POINTERUP: {
    // 从消息参数获取指针 ID
    UINT32 pointerId = GET_POINTERID_WPARAM(wParam);

    // 查询指针信息
    POINTER_INFO pointerInfo = {};
    POINTER_TOUCH_INFO touchInfo = {};
    POINTER_PEN_INFO penInfo = {};

    if (GetPointerTouchInfo(pointerId, &touchInfo)) {
        // 这是触控输入
        POINT pt = touchInfo.pointerInfo.ptPixelLocation;
        ScreenToClient(hWnd, &pt);

        // touchInfo 有触控特有信息：
        // touchInfo.touchFlags, touchInfo.rcContact 等

        // 判断触控状态
        if (touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_DOWN) {
            // 指针按下
        }
        if (touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_UPDATE) {
            // 指针更新
        }
        if (touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_UP) {
            // 指针抬起
        }
    }
    else if (GetPointerPenInfo(pointerId, &penInfo)) {
        // 这是笔输入
        POINT pt = penInfo.pointerInfo.ptPixelLocation;
        ScreenToClient(hWnd, &pt);

        // penInfo 有笔特有信息：
        // penInfo.pressure（压力，0-1024）
        // penInfo.tiltX, penInfo.tiltY（倾斜角度）
        // penInfo.rotation（旋转，0-359）
    }
    else if (GetPointerInfo(pointerId, &pointerInfo)) {
        // 鼠标或其他通用指针
        POINT pt = pointerInfo.ptPixelLocation;
        ScreenToClient(hWnd, &pt);
    }

    return 0;
}
```

### WM_POINTER vs WM_TOUCH 怎么选

| | WM_TOUCH | WM_POINTER |
|---|----------|------------|
| 最低系统要求 | Windows 7 | Windows 8 |
| 输入类型 | 仅触控 | 触控 + 笔 + 鼠标 |
| 多点触控 | 一次消息包含所有触控点 | 每个触控点独立消息 |
| API 复杂度 | 较简单 | 稍复杂 |
| 推荐 | 兼容老系统时使用 | 新项目首选 |

如果你的目标系统是 Windows 8+，优先用 `WM_POINTER`。它更统一、更灵活，也是微软推荐的方向。

---

## 第三步——WM_INPUT：Raw Input 原始输入

### 什么是 Raw Input

普通鼠标消息 `WM_MOUSEMOVE` 会经过系统的处理——加速、合并、精度调整。对于一般应用这很好，但有些场景你需要拿到最原始的设备数据：

* **游戏**：第一人称射击游戏需要最精确的鼠标移动数据，不能有系统加速。
* **绘图软件**：数位板需要精确的笔位置和压力数据。
* **设备调试**：读取 HID 设备（如游戏手柄、飞行摇杆）的原始报告。

`WM_INPUT`（Raw Input）就是为此设计的。它直接从设备驱动拿到数据，不经过系统的任何翻译或处理。

### 注册原始输入设备

```cpp
// 注册鼠标原始输入
RAWINPUTDEVICE rid = {};
rid.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
rid.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
rid.dwFlags = 0;        // 默认：前台接收
rid.hwndTarget = hWnd;

if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
    // 注册失败
}
```

常用的 Usage Page / Usage 组合：

| 设备 | usUsagePage | usUsage |
|------|------------|---------|
| 鼠标 | 0x01 | 0x02 |
| 键盘 | 0x01 | 0x06 |
| 游戏手柄 | 0x01 | 0x04 |
| 游戏控制器 | 0x01 | 0x05 |

`dwFlags` 控制接收范围：

| 标志 | 含义 |
|------|------|
| 0 | 仅窗口在前台时接收 |
| RIDEV_INPUTSINK | 窗口在后台也接收 |
| RIDEV_NOLEGACY | 不再发送传统鼠标/键盘消息 |
| RIDEV_REMOVE | 停止接收原始输入 |

### 处理 WM_INPUT

```cpp
case WM_INPUT: {
    // 第一步：确定需要多大的缓冲区
    UINT dataSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dataSize,
                    sizeof(RAWINPUTHEADER));

    if (dataSize == 0) break;

    // 第二步：读取数据
    std::vector<BYTE> buffer(dataSize);
    RAWINPUT* raw = (RAWINPUT*)buffer.data();
    UINT bytesWritten = GetRawInputData(
        (HRAWINPUT)lParam, RID_INPUT, raw, &dataSize,
        sizeof(RAWINPUTHEADER));

    if (bytesWritten != dataSize) break;

    // 第三步：根据设备类型处理
    if (raw->header.dwType == RIM_TYPEMOUSE) {
        // 鼠标原始数据
        RAWMOUSE& mouse = raw->data.mouse;

        // 相对移动量（不受系统加速影响）
        LONG dx = mouse.lLastX;
        LONG dy = mouse.lLastY;

        // 按钮状态
        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
            // 左键按下
        }
        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
            // 左键抬起
        }
        if (mouse.usButtonFlags & RI_MOUSE_WHEEL) {
            // 滚轮，增量在 mouse.usButtonData 中
            SHORT delta = (SHORT)mouse.usButtonData;
        }
    }
    else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        // 键盘原始数据
        RAWKEYBOARD& kb = raw->data.keyboard;

        USHORT scanCode = kb.MakeCode;
        USHORT flags = kb.Flags;

        // RI_KEY_MAKE = 按下, RI_KEY_BREAK = 抬起
        bool keyDown = !(flags & RI_KEY_BREAK);
        bool isExtended = flags & RI_KEY_E0;

        // 获取虚拟键码
        UINT vkCode = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);
    }
    else if (raw->header.dwType == RIM_TYPEHID) {
        // 自定义 HID 设备数据
        RAWHID& hid = raw->data.hid;
        // hid.dwSizeHid: 每个报告的大小
        // hid.dwCount: 报告数量
        // hid.bRawData: 原始数据
    }

    // 如果还需要传统消息，调用 DefWindowProc
    // 如果设置了 RIDEV_NOLEGACY，则不需要
    break;
}
```

### Raw Input 鼠标 vs 普通鼠标消息

```cpp
// 如果你想在游戏中使用 Raw Input 鼠标（不受系统加速影响），
// 注册时使用 RIDEV_NOLEGACY 标志：

RAWINPUTDEVICE rid = {};
rid.usUsagePage = 0x01;
rid.usUsage = 0x02;
rid.dwFlags = RIDEV_NOLEGACY;  // 不再发送 WM_MOUSEMOVE 等
rid.hwndTarget = hWnd;
RegisterRawInputDevices(&rid, 1, sizeof(rid));
```

**注意**：使用 `RIDEV_NOLEGACY` 后，`WM_MOUSEMOVE`、`WM_LBUTTONDOWN` 等鼠标消息将不再发送。如果你只在特定场景（如进入游戏模式）需要 Raw Input，记得在退出时用 `RIDEV_REMOVE` 重新启用传统消息。

### 枚举输入设备

你可以查询系统上所有可用的输入设备：

```cpp
UINT deviceCount = 0;
UINT err = GetRawInputDeviceList(nullptr, &deviceCount,
                                 sizeof(RAWINPUTDEVICELIST));
if (err != (UINT)-1 && deviceCount > 0) {
    std::vector<RAWINPUTDEVICELIST> devices(deviceCount);
    GetRawInputDeviceList(devices.data(), &deviceCount,
                          sizeof(RAWINPUTDEVICELIST));

    for (UINT i = 0; i < deviceCount; i++) {
        // devices[i].dwType: RIM_TYPEMOUSE / RIM_TYPEKEYBOARD / RIM_TYPEHID
        // devices[i].hDevice: 设备句柄

        // 获取设备名称
        UINT nameSize = 0;
        GetRawInputDeviceInfo(devices[i].hDevice, RIDI_DEVICENAME,
                              nullptr, &nameSize);
        std::wstring name(nameSize, L'\0');
        GetRawInputDeviceInfo(devices[i].hDevice, RIDI_DEVICENAME,
                              name.data(), &nameSize);
    }
}
```

---

## 第四步——WM_WINDOWPOSCHANGING 与 WM_WINDOWPOSCHANGED

### 窗口位置消息的工作机制

当窗口的位置或大小即将改变时（用户拖动、调用 `SetWindowPos`、系统排列等），系统会发送两个消息：

```
WM_WINDOWPOSCHANGING  →  窗口位置"即将"改变（你可以修改或阻止）
         ↓
窗口位置实际改变
         ↓
WM_WINDOWPOSCHANGED   →  窗口位置"已经"改变
```

### WINDOWPOS 结构

这两个消息的 `lParam` 都指向一个 `WINDOWPOS` 结构：

```cpp
typedef struct tagWINDOWPOS {
    HWND hwndInsertAfter;  // Z 序位置
    int  x;                // 窗口左上角 x
    int  y;                // 窗口左上角 y
    int  cx;               // 窗口宽度
    int  cy;               // 窗口高度
    UINT flags;            // 标志位
} WINDOWPOS;
```

### WM_WINDOWPOSCHANGING：拦截并修改

这是你唯一能在窗口位置改变"之前"进行干预的机会。常见用法：

```cpp
case WM_WINDOWPOSCHANGING: {
    WINDOWPOS* wp = (WINDOWPOS*)lParam;

    // 示例 1：限制窗口最小尺寸
    if (wp->cx < 400) wp->cx = 400;
    if (wp->cy < 300) wp->cy = 300;

    // 示例 2：让窗口只能水平移动，不允许垂直移动
    // wp->y = 当前 y 坐标;

    // 示例 3：让窗口吸附到屏幕边缘（磁吸效果）
    int snapDistance = 20;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    if (abs(wp->x) < snapDistance) wp->x = 0;
    if (abs(wp->x + wp->cx - screenW) < snapDistance)
        wp->x = screenW - wp->cx;
    if (abs(wp->y) < snapDistance) wp->y = 0;
    if (abs(wp->y + wp->cy - screenH) < snapDistance)
        wp->y = screenH - wp->cy;

    return 0;
}
```

### WM_WINDOWPOSCHANGED：响应位置变化

窗口位置已经改变后，你需要更新内部状态：

```cpp
case WM_WINDOWPOSCHANGED: {
    WINDOWPOS* wp = (WINDOWPOS*)lParam;

    // 注意：WM_SIZE 和 WM_MOVE 会紧随 WM_WINDOWPOSCHANGED 发送
    // 如果你只需要知道最终尺寸和位置，处理 WM_SIZE/WM_MOVE 就够了
    // WM_WINDOWPOSCHANGED 的优势是能拿到更多信息（如 Z 序变化）

    // 检查 Z 序是否变化
    if (!(wp->flags & SWP_NOZORDER)) {
        // Z 序改变了，wp->hwndInsertAfter 是新的 Z 序参考
    }

    // 检查位置是否变化
    if (!(wp->flags & SWP_NOMOVE)) {
        // 位置改变了
    }

    // 检查尺寸是否变化
    if (!(wp->flags & SWP_NOSIZE)) {
        // 尺寸改变了
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
```

### flags 常用值

| 标志 | 含义 |
|------|------|
| SWP_NOSIZE | 尺寸不变 |
| SWP_NOMOVE | 位置不变 |
| SWP_NOZORDER | Z 序不变 |
| SWP_NOREDRAW | 不重绘 |
| SWP_NOACTIVATE | 不激活 |

当你在 `WM_WINDOWPOSCHANGING` 中检查 `flags` 时，如果某个标志被设置，说明对应的属性不会改变，你不需要处理它。

---

## 第五步——WM_PRINTCLIENT：让别人绘制你的窗口

### 使用场景

正常情况下，你的窗口内容在 `WM_PAINT` 中绘制。但有些场景下，系统或其他程序需要你的窗口内容，但不想走 `WM_PAINT` 的流程：

* **打印**：系统打印对话框需要获取窗口的外观。
* **截图工具**：第三方截图工具通过 `PrintWindow` 获取窗口内容。
* **`BitBlt` 截取**：其他程序用 `BitBlt` 从你的窗口读取像素时。
* **MDI 子窗口**：父窗口在重绘时可能需要获取子窗口的内容。

### 处理 WM_PRINTCLIENT

```cpp
case WM_PRINTCLIENT: {
    HDC hdc = (HDC)wParam;

    // 获取客户区尺寸
    RECT rc;
    GetClientRect(hWnd, &rc);

    // 在提供的 DC 上绘制客户区内容
    // 这里的绘制代码应该和 WM_PAINT 中的一样
    // 或者调用同一个绘制函数
    DrawContent(hdc, rc);

    return 0;
}
```

### 与 WM_PAINT 的关系

`WM_PRINTCLIENT` 和 `WM_PAINT` 的区别：

| | WM_PAINT | WM_PRINTCLIENT |
|---|----------|----------------|
| DC 来源 | `BeginPaint` 获取 | `lParam` 直接提供 |
| 脏区信息 | `ps.rcPaint` 提供脏区 | 没有，绘制整个客户区 |
| 触发方式 | 系统在需要时自动触发 | 外部调用 `PrintWindow` 或 `WM_PRINT` 触发 |
| 裁切区域 | 系统自动裁切到脏区 | 由调用方设置 |

最佳实践是抽取一个共用的绘制函数，`WM_PAINT` 和 `WM_PRINTCLIENT` 都调用它：

```cpp
void DrawContent(HWND hWnd, HDC hdc, const RECT& rc) {
    // 所有绘制逻辑都在这里
    // ...
}

// WM_PAINT 调用
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);
    DrawContent(hWnd, hdc, rc);
    EndPaint(hWnd, &ps);
    return 0;
}

// WM_PRINTCLIENT 调用
case WM_PRINTCLIENT: {
    HDC hdc = (HDC)wParam;
    RECT rc;
    GetClientRect(hWnd, &rc);
    DrawContent(hWnd, hdc, rc);
    return 0;
}
```

如果你不处理 `WM_PRINTCLIENT`，`PrintWindow` 截取你的窗口时客户区会是空白的。

---

## 第六步——WM_DISPLAYCHANGE：显示器变化通知

### 使用场景

当用户改变屏幕分辨率、插拔外接显示器、或者旋转屏幕方向时，系统会发送 `WM_DISPLAYCHANGE` 消息。如果你的程序需要：

* 窗口吸附在屏幕边缘（分辨率变化后需要重新计算）
* 多显示器布局管理
* 全屏模式切换
* DPI 相关的布局调整

你就需要处理这个消息。

### 处理 WM_DISPLAYCHANGE

```cpp
case WM_DISPLAYCHANGE: {
    // 新的颜色深度（位每像素）
    int bpp = (int)wParam;

    // 新的屏幕分辨率
    int screenWidth = LOWORD(lParam);
    int screenHeight = HIWORD(lParam);

    // 注意：这个分辨率是主显示器的分辨率
    // 多显示器环境需要用 EnumDisplayMonitors 获取完整信息

    // 示例：确保窗口在新分辨率下仍然可见
    RECT rc;
    GetWindowRect(hWnd, &rc);

    if (rc.right > screenWidth) {
        // 窗口右边超出屏幕，调整位置
        int newLeft = screenWidth - (rc.right - rc.left);
        SetWindowPos(hWnd, nullptr, newLeft, rc.top,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    if (rc.bottom > screenHeight) {
        // 窗口底部超出屏幕，调整位置
        int newTop = screenHeight - (rc.bottom - rc.top);
        SetWindowPos(hWnd, nullptr, rc.left, newTop,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    return 0;
}
```

### 多显示器环境

在多显示器环境下，`WM_DISPLAYCHANGE` 只提供主显示器的分辨率信息。要获取完整的显示器布局，使用 `EnumDisplayMonitors`：

```cpp
// 枚举所有显示器
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
                               LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFOEX mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

    // mi.rcMonitor: 整个显示器的矩形（包含任务栏等）
    // mi.rcWork:    工作区矩形（排除任务栏等）
    // mi.szDevice:  设备名称（如 "\\.\DISPLAY1"）

    return TRUE; // 继续枚举
}

EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);
```

---

## 实战示例：触控画板

把上面学的几个消息组合起来，写一个简单的触控画板——支持鼠标和触控输入，窗口有最小尺寸限制，内容可以被截图工具正确截取。

```cpp
#include <vector>
#include <Windows.h>
#include <windowsx.h>

struct StrokePoint {
    int x, y;
    int pressure;  // 0-1024（仅笔输入有意义）
};

struct Stroke {
    std::vector<StrokePoint> points;
    COLORREF color;
};

static std::vector<Stroke> g_strokes;
static Stroke g_currentStroke;
static bool g_drawing = false;
static COLORREF g_currentColor = RGB(0, 0, 0);

void DrawContent(HWND hWnd, HDC hdc, const RECT& rc) {
    // 白色背景
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    // 绘制所有笔画
    for (const auto& stroke : g_strokes) {
        if (stroke.points.size() < 2) continue;

        HPEN hPen = CreatePen(PS_SOLID, 2, stroke.color);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);

        MoveToEx(hdc, stroke.points[0].x, stroke.points[0].y, nullptr);
        for (size_t i = 1; i < stroke.points.size(); i++) {
            LineTo(hdc, stroke.points[i].x, stroke.points[i].y);
        }

        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // 注册触控输入
        RegisterTouchWindow(hWnd, TWF_FINETOUCH);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        g_drawing = true;
        g_currentStroke = {};
        g_currentStroke.color = g_currentColor;
        StrokePoint pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0 };
        g_currentStroke.points.push_back(pt);
        SetCapture(hWnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_drawing) return 0;
        StrokePoint pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0 };
        g_currentStroke.points.push_back(pt);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!g_drawing) return 0;
        g_drawing = false;
        if (g_currentStroke.points.size() >= 2) {
            g_strokes.push_back(g_currentStroke);
        }
        ReleaseCapture();
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_TOUCH: {
        HTOUCHINPUT hTouch = (HTOUCHINPUT)lParam;
        UINT nInputs = LOWORD(wParam);
        std::vector<TOUCHINPUT> inputs(nInputs);

        if (GetTouchInputInfo(hTouch, nInputs, inputs.data(),
                              sizeof(TOUCHINPUT))) {
            for (UINT i = 0; i < nInputs; i++) {
                POINT pt = { inputs[i].x / 100, inputs[i].y / 100 };
                ScreenToClient(hWnd, &pt);

                if (inputs[i].dwFlags & TOUCHEVENTF_DOWN) {
                    g_drawing = true;
                    g_currentStroke = {};
                    g_currentStroke.color = g_currentColor;
                    g_currentStroke.points.push_back({ pt.x, pt.y, 0 });
                }
                else if (inputs[i].dwFlags & TOUCHEVENTF_MOVE && g_drawing) {
                    g_currentStroke.points.push_back({ pt.x, pt.y, 0 });
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                else if (inputs[i].dwFlags & TOUCHEVENTF_UP) {
                    g_drawing = false;
                    if (g_currentStroke.points.size() >= 2) {
                        g_strokes.push_back(g_currentStroke);
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
        }
        CloseTouchInputHandle(hTouch);
        return 0;
    }

    case WM_WINDOWPOSCHANGING: {
        // 限制窗口最小尺寸为 400x300
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        if (wp->cx < 400) wp->cx = 400;
        if (wp->cy < 300) wp->cy = 300;
        return 0;
    }

    case WM_PRINTCLIENT: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        DrawContent(hWnd, hdc, rc);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 绘制已完成的笔画
        RECT rc;
        GetClientRect(hWnd, &rc);
        DrawContent(hWnd, hdc, rc);

        // 绘制正在画的笔画
        if (g_drawing && g_currentStroke.points.size() >= 2) {
            HPEN hPen = CreatePen(PS_SOLID, 2, g_currentStroke.color);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            MoveToEx(hdc, g_currentStroke.points[0].x,
                     g_currentStroke.points[0].y, nullptr);
            for (size_t i = 1; i < g_currentStroke.points.size(); i++) {
                LineTo(hdc, g_currentStroke.points[i].x,
                       g_currentStroke.points[i].y);
            }
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_KEYDOWN: {
        // 按 C 键清空画布
        if (wParam == 'C') {
            g_strokes.clear();
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        // 按 R/G/B 切换颜色
        else if (wParam == 'R') g_currentColor = RGB(255, 0, 0);
        else if (wParam == 'G') g_currentColor = RGB(0, 128, 0);
        else if (wParam == 'B') g_currentColor = RGB(0, 0, 255);
        return 0;
    }

    case WM_DISPLAYCHANGE: {
        // 显示器分辨率变化时，确保窗口仍在可见区域
        RECT rc;
        GetWindowRect(hWnd, &rc);
        int screenW = LOWORD(lParam);
        int screenH = HIWORD(lParam);

        if (rc.left > screenW || rc.top > screenH) {
            SetWindowPos(hWnd, nullptr, 100, 100, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_DESTROY: {
        UnregisterTouchWindow(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}
```

这个示例综合使用了 `WM_TOUCH`（触控绘制）、`WM_WINDOWPOSCHANGING`（最小尺寸限制）、`WM_PRINTCLIENT`（截图支持）和 `WM_DISPLAYCHANGE`（显示器变化处理）。

---

## 消息速查表

| 消息 | 用途 | 最低系统 | 关键结构/函数 |
|------|------|----------|--------------|
| WM_TOUCH | 多点触控 | Win 7 | `RegisterTouchWindow`, `GetTouchInputInfo` |
| WM_POINTER* | 统一指针输入 | Win 8 | `GetPointerInfo`, `GetPointerTouchInfo` |
| WM_INPUT | Raw Input（原始设备数据） | Win XP | `RegisterRawInputDevices`, `GetRawInputData` |
| WM_WINDOWPOSCHANGING | 拦截窗口位置变化 | Win 2000 | `WINDOWPOS` |
| WM_WINDOWPOSCHANGED | 响应窗口位置变化 | Win 2000 | `WINDOWPOS` |
| WM_PRINTCLIENT | 外部请求绘制客户区 | Win 2000 | 无 |
| WM_DISPLAYCHANGE | 显示器分辨率/数量变化 | Win 2000 | wParam = bpp, lParam = 宽高 |

---

## 练习

1. **触控画板增强**：为上面的画板示例添加 `WM_POINTER` 支持（Windows 8+），让触控笔的压力值影响笔画粗细。提示：使用 `GetPointerPenInfo` 获取 `pressure` 值。

2. **Raw Input 鼠标查看器**：注册鼠标 Raw Input，在窗口中实时显示鼠标的相对移动量（dx, dy），对比普通 `WM_MOUSEMOVE` 的绝对坐标。观察打开/关闭系统鼠标加速后两者的差异。

3. **窗口磁吸效果**：使用 `WM_WINDOWPOSCHANGING` 实现窗口拖动时的屏幕边缘磁吸（距离边缘 20 像素内自动吸附），并支持多显示器。

4. **设备枚举器**：使用 `GetRawInputDeviceList` 和 `GetRawInputDeviceInfo` 枚举系统上所有输入设备，在 `ListView` 中显示设备类型、名称和句柄。

---

**参考资料**:
- [Touch Input - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/wintouch/windows-touch-programming-guide)
- [Pointer Input - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputmsg/messages-and-notifications-portal)
- [Raw Input - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/inputdev/raw-input)
- [WM_WINDOWPOSCHANGING - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)
- [WM_PRINTCLIENT - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-printclient)
- [WM_DISPLAYCHANGE - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-displaychange)
- [RegisterTouchWindow function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registertouchwindow)
- [GetPointerTouchInfo function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getpointertouchinfo)
