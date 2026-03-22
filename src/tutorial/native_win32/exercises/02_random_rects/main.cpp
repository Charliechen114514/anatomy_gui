#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <vector>
#include <ctime>
#include <cstdlib>

// 存储所有已画的矩形及其颜色
struct RectData {
    RECT rect;
    COLORREF color;
};

std::vector<RectData> g_rects;

// RAII 封装的 GDI 画刷
class ScopedBrush {
public:
    explicit ScopedBrush(HBRUSH brush) : m_brush(brush) {}
    ~ScopedBrush() { if (m_brush) DeleteObject(m_brush); }
    ScopedBrush(const ScopedBrush&) = delete;
    ScopedBrush& operator=(const ScopedBrush&) = delete;
    operator HBRUSH() const { return m_brush; }
private:
    HBRUSH m_brush;
};

// 生成随机颜色
COLORREF RandomColor() {
    return RGB(
        rand() % 256,
        rand() % 256,
        rand() % 256
    );
}

// 生成指定范围内的随机数
int RandomInRange(int min, int max) {
    return min + rand() % (max - min + 1);
}

// 添加一个新的随机矩形
void AddRandomRect(HWND hwnd) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    // 矩形大小范围: 20x20 到 100x100
    int width = RandomInRange(20, 100);
    int height = RandomInRange(20, 100);

    // 随机位置 (确保矩形至少部分可见)
    int x = RandomInRange(0, clientWidth - width / 2);
    int y = RandomInRange(0, clientHeight - height / 2);

    RectData data;
    data.rect.left = x;
    data.rect.top = y;
    data.rect.right = x + width;
    data.rect.bottom = y + height;
    data.color = RandomColor();

    g_rects.push_back(data);
}

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 获取原画刷，稍后恢复
            HGDIOBJ hOldBrush = nullptr;

            // 绘制所有已保存的矩形
            for (const auto& data : g_rects) {
                ScopedBrush brush(CreateSolidBrush(data.color));
                if (brush) {
                    HGDIOBJ hPrevBrush = SelectObject(hdc, brush);
                    Rectangle(hdc,
                        data.rect.left,
                        data.rect.top,
                        data.rect.right,
                        data.rect.bottom);
                    SelectObject(hdc, hPrevBrush);
                }
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // 添加一个随机矩形
            AddRandomRect(hwnd);
            // 触发重绘
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // 初始化随机数种子
    srand(static_cast<unsigned>(time(nullptr)));

    const wchar_t CLASS_NAME[] = L"RandomRectsWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"随机方块画板 - 点击左键添加矩形",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
