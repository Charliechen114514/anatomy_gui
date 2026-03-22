// 04_simple_notepad - 简单记事本练习
// 功能：最简单的文本编辑器，支持字符输入、退格删除、光标位置显示
//
// 技术要点：
// - WM_CHAR 处理字符输入
// - WM_KEYDOWN 处理特殊键（退格、回车、方向键等）
// - WM_PAINT 使用 DrawTextW 显示多行文本
// - 光标位置计算（行号、列号）

#include <windows.h>
#include <string>
#include <vector>

// 窗口类名
const wchar_t CLASS_NAME[] = L"SimpleNotepadWindow";

// 全局变量：存储文本内容
std::wstring g_text;

// 光标位置（字符索引）
size_t g_cursorPos = 0;

// 视图偏移（用于滚动）
int g_scrollY = 0;

// 字体
HFONT g_hFont = nullptr;

// 字符高度和宽度
int g_charWidth = 8;
int g_charHeight = 16;

// 辅助函数：将文本按行分割
std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    size_t start = 0;

    while (start < text.length()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    if (lines.empty()) {
        lines.push_back(L"");
    }

    return lines;
}

// 辅助函数：获取光标所在的行和列
void GetCursorLineCol(size_t cursorPos, const std::vector<std::wstring>& lines, int& line, int& col) {
    line = 0;
    col = 0;
    size_t pos = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        size_t lineLen = lines[i].length();
        // 每行末尾有一个换行符（除了最后一行）
        size_t totalLen = lineLen + (i < lines.size() - 1 ? 1 : 0);

        if (pos + totalLen > cursorPos || i == lines.size() - 1) {
            line = static_cast<int>(i);
            col = static_cast<int>(cursorPos - pos);
            return;
        }

        pos += totalLen;
    }
}

// 辅助函数：从行列计算字符位置
size_t GetPosFromLineCol(int line, int col, const std::vector<std::wstring>& lines) {
    size_t pos = 0;

    for (int i = 0; i < line && i < static_cast<int>(lines.size()); ++i) {
        pos += lines[i].length() + 1; // +1 for newline
    }

    pos += col;

    return pos;
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // 创建等宽字体
        g_hFont = CreateFontW(
            16,                    // 字符高度
            8,                     // 字符宽度
            0,                     // 转义角度
            0,                     // 方向角度
            FW_NORMAL,             // 字体粗细
            FALSE,                 // 斜体
            FALSE,                 // 下划线
            FALSE,                 // 删除线
            DEFAULT_CHARSET,       // 字符集
            OUT_DEFAULT_PRECIS,    // 输出精度
            CLIP_DEFAULT_PRECIS,   // 裁剪精度
            DEFAULT_QUALITY,       // 输出质量
            FIXED_PITCH | FF_MODERN, // 字体间距和族
            L"Consolas"            // 字体名称
        );

        // 获取字符尺寸
        HDC hdc = GetDC(hwnd);
        HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        g_charWidth = tm.tmAveCharWidth;
        g_charHeight = tm.tmHeight + tm.tmExternalLeading;
        SelectObject(hdc, hOldFont);
        ReleaseDC(hwnd, hdc);

        // 初始化空文本
        g_text = L"";
        g_cursorPos = 0;

        // 创建插入符（光标）
        CreateCaret(hwnd, nullptr, 2, g_charHeight);
        SetCaretPos(2, 2);
        ShowCaret(hwnd);

        return 0;
    }

    case WM_DESTROY: {
        if (g_hFont) {
            DeleteObject(g_hFont);
            g_hFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    case WM_SIZE: {
        // 窗口大小改变时重新计算滚动范围
        RECT rect;
        GetClientRect(hwnd, &rect);
        int clientHeight = rect.bottom - rect.top;

        std::vector<std::wstring> lines = SplitLines(g_text);
        int totalHeight = static_cast<int>(lines.size()) * g_charHeight;

        SCROLLINFO si = {0};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = totalHeight;
        si.nPage = clientHeight;
        si.nPos = g_scrollY;

        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {0};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int yPos = si.nPos;

        switch (LOWORD(wParam)) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
        case SB_LINEUP:
            si.nPos -= g_charHeight;
            break;
        case SB_LINEDOWN:
            si.nPos += g_charHeight;
            break;
        case SB_PAGEUP:
            si.nPos -= si.nPage;
            break;
        case SB_PAGEDOWN:
            si.nPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
        }

        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);

        if (yPos != si.nPos) {
            ScrollWindow(hwnd, 0, yPos - si.nPos, nullptr, nullptr);
            g_scrollY = si.nPos;
            UpdateWindow(hwnd);
        }

        return 0;
    }

    case WM_SETFOCUS: {
        // 获得焦点时显示光标
        if (!g_hFont) {
            CreateCaret(hwnd, nullptr, 2, g_charHeight);
        }
        ShowCaret(hwnd);
        return 0;
    }

    case WM_KILLFOCUS: {
        // 失去焦点时隐藏光标
        HideCaret(hwnd);
        return 0;
    }

    case WM_CHAR: {
        // 处理可打印字符
        if (wParam >= 32 && wParam != 127) {
            wchar_t ch = static_cast<wchar_t>(wParam);

            // 在光标位置插入字符
            g_text.insert(g_cursorPos, 1, ch);
            g_cursorPos++;

            // 更新插入符位置
            std::vector<std::wstring> lines = SplitLines(g_text);
            int line, col;
            GetCursorLineCol(g_cursorPos, lines, line, col);

            int caretY = 2 + line * g_charHeight - g_scrollY;
            int caretX = 2 + col * g_charWidth;
            SetCaretPos(caretX, caretY);

            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        std::vector<std::wstring> lines = SplitLines(g_text);
        int line, col;
        GetCursorLineCol(g_cursorPos, lines, line, col);

        switch (wParam) {
        case VK_BACK: {
            // 退格键：删除光标前一个字符
            if (g_cursorPos > 0) {
                g_cursorPos--;
                g_text.erase(g_cursorPos, 1);

                GetCursorLineCol(g_cursorPos, lines, line, col);
                int caretY = 2 + line * g_charHeight - g_scrollY;
                int caretX = 2 + col * g_charWidth;
                SetCaretPos(caretX, caretY);

                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        }

        case VK_DELETE: {
            // Delete键：删除光标后一个字符
            if (g_cursorPos < g_text.length()) {
                g_text.erase(g_cursorPos, 1);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        }

        case VK_RETURN: {
            // 回车键：插入换行符
            g_text.insert(g_cursorPos, 1, L'\n');
            g_cursorPos++;

            lines = SplitLines(g_text);
            GetCursorLineCol(g_cursorPos, lines, line, col);

            int caretY = 2 + line * g_charHeight - g_scrollY;
            int caretX = 2 + col * g_charWidth;
            SetCaretPos(caretX, caretY);

            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }

        case VK_LEFT: {
            // 左箭头：光标左移
            if (g_cursorPos > 0) {
                g_cursorPos--;
                GetCursorLineCol(g_cursorPos, lines, line, col);
                int caretY = 2 + line * g_charHeight - g_scrollY;
                int caretX = 2 + col * g_charWidth;
                SetCaretPos(caretX, caretY);
            }
            break;
        }

        case VK_RIGHT: {
            // 右箭头：光标右移
            if (g_cursorPos < g_text.length()) {
                g_cursorPos++;
                GetCursorLineCol(g_cursorPos, lines, line, col);
                int caretY = 2 + line * g_charHeight - g_scrollY;
                int caretX = 2 + col * g_charWidth;
                SetCaretPos(caretX, caretY);
            }
            break;
        }

        case VK_UP: {
            // 上箭头：光标上移一行
            if (line > 0) {
                int prevLineLen = static_cast<int>(lines[line - 1].length());
                int newCol = (col < prevLineLen) ? col : prevLineLen;
                g_cursorPos = GetPosFromLineCol(line - 1, newCol, lines);

                int caretY = 2 + (line - 1) * g_charHeight - g_scrollY;
                int caretX = 2 + newCol * g_charWidth;
                SetCaretPos(caretX, caretY);
            }
            break;
        }

        case VK_DOWN: {
            // 下箭头：光标下移一行
            if (line < static_cast<int>(lines.size()) - 1) {
                int nextLineLen = static_cast<int>(lines[line + 1].length());
                int newCol = (col < nextLineLen) ? col : nextLineLen;
                g_cursorPos = GetPosFromLineCol(line + 1, newCol, lines);

                int caretY = 2 + (line + 1) * g_charHeight - g_scrollY;
                int caretX = 2 + newCol * g_charWidth;
                SetCaretPos(caretX, caretY);
            }
            break;
        }

        case VK_HOME: {
            // Home键：移到行首
            g_cursorPos = GetPosFromLineCol(line, 0, lines);
            int caretY = 2 + line * g_charHeight - g_scrollY;
            SetCaretPos(2, caretY);
            break;
        }

        case VK_END: {
            // End键：移到行尾
            int lineLen = static_cast<int>(lines[line].length());
            g_cursorPos = GetPosFromLineCol(line, lineLen, lines);
            int caretY = 2 + line * g_charHeight - g_scrollY;
            int caretX = 2 + lineLen * g_charWidth;
            SetCaretPos(caretX, caretY);
            break;
        }
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 设置背景色
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

        // 选择字体
        HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(255, 255, 255));

        // 绘制文本
        std::vector<std::wstring> lines = SplitLines(g_text);

        // 计算可见行范围
        int firstLine = g_scrollY / g_charHeight;
        int lastLine = firstLine + (rect.bottom - rect.top) / g_charHeight + 1;
        if (lastLine > static_cast<int>(lines.size())) {
            lastLine = static_cast<int>(lines.size());
        }

        int yPos = 2 - (g_scrollY % g_charHeight);
        for (int i = firstLine; i < lastLine; ++i) {
            TextOutW(hdc, 2, yPos + i * g_charHeight,
                    lines[i].c_str(), static_cast<int>(lines[i].length()));
        }

        SelectObject(hdc, hOldFont);

        // 绘制状态栏
        RECT statusRect;
        statusRect.left = 0;
        statusRect.top = rect.bottom - 24;
        statusRect.right = rect.right;
        statusRect.bottom = rect.bottom;

        FillRect(hdc, &statusRect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

        // 获取光标位置
        int cursorLine, cursorCol;
        GetCursorLineCol(g_cursorPos, lines, cursorLine, cursorCol);

        // 显示状态信息
        wchar_t status[256];
        swprintf_s(status, L" 行: %d  列: %d  字符数: %zu ",
                  cursorLine + 1, cursorCol + 1, g_text.length());

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(192, 192, 192));
        TextOutW(hdc, 10, rect.bottom - 18, status, static_cast<int>(wcslen(status)));

        // 绘制分隔线
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, rect.bottom - 25, nullptr);
        LineTo(hdc, rect.right, rect.bottom - 25);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassW(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowExW(
        0,                              // 扩展窗口样式
        CLASS_NAME,                     // 窗口类名
        L"简单记事本 - Win32 练习",     // 窗口标题
        WS_OVERLAPPEDWINDOW | WS_VSCROLL, // 窗口样式
        CW_USEDEFAULT, CW_USEDEFAULT,   // 位置
        640, 480,                       // 大小
        nullptr,                        // 父窗口
        nullptr,                        // 菜单
        hInstance,                      // 实例句柄
        nullptr                         // 附加数据
    );

    if (!hwnd) {
        return 0;
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg = {0};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
