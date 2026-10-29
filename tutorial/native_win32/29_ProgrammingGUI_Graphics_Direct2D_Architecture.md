# 通用GUI编程技术——图形渲染实战（二十九）——Direct2D架构与资源体系：GPU加速2D渲染入门

> 到这里，我们已经把 Windows 上 CPU 时代的 2D 图形 API 都过了一遍——从原始的 GDI 到封装更优雅的 GDI+。说实话，GDI+ 在功能上已经能满足很多需求了，但如果你尝试用它做高频率刷新的动画，或者在高 DPI 显示器上绘制复杂场景，你会立刻感受到性能瓶颈。GDI 和 GDI+ 都是 CPU 绘制——每一条线、每一个像素都由 CPU 逐个计算，GPU 在旁边闲着看热闹。
>
> 今天我们要跨入一个全新的领域：Direct2D。这是微软在 Windows 7 时代推出的 GPU 加速 2D 渲染 API。它的设计哲学和 GDI 完全不同——所有的绘制操作都由 GPU 执行，CPU 只负责下达指令。这意味着你可以轻松实现 60fps 的流畅动画，同时保持高质量的抗锯齿渲染。但天下没有免费的午餐，GPU 加速带来了新的复杂性：资源管理模型变了，设备丢失需要处理，COM 接口的引用计数得操心。今天我们就来拆解 Direct2D 的架构，从初始化到第一次绘制。

## 环境说明

- **操作系统**: Windows 10/11（Direct2D 需要 Windows 7 SP1 + 平台更新或更高）
- **编译器**: MSVC (Visual Studio 2022, v17.x 工具集)
- **目标平台**: Win32 原生桌面应用
- **图形库**: Direct2D（链接 `d2d1.lib`，头文件 `<d2d1.h>`）
- **字符集**: Unicode

⚠️ 注意，Direct2D 是基于 COM（Component Object Model）的 API。所有接口（如 `ID2D1Factory`、`ID2D1HwndRenderTarget`）都是 COM 接口，通过引用计数管理生命周期。你在使用完之后必须调用 `Release()` 方法释放，而不是 `delete`。这一点和 GDI+ 的 C++ 类（自动析构）完全不同。

## Direct2D 的定位与设计哲学

Direct2D 的官方定位是"GDI 的现代替代品"。但如果你用过 GDI，你会发现 Direct2D 的编程模型和 GDI 差异巨大，几乎是从零开始学。

Direct2D 底层通过 Direct3D 10.1 的 API 与 GPU 通信，所有的 2D 绘制操作最终都被转换成 GPU 能理解的三角形和纹理采样操作。这意味着 Direct2D 天然支持硬件加速，绘制性能远超 CPU 方案。同时，Direct2D 也提供了软件回退（WARP — Windows Advanced Rasterization Platform），在没有 GPU 或 GPU 驱动异常的环境下仍能正常工作。

Direct2D 的设计遵循两个核心原则：第一是硬件加速优先，所有渲染都通过 GPU 完成；第二是高 DPI 感知，API 内部使用浮点坐标，自动处理 DPI 缩放。

## 资源的两级分类

理解 Direct2D 资源管理的关键在于区分两类资源：**设备无关资源**和**设备相关资源**。

设备无关资源不依赖于特定的 GPU 硬件或渲染配置。创建一次，可以重复使用。典型的设备无关资源包括 `ID2D1Factory`（Direct2D 的工厂接口，用于创建所有其他对象）、`ID2D1StrokeStyle`（线条样式）、`ID2D1Geometry` 及其子类（矩形、椭圆、路径几何体等）。这些资源通常在程序启动时创建，程序退出时释放。

设备相关资源依赖于特定的渲染目标（Render Target），与 GPU 硬件绑定。渲染目标本身就是最重要的设备相关资源。`ID2D1HwndRenderTarget`（窗口渲染目标）、`ID2D1Brush`（画刷，包括纯色、渐变、位图画刷）、`ID2D1Bitmap`（位图）都属于这一类。设备相关资源有一个致命特性：**当设备丢失时（比如 GPU 驱动崩溃恢复、显示设置变更），所有设备相关资源都会失效，必须全部重建**。

⚠️ 注意，这个"设备丢失"不是理论上的极端情况。在笔记本外接显示器热插拔、远程桌面连接断开、GPU 驱动更新等场景下都会触发。如果你的程序不处理设备丢失，轻则画面消失，重则崩溃。我们后面会专门讲怎么处理这个问题。

## Direct2D 初始化：标准骨架

让我们从最基础的初始化开始，一步步搭建一个完整的 Direct2D 程序。

### 创建 ID2D1Factory

`ID2D1Factory` 是 Direct2D 的入口点，所有其他 Direct2D 对象都通过它创建。创建方式如下：

```cpp
ID2D1Factory* pFactory = NULL;
HRESULT hr = D2D1CreateFactory(
    D2D1_FACTORY_TYPE_SINGLE_THREADED,  // 单线程模式
    &pFactory
);
if (FAILED(hr)) {
    // 创建失败处理
    return;
}
```

`D2D1CreateFactory` 的第一个参数指定工厂类型。`D2D1_FACTORY_TYPE_SINGLE_THREADED` 表示你的程序只在一个线程中使用 Direct2D，这是最常见的情况，性能也最好。`D2D1_FACTORY_TYPE_MULTI_THREADED` 表示多线程使用，Direct2D 内部会加锁保护，但会带来性能开销。除非你确实需要从多个线程同时调用 Direct2D，否则始终用单线程模式。

### 创建 HwndRenderTarget

有了工厂之后，下一步是创建窗口渲染目标（HwndRenderTarget），它负责将 GPU 渲染结果呈现到指定的窗口上：

```cpp
RECT rc;
GetClientRect(hwnd, &rc);

ID2D1HwndRenderTarget* pRenderTarget = NULL;
hr = pFactory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),  // 默认渲染目标属性
    D2D1::HwndRenderTargetProperties(
        hwnd,                                    // 目标窗口句柄
        D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)  // 像素尺寸
    ),
    &pRenderTarget
);
if (FAILED(hr)) {
    // 创建失败处理
    return;
}
```

`RenderTargetProperties` 控制渲染目标的一般属性，比如像素格式、DPI 设置、渲染模式等。默认参数（`RenderTargetProperties()` 无参版本）会使用 32 位 BGRA 格式、系统默认 DPI 和硬件加速渲染。大多数情况下默认就够了。

`HwndRenderTargetProperties` 是窗口渲染目标的专用属性，需要你提供窗口句柄和初始像素尺寸。这里的尺寸使用像素单位，不是逻辑单位。

### 创建画刷

在 Direct2D 中，所有"填充"和"描边"操作都需要画刷（Brush）。最基本的画刷是纯色画刷：

```cpp
ID2D1SolidColorBrush* pBrush = NULL;
hr = pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::White),  // 白色
    &pBrush
);
```

`D2D1::ColorF` 是一个辅助类，可以用多种方式指定颜色。除了使用预定义的颜色常量（如 `ColorF::White`、`ColorF::Red`），你也可以传入 RGB 值：

```cpp
// 自定义颜色：浅灰蓝色 (R:0.85, G:0.90, B:0.95, Alpha:1.0)
pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(0.85f, 0.90f, 0.95f, 1.0f),
    &pBrush
);
```

⚠️ 注意，Direct2D 的颜色分量范围是 0.0 到 1.0 的浮点数，不是 GDI 那种 0-255 的整数。如果你习惯了 GDI 的 `RGB(255, 128, 0)` 写法，需要手动除以 255 转换。

## 绘制循环：BeginDraw / EndDraw

Direct2D 的绘制操作必须在 `BeginDraw` 和 `EndDraw` 之间进行。这是一个硬性要求，在 `BeginDraw` 之外调用任何绘制方法都会静默失败（不报错，但也不绘制）。

```cpp
void OnPaint(ID2D1HwndRenderTarget* pRenderTarget,
             ID2D1SolidColorBrush* pBrush)
{
    pRenderTarget->BeginDraw();

    // 清除背景为深灰色
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::DarkSlateGray));

    // 绘制一个填充的圆角矩形
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
        D2D1::RectF(50.0f, 50.0f, 350.0f, 200.0f),  // 矩形区域
        10.0f,  // X 方向圆角半径
        10.0f   // Y 方向圆角半径
    );

    pBrush->SetColor(D2D1::ColorF(0.2f, 0.6f, 0.9f, 1.0f));  // 蓝色
    pRenderTarget->FillRoundedRectangle(roundedRect, pBrush);

    // 结束绘制
    HRESULT hr = pRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        // 设备丢失！需要重建所有设备相关资源
        // 后面会详细讲解
    }
}
```

`BeginDraw` 会标记渲染目标的内部状态为"正在绘制"，`EndDraw` 会提交所有绘制命令并呈现结果。如果在 `BeginDraw` 和 `EndDraw` 之间发生了错误，`EndDraw` 会返回对应的 `HRESULT`。

这里有一个非常重要的返回值需要处理：`D2DERR_RECREATE_TARGET`。这个返回值表示渲染目标关联的 GPU 设备已经丢失（比如驱动崩溃后恢复），当前的渲染目标和所有设备相关资源（画刷、位图等）都已失效。你必须在收到这个错误后重建所有设备相关资源。

## 处理窗口大小变化

当窗口大小改变时，你需要调用 `Resize` 方法通知渲染目标更新其后备缓冲区的大小：

```cpp
case WM_SIZE:
{
    if (pRenderTarget != NULL) {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        pRenderTarget->Resize(D2D1::SizeU(width, height));
        InvalidateRect(hwnd, NULL, FALSE);  // 触发重绘
    }
    return 0;
}
```

如果你不调用 `Resize`，渲染目标的后备缓冲区大小不会自动跟随窗口变化，结果就是窗口变大后只有一部分区域有内容，或者窗口变小后渲染目标浪费内存。

## 设备丢失的完整处理模式

现在我们来处理 `D2DERR_RECREATE_TARGET`。标准的处理模式是：在 `EndDraw` 之后检查返回值，如果收到重建信号，就释放所有设备相关资源并重建：

```cpp
// 全局变量
ID2D1Factory* g_pFactory = NULL;
ID2D1HwndRenderTarget* g_pRenderTarget = NULL;
ID2D1SolidColorBrush* g_pBrush = NULL;

void CreateDeviceResources(HWND hwnd)
{
    if (g_pRenderTarget != NULL) return;  // 已存在，跳过

    RECT rc;
    GetClientRect(hwnd, &rc);

    HRESULT hr = g_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            hwnd,
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
        ),
        &g_pRenderTarget
    );
    if (FAILED(hr)) return;

    // 创建画刷等设备相关资源
    g_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), &g_pBrush);
}

void DiscardDeviceResources()
{
    // 按创建的逆序释放
    SafeRelease(&g_pBrush);
    SafeRelease(&g_pRenderTarget);
}

void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);

    if (g_pRenderTarget == NULL) return;

    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f));

    // ... 绘制代码 ...

    HRESULT hr = g_pRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);  // 立即触发重绘以重建资源
    }
}

// SafeRelease 辅助函数
template<class T> void SafeRelease(T** ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}
```

这个模式的核心思想是"延迟创建 + 按需重建"。`CreateDeviceResources` 在每次 `OnPaint` 时检查资源是否存在，不存在才创建。`DiscardDeviceResources` 在设备丢失时释放所有设备相关资源。下次 `OnPaint` 时 `CreateDeviceResources` 发现资源为 `NULL`，会自动重建。注意 `ID2D1Factory` 是设备无关资源，不需要在设备丢失时重建。

## 完整的最小 Direct2D 程序

下面是一个可以直接编译运行的完整示例：

```cpp
#include <windows.h>
#include <d2d1.h>
#include <cmath>
#pragma comment(lib, "d2d1.lib")

template<class T> void SafeRelease(T** ppT)
{
    if (*ppT) { (*ppT)->Release(); *ppT = NULL; }
}

ID2D1Factory* g_pFactory = NULL;
ID2D1HwndRenderTarget* g_pRT = NULL;
ID2D1SolidColorBrush* g_pBrush = NULL;
float g_angle = 0.0f;

void CreateDeviceResources(HWND hwnd)
{
    if (g_pRT) return;
    RECT rc; GetClientRect(hwnd, &rc);
    g_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd,
            D2D1::SizeU(rc.right, rc.bottom)),
        &g_pRT);
    if (g_pRT)
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_pBrush);
}

void DiscardDeviceResources()
{
    SafeRelease(&g_pBrush);
    SafeRelease(&g_pRT);
}

void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);
    if (!g_pRT) return;

    g_pRT->BeginDraw();
    g_pRT->Clear(D2D1::ColorF(0.1f, 0.1f, 0.15f, 1.0f));

    RECT rc; GetClientRect(hwnd, &rc);
    float cx = (rc.right - rc.left) / 2.0f;
    float cy = (rc.bottom - rc.top) / 2.0f;
    float r = 80.0f;

    // 绘制旋转的三角形
    D2D1_POINT_2F points[3];
    for (int i = 0; i < 3; i++) {
        float a = g_angle + i * (2.0f * 3.14159f / 3.0f);
        points[i] = D2D1::Point2F(cx + r * cosf(a), cy + r * sinf(a));
    }

    g_pBrush->SetColor(D2D1::ColorF(0.3f, 0.7f, 1.0f, 0.8f));
    g_pRT->DrawLine(points[0], points[1], g_pBrush, 3.0f);
    g_pRT->DrawLine(points[1], points[2], g_pBrush, 3.0f);
    g_pRT->DrawLine(points[2], points[0], g_pBrush, 3.0f);

    HRESULT hr = g_pRT->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_PAINT:
        OnPaint(hwnd);
        ValidateRect(hwnd, NULL);
        return 0;
    case WM_TIMER:
        g_angle += 0.05f;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SIZE:
        if (g_pRT) {
            g_pRT->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_DESTROY:
        DiscardDeviceResources();
        SafeRelease(&g_pFactory);
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pFactory);

    const wchar_t cls[] = L"D2DDemo";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(cls, L"Direct2D 旋转三角形",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);

    SetTimer(hwnd, 1, 16, NULL);  // ~60fps

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
```

## 常见问题与调试

### 问题1：D2D1CreateFactory 返回 E_FAIL

这通常意味着系统中没有安装 Direct2D 运行时。Direct2D 需要 Windows 7 SP1 + 平台更新或更高版本。在 Windows Vista 或未打补丁的 Windows 7 上，`D2D1CreateFactory` 会失败。检查你的目标系统版本要求。

### 问题2：BeginDraw / EndDraw 外调用绘制方法

这是一个静默失败的场景——不会报错，不会崩溃，但画面上什么都不会出现。如果你发现 `DrawRectangle`、`FillEllipse` 等方法"不工作"，首先检查你是在 `BeginDraw` 和 `EndDraw` 之间调用它们的。

### 问题3：画面闪烁

如果你在 `WM_PAINT` 处理中既用 GDI（`BeginPaint`/`EndPaint`）又用 Direct2D，可能会出现闪烁。因为 GDI 的 `BeginPaint` 会擦除背景，而 Direct2D 的 `BeginDraw` 不会。解决办法是只使用 Direct2D 绘制，或者处理 `WM_ERASEBKGND` 返回 `TRUE` 阻止背景擦除。

## 总结

到这里，我们已经把 Direct2D 的基础架构梳理清楚了。核心概念是资源的两级分类（设备无关 vs 设备相关）、`BeginDraw`/`EndDraw` 的绘制框架、以及 `D2DERR_RECREATE_TARGET` 的设备丢失处理。这些是你使用 Direct2D 时每天都在打交道的模式。

和 GDI 相比，Direct2D 的初始化确实更复杂——需要创建工厂、创建渲染目标、创建画刷，而且还要处理 COM 引用计数和设备丢失。但这些都是一次性的模板代码，复制粘贴即可。真正的好处在于：从今天开始，你所有的 2D 绘制都由 GPU 加速了。

下一步，我们要深入 Direct2D 的几何体系统。Direct2D 提供了比 GDI 丰富得多的几何体类型——从简单的矩形和椭圆，到贝塞尔曲线和布尔运算组合的复杂路径。更重要的是，Direct2D 的几何体支持精确的命中测试，这是构建交互式图形应用的基础。

---

## 练习

1. 用 Direct2D 重写之前 GDI 的双缓冲动画示例（弹跳球），对比两者的代码结构和运行性能。
2. 实现一个渐变背景窗口：使用 `ID2D1LinearGradientBrush` 从窗口顶部到底部绘制蓝色到紫色的渐变。
3. 处理 `WM_DPICHANGED` 消息，在 DPI 变化时正确调整渲染目标的大小和画刷参数。
4. 实现设备丢失的自动恢复：在 `EndDraw` 返回 `D2DERR_RECREATE_TARGET` 后，重建所有资源并重绘，确保用户看不到任何闪烁或中断。

---

**参考资料**:
- [D2D1CreateFactory function - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-d2d1createfactory)
- [ID2D1HwndRenderTarget interface - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1hwndrendertarget)
- [ID2D1RenderTarget::BeginDraw - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-begindraw)
- [ID2D1SolidColorBrush - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nn-d2d1-id2d1solidcolorbrush)
- [Handling Device Loss in Direct2D - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/how-to-handle-device-lost)
- [Direct2D and GDI Interoperability Overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-gdi-interoperation-overview)
