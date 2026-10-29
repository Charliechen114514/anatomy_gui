/**
 * Win32 Global Variable State Management Example
 *
 * Demonstrates using global variables to manage application state.
 * This is a simple and practical approach for small applications.
 */

#include <windows.h>
#include <tchar.h>

// Global state variables
int g_clickCount = 0;           // Stores the number of clicks
TCHAR g_windowText[256] = { 0 }; // Buffer for display text

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateDisplayText();

// Entry point
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Register window class
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("GlobalStateWindow");

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, _T("Window class registration failed!"), _T("Error"), MB_ICONERROR);
        return 1;
    }

    // Create window
    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        _T("Global Variable State Management"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        MessageBox(NULL, _T("Window creation failed!"), _T("Error"), MB_ICONERROR);
        return 1;
    }

    // Initialize display text
    UpdateDisplayText();

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    {
        // Update global state: increment click count
        g_clickCount++;

        // Update display text based on new state
        UpdateDisplayText();

        // Trigger redraw to show updated state
        InvalidateRect(hwnd, NULL, TRUE);

        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        // Right click resets the counter
        g_clickCount = 0;
        UpdateDisplayText();
        InvalidateRect(hwnd, NULL, TRUE);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Set text properties
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // Draw main text
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Draw title
        DrawText(hdc, _T("Click anywhere to increment!"), -1, &rc,
                 DT_CENTER | DT_TOP | DT_SINGLELINE);

        // Draw click count (centered)
        DrawText(hdc, g_windowText, -1, &rc,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Draw instruction at bottom
        DrawText(hdc, _T("Right-click to reset"), -1, &rc,
                 DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Update display text based on current global state
void UpdateDisplayText()
{
    // Format the click count into the display buffer
    wsprintf(g_windowText, _T("Click Count: %d"), g_clickCount);
}
