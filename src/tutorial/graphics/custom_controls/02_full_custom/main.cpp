/**
 * 02_full_custom - 完全自绘控件示例
 *
 * 本程序演示了如何创建一个完全自定义的 Win32 控件：
 * 1. RegisterClassEx 注册自定义窗口类 ("ColorPickerCtrl")
 * 2. 自定义 WndProc 独立处理所有消息
 * 3. WM_PAINT — 使用 GDI Pie 函数绘制 HSL 色轮
 * 4. WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_LBUTTONUP — 点击和拖拽选色
 * 5. WM_GETDLGCODE — 返回 DLGC_WANTARROWS | DLGC_WANTCHARS 支持键盘
 * 6. 自定义通知: SendMessage(WM_USER+1) 通知父窗口颜色变化
 *
 * 参考: tutorial/native_win32/49_ProgrammingGUI_Graphics_CustomCtrl_FullCustom.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <cmath>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* MAIN_WINDOW_CLASS = L"FullCustomDemoClass";
static const wchar_t* COLOR_PICKER_CLASS = L"ColorPickerCtrl";

// 自定义消息：颜色选择控件通知父窗口颜色已改变
static const UINT WM_COLOR_CHANGED = WM_USER + 1;

// 控件和子窗口 ID
static const int ID_COLOR_PICKER = 101;
static const int ID_LABEL_RGB    = 201;
static const int ID_LABEL_HSL    = 202;
static const int ID_LABEL_INFO   = 203;

// ============================================================================
// HSL 与 RGB 颜色转换
// ============================================================================

/**
 * HSL 转 RGB
 * @param h 色相 [0, 360)
 * @param s 饱和度 [0, 1]
 * @param l 亮度 [0, 1]
 * @return COLORREF (RGB)
 */
static COLORREF HSLtoRGB(double h, double s, double l)
{
    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = l - c / 2.0;

    double r = 0, g = 0, b = 0;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    int ri = (int)((r + m) * 255.0 + 0.5);
    int gi = (int)((g + m) * 255.0 + 0.5);
    int bi = (int)((b + m) * 255.0 + 0.5);

    if (ri < 0) ri = 0; if (ri > 255) ri = 255;
    if (gi < 0) gi = 0; if (gi > 255) gi = 255;
    if (bi < 0) bi = 0; if (bi > 255) bi = 255;

    return RGB(ri, gi, bi);
}

/**
 * RGB 转 HSL
 * 返回值通过指针参数输出 h, s, l
 */
static void RGBtoHSL(COLORREF color, double* h, double* s, double* l)
{
    double r = GetRValue(color) / 255.0;
    double g = GetGValue(color) / 255.0;
    double b = GetBValue(color) / 255.0;

    double maxVal = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double minVal = r < g ? (r < b ? r : b) : (g < b ? g : b);

    *l = (maxVal + minVal) / 2.0;

    if (maxVal == minVal)
    {
        *h = 0;
        *s = 0;
    }
    else
    {
        double d = maxVal - minVal;
        *s = (*l > 0.5) ? (d / (2.0 - maxVal - minVal)) : (d / (maxVal + minVal));

        if (maxVal == r)
            *h = fmod((g - b) / d + (g < b ? 6.0 : 0.0), 6.0) * 60.0;
        else if (maxVal == g)
            *h = ((b - r) / d + 2.0) * 60.0;
        else
            *h = ((r - g) / d + 4.0) * 60.0;
    }
}

// ============================================================================
// ColorPickerCtrl 控件的内部状态
// ============================================================================

struct ColorPickerState
{
    double  hue;            // 当前选中的色相 [0, 360)
    bool    dragging;       // 是否正在拖拽
    int     centerX;        // 色轮中心 X
    int     centerY;        // 色轮中心 Y
    int     radius;         // 色轮半径
};

// ============================================================================
// ColorPickerCtrl 的窗口过程
// ============================================================================

/**
 * 自定义色轮控件的窗口过程
 *
 * 这是一个完全独立的 WndProc，拥有自己的消息处理逻辑。
 * 注册到 "ColorPickerCtrl" 窗口类后，可以像标准控件一样创建使用。
 */
LRESULT CALLBACK ColorPickerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ColorPickerState* state = (ColorPickerState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_NCCREATE:
    {
        // 在窗口创建时分配状态结构体
        state = new ColorPickerState();
        state->hue = 0.0;
        state->dragging = false;
        state->centerX = 0;
        state->centerY = 0;
        state->radius = 0;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return TRUE;
    }

    case WM_NCDESTROY:
    {
        // 窗口销毁时释放状态
        if (state) delete state;
        return 0;
    }

    case WM_GETDLGCODE:
    {
        // 告诉对话框管理器：此控件需要处理箭头键和字符消息
        // 这样键盘事件不会被对话框管理器拦截
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }

    case WM_SIZE:
    {
        // 窗口大小变化时更新色轮参数
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        state->centerX = w / 2;
        state->centerY = h / 2;
        state->radius = (w < h ? w : h) / 2 - 15;
        if (state->radius < 10) state->radius = 10;
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int savedState = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充控件背景（浅灰色）
        HBRUSH hBgBrush = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &clientRc, hBgBrush);
        DeleteObject(hBgBrush);

        int cx = state->centerX;
        int cy = state->centerY;
        int r  = state->radius;

        // ========== 绘制色轮 ==========
        // 将 360 度分为 36 个扇形 (每个 10 度)，使用 Pie 函数绘制
        int segments = 36;
        double segAngle = 360.0 / segments;

        for (int i = 0; i < segments; ++i)
        {
            double h1 = i * segAngle;
            double h2 = (i + 1) * segAngle;

            // 扇形颜色：纯色相（饱和度=1，亮度=0.5）
            COLORREF segColor = HSLtoRGB(h1, 1.0, 0.5);
            HBRUSH hSegBrush = CreateSolidBrush(segColor);
            HPEN hSegPen = CreatePen(PS_SOLID, 1, segColor);

            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hSegBrush);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hSegPen);

            // Pie 函数使用 Windows 坐标系（Y 轴向下，角度从右侧逆时针）
            // 将 HSL 角度转换为 Windows 角度：Windows 中 0 度在右，逆时针为正
            // GDI Pie 的角度以 1/10 度为单位
            int startAngle = (int)(h1 * 10.0);
            int sweepAngle = (int)(segAngle * 10.0);

            Pie(hdc,
                cx - r, cy - r,       // 边界矩形左上角
                cx + r, cy + r,       // 边界矩形右下角
                cx + (int)(r * cos(h1 * 3.14159265 / 180.0)),   // 弧线起点 X
                cy - (int)(r * sin(h1 * 3.14159265 / 180.0)),   // 弧线起点 Y
                cx + (int)(r * cos(h2 * 3.14159265 / 180.0)),   // 弧线终点 X
                cy - (int)(r * sin(h2 * 3.14159265 / 180.0))    // 弧线终点 Y
            );

            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hSegBrush);
            DeleteObject(hSegPen);
        }

        // 在色轮中心绘制白色圆（形成环形效果）
        int innerR = r / 3;
        HBRUSH hWhiteBrush = CreateSolidBrush(RGB(245, 245, 245));
        HPEN hWhitePen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hdc, hWhiteBrush);
        HPEN hOldPen2 = (HPEN)SelectObject(hdc, hWhitePen);
        Ellipse(hdc, cx - innerR, cy - innerR, cx + innerR, cy + innerR);
        SelectObject(hdc, hOldBrush2);
        SelectObject(hdc, hOldPen2);
        DeleteObject(hWhiteBrush);
        DeleteObject(hWhitePen);

        // ========== 绘制选中指示器 ==========
        double selRad = state->hue * 3.14159265 / 180.0;
        int indicatorR = (r + innerR) / 2;   // 指示器在环的中部
        int selX = cx + (int)(indicatorR * cos(selRad));
        int selY = cy - (int)(indicatorR * sin(selRad));

        // 外圈白色边框
        HBRUSH hSelBrush = CreateSolidBrush(RGB(255, 255, 255));
        HPEN hSelPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        HBRUSH hOldBrush3 = (HBRUSH)SelectObject(hdc, hSelBrush);
        HPEN hOldPen3 = (HPEN)SelectObject(hdc, hSelPen);
        Ellipse(hdc, selX - 10, selY - 10, selX + 10, selY + 10);
        SelectObject(hdc, hOldBrush3);
        SelectObject(hdc, hOldPen3);
        DeleteObject(hSelBrush);
        DeleteObject(hSelPen);

        // 内圈显示选中颜色
        COLORREF selColor = HSLtoRGB(state->hue, 1.0, 0.5);
        HBRUSH hSelColorBrush = CreateSolidBrush(selColor);
        HPEN hSelColorPen = CreatePen(PS_SOLID, 1, selColor);
        HBRUSH hOldBrush4 = (HBRUSH)SelectObject(hdc, hSelColorBrush);
        HPEN hOldPen4 = (HPEN)SelectObject(hdc, hSelColorPen);
        Ellipse(hdc, selX - 7, selY - 7, selX + 7, selY + 7);
        SelectObject(hdc, hOldBrush4);
        SelectObject(hdc, hOldPen4);
        DeleteObject(hSelColorBrush);
        DeleteObject(hSelColorPen);

        // 绘制边框
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        HPEN hOldBrdPen = (HPEN)SelectObject(hdc, hBorderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, clientRc.left, clientRc.top, clientRc.right, clientRc.bottom);
        SelectObject(hdc, hOldBrdPen);
        DeleteObject(hBorderPen);

        RestoreDC(hdc, savedState);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // 鼠标按下：计算点击位置对应的色相
        int x = (SHORT)LOWORD(lParam);
        int y = (SHORT)HIWORD(lParam);

        int dx = x - state->centerX;
        int dy = -(y - state->centerY);   // Y 轴翻转（数学坐标系）
        double dist = sqrt((double)(dx * dx + dy * dy));

        int innerR = state->radius / 3;
        int midR = (state->radius + innerR) / 2;

        // 只有点击在色轮环区域内才响应
        if (dist >= innerR - 5 && dist <= state->radius + 5)
        {
            // 计算角度 [0, 360)
            double angle = atan2((double)dy, (double)dx) * 180.0 / 3.14159265;
            if (angle < 0) angle += 360.0;

            state->hue = angle;
            state->dragging = true;
            SetCapture(hwnd);   // 捕获鼠标，确保拖拽时不会丢失消息

            // 通知父窗口颜色已改变
            HWND hParent = GetParent(hwnd);
            if (hParent)
            {
                SendMessage(hParent, WM_COLOR_CHANGED, (WPARAM)hwnd, (LPARAM)0);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
        }

        // 设置焦点到本控件
        SetFocus(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        // 鼠标移动：如果正在拖拽，更新色相
        if (state && state->dragging)
        {
            int x = (SHORT)LOWORD(lParam);
            int y = (SHORT)HIWORD(lParam);

            int dx = x - state->centerX;
            int dy = -(y - state->centerY);

            double angle = atan2((double)dy, (double)dx) * 180.0 / 3.14159265;
            if (angle < 0) angle += 360.0;

            state->hue = angle;

            // 通知父窗口
            HWND hParent = GetParent(hwnd);
            if (hParent)
            {
                SendMessage(hParent, WM_COLOR_CHANGED, (WPARAM)hwnd, (LPARAM)0);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        // 鼠标释放：结束拖拽
        if (state && state->dragging)
        {
            state->dragging = false;
            ReleaseCapture();  // 释放鼠标捕获
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        // 键盘操作：左右箭头键微调色相
        if (state)
        {
            switch (wParam)
            {
            case VK_LEFT:
                state->hue -= 2.0;
                if (state->hue < 0) state->hue += 360.0;
                break;
            case VK_RIGHT:
                state->hue += 2.0;
                if (state->hue >= 360.0) state->hue -= 360.0;
                break;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }

            HWND hParent = GetParent(hwnd);
            if (hParent)
            {
                SendMessage(hParent, WM_COLOR_CHANGED, (WPARAM)hwnd, (LPARAM)0);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册自定义控件窗口类
// ============================================================================

ATOM RegisterColorPickerClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc   = ColorPickerProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = nullptr;
    wc.hCursor       = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = COLOR_PICKER_CLASS;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 主窗口过程
// ============================================================================

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;

        // 创建自定义色轮控件
        // 就像创建标准控件一样，使用自定义的窗口类名
        CreateWindowEx(
            0,
            COLOR_PICKER_CLASS,        // 自定义窗口类
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            15, 15,                    // 位置
            280, 280,                  // 大小
            hwnd,
            (HMENU)(LONG_PTR)ID_COLOR_PICKER,
            cs->hInstance,
            nullptr
        );

        // 创建颜色预览区域边框标签
        CreateWindowEx(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
            310, 15, 220, 80,
            hwnd, (HMENU)(LONG_PTR)ID_LABEL_INFO,
            cs->hInstance, nullptr
        );

        // 创建 RGB 数值标签
        CreateWindowEx(
            0, L"STATIC", L"RGB: (255, 0, 0)",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            310, 110, 220, 25,
            hwnd, (HMENU)(LONG_PTR)ID_LABEL_RGB,
            cs->hInstance, nullptr
        );

        // 创建 HSL 数值标签
        CreateWindowEx(
            0, L"STATIC", L"HSL: (0°, 100%, 50%)",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            310, 140, 220, 25,
            hwnd, (HMENU)(LONG_PTR)ID_LABEL_HSL,
            cs->hInstance, nullptr
        );

        // 设置标签字体
        HFONT hFont = CreateFont(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        SendDlgItemMessage(hwnd, ID_LABEL_RGB,  WM_SETFONT, (WPARAM)hFont, TRUE);
        SendDlgItemMessage(hwnd, ID_LABEL_HSL,  WM_SETFONT, (WPARAM)hFont, TRUE);

        // 初始触发一次颜色通知
        HWND hPicker = GetDlgItem(hwnd, ID_COLOR_PICKER);
        if (hPicker)
        {
            SendMessage(hPicker, WM_LBUTTONDOWN, 0, MAKELPARAM(0, 0));
        }

        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_COLOR_CHANGED:
    {
        // 接收自定义控件的颜色变化通知
        // wParam = 控件句柄, lParam = 保留
        HWND hPicker = GetDlgItem(hwnd, ID_COLOR_PICKER);
        if (!hPicker) return 0;

        // 从控件状态中读取当前色相
        ColorPickerState* state =
            (ColorPickerState*)GetWindowLongPtr(hPicker, GWLP_USERDATA);
        if (!state) return 0;

        double hue = state->hue;
        COLORREF color = HSLtoRGB(hue, 1.0, 0.5);

        int r = GetRValue(color);
        int g = GetGValue(color);
        int b = GetBValue(color);

        // 更新 RGB 标签
        wchar_t rgbText[64];
        wsprintf(rgbText, L"RGB: (%d, %d, %d)", r, g, b);
        SetDlgItemText(hwnd, ID_LABEL_RGB, rgbText);

        // 更新 HSL 标签
        wchar_t hslText[64];
        wsprintf(hslText, L"HSL: (%d°, 100%%, 50%%)", (int)hue);
        SetDlgItemText(hwnd, ID_LABEL_HSL, hslText);

        // 重绘颜色预览区
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        int savedState = SaveDC(hdc);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充窗口背景
        HBRUSH hBg = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        // 获取当前选中的颜色
        HWND hPicker = GetDlgItem(hwnd, ID_COLOR_PICKER);
        ColorPickerState* pickerState = nullptr;
        if (hPicker)
            pickerState = (ColorPickerState*)GetWindowLongPtr(hPicker, GWLP_USERDATA);

        if (pickerState)
        {
            COLORREF selColor = HSLtoRGB(pickerState->hue, 1.0, 0.5);

            // 绘制颜色预览矩形
            RECT previewRc = { 315, 20, 525, 85 };
            HBRUSH hColorBrush = CreateSolidBrush(selColor);
            FillRect(hdc, &previewRc, hColorBrush);
            DeleteObject(hColorBrush);

            // 在预览框内显示 "选中颜色" 文字
            SetTextColor(hdc,
                (GetRValue(selColor) + GetGValue(selColor) + GetBValue(selColor)) > 380
                    ? RGB(0, 0, 0) : RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);

            HFONT hPreviewFont = CreateFont(
                16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hPreviewFont);
            DrawText(hdc, L"选中颜色", -1, &previewRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);
            DeleteObject(hPreviewFont);
        }

        // 底部说明文字
        SetTextColor(hdc, RGB(100, 100, 100));
        SetBkMode(hdc, TRANSPARENT);

        HFONT hNoteFont = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
        );
        HFONT hOldFont2 = (HFONT)SelectObject(hdc, hNoteFont);

        RECT noteRc = { 15, 310, clientRc.right - 15, clientRc.bottom - 10 };
        DrawText(hdc,
            L"完全自绘控件演示:\n"
            L"• RegisterClassEx 注册自定义窗口类 \"ColorPickerCtrl\"\n"
            L"• WM_PAINT 使用 GDI Pie 绘制 HSL 色轮\n"
            L"• WM_LBUTTONDOWN/WM_MOUSEMOVE/WM_LBUTTONUP 实现拖拽选色\n"
            L"• WM_GETDLGCODE 返回 DLGC_WANTARROWS 支持键盘操作\n"
            L"• SendMessage(WM_USER+1) 自定义通知父窗口颜色变化\n"
            L"• 点击或拖拽色轮环选择颜色，左右箭头键微调",
            -1, &noteRc, DT_LEFT | DT_WORDBREAK);

        SelectObject(hdc, hOldFont2);
        DeleteObject(hNoteFont);

        RestoreDC(hdc, savedState);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ============================================================================
// 注册主窗口类
// ============================================================================

ATOM RegisterMainWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    return RegisterClassEx(&wc);
}

// ============================================================================
// 创建主窗口
// ============================================================================

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,                              // 扩展窗口样式
        MAIN_WINDOW_CLASS,              // 窗口类名称
        L"完全自绘控件示例",             // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        550, 450,                       // 窗口大小
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

    // 先注册自定义控件窗口类
    if (RegisterColorPickerClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"自定义控件类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 再注册主窗口类
    if (RegisterMainWindowClass(hInstance) == 0)
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (hwnd == nullptr)
    {
        MessageBox(nullptr, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
