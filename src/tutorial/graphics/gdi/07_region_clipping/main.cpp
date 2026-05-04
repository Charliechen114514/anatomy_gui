/**
 * 07_region_clipping - GDI Region 裁切示例
 *
 * 演示 GDI 区域 (Region) 和裁切 (Clipping) 的核心 API：
 * 1. CreateRectRgn — 矩形区域，将绘制裁切到矩形内
 * 2. CreateEllipticRgn — 椭圆区域，绘制星形图案裁切到圆形内
 * 3. CreatePolygonRgn — 多边形（三角形）区域
 * 4. CombineRgn — 区域组合操作：RGN_AND / RGN_OR / RGN_XOR / RGN_DIFF
 * 5. SelectClipRgn / ExtSelectClipRgn — 在 HDC 上设置裁切区域
 * 6. PtInRegion — 点击测试鼠标是否在区域内，显示结果
 *
 * 窗口分为 4 个象限：矩形裁切、椭圆裁切、多边形裁切、CombineRgn 演示
 * 底部：可点击的区域测试区域
 *
 * 参考: tutorial/native_win32/24_ProgrammingGUI_Graphics_GDI_Region.md
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <algorithm>
#include <cmath>

// 窗口类名称
static const wchar_t* WINDOW_CLASS_NAME = L"GdiRegionClipClass";

// 窗口尺寸
static const int WINDOW_WIDTH  = 650;
static const int WINDOW_HEIGHT = 550;

// 全局区域句柄（用于底部 PtInRegion 测试）
static HRGN g_hTestRgnRect    = nullptr;
static HRGN g_hTestRgnEllipse = nullptr;
static HRGN g_hTestRgnTriangle = nullptr;

// 上次 PtInRegion 的测试结果
static int g_lastHitResult = -1;  // -1=无, 0=矩形, 1=椭圆, 2=三角形

/**
 * 绘制面板标题
 */
static void DrawPanelTitle(HDC hdc, const wchar_t* text, int x, int y, int width)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(30, 30, 30));

    HFONT hFont = CreateFont(
        15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT rc = { x, y, x + width, y + 22 };
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/**
 * 绘制完整的网格背景（用于展示裁切效果）
 *
 * 在整个区域内绘制网格线，裁切区域外的部分不会被显示。
 */
static void DrawGridBackground(HDC hdc, int x, int y, int w, int h)
{
    // 浅灰背景
    HBRUSH hBg = CreateSolidBrush(RGB(245, 245, 245));
    RECT rc = { x, y, x + w, y + h };
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    // 网格线
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    for (int i = x; i <= x + w; i += 15)
    {
        MoveToEx(hdc, i, y, nullptr);
        LineTo(hdc, i, y + h);
    }
    for (int j = y; j <= y + h; j += 15)
    {
        MoveToEx(hdc, x, j, nullptr);
        LineTo(hdc, x + w, j);
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

/**
 * 绘制彩色对角线条纹（用于展示裁切效果）
 */
static void DrawDiagonalStripes(HDC hdc, int x, int y, int w, int h)
{
    COLORREF colors[] = {
        RGB(255, 120, 120),
        RGB(120, 255, 120),
        RGB(120, 120, 255),
        RGB(255, 255, 120),
    };
    int numColors = 4;

    for (int i = -h; i < w + h; i += 12)
    {
        HBRUSH hBrush = CreateSolidBrush(colors[((i + 1000) / 12) % numColors]);
        HPEN hPen = CreatePen(PS_SOLID, 1, colors[((i + 1000) / 12) % numColors]);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

        POINT pts[4] = {
            { x + i,     y },
            { x + i + 6, y },
            { x + i + 6 - h, y + h },
            { x + i - h,     y + h }
        };
        Polygon(hdc, pts, 4);

        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hBrush);
        DeleteObject(hPen);
    }
}

/**
 * 绘制星形图案
 */
static void DrawStarPattern(HDC hdc, int cx, int cy, int outerR, int innerR)
{
    COLORREF colors[] = {
        RGB(255, 215, 0),    // 金色
        RGB(255, 140, 0),    // 橙色
        RGB(255, 69, 0),     // 红橙
        RGB(255, 99, 71),    // 番茄色
        RGB(255, 160, 122),  // 浅鲑鱼色
    };

    for (int i = 0; i < 5; i++)
    {
        double angle1 = (i * 72.0 - 90.0) * 3.14159265 / 180.0;
        double angle2 = ((i * 72.0 + 36.0) - 90.0) * 3.14159265 / 180.0;
        double angle3 = ((i + 1) * 72.0 - 90.0) * 3.14159265 / 180.0;

        POINT pts[3] = {
            { cx + (int)(outerR * cos(angle1)), cy + (int)(outerR * sin(angle1)) },
            { cx + (int)(innerR * cos(angle2)), cy + (int)(innerR * sin(angle2)) },
            { cx + (int)(outerR * cos(angle3)), cy + (int)(outerR * sin(angle3)) },
        };

        HBRUSH hBrush = CreateSolidBrush(colors[i]);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hPen = CreatePen(PS_SOLID, 1, colors[i]);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        Polygon(hdc, pts, 3);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hBrush);
        DeleteObject(hPen);
    }
}

/**
 * 窗口过程函数
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        // 清理全局区域对象
        if (g_hTestRgnRect)    DeleteObject(g_hTestRgnRect);
        if (g_hTestRgnEllipse) DeleteObject(g_hTestRgnEllipse);
        if (g_hTestRgnTriangle) DeleteObject(g_hTestRgnTriangle);
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
    {
        // --- PtInRegion 测试 ---
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);

        g_lastHitResult = -1;
        if (g_hTestRgnRect && PtInRegion(g_hTestRgnRect, mx, my))
            g_lastHitResult = 0;
        else if (g_hTestRgnEllipse && PtInRegion(g_hTestRgnEllipse, mx, my))
            g_lastHitResult = 1;
        else if (g_hTestRgnTriangle && PtInRegion(g_hTestRgnTriangle, mx, my))
            g_lastHitResult = 2;

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int clientW = rcClient.right;
        int clientH = rcClient.bottom;

        // 布局：上方 4 个象限，底部测试区域
        int topAreaH  = (clientH - 80) * 2 / 3;  // 上半区（4 个象限）
        int bottomY   = topAreaH + 44;            // 底部测试区 Y 起点
        int bottomH   = clientH - bottomY - 8;    // 底部测试区高度

        int halfW = clientW / 2;
        int halfH = topAreaH / 2;

        // 填充白色背景
        HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(hdc, &rcClient, hWhite);

        // ============================================================
        // 象限 1 (左上): CreateRectRgn — 矩形裁切
        // ============================================================
        {
            int qx = 4, qy = 4;
            int qw = halfW - 8, qh = halfH - 8;

            DrawPanelTitle(hdc, L"CreateRectRgn 矩形裁切", qx, qy, qw);

            int drawY = qy + 24;
            int drawH = qh - 24;

            // 先绘制完整网格背景（裁切前）
            DrawGridBackground(hdc, qx, drawY, qw, drawH);

            // --- 创建矩形区域 ---
            // CreateRectRgn(left, top, right, bottom)
            int margin = 20;
            HRGN hRgn = CreateRectRgn(
                qx + margin,          drawY + margin,
                qx + qw - margin,     drawY + drawH - margin
            );

            // --- SelectClipRgn: 设置裁切区域 ---
            // 将区域设为 DC 的裁切区域，后续绘制只在此区域内可见
            SelectClipRgn(hdc, hRgn);

            // 在裁切区域内绘制条纹 —— 只有矩形内的部分会显示
            DrawDiagonalStripes(hdc, qx, drawY, qw, drawH);

            // 绘制裁切区域边框
            HBRUSH hFrameBrush = CreateSolidBrush(RGB(255, 100, 100));
            FrameRgn(hdc, hRgn, hFrameBrush, 2, 2);
            DeleteObject(hFrameBrush);

            // 取消裁切（传入 nullptr 重置裁切区域）
            SelectClipRgn(hdc, nullptr);
            DeleteObject(hRgn);
        }

        // ============================================================
        // 象限 2 (右上): CreateEllipticRgn — 椭圆裁切
        // ============================================================
        {
            int qx = halfW + 4, qy = 4;
            int qw = halfW - 8, qh = halfH - 8;

            DrawPanelTitle(hdc, L"CreateEllipticRgn 椭圆裁切", qx, qy, qw);

            int drawY = qy + 24;
            int drawH = qh - 24;
            int cx = qx + qw / 2;
            int cy = drawY + drawH / 2;
            int rx = qw / 2 - 16;
            int ry = drawH / 2 - 16;

            DrawGridBackground(hdc, qx, drawY, qw, drawH);

            // --- CreateEllipticRgn: 创建椭圆区域 ---
            HRGN hRgn = CreateEllipticRgn(cx - rx, cy - ry, cx + rx, cy + ry);

            // 设置裁切区域
            SelectClipRgn(hdc, hRgn);

            // 在裁切区域内绘制星形图案
            DrawStarPattern(hdc, cx, cy, rx - 5, ry / 3);

            // 绘制裁切区域边框
            HBRUSH hFrameBrush = CreateSolidBrush(RGB(100, 100, 255));
            FrameRgn(hdc, hRgn, hFrameBrush, 2, 2);
            DeleteObject(hFrameBrush);

            SelectClipRgn(hdc, nullptr);
            DeleteObject(hRgn);
        }

        // ============================================================
        // 象限 3 (左下): CreatePolygonRgn — 多边形裁切
        // ============================================================
        {
            int qx = 4, qy = halfH + 4;
            int qw = halfW - 8, qh = halfH - 8;

            DrawPanelTitle(hdc, L"CreatePolygonRgn 多边形裁切", qx, qy, qw);

            int drawY = qy + 24;
            int drawH = qh - 24;
            int cx = qx + qw / 2;
            int cy = drawY + drawH / 2;
            int size = std::min(qw, drawH) / 2 - 16;

            DrawGridBackground(hdc, qx, drawY, qw, drawH);

            // --- CreatePolygonRgn: 创建三角形区域 ---
            POINT triPts[3] = {
                { cx, cy - size },                       // 顶点
                { cx - size, cy + (int)(size * 0.7f) },  // 左下
                { cx + size, cy + (int)(size * 0.7f) },  // 右下
            };
            // ALTERNATE 交替填充模式
            HRGN hRgn = CreatePolygonRgn(triPts, 3, ALTERNATE);

            SelectClipRgn(hdc, hRgn);

            // 绘制水平彩色条纹
            COLORREF stripeColors[] = {
                RGB(255, 100, 100), RGB(100, 255, 100),
                RGB(100, 100, 255), RGB(255, 200, 50),
                RGB(200, 50, 200),  RGB(50, 200, 200),
                RGB(255, 150, 50),  RGB(150, 255, 50),
            };
            for (int i = 0; i < 8; i++)
            {
                int sy = drawY + i * drawH / 8;
                int sh = drawH / 8 + 1;
                HBRUSH hBrush = CreateSolidBrush(stripeColors[i]);
                RECT rc = { qx, sy, qx + qw, sy + sh };
                FillRect(hdc, &rc, hBrush);
                DeleteObject(hBrush);
            }

            HBRUSH hFrameBrush = CreateSolidBrush(RGB(100, 200, 100));
            FrameRgn(hdc, hRgn, hFrameBrush, 2, 2);
            DeleteObject(hFrameBrush);

            SelectClipRgn(hdc, nullptr);
            DeleteObject(hRgn);
        }

        // ============================================================
        // 象限 4 (右下): CombineRgn — 区域组合
        // ============================================================
        {
            int qx = halfW + 4, qy = halfH + 4;
            int qw = halfW - 8, qh = halfH - 8;

            DrawPanelTitle(hdc, L"CombineRgn 组合区域", qx, qy, qw);

            int drawY = qy + 24;
            int drawH = qh - 24;

            // 将象限进一步分为 4 个小格子展示不同组合模式
            int cellW = qw / 2;
            int cellH = (drawH - 20) / 2;
            int labelH = 18;

            // 两个重叠的矩形区域
            int overlap = 25;

            struct CombineDemo {
                const wchar_t* label;
                int combineMode;
                COLORREF color;
            };

            CombineDemo demos[] = {
                { L"RGN_AND (交集)",  RGN_AND, RGB(255, 80, 80) },
                { L"RGN_OR (并集)",   RGN_OR,  RGB(80, 200, 80) },
                { L"RGN_XOR (异或)",  RGN_XOR, RGB(80, 80, 255) },
                { L"RGN_DIFF (差集)", RGN_DIFF, RGB(255, 200, 50) },
            };

            for (int i = 0; i < 4; i++)
            {
                int col = i % 2;
                int row = i / 2;
                int cx = qx + col * cellW;
                int cy = drawY + row * (cellH + 20);

                // 绘制小标题
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(60, 60, 60));
                HFONT hSmallFont = CreateFont(
                    12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
                );
                HFONT hOldFont = (HFONT)SelectObject(hdc, hSmallFont);
                RECT rcLabel = { cx + 4, cy, cx + cellW - 4, cy + labelH };
                DrawText(hdc, demos[i].label, -1, &rcLabel,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
                DeleteObject(hSmallFont);

                int dcy = cy + labelH + 2;
                int dcw = cellW - 16;
                int dch = cellH - labelH - 4;
                int dcx = cx + 8;

                // 灰色背景
                HBRUSH hBgBrush = CreateSolidBrush(RGB(240, 240, 240));
                RECT rcBg = { dcx, dcy, dcx + dcw, dcy + dch };
                FillRect(hdc, &rcBg, hBgBrush);
                DeleteObject(hBgBrush);

                // 创建两个重叠的矩形区域
                HRGN hRgn1 = CreateRectRgn(
                    dcx + 8, dcy + 8,
                    dcx + dcw / 2 + overlap, dcy + dch - 8
                );
                HRGN hRgn2 = CreateRectRgn(
                    dcx + dcw / 2 - overlap, dcy + 8,
                    dcx + dcw - 8, dcy + dch - 8
                );

                // --- CombineRgn: 组合两个区域 ---
                // 第 1 个参数是结果区域，第 2、3 个是输入区域
                // 返回值指示结果区域的类型（SIMPLEREGION, COMPLEXREGION, NULLREGION 等）
                HRGN hCombined = CreateRectRgn(0, 0, 1, 1);  // 需要预分配
                int result = CombineRgn(hCombined, hRgn1, hRgn2, demos[i].combineMode);

                // 绘制原始区域轮廓（虚线）
                HBRUSH hFrameBrush1 = CreateSolidBrush(RGB(180, 180, 180));
                FrameRgn(hdc, hRgn1, hFrameBrush1, 1, 1);
                FrameRgn(hdc, hRgn2, hFrameBrush1, 1, 1);
                DeleteObject(hFrameBrush1);

                // 填充组合结果区域
                HBRUSH hFillBrush = CreateSolidBrush(demos[i].color);
                FillRgn(hdc, hCombined, hFillBrush);
                DeleteObject(hFillBrush);

                // 组合结果边框
                HBRUSH hFrameBrush2 = CreateSolidBrush(RGB(0, 0, 0));
                FrameRgn(hdc, hCombined, hFrameBrush2, 1, 1);
                DeleteObject(hFrameBrush2);

                DeleteObject(hRgn1);
                DeleteObject(hRgn2);
                DeleteObject(hCombined);
            }
        }

        // ============================================================
        // 底部: PtInRegion 测试区域
        // ============================================================
        {
            // 重建测试区域（使用窗口坐标）
            int bottomW = clientW - 16;
            int testAreaH = bottomH;

            // 绘制标题
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(30, 30, 30));
            HFONT hFont = CreateFont(
                13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            RECT rcTitle = { 8, bottomY - 20, clientW - 8, bottomY - 2 };
            DrawText(hdc, L"PtInRegion 点击测试区域 (点击下方区域)",
                     -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            // 背景边框
            HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(LTGRAY_BRUSH));
            Rectangle(hdc, 8, bottomY, clientW - 8, bottomY + testAreaH);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hBorderPen);

            // 清除旧的测试区域
            if (g_hTestRgnRect)     DeleteObject(g_hTestRgnRect);
            if (g_hTestRgnEllipse)  DeleteObject(g_hTestRgnEllipse);
            if (g_hTestRgnTriangle) DeleteObject(g_hTestRgnTriangle);

            // 将底部区域分为 3 段
            int segW = bottomW / 3;

            // 矩形区域
            int rLeft   = 8 + segW * 0 + 20;
            int rRight  = 8 + segW * 1 - 20;
            int rTop    = bottomY + 15;
            int rBottom = bottomY + testAreaH - 15;
            g_hTestRgnRect = CreateRectRgn(rLeft, rTop, rRight, rBottom);
            HBRUSH hBrushR = CreateSolidBrush(RGB(255, 180, 180));
            FillRgn(hdc, g_hTestRgnRect, hBrushR);
            DeleteObject(hBrushR);
            HBRUSH hFrameR = CreateSolidBrush(RGB(200, 60, 60));
            FrameRgn(hdc, g_hTestRgnRect, hFrameR, 2, 2);
            DeleteObject(hFrameR);

            // 椭圆区域
            int eLeft   = 8 + segW * 1 + 20;
            int eRight  = 8 + segW * 2 - 20;
            g_hTestRgnEllipse = CreateEllipticRgn(eLeft, rTop, eRight, rBottom);
            HBRUSH hBrushE = CreateSolidBrush(RGB(180, 255, 180));
            FillRgn(hdc, g_hTestRgnEllipse, hBrushE);
            DeleteObject(hBrushE);
            HBRUSH hFrameE = CreateSolidBrush(RGB(60, 160, 60));
            FrameRgn(hdc, g_hTestRgnEllipse, hFrameE, 2, 2);
            DeleteObject(hFrameE);

            // 三角形区域
            int tLeft   = 8 + segW * 2 + 20;
            int tRight  = 8 + segW * 3 - 20;
            int tMidX   = (tLeft + tRight) / 2;
            POINT triPts[3] = {
                { tMidX,  rTop },
                { tLeft,  rBottom },
                { tRight, rBottom },
            };
            g_hTestRgnTriangle = CreatePolygonRgn(triPts, 3, ALTERNATE);
            HBRUSH hBrushT = CreateSolidBrush(RGB(180, 180, 255));
            FillRgn(hdc, g_hTestRgnTriangle, hBrushT);
            DeleteObject(hBrushT);
            HBRUSH hFrameT = CreateSolidBrush(RGB(60, 60, 200));
            FrameRgn(hdc, g_hTestRgnTriangle, hFrameT, 2, 2);
            DeleteObject(hFrameT);

            // 在区域中心标注名称
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(40, 40, 40));
            HFONT hSmallFont = CreateFont(
                12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
            );
            HFONT hOldFont2 = (HFONT)SelectObject(hdc, hSmallFont);

            const wchar_t* names[] = { L"矩形区域", L"椭圆区域", L"三角形区域" };
            int centersX[] = { (rLeft + rRight) / 2, (eLeft + eRight) / 2, tMidX };
            int centersY[] = { (rTop + rBottom) / 2, (rTop + rBottom) / 2, (rTop + rBottom) / 2 };

            for (int i = 0; i < 3; i++)
            {
                RECT rc = { centersX[i] - 40, centersY[i] - 8, centersX[i] + 40, centersY[i] + 8 };
                DrawText(hdc, names[i], -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // 显示 PtInRegion 点击结果
            if (g_lastHitResult >= 0)
            {
                const wchar_t* hitNames[] = { L"命中: 矩形区域!", L"命中: 椭圆区域!", L"命中: 三角形区域!" };
                COLORREF hitColors[] = { RGB(200, 0, 0), RGB(0, 150, 0), RGB(0, 0, 200) };
                SetTextColor(hdc, hitColors[g_lastHitResult]);

                HFONT hBigFont = CreateFont(
                    16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑"
                );
                SelectObject(hdc, hOldFont2);
                DeleteObject(hSmallFont);
                hSmallFont = nullptr;
                HFONT hOldFont3 = (HFONT)SelectObject(hdc, hBigFont);

                RECT rcHit = { 8, bottomY - 20, clientW - 8, bottomY - 2 };
                DrawText(hdc, hitNames[g_lastHitResult], -1, &rcHit,
                         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, hOldFont3);
                DeleteObject(hBigFont);
            }

            if (hSmallFont)
            {
                SelectObject(hdc, hOldFont2);
                DeleteObject(hSmallFont);
            }
            else
            {
                SelectObject(hdc, hOldFont2);
            }
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
        L"GDI Region 裁切示例",
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
