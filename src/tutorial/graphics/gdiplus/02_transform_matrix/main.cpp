/**
 * 02_transform_matrix - GDI+ 坐标变换示例
 *
 * 本程序演示了 GDI+ 的矩阵变换功能：
 * 1. Gdiplus::Matrix — 旋转(Rotate)、缩放(Scale)、平移(Translate)
 * 2. Graphics::SetTransform — 设置当前变换矩阵
 * 3. Graphics::MultiplyTransform — 组合多个变换
 * 4. MatrixOrderAppend vs MatrixOrderPrepend — 变换顺序的区别
 * 5. 绘制一个房屋形状（矩形 + 三角形屋顶），通过按钮交互式应用变换
 *
 * 参考: tutorial/native_win32/27_ProgrammingGUI_Graphics_GdiPlus_Transform.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cstdio>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GdiPlusTransformClass";

// 按钮子窗口 ID
static const int ID_BTN_ROTATE  = 101;
static const int ID_BTN_SCALE   = 102;
static const int ID_BTN_RESET   = 103;

// ============================================================================
// 应用程序状态
// ============================================================================

/**
 * 变换状态 — 记录累积的旋转角度、缩放倍数，以及变换顺序
 */
struct TransformState
{
    float rotationDeg;      // 累积旋转角度（度）
    float scaleFactor;      // 累积缩放倍数
    int   matrixOrder;      // 0 = MatrixOrderPrepend, 1 = MatrixOrderAppend
};

// ============================================================================
// 绘制房屋形状
// ============================================================================

/**
 * 在原点 (0,0) 附近绘制一个房屋：
 *   - 矩形主体: (-40, -20) 到 (40, 40)
 *   - 三角形屋顶: (-50, -20) -> (0, -60) -> (50, -20)
 *   - 矩形门: (-10, 10) 到 (10, 40)
 */
static void DrawHouse(Gdiplus::Graphics& graphics)
{
    using namespace Gdiplus;

    // 房屋主体 — 矩形
    SolidBrush bodyBrush(Color(255, 255, 235, 200));     // 米黄色
    Pen bodyPen(Color(255, 139, 90, 43), 2.0f);          // 棕色边框
    graphics.FillRectangle(&bodyBrush, (REAL)(-40), (REAL)(-20), (REAL)(80), (REAL)(60));
    graphics.DrawRectangle(&bodyPen, (REAL)(-40), (REAL)(-20), (REAL)(80), (REAL)(60));

    // 屋顶 — 三角形路径
    GraphicsPath roofPath;
    roofPath.AddLine(-50, -20, 0, -60);
    roofPath.AddLine(0, -60, 50, -20);
    roofPath.CloseFigure();
    SolidBrush roofBrush(Color(255, 220, 80, 60));       // 红色屋顶
    Pen roofPen(Color(255, 160, 40, 30), 2.0f);
    graphics.FillPath(&roofBrush, &roofPath);
    graphics.DrawPath(&roofPen, &roofPath);

    // 门 — 矩形
    SolidBrush doorBrush(Color(255, 139, 69, 19));        // 深棕色门
    graphics.FillRectangle(&doorBrush, (REAL)(-10), (REAL)(10), (REAL)(20), (REAL)(30));
    graphics.DrawRectangle(&bodyPen, (REAL)(-10), (REAL)(10), (REAL)(20), (REAL)(30));

    // 窗户 — 两个小正方形
    SolidBrush windowBrush(Color(255, 173, 216, 230));   // 浅蓝色窗户
    Pen windowPen(Color(255, 100, 100, 100), 1.5f);
    graphics.FillRectangle(&windowBrush, (REAL)(-35), (REAL)(-10), (REAL)(18), (REAL)(18));
    graphics.DrawRectangle(&windowPen, (REAL)(-35), (REAL)(-10), (REAL)(18), (REAL)(18));
    graphics.FillRectangle(&windowBrush, (REAL)(17), (REAL)(-10), (REAL)(18), (REAL)(18));
    graphics.DrawRectangle(&windowPen, (REAL)(17), (REAL)(-10), (REAL)(18), (REAL)(18));

    // 门把手 — 小圆
    SolidBrush knobBrush(Color(255, 255, 215, 0));
    graphics.FillEllipse(&knobBrush, (REAL)(5), (REAL)(22), (REAL)(5), (REAL)(5));
}

// ============================================================================
// 绘制坐标轴辅助线
// ============================================================================

static void DrawAxes(Gdiplus::Graphics& graphics, float length)
{
    using namespace Gdiplus;

    // X 轴 — 红色
    Pen xPen(Color(150, 255, 0, 0), 1.0f);
    xPen.SetDashStyle(DashStyleDash);
    graphics.DrawLine(&xPen, (REAL)-length, (REAL)0, (REAL)length, (REAL)0);

    // Y 轴 — 蓝色
    Pen yPen(Color(150, 0, 0, 255), 1.0f);
    yPen.SetDashStyle(DashStyleDash);
    graphics.DrawLine(&yPen, (REAL)0, (REAL)-length, (REAL)0, (REAL)length);

    // 原点标记
    SolidBrush originBrush(Color(255, 0, 0, 0));
    graphics.FillEllipse(&originBrush, (REAL)(-3), (REAL)(-3), (REAL)(6), (REAL)(6));
}

// ============================================================================
// 绘制信息文字
// ============================================================================

static void DrawInfoText(Gdiplus::Graphics& graphics, const RECT& clientRc,
                         const TransformState& state)
{
    using namespace Gdiplus;

    FontFamily fontFamily(L"微软雅黑");
    Font infoFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    SolidBrush infoBrush(Color(255, 80, 80, 80));

    wchar_t text[256];
    _snwprintf(text, 256,
               L"旋转: %.1f°   缩放: %.2fx   变换顺序: %s",
               state.rotationDeg,
               state.scaleFactor,
               state.matrixOrder == 0 ? L"Prepend(先变换)" : L"Append(后变换)");

    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.DrawString(text, -1, &infoFont,
                        PointF(10.0f, 10.0f), &infoBrush);

    // 说明变换顺序的区别
    Font smallFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    SolidBrush hintBrush(Color(255, 120, 120, 120));

    const wchar_t* hint = L"Prepend: 新变换先生效 (M_new * M_old)  |  "
                          L"Append: 旧变换先生效 (M_old * M_new)";
    graphics.DrawString(hint, -1, &smallFont,
                        PointF(10.0f, 30.0f), &hintBrush);
}

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 初始化变换状态
        TransformState* state = new TransformState();
        state->rotationDeg = 0.0f;
        state->scaleFactor = 1.0f;
        state->matrixOrder = 0;     // 默认 Prepend
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 创建控制按钮
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        int btnY = 5;
        int btnW = 80;
        int btnH = 32;
        int btnX = 5;

        CreateWindowEx(0, L"BUTTON", L"旋转 +30°",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, btnY, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_ROTATE,
            cs->hInstance, nullptr);
        btnX += btnW + 5;

        CreateWindowEx(0, L"BUTTON", L"缩放 x1.5",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, btnY, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_SCALE,
            cs->hInstance, nullptr);
        btnX += btnW + 5;

        CreateWindowEx(0, L"BUTTON", L"重置",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            btnX, btnY, btnW, btnH,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_RESET,
            cs->hInstance, nullptr);
        btnX += btnW + 10;

        // 变换顺序切换按钮 — 使用复选框风格
        CreateWindowEx(0, L"BUTTON", L"Append 模式",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            btnX, btnY + 5, 120, btnH - 10,
            hwnd, (HMENU)200,
            cs->hInstance, nullptr);

        return 0;
    }

    case WM_DESTROY:
    {
        TransformState* state = (TransformState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_COMMAND:
    {
        TransformState* state = (TransformState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state) return 0;

        switch (LOWORD(wParam))
        {
        case ID_BTN_ROTATE:
            state->rotationDeg += 30.0f;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case ID_BTN_SCALE:
            state->scaleFactor *= 1.5f;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case ID_BTN_RESET:
            state->rotationDeg = 0.0f;
            state->scaleFactor = 1.0f;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        // 检查 Append 模式复选框
        HWND hCheck = GetDlgItem(hwnd, 200);
        if (hCheck)
        {
            LRESULT checkState = SendMessage(hCheck, BM_GETCHECK, 0, 0);
            state->matrixOrder = (checkState == BST_CHECKED) ? 1 : 0;
        }

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        TransformState* state = (TransformState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state)
        {
            EndPaint(hwnd, &ps);
            return 0;
        }

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 使用 GDI 填充白色背景
        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        using namespace Gdiplus;

        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

        // ---- 绘制信息文字（变换前，使用原始坐标系）----
        DrawInfoText(graphics, clientRc, *state);

        // ---- 应用坐标变换 ----
        // 计算画布中心作为平移目标
        float centerX = (float)(clientRc.right / 2);
        float centerY = (float)(clientRc.bottom / 2 + 20);

        // 创建变换矩阵
        Matrix transform;

        if (state->matrixOrder == 0)
        {
            // Prepend 模式: 先平移到中心，再缩放，再旋转
            // MatrixOrderPrepend 意味着新的变换放在矩阵乘法的左边
            // 实际执行顺序（从右到左）: 旋转 -> 缩放 -> 平移
            transform.Translate(centerX, centerY, MatrixOrderPrepend);
            transform.Scale(state->scaleFactor, state->scaleFactor, MatrixOrderPrepend);
            transform.Rotate(state->rotationDeg, MatrixOrderPrepend);
        }
        else
        {
            // Append 模式: 先旋转，再缩放，再平移
            // MatrixOrderAppend 意味着新的变换放在矩阵乘法的右边
            // 实际执行顺序（从左到右）: 旋转 -> 缩放 -> 平移
            transform.Rotate(state->rotationDeg, MatrixOrderAppend);
            transform.Scale(state->scaleFactor, state->scaleFactor, MatrixOrderAppend);
            transform.Translate(centerX, centerY, MatrixOrderAppend);
        }

        graphics.SetTransform(&transform);

        // ---- 绘制坐标轴 ----
        float axisLen = 120.0f;
        DrawAxes(graphics, axisLen);

        // ---- 绘制房屋 ----
        DrawHouse(graphics);

        // ---- 绘制原始位置参考（灰色虚影）----
        // 重置变换，仅在中心位置绘制原始位置的虚影
        graphics.ResetTransform();
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        // 原始位置参考：在中心绘制虚线边框
        Pen ghostPen(Color(80, 180, 180, 180), 1.0f);
        ghostPen.SetDashStyle(DashStyleDot);
        graphics.DrawRectangle(&ghostPen,
                               (REAL)(centerX - 50), (REAL)(centerY - 60), (REAL)100, (REAL)100);

        // 标注原始位置
        FontFamily fontFamily(L"微软雅黑");
        Font ghostFont(&fontFamily, 9, FontStyleRegular, UnitPixel);
        SolidBrush ghostBrush(Color(255, 180, 180, 180));
        graphics.DrawString(L"原始位置", -1, &ghostFont,
                            PointF(centerX - 20, centerY + 45), &ghostBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册窗口类
// ============================================================================

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 创建主窗口
// ============================================================================

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        WINDOW_CLASS_NAME,              // 窗口类名称
        L"GDI+ 坐标变换示例",            // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500,                       // 窗口大小
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr)
        return nullptr;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

// ============================================================================
// 程序入口
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    // ---- GDI+ 初始化 ----
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    if (RegisterWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // ---- GDI+ 清理 ----
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
