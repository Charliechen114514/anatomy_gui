/**
 * 05_bitmap_operations - GDI 位图操作示例
 *
 * 演示 GDI 位图相关的核心 API：
 * 1. CreateCompatibleBitmap + CreateCompatibleDC — 创建离屏位图与兼容内存 DC
 * 2. 在内存 DC 上绘制彩色矩形图案
 * 3. BitBlt — 将内存 DC 内容复制到屏幕 DC（SRCCOPY）
 * 4. StretchBlt — 将小位图拉伸到更大区域
 * 5. CreateDIBSection — 创建可直接访问像素的 DIB，填充渐变色
 * 6. PatBlt — 使用图案画刷填充矩形
 *
 * 窗口分为 3 个面板："BitBlt 复制"、"StretchBlt 缩放"、"DIB 像素操作"
 *
 * 参考: tutorial/native_win32/22_ProgrammingGUI_NativeWindows_GDI_Bitmap.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"GdiBitmapOpsClass";

// 窗口尺寸
static const int WINDOW_WIDTH  = 700;
static const int WINDOW_HEIGHT = 450;

// 每个面板的间距和标签高度
static const int LABEL_HEIGHT = 28;
static const int PADDING      = 8;

/**
 * 在内存 DC 上绘制一组彩色矩形图案
 *
 * 参数:
 *   hdcMem  - 内存设备上下文
 *   width   - 绘制区域宽度
 *   height  - 绘制区域高度
 */
static void DrawPatternOnMemDC(HDC hdcMem, int width, int height)
{
    // 定义 6 种颜色
    COLORREF colors[] = {
        RGB(220,  50,  50),   // 红
        RGB( 50, 180,  50),   // 绿
        RGB( 50,  80, 220),   // 蓝
        RGB(220, 200,  40),   // 黄
        RGB(200,  50, 200),   // 紫
        RGB( 40, 200, 200),   // 青
    };

    // 计算每个矩形的尺寸（3 列 x 2 行）
    int cols = 3;
    int rows = 2;
    int cellW = width  / cols;
    int cellH = height / rows;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int index = r * cols + c;
            // 创建纯色画刷
            HBRUSH hBrush = CreateSolidBrush(colors[index]);
            RECT rc = {
                c * cellW,           // left
                r * cellH,           // top
                (c + 1) * cellW,     // right
                (r + 1) * cellH      // bottom
            };
            // 用画刷填充矩形
            FillRect(hdcMem, &rc, hBrush);
            DeleteObject(hBrush);  // 用完即删
        }
    }
}

/**
 * 创建一幅渐变色的 DIB 位图
 *
 * 使用 CreateDIBSection 创建 32 位 DIB，直接写入像素数据。
 * 生成从左到右的红-绿渐变，从上到下的亮度变化。
 *
 * 参数:
 *   width  - 位图宽度
 *   height - 位图高度
 *
 * 返回:
 *   HBITMAP 位图句柄，需调用者 DeleteObject 释放
 */
static HBITMAP CreateGradientDIB(int width, int height)
{
    // 定义 BITMAPINFO 结构（包含一个颜色项的空间）
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = height;  // 正值 = 底朝上 DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;      // 每像素 32 位（BGRA）
    bmi.bmiHeader.biCompression = BI_RGB;

    // 指向像素数据的指针，由系统分配
    BYTE* pBits = nullptr;

    // CreateDIBSection 创建可直接访问像素的 DIB
    // 第 3 个参数 DIB_RGB_COLORS 表示颜色表使用 RGB 值
    HBITMAP hBitmap = CreateDIBSection(
        nullptr,            // 设备上下文（DIB_RGB_COLORS 时可为 nullptr）
        &bmi,               // 位图信息
        DIB_RGB_COLORS,     // 颜色格式
        (void**)&pBits,     // 接收像素数据指针
        nullptr,            // 文件映射句柄（nullptr 表示系统分配）
        0                   // 文件映射偏移
    );

    if (hBitmap && pBits)
    {
        // 直接操作像素数据 —— 逐像素写入渐变色
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                // 计算渐变比例
                float fx = (float)x / (float)(width  - 1);   // 0.0 ~ 1.0
                float fy = (float)y / (float)(height - 1);   // 0.0 ~ 1.0

                // 红色通道：从左到右 0->255
                BYTE blue  = (BYTE)(fx * 255.0f);
                // 绿色通道：从左到右 255->0
                BYTE green = (BYTE)((1.0f - fx) * 255.0f);
                // 蓝色通道：从上到下 0->255
                BYTE red   = (BYTE)(fy * 255.0f);

                // DIB 底朝上存储，第一行是图像最底行
                // 像素格式为 BGRA（每像素 4 字节）
                int row = height - 1 - y;  // 翻转行号使渐变方向符合直觉
                int offset = (row * width + x) * 4;
                pBits[offset + 0] = blue;   // Blue
                pBits[offset + 1] = green;  // Green
                pBits[offset + 2] = red;    // Red
                pBits[offset + 3] = 255;    // Alpha（不透明）
            }
        }
    }

    return hBitmap;
}

/**
 * 绘制面板标题文字
 */
static void DrawPanelTitle(HDC hdc, const wchar_t* text, int x, int y, int width)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(40, 40, 40));

    // 创建加粗字体
    HFONT hFont = CreateFont(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT rc = { x, y, x + width, y + LABEL_HEIGHT };
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/**
 * 窗口过程函数
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int clientW = rcClient.right;
        int clientH = rcClient.bottom;

        // 计算三个面板的布局
        int panelW = (clientW - 4 * PADDING) / 3;  // 三个等宽面板
        int drawH  = clientH - LABEL_HEIGHT - 2 * PADDING;  // 可绘制高度

        // ============================================================
        // 面板 1: BitBlt 复制
        // ============================================================
        {
            int px = PADDING;
            int py = PADDING;

            // 绘制标题
            DrawPanelTitle(hdc, L"BitBlt 复制", px, py, panelW);
            py += LABEL_HEIGHT;

            // --- 创建兼容内存 DC 和位图 ---
            // CreateCompatibleDC 创建与指定 DC 兼容的内存 DC
            HDC hdcMem = CreateCompatibleDC(hdc);
            // CreateCompatibleBitmap 创建与指定 DC 兼容的位图
            HBITMAP hBmp = CreateCompatibleBitmap(hdc, panelW, drawH);
            // 将位图选入内存 DC（内存 DC 默认只有 1x1 单色位图）
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

            // 在内存 DC 上绘制彩色矩形图案
            DrawPatternOnMemDC(hdcMem, panelW, drawH);

            // --- BitBlt: 将内存 DC 内容复制到屏幕 DC ---
            // SRCCOPY 表示直接复制源像素到目标
            BitBlt(
                hdc,        // 目标 DC（屏幕）
                px, py,     // 目标位置
                panelW,     // 宽度
                drawH,      // 高度
                hdcMem,     // 源 DC（内存）
                0, 0,       // 源位置
                SRCCOPY     // 光栅操作码：直接复制
            );

            // 清理 GDI 对象
            SelectObject(hdcMem, hOldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdcMem);
        }

        // ============================================================
        // 面板 2: StretchBlt 缩放
        // ============================================================
        {
            int px = PADDING * 2 + panelW;
            int py = PADDING;

            DrawPanelTitle(hdc, L"StretchBlt 缩放", px, py, panelW);
            py += LABEL_HEIGHT;

            // 创建一个较小的位图（面板尺寸的 1/3）
            int smallW = panelW / 3;
            int smallH = drawH  / 3;

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hBmp = CreateCompatibleBitmap(hdc, smallW, smallH);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

            // 在小位图上绘制图案
            DrawPatternOnMemDC(hdcMem, smallW, smallH);

            // --- StretchBlt: 将小位图拉伸到完整面板尺寸 ---
            // 将源矩形 (0,0,smallW,smallH) 拉伸到目标矩形 (px,py,panelW,drawH)
            SetStretchBltMode(hdc, HALFTONE);  // 设置拉伸模式以获得更好质量
            StretchBlt(
                hdc,        // 目标 DC
                px, py,     // 目标左上角
                panelW, drawH,  // 目标尺寸
                hdcMem,     // 源 DC
                0, 0,       // 源左上角
                smallW, smallH, // 源尺寸
                SRCCOPY     // 光栅操作码
            );

            // 在右下角显示原始大小的对比
            RECT rcSmall = {
                px + panelW - smallW - 4,
                py + drawH - smallH - 4,
                px + panelW - 4,
                py + drawH - 4
            };
            // 绘制白色背景边框以区分
            HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
            FillRect(hdc, &rcSmall, hWhite);
            // 直接复制原始大小
            BitBlt(hdc, rcSmall.left + 2, rcSmall.top + 2,
                   smallW - 4, smallH - 4, hdcMem, 0, 0, SRCCOPY);
            // 绘制边框
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rcSmall.left, rcSmall.top, rcSmall.right, rcSmall.bottom);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            SelectObject(hdcMem, hOldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdcMem);
        }

        // ============================================================
        // 面板 3: DIB 像素操作（CreateDIBSection + PatBlt）
        // ============================================================
        {
            int px = PADDING * 3 + panelW * 2;
            int py = PADDING;

            DrawPanelTitle(hdc, L"DIB 像素操作", px, py, panelW);
            py += LABEL_HEIGHT;

            // --- CreateDIBSection: 创建可直接访问像素的 DIB ---
            int dibW = panelW;
            int dibH = drawH / 2 - 4;  // 上半部分
            HBITMAP hDIB = CreateGradientDIB(dibW, dibH);

            if (hDIB)
            {
                // 将 DIB 选入内存 DC 并复制到屏幕
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hDIB);
                BitBlt(hdc, px, py, dibW, dibH, hdcMem, 0, 0, SRCCOPY);
                SelectObject(hdcMem, hOldBmp);
                DeleteObject(hDIB);
                DeleteDC(hdcMem);
            }

            py += dibH + 8;

            // --- PatBlt: 使用图案画刷填充矩形 ---
            // 创建一个 8x8 的图案位图（棋盘格）
            WORD patternBits[8] = {
                0xAAAA,  // 10101010
                0x5555,  // 01010101
                0xAAAA,
                0x5555,
                0xAAAA,
                0x5555,
                0xAAAA,
                0x5555,
            };
            HBITMAP hPattern = CreateBitmap(8, 8, 1, 1, patternBits);
            // 创建图案画刷
            HBRUSH hPatBrush = CreatePatternBrush(hPattern);

            // 选入画刷并设置背景色
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hPatBrush);
            SetBkColor(hdc, RGB(255, 220, 100));       // 图案中的"1"像素颜色
            SetTextColor(hdc, RGB(180, 100, 40));       // 图案中的"0"像素颜色

            // PatBlt 使用当前选入的画刷来填充指定矩形
            // PATCOPY: 将图案复制到目标
            PatBlt(hdc, px, py, panelW, drawH - dibH - 8, PATCOPY);

            // 恢复并清理
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPatBrush);
            DeleteObject(hPattern);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/**
 * 注册窗口类
 */
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassEx(&wc);
}

/**
 * 创建主窗口
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    HWND hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"GDI 位图操作示例",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd)
    {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

/**
 * wWinMain - 应用程序入口点
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    if (!RegisterWindowClass(hInstance))
    {
        MessageBox(nullptr, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd)
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
