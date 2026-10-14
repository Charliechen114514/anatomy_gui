#include <windows.h>

// Control IDs. Defined here, consumed far away: once in CreateWindowExW
// below, once in the WM_COMMAND branch of WndProc above. Adding a fourth
// control means touching all three places (fuel for pain point 2).
constexpr int kIdEdit   = 1001;
constexpr int kIdButton = 1002;
constexpr int kIdLabel  = 1003;

// Window class name. Used once when registering the class and once more when
// creating the window, hence hoisted to a constant.
constexpr wchar_t kClassName[] = L"AnatomyGuiM1a";

// The HWNDs of the controls. The click handler reaches the edit and the label
// through these. The button once managed without a global, because WM_COMMAND
// hands its HWND over in lParam - then hand layout arrived and asked for all
// three. Globals it is (fuel for pain point 1); static keeps them file-local.
static HWND g_edit_hwnd   = nullptr;
static HWND g_button_hwnd = nullptr;
static HWND g_label_hwnd  = nullptr;

// The line this program paints itself, plus the rectangle layout_controls
// hands it. More file-scope state, same debt as the HWNDs above (fuel for
// pain point 1).
static wchar_t g_drawn_text[256] = L"（这行字是本程序自己画的，还没被更新过）";
static RECT    g_drawn_rect{};

// Place the controls by hand. Every resize runs this, and every new control
// costs one more SetWindowPos plus more arithmetic (fuel for pain point 4).
// The margin lives here as a local constant and nothing else can read it.
//
// Ordering trap: the first WM_SIZE arrives from CreateWindowExW before the
// child windows exist, so the guard below is load-bearing.
static void layout_controls(HWND parent) {
    if (g_edit_hwnd == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(parent, &client);

    const int kMargin       = 20;
    const int kGap          = 8;
    const int kEditHeight   = 28;
    const int kButtonHeight = 36;
    const int kButtonWidth  = 120;
    const int kLabelHeight  = 32;
    const int kDrawnHeight  = 32;

    const int kContentWidth = client.right - client.left - (kMargin * 2);
    const int kEditTop      = kMargin;
    const int kButtonTop    = kEditTop + kEditHeight + kGap;
    const int kLabelTop     = kButtonTop + kButtonHeight + kGap;
    const int kDrawnTop     = kLabelTop + kLabelHeight + kGap;

    SetWindowPos(g_edit_hwnd, nullptr, kMargin, kEditTop, kContentWidth, kEditHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_button_hwnd, nullptr, kMargin, kButtonTop, kButtonWidth, kButtonHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_label_hwnd, nullptr, kMargin, kLabelTop, kContentWidth, kLabelHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // The painted line is not a window, so it gets no SetWindowPos: its
    // rectangle is arithmetic handed to WM_PAINT through a global.
    g_drawn_rect.left   = kMargin;
    g_drawn_rect.top    = kDrawnTop;
    g_drawn_rect.right  = kMargin + kContentWidth;
    g_drawn_rect.bottom = kDrawnTop + kDrawnHeight;
}

// The window procedure: where every message this program receives lands.
// static: only the system calls it back, and only this file needs it.
// Naming exemption: WndProc / wParam / lParam are the SDK's own words. The
// platform boundary skips this repo's naming rules - readers hold this code
// against the official docs.
// NOLINTBEGIN(readability-identifier-naming)
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // NOLINTEND(readability-identifier-naming)
    // Six messages known now - the switch keeps growing. Every control added
    // from here lengthens this branch (fuel for pain point 3).
    switch (msg) {
    case WM_COMMAND:
        // A control notified its parent. Unpack the envelope:
        //   LOWORD(wParam) = control ID, HIWORD(wParam) = notification code,
        //   lParam = the control's own HWND.
        if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) {
            // The click handler: read the edit box, feed both outputs.
            wchar_t text[256];
            GetWindowTextW(g_edit_hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));

            // Output 1 - a system-drawn STATIC. SetWindowTextW invalidates the
            // control itself, so the new text appears with no help from here.
            SetWindowTextW(g_label_hwnd, text);

            // Output 2 - the line painted in WM_PAINT below. Copy the text and
            // stop: no InvalidateRect, on purpose. Nothing marks that
            // rectangle dirty, so the line keeps showing its previous value.
            // Pain point 5, left in place deliberately; the abstract pass
            // makes this call unnecessary instead of mandatory.
            lstrcpynW(g_drawn_text, text, 256);
            return 0;
        }
        break;  // someone else's command - hand it to DefWindowProc below

    case WM_SIZE:
        // Nothing else moves the controls: they sit where they were put until
        // this branch runs again (fuel for pain point 4).
        if (wParam != SIZE_MINIMIZED) {
            layout_controls(hwnd);
        }
        return 0;

    case WM_ERASEBKGND:
        // Claim the erase and do nothing. The class brush set in wWinMain
        // never runs now: WM_PAINT below owns the whole client area. Letting
        // the system erase first is what makes a resize flash.
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC         window_dc = BeginPaint(hwnd, &paint);

        // Double buffer: compose the frame in an off-screen bitmap, then blit
        // it in one go. Painting straight into the window DC leaves a visible
        // gap between filling the background and drawing the text.
        RECT client{};
        GetClientRect(hwnd, &client);
        HDC     buffer_dc = CreateCompatibleDC(window_dc);
        HBITMAP buffer    = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
        HGDIOBJ previous  = SelectObject(buffer_dc, buffer);

        // WM_ERASEBKGND above refused to erase, so the frame starts here.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        FillRect(buffer_dc, &client,
                 reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)));
        // NOLINTEND(performance-no-int-to-ptr)

        // The line this program paints itself. The STATIC above refreshes on
        // its own; this one changes only when something asks for an
        // invalidate (fuel for pain point 5).
        SetBkMode(buffer_dc, TRANSPARENT);
        DrawTextW(buffer_dc, g_drawn_text, -1, &g_drawn_rect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);

        SelectObject(buffer_dc, previous);
        DeleteObject(buffer);
        DeleteDC(buffer_dc);

        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_DPICHANGED: {
        // The other half of the DPI bill. Declaring per-monitor awareness in
        // the manifest means the window must resize itself when the system
        // moves it to a monitor with a different scale. Windows hands over a
        // suggested rectangle in lParam and expects SetWindowPos back; the
        // WM_SIZE that follows re-runs layout_controls.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        // NOLINTEND(performance-no-int-to-ptr)
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DESTROY:
        // Window destroyed != process exit. PostQuitMessage drops WM_QUIT
        // into the queue, GetMessage then returns 0 and the loop ends -
        // two separate things.
        PostQuitMessage(0);
        return 0;

    default:
        // Unhandled messages must go back to the system, or the window
        // cannot even paint its title bar and frame.
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // A case that broke out instead of returning lands here.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Naming exemption as above: parameter names follow the winbase.h declaration
// and MSDN (nShowCmd matches the header's own word).
// NOLINTBEGIN(readability-identifier-naming)
int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] PWSTR pCmdLine, int nShowCmd) {
    // NOLINTEND(readability-identifier-naming)
    // 1) Register the window class: tell the system what this kind of window
    //    looks like and who receives its messages.
    WNDCLASSEXW window_class{};
    window_class.cbSize        = sizeof(window_class);
    window_class.lpfnWndProc   = WndProc;     // where messages land
    window_class.hInstance     = hInstance;   // this module's instance handle
    window_class.lpszClassName = kClassName;  // class name, unique in the process
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    // The SDK's official idiom: the background brush comes from a system
    // color index (COLOR_* + 1); the integer-to-handle cast is unavoidable.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    // NOLINTEND(performance-no-int-to-ptr)
    if (RegisterClassExW(&window_class) == 0) {
        return 1;
    }

    // 2) Create the window. WS_CLIPCHILDREN stops the parent's repaints
    //    from smearing over the controls - without it, resizing leaves
    //    ghosts. ShowWindow has not run yet, so it stays invisible.
    HWND hwnd = CreateWindowExW(0, kClassName, L"Anatomy GUI · M1a",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                720, 480, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        return 1;
    }

    // 3) Create the controls. They are windows too - same CreateWindowExW,
    //    just with a parent and the system's pre-registered class names
    //    (EDIT / BUTTON / STATIC). Child coordinates are relative to the
    //    parent's client area, in pixels, and they never move on their own:
    //    layout_controls has to move them (fuel for pain point 4).
    // The zeros below are placeholders. The real geometry arrives from
    // layout_controls, which runs before ShowWindow, so nothing shows at 0,0.
    // The SDK pun: for a child window, hMenu carries the control ID - an
    // integer smuggled through a handle parameter.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    g_edit_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEdit)), hInstance, nullptr);

    g_button_hwnd = CreateWindowExW(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdButton)),
                                    hInstance, nullptr);

    g_label_hwnd = CreateWindowExW(
        0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLabel)), hInstance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)

    // Controls default to the antique SYSTEM_FONT - small and ugly for
    // Chinese text. One WM_SETFONT each with a stock font fixes it. Stock
    // objects are shared; never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(g_edit_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(g_button_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(g_label_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

    // Place the controls once before the window appears. The WM_SIZE that
    // arrived during creation fired while the children did not exist yet.
    layout_controls(hwnd);

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    // 4) The message loop: the heartbeat of the program.
    //    GetMessage returns 0 on WM_QUIT and -1 on failure.
    //    Writing while (GetMessage(...)) treats -1 as "keep going" - a
    //    classic trap.
    MSG msg{};
    while (true) {
        const BOOL kResult = GetMessageW(&msg, nullptr, 0, 0);
        if (kResult == 0) {
            break;  // WM_QUIT
        }
        if (kResult == -1) {
            return 1;  // failure
        }

        TranslateMessage(&msg);  // turns key messages into character messages
        DispatchMessageW(&msg);  // delivers the message to the window procedure
    }

    return static_cast<int>(msg.wParam);
}
