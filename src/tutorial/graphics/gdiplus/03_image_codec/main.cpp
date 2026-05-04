/**
 * 03_image_codec - GDI+ 图像编解码示例
 *
 * 本程序演示了 GDI+ 的图像编解码功能：
 * 1. 创建内存中的 GDI+ Bitmap (100x100 渐变图)
 * 2. 使用 GetImageEncoders 枚举系统可用的编码器 (PNG, JPEG, BMP 等)
 * 3. 使用 Bitmap::Save 将位图保存为 PNG 文件到 %TEMP% 目录
 * 4. 使用 Bitmap::FromFile 从文件加载图像
 * 5. 使用 Graphics::DrawImage 显示加载的图像
 * 6. 在 ListBox 中显示编码器信息 (CLSID, MIME 类型, 文件扩展名)
 * 7. "保存" 和 "加载" 按钮实现交互式操作
 *
 * 参考: tutorial/native_win32/28_ProgrammingGUI_Graphics_GdiPlus_ImageCodec.md
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <string>

// ============================================================================
// 常量定义
// ============================================================================

static const wchar_t* WINDOW_CLASS_NAME = L"GdiPlusCodecClass";

// 控件子窗口 ID
static const int ID_BTN_SAVE   = 101;
static const int ID_BTN_LOAD   = 102;
static const int ID_LIST_CODECS = 103;

// ============================================================================
// 应用程序状态
// ============================================================================

struct AppState
{
    bool imageLoaded;       // 是否已加载图像
    wchar_t savePath[MAX_PATH];    // 保存路径
};

// ============================================================================
// 辅助函数：获取 CLSID 字符串表示
// ============================================================================

/**
 * 将 CLSID 转换为可读的字符串格式
 * 格式: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
 */
static std::wstring ClsidToString(const CLSID& clsid)
{
    wchar_t buf[64];
    _snwprintf(buf, 64,
        L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        clsid.Data1, clsid.Data2, clsid.Data3,
        clsid.Data4[0], clsid.Data4[1],
        clsid.Data4[2], clsid.Data4[3],
        clsid.Data4[4], clsid.Data4[5],
        clsid.Data4[6], clsid.Data4[7]);
    return std::wstring(buf);
}

// ============================================================================
// 辅助函数：通过 MIME 类型获取编码器 CLSID
// ============================================================================

/**
 * 根据指定的 MIME 类型查找对应的图像编码器 CLSID。
 * GDI+ 保存图像时需要传入编码器的 CLSID。
 *
 * @param format  MIME 类型字符串，如 L"image/png"
 * @param pClsid  输出参数，接收找到的 CLSID
 * @return 成功返回 true，未找到返回 false
 */
static bool GetEncoderClsid(const wchar_t* format, CLSID* pClsid)
{
    using namespace Gdiplus;

    UINT num = 0;    // 编码器数量
    UINT size = 0;   // 编码器信息数组大小

    // 第一次调用获取需要的缓冲区大小
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return false;

    // 分配缓冲区
    ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(size);
    if (pCodecInfo == nullptr)
        return false;

    // 第二次调用获取实际的编码器信息
    GetImageEncoders(num, size, pCodecInfo);

    // 遍历查找匹配的 MIME 类型
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(pCodecInfo[i].MimeType, format) == 0)
        {
            *pClsid = pCodecInfo[i].Clsid;
            free(pCodecInfo);
            return true;
        }
    }

    free(pCodecInfo);
    return false;
}

// ============================================================================
// 创建渐变位图
// ============================================================================

/**
 * 在内存中创建一个 100x100 的渐变位图
 * 使用 LinearGradientBrush 绘制从蓝到绿的渐变，并在中心绘制文字
 */
static Gdiplus::Bitmap* CreateGradientBitmap()
{
    using namespace Gdiplus;

    // 创建 100x100 的 32 位 ARGB 位图
    Bitmap* bmp = new Bitmap(100, 100, PixelFormat32bppARGB);

    Graphics graphics(bmp);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // 绘制渐变背景
    LinearGradientBrush gradientBrush(
        Point(0, 0), Point(100, 100),
        Color(255, 30, 144, 255),    // 蓝色起点
        Color(255, 50, 205, 50)      // 绿色终点
    );
    graphics.FillRectangle(&gradientBrush, (REAL)(0), (REAL)(0), (REAL)(100), (REAL)(100));

    // 在中心绘制圆形装饰
    SolidBrush circleBrush(Color(180, 255, 255, 255));   // 半透明白色
    graphics.FillEllipse(&circleBrush, (REAL)(25), (REAL)(25), (REAL)(50), (REAL)(50));

    // 绘制中心文字
    FontFamily fontFamily(L"Arial");
    Font font(&fontFamily, 14, FontStyleBold, UnitPixel);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF textRect(0, 0, 100.0f, 100.0f);
    graphics.DrawString(L"GDI+", -1, &font, textRect, &sf, &textBrush);

    // 绘制边框
    Pen borderPen(Color(255, 255, 255, 255), 2.0f);
    graphics.DrawRectangle(&borderPen, (REAL)(1), (REAL)(1), (REAL)(98), (REAL)(98));

    return bmp;
}

// ============================================================================
// 枚举编码器并填充 ListBox
// ============================================================================

/**
 * 获取系统所有可用的图像编码器，将信息添加到 ListBox
 * 每条记录包含: 编号 + MIME 类型 + 文件扩展名
 */
static void EnumerateCodecs(HWND hListBox)
{
    using namespace Gdiplus;

    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return;

    ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pCodecInfo) return;

    GetImageEncoders(num, size, pCodecInfo);

    for (UINT i = 0; i < num; ++i)
    {
        std::wstring entry = std::to_wstring(i + 1) + L". " +
                             pCodecInfo[i].MimeType +
                             L"  (*" + pCodecInfo[i].FilenameExtension + L")";
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)entry.c_str());
    }

    free(pCodecInfo);
}

// ============================================================================
// 显示编码器详细信息
// ============================================================================

/**
 * 当用户在 ListBox 中选择一个编码器时，显示其详细信息
 */
static void ShowCodecDetail(HWND hwnd, int selectedIndex)
{
    using namespace Gdiplus;

    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return;

    ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pCodecInfo) return;

    GetImageEncoders(num, size, pCodecInfo);

    if (selectedIndex >= 0 && selectedIndex < (int)num)
    {
        ImageCodecInfo& codec = pCodecInfo[selectedIndex];
        std::wstring detail =
            L"编码器详细信息:\n\n"
            L"CLSID: " + ClsidToString(codec.Clsid) + L"\n\n"
            L"MIME 类型: " + codec.MimeType + L"\n\n"
            L"文件扩展名: *" + codec.FilenameExtension + L"\n\n"
            L"格式描述: " + codec.FormatDescription;
        MessageBox(hwnd, detail.c_str(), L"编码器详情", MB_OK | MB_ICONINFORMATION);
    }

    free(pCodecInfo);
}

// ============================================================================
// 绘制图像预览区域
// ============================================================================

/**
 * 在指定区域内绘制图像预览
 * 如果图像已加载，则显示加载的图像；否则显示提示文字
 */
static void DrawPreview(Gdiplus::Graphics& graphics, const RECT& rc,
                        Gdiplus::Image* pImage)
{
    using namespace Gdiplus;

    // 绘制预览区域背景
    SolidBrush bgBrush(Color(255, 250, 250, 250));
    graphics.FillRectangle(&bgBrush, (INT)rc.left, (INT)rc.top,
                           (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));

    // 绘制边框
    Pen borderPen(Color(255, 200, 200, 200), 1.0f);
    graphics.DrawRectangle(&borderPen, (INT)rc.left, (INT)rc.top,
                           (INT)(rc.right - rc.left - 1), (INT)(rc.bottom - rc.top - 1));

    if (pImage != nullptr)
    {
        // 计算居中缩放显示
        float imgW = (float)pImage->GetWidth();
        float imgH = (float)pImage->GetHeight();
        float areaW = (float)(rc.right - rc.left - 20);
        float areaH = (float)(rc.bottom - rc.top - 20);

        // 保持宽高比缩放
        float scale = std::min(areaW / imgW, areaH / imgH);
        if (scale > 1.0f) scale = 1.0f;     // 不放大

        float drawW = imgW * scale;
        float drawH = imgH * scale;
        float drawX = rc.left + (float)(rc.right - rc.left - drawW) / 2;
        float drawY = rc.top + (float)(rc.bottom - rc.top - drawH) / 2;

        graphics.DrawImage(pImage, drawX, drawY, drawW, drawH);

        // 显示图像信息
        FontFamily fontFamily(L"微软雅黑");
        Font infoFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
        SolidBrush infoBrush(Color(255, 100, 100, 100));

        wchar_t info[128];
        _snwprintf(info, 128, L"图像尺寸: %.0f x %.0f  缩放: %.1fx",
                   imgW, imgH, scale);
        graphics.DrawString(info, -1, &infoFont,
                            PointF((REAL)rc.left + 5, (REAL)(rc.bottom - 22)),
                            &infoBrush);
    }
    else
    {
        // 未加载图像 — 显示提示
        FontFamily fontFamily(L"微软雅黑");
        Font hintFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
        SolidBrush hintBrush(Color(255, 180, 180, 180));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF textRect((REAL)rc.left, (REAL)rc.top,
                       (REAL)(rc.right - rc.left), (REAL)(rc.bottom - rc.top));
        graphics.DrawString(L"点击 \"加载\" 按钮加载已保存的图像", -1,
                            &hintFont, textRect, &sf, &hintBrush);
    }
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
        // 初始化应用状态
        AppState* state = new AppState();
        state->imageLoaded = false;

        // 构造保存路径: %TEMP%\gdiplus_test.png
        GetTempPathW(MAX_PATH, state->savePath);
        wcscat_s(state->savePath, MAX_PATH, L"gdiplus_test.png");

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // 创建控件
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        int clientW = cs->cx;
        int clientH = cs->cy;

        // ---- 初始化通用控件 ----
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        // ---- 右侧面板布局 ----
        int rightX = clientW - 260;

        // "保存" 按钮
        CreateWindowEx(0, L"BUTTON", L"保存 PNG",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rightX + 10, 10, 110, 32,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_SAVE,
            cs->hInstance, nullptr);

        // "加载" 按钮
        CreateWindowEx(0, L"BUTTON", L"加载 PNG",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rightX + 130, 10, 110, 32,
            hwnd, (HMENU)(LONG_PTR)ID_BTN_LOAD,
            cs->hInstance, nullptr);

        // "可用编码器列表" 标签
        CreateWindowEx(0, L"STATIC", L"可用编码器列表:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            rightX + 10, 50, 200, 20,
            hwnd, nullptr, cs->hInstance, nullptr);

        // ListBox — 显示编码器信息
        HWND hListBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
            rightX + 10, 70, 235, clientH - 80,
            hwnd, (HMENU)(LONG_PTR)ID_LIST_CODECS,
            cs->hInstance, nullptr);

        // 填充编码器列表
        EnumerateCodecs(hListBox);

        return 0;
    }

    case WM_DESTROY:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (state) delete state;
        PostQuitMessage(0);
        return 0;
    }

    case WM_COMMAND:
    {
        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state) return 0;

        switch (LOWORD(wParam))
        {
        case ID_BTN_SAVE:
        {
            // ---- 保存位图到文件 ----
            // 1. 创建内存中的渐变位图
            Gdiplus::Bitmap* bmp = CreateGradientBitmap();
            if (!bmp)
            {
                MessageBox(hwnd, L"创建位图失败!", L"错误", MB_ICONERROR);
                return 0;
            }

            // 2. 获取 PNG 编码器的 CLSID
            CLSID pngClsid;
            if (!GetEncoderClsid(L"image/png", &pngClsid))
            {
                MessageBox(hwnd, L"未找到 PNG 编码器!", L"错误", MB_ICONERROR);
                delete bmp;
                return 0;
            }

            // 3. 保存位图到 %TEMP%\gdiplus_test.png
            Gdiplus::Status status = bmp->Save(state->savePath, &pngClsid, nullptr);
            delete bmp;

            if (status == Gdiplus::Ok)
            {
                std::wstring msg = L"图像已保存到:\n" + std::wstring(state->savePath);
                MessageBox(hwnd, msg.c_str(), L"保存成功", MB_OK | MB_ICONINFORMATION);
                state->imageLoaded = false;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else
            {
                MessageBox(hwnd, L"保存图像失败!", L"错误", MB_ICONERROR);
            }
            break;
        }

        case ID_BTN_LOAD:
        {
            // ---- 从文件加载图像 ----
            // 检查文件是否存在
            if (GetFileAttributes(state->savePath) == INVALID_FILE_ATTRIBUTES)
            {
                MessageBox(hwnd,
                    L"文件不存在!\n请先点击 \"保存\" 按钮创建图像。",
                    L"提示", MB_OK | MB_ICONWARNING);
                return 0;
            }

            state->imageLoaded = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        case ID_LIST_CODECS:
        {
            // ListBox 通知消息
            if (HIWORD(wParam) == LBN_DBLCLK)
            {
                // 双击编码器条目 — 显示详细信息
                HWND hListBox = (HWND)lParam;
                int sel = (int)SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                {
                    ShowCodecDetail(hwnd, sel);
                }
            }
            break;
        }
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!state)
        {
            EndPaint(hwnd, &ps);
            return 0;
        }

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        // 填充背景
        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &clientRc, hBg);
        DeleteObject(hBg);

        using namespace Gdiplus;

        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

        // ---- 左侧: 图像预览区域 ----
        int rightPanelWidth = 260;
        RECT previewRc = { 5, 5,
                           clientRc.right - rightPanelWidth - 10,
                           clientRc.bottom - 5 };

        // 加载或显示图像
        Image* pImage = nullptr;
        if (state->imageLoaded)
        {
            pImage = Bitmap::FromFile(state->savePath);
        }

        DrawPreview(graphics, previewRc, pImage);

        if (pImage)
            delete pImage;

        // ---- 右侧分隔线 ----
        Pen dividerPen(Color(255, 220, 220, 220), 1.0f);
        int dividerX = clientRc.right - rightPanelWidth;
        graphics.DrawLine(&dividerPen,
                          (REAL)dividerX, (REAL)0,
                          (REAL)dividerX, (REAL)clientRc.bottom);

        // ---- 右侧面板标题 ----
        FontFamily fontFamily(L"微软雅黑");
        Font sectionFont(&fontFamily, 11, FontStyleBold, UnitPixel);
        SolidBrush sectionBrush(Color(255, 80, 80, 80));

        // 编码器提示文字
        SolidBrush tipBrush(Color(255, 130, 130, 130));
        Font tipFont(&fontFamily, 10, FontStyleRegular, UnitPixel);
        graphics.DrawString(L"双击条目查看编码器详细信息",
                            -1, &tipFont,
                            PointF((REAL)(dividerX + 10), 52), &tipBrush);

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
        L"GDI+ 图像编解码示例",          // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 450,                       // 窗口大小
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
