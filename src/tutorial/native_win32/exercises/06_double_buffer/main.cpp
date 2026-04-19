/*
 * Double Buffer Animation Demo
 *
 * 本程序演示如何使用双缓冲技术消除闪烁。
 *
 * 技术要点:
 * 1. 创建内存 DC (CreateCompatibleDC)
 * 2. 创建内存位图 (CreateCompatibleBitmap)
 * 3. 在内存 DC 上绘制所有内容
 * 4. 用 BitBlt 一次性复制到窗口
 * 5. 正确释放所有 GDI 对象 (使用 RAII 模式)
 * 6. 使用定时器 (SetTimer) 驱动动画
 *
 * 编译:
 *   cmake -B build
 *   cmake --build build
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <cmath>
#include <vector>
#include <random>

// ============================================================================
// RAII 包装类 - 自动管理 GDI 对象
// ============================================================================

// GDI 对象选择器 - 自动恢复旧对象
class GDISelector {
public:
    GDISelector(HDC hdc, HGDIOBJ obj) : hdc_(hdc), old_obj_(SelectObject(hdc, obj)) {}
    ~GDISelector() { if (old_obj_) SelectObject(hdc_, old_obj_); }
    GDISelector(const GDISelector&) = delete;
    GDISelector& operator=(const GDISelector&) = delete;
private:
    HDC hdc_;
    HGDIOBJ old_obj_;
};

// DC 释放器
class DCReleaser {
public:
    DCReleaser(HWND hwnd) : hdc_(GetDC(hwnd)), hwnd_(hwnd) {}
    ~DCReleaser() { if (hdc_) ReleaseDC(hwnd_, hdc_); }
    operator HDC() const { return hdc_; }
    DCReleaser(const DCReleaser&) = delete;
    DCReleaser& operator=(const DCReleaser&) = delete;
private:
    HDC hdc_;
    HWND hwnd_;
};

// 内存 DC 释放器
class MemDCReleaser {
public:
    explicit MemDCReleaser(HDC hdc) : hdc_(CreateCompatibleDC(hdc)) {}
    ~MemDCReleaser() { if (hdc_) DeleteDC(hdc_); }
    operator HDC() const { return hdc_; }
    MemDCReleaser(const MemDCReleaser&) = delete;
    MemDCReleaser& operator=(const MemDCReleaser&) = delete;
private:
    HDC hdc_;
};

// 位图删除器
class BitmapDeleter {
public:
    explicit BitmapDeleter(HBITMAP bmp) : bmp_(bmp) {}
    ~BitmapDeleter() { if (bmp_) DeleteObject(bmp_); }
    operator HBITMAP() const { return bmp_; }
    BitmapDeleter(const BitmapDeleter&) = delete;
    BitmapDeleter& operator=(const BitmapDeleter&) = delete;
private:
    HBITMAP bmp_;
};

// 画刷选择器
class BrushSelector {
public:
    BrushSelector(HDC hdc, HBRUSH brush) : hdc_(hdc) {
        old_ = (HBRUSH)SelectObject(hdc, brush);
    }
    ~BrushSelector() { SelectObject(hdc_, old_); }
private:
    HDC hdc_;
    HBRUSH old_;
};

// 画笔选择器
class PenSelector {
public:
    PenSelector(HDC hdc, HPEN pen) : hdc_(hdc) {
        old_ = (HPEN)SelectObject(hdc, pen);
    }
    ~PenSelector() { SelectObject(hdc_, old_); }
private:
    HDC hdc_;
    HPEN old_;
};

// ============================================================================
// 粒子/小球 类
// ============================================================================

struct Ball {
    float x, y;          // 位置
    float vx, vy;        // 速度
    int radius;          // 半径
    COLORREF color;      // 颜色

    Ball(int clientWidth, int clientHeight) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(0.1f, 0.9f);
        std::uniform_real_distribution<float> velDist(-3.0f, 3.0f);
        std::uniform_int_distribution<int> radiusDist(15, 40);
        std::uniform_int_distribution<int> colorDist(0, 255);

        x = clientWidth * posDist(gen);
        y = clientHeight * posDist(gen);
        vx = velDist(gen);
        vy = velDist(gen);
        radius = radiusDist(gen);

        // 生成鲜艳的颜色
        color = RGB(
            colorDist(gen),
            colorDist(gen),
            colorDist(gen)
        );
    }

    void update(int clientWidth, int clientHeight) {
        x += vx;
        y += vy;

        // 边界碰撞检测
        if (x - radius < 0) {
            x = static_cast<float>(radius);
            vx = -vx;
        }
        if (x + radius > clientWidth) {
            x = static_cast<float>(clientWidth) - radius;
            vx = -vx;
        }
        if (y - radius < 0) {
            y = static_cast<float>(radius);
            vy = -vy;
        }
        if (y + radius > clientHeight) {
            y = static_cast<float>(clientHeight) - radius;
            vy = -vy;
        }
    }

    void draw(HDC hdc) {
        // 创建画刷
        HBRUSH brush = CreateSolidBrush(color);
        BrushSelector brushSel(hdc, brush);

        // 创建黑色画笔
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        PenSelector penSel(hdc, pen);

        // 绘制圆形
        Ellipse(hdc,
            static_cast<int>(x - radius),
            static_cast<int>(y - radius),
            static_cast<int>(x + radius),
            static_cast<int>(y + radius)
        );

        // 绘制高光效果
        HPEN nullPen = CreatePen(PS_NULL, 0, 0);
        PenSelector nullPenSel(hdc, nullPen);

        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        BrushSelector whiteBrushSel(hdc, whiteBrush);

        Ellipse(hdc,
            static_cast<int>(x - radius * 0.3f),
            static_cast<int>(y - radius * 0.3f),
            static_cast<int>(x - radius * 0.1f),
            static_cast<int>(y - radius * 0.1f)
        );

        DeleteObject(brush);
        DeleteObject(pen);
        DeleteObject(nullPen);
        DeleteObject(whiteBrush);
    }
};

// ============================================================================
// 双缓冲渲染器
// ============================================================================

class DoubleBufferRenderer {
public:
    DoubleBufferRenderer() : memDC_(nullptr), memBitmap_(nullptr), width_(0), height_(0) {}

    ~DoubleBufferRenderer() {
        cleanup();
    }

    void resize(int width, int height, HDC refDC) {
        if (width_ != width || height_ != height) {
            cleanup();
            width_ = width;
            height_ = height;

            if (refDC && width > 0 && height > 0) {
                // 创建内存 DC
                memDC_ = CreateCompatibleDC(refDC);
                // 创建内存位图
                memBitmap_ = CreateCompatibleBitmap(refDC, width, height);

                if (memDC_ && memBitmap_) {
                    // 将位图选入内存 DC
                    oldBitmap_ = (HBITMAP)SelectObject(memDC_, memBitmap_);
                }
            }
        }
    }

    HDC getMemDC() const { return memDC_; }

    void present(HDC targetDC, int x, int y, int width, int height) {
        if (memDC_) {
            // 一次性将内存 DC 的内容复制到目标 DC
            BitBlt(targetDC, x, y, width, height, memDC_, 0, 0, SRCCOPY);
        }
    }

    void clear(COLORREF color) {
        if (memDC_) {
            // 填充背景
            HBRUSH brush = CreateSolidBrush(color);
            RECT rect = { 0, 0, width_, height_ };
            FillRect(memDC_, &rect, brush);
            DeleteObject(brush);
        }
    }

private:
    void cleanup() {
        if (memDC_) {
            if (oldBitmap_) {
                SelectObject(memDC_, oldBitmap_);
                oldBitmap_ = nullptr;
            }
            DeleteDC(memDC_);
            memDC_ = nullptr;
        }
        if (memBitmap_) {
            DeleteObject(memBitmap_);
            memBitmap_ = nullptr;
        }
        width_ = 0;
        height_ = 0;
    }

    HDC memDC_;
    HBITMAP memBitmap_;
    HBITMAP oldBitmap_;
    int width_;
    int height_;
};

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState {
    std::vector<Ball> balls;
    DoubleBufferRenderer renderer;
    bool useDoubleBuffer = true;
    int frameCount = 0;

    void initBalls(int count, int width, int height) {
        balls.clear();
        for (int i = 0; i < count; ++i) {
            balls.emplace_back(width, height);
        }
    }

    void update(int width, int height) {
        for (auto& ball : balls) {
            ball.update(width, height);
        }
        ++frameCount;
    }

    void renderWithoutDoubleBuffer(HDC hdc, int width, int height) {
        // 直接在窗口 DC 上绘制 - 会产生闪烁
        // 清除背景
        RECT rect = { 0, 0, width, height };
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

        // 绘制所有小球
        for (auto& ball : balls) {
            ball.draw(hdc);
        }

        // 绘制提示信息
        drawInfo(hdc);
    }

    void renderWithDoubleBuffer(HDC hdc, int width, int height) {
        // 调整缓冲区大小
        renderer.resize(width, height, hdc);

        HDC memDC = renderer.getMemDC();
        if (!memDC) {
            renderWithoutDoubleBuffer(hdc, width, height);
            return;
        }

        // 在内存 DC 上清除背景
        renderer.clear(RGB(255, 255, 255));

        // 在内存 DC 上绘制所有小球
        for (auto& ball : balls) {
            ball.draw(memDC);
        }

        // 在内存 DC 上绘制提示信息
        drawInfo(memDC);

        // 一次性复制到窗口
        renderer.present(hdc, 0, 0, width, height);
    }

    void render(HDC hdc, int width, int height) {
        if (useDoubleBuffer) {
            renderWithDoubleBuffer(hdc, width, height);
        } else {
            renderWithoutDoubleBuffer(hdc, width, height);
        }
    }

    void drawInfo(HDC hdc) {
        // 设置文字颜色
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // 使用系统字体
        HFONT font = (HFONT)GetStockObject(ANSI_VAR_FONT);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        wchar_t buffer[256];
        int y = 10;

        // 绘制模式信息
        wsprintf(buffer, L"Mode: %s Buffering",
                 useDoubleBuffer ? L"Double" : L"Single (No Double)");
        TextOut(hdc, 10, y, buffer, lstrlen(buffer));
        y += 25;

        // 绘制提示
        TextOut(hdc, 10, y, L"Press SPACE to toggle double buffering", 38);
        y += 25;

        // 绘制小球数量
        wsprintf(buffer, L"Balls: %zu", balls.size());
        TextOut(hdc, 10, y, buffer, lstrlen(buffer));
        y += 25;

        // 绘制帧数
        wsprintf(buffer, L"Frame: %d", frameCount);
        TextOut(hdc, 10, y, buffer, lstrlen(buffer));

        SelectObject(hdc, oldFont);
    }

    void toggleBufferMode() {
        useDoubleBuffer = !useDoubleBuffer;
    }
};

// ============================================================================
// 窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static AppState* state = nullptr;

    switch (msg) {
        case WM_CREATE: {
            state = new AppState();
            RECT rect;
            GetClientRect(hwnd, &rect);
            state->initBalls(15, rect.right, rect.bottom);

            // 设置定时器 - 60 FPS
            SetTimer(hwnd, 1, 1000 / 60, nullptr);
            break;
        }

        case WM_TIMER: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            state->update(rect.right, rect.bottom);
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            state->render(hdc, rect.right, rect.bottom);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_SPACE) {
                state->toggleBufferMode();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // 如果窗口变大，可能需要重新初始化小球位置
            if (state && state->balls.empty()) {
                state->initBalls(15, width, height);
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            delete state;
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// ============================================================================
// 主函数
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DoubleBufferWindow";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBox(nullptr, L"Window registration failed!", L"Error",
                   MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    // 创建窗口
    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"Double Buffer Animation Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!hwnd) {
        MessageBox(nullptr, L"Window creation failed!", L"Error",
                   MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
