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

// The HWNDs of the controls the click handler must reach. The button needs
// no global - WM_COMMAND hands its HWND over in lParam. The edit and the
// label send no messages, so their HWNDs have nowhere to live yet. Globals
// it is (fuel for pain point 1); static keeps them file-local.
static HWND g_edit_hwnd  = nullptr;
static HWND g_label_hwnd = nullptr;

// The window procedure: where every message this program receives lands.
// static: only the system calls it back, and only this file needs it.
// Naming exemption: WndProc / wParam / lParam are the SDK's own words. The
// platform boundary skips this repo's naming rules - readers hold this code
// against the official docs.
// NOLINTBEGIN(readability-identifier-naming)
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // NOLINTEND(readability-identifier-naming)
    // Two messages known now - the switch is back. Every control added from
    // here lengthens this branch (fuel for pain point 3).
    switch (msg) {
    case WM_COMMAND:
        // A control notified its parent. Unpack the envelope:
        //   LOWORD(wParam) = control ID, HIWORD(wParam) = notification code,
        //   lParam = the control's own HWND.
        if (LOWORD(wParam) == kIdButton && HIWORD(wParam) == BN_CLICKED) {
            // The click handler: read the edit box, write the label.
            wchar_t text[256];
            GetWindowTextW(g_edit_hwnd, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
            SetWindowTextW(g_label_hwnd, text);
            return 0;
        }
        break;  // someone else's command - hand it to DefWindowProc below

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
    //    parent's client area, in pixels - they never move on their own
    //    (fuel for pain point 4).
    // The SDK pun: for a child window, hMenu carries the control ID - an
    // integer smuggled through a handle parameter.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    g_edit_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 20, 420, 28,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEdit)), hInstance, nullptr);

    HWND button_hwnd = CreateWindowExW(
        0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE, 20, 60, 120, 36, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdButton)), hInstance, nullptr);

    g_label_hwnd = CreateWindowExW(
        0, L"STATIC", L"（点按钮后，输入框的内容会显示在这里）", WS_CHILD | WS_VISIBLE, 20, 110,
        420, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLabel)), hInstance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)

    // Controls default to the antique SYSTEM_FONT - small and ugly for
    // Chinese text. One WM_SETFONT each with a stock font fixes it. Stock
    // objects are shared; never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(g_edit_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(button_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
    SendMessageW(g_label_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

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
