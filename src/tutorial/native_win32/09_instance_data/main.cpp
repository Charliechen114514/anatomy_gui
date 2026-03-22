/**
 * Win32 Instance Data State Management Example
 *
 * This example demonstrates the recommended way to manage per-window state
 * in Win32 applications using GWLP_USERDATA and the lpParam mechanism.
 *
 * Key Concepts:
 * 1. Define a StateInfo structure to hold application-specific state
 * 2. Pass the state pointer via CreateWindowEx's lpParam parameter
 * 3. Store/retrieve the state using SetWindowLongPtr/GetWindowLongPtr
 * 4. This is the recommended approach for multi-window applications
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string.h>
#include <wchar.h>  // swprintf

/**
 * Application state structure stored with each window instance.
 * This allows multiple windows to have independent state.
 */
struct StateInfo {
    int clickCount;          // Number of mouse clicks
    wchar_t text[256];       // Display text
    COLORREF bgColor;        // Background color
};

/**
 * Helper function to retrieve the application state from a window handle.
 * Returns nullptr if the window has no associated state.
 */
StateInfo* GetAppState(HWND hwnd) {
    return reinterpret_cast<StateInfo*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

/**
 * Window procedure for our main window.
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    StateInfo* pState = nullptr;

    switch (uMsg) {
        case WM_NCCREATE:
        {
            // Extract the state pointer passed via lpParam in CreateWindowEx
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pState = reinterpret_cast<StateInfo*>(pCreate->lpCreateParams);

            // Store the state pointer in the window's user data
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pState));

            // Initialize state with default values if needed
            if (pState->clickCount == 0 && pState->text[0] == L'\0') {
                wcscpy_s(pState->text, 256, L"Click anywhere!");
                pState->bgColor = RGB(240, 240, 240);
            }

            // Continue with window creation
            return 1;
        }

        case WM_LBUTTONDOWN:
        {
            pState = GetAppState(hwnd);
            if (pState) {
                // Update state on mouse click
                pState->clickCount++;

                // Update text based on click count
                if (pState->clickCount == 1) {
                    wcscpy_s(pState->text, 256, L"Clicked once!");
                } else {
                    swprintf_s(pState->text, 256, L"Clicked %d times!", pState->clickCount);
                }

                // Change background color slightly on each click
                pState->bgColor = RGB(
                    240 - (pState->clickCount * 10) % 100,
                    240 - (pState->clickCount * 5) % 100,
                    240
                );

                // Force a repaint to show the updated state
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            pState = GetAppState(hwnd);
            if (pState) {
                // Draw background using state color
                RECT rect;
                GetClientRect(hwnd, &rect);
                HBRUSH hBrush = CreateSolidBrush(pState->bgColor);
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);

                // Draw the state text
                SetTextColor(hdc, RGB(0, 0, 0));
                SetBkMode(hdc, TRANSPARENT);

                // Draw click count
                wchar_t countText[64];
                swprintf_s(countText, 64, L"Click Count: %d", pState->clickCount);
                TextOut(hdc, 50, 50, countText, static_cast<int>(wcslen(countText)));

                // Draw the text from state
                TextOut(hdc, 50, 100, pState->text, static_cast<int>(wcslen(pState->text)));

                // Draw instructions
                const wchar_t* instructions = L"Left-click to increment counter";
                TextOut(hdc, 50, 150, instructions, static_cast<int>(wcslen(instructions)));
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
        {
            // Clean up the state when window is destroyed
            pState = GetAppState(hwnd);
            if (pState) {
                delete pState;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
            }
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/**
 * Entry point for the application.
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class
    const wchar_t CLASS_NAME[] = L"InstanceDataWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    // Create and initialize the state structure
    StateInfo* pState = new StateInfo();
    pState->clickCount = 0;
    wcscpy_s(pState->text, 256, L"Click anywhere!");
    pState->bgColor = RGB(240, 240, 240);

    // Create the window, passing the state pointer via lpParam
    HWND hwnd = CreateWindowEx(
        0,                          // Optional window styles
        CLASS_NAME,                 // Window class name
        L"Instance Data State Management",  // Window title
        WS_OVERLAPPEDWINDOW,        // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,  // Position and size
        nullptr,                    // Parent window
        nullptr,                    // Menu
        hInstance,                  // Instance handle
        pState                      // Additional application data (lpParam)
    );

    if (hwnd == nullptr) {
        delete pState;
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
