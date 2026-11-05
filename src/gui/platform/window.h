/**
 * @file window.h
 * @brief A window together with the object that owns it.
 */

#pragma once

#include <windows.h>

/**
 * @brief A window plus the object that owns it, tied so the window procedure
 * can find its way back to the instance.
 *
 * The window procedure is a C callback that knows nothing about `this`, so the
 * object travels through the creation parameters and gets stashed on the window
 * itself. This is the answer to pain point 1 of the concrete pass: state
 * finally has somewhere to live.
 *
 * Parameter names keep the SDK's spelling (msg / wparam / lparam) so this code
 * still reads against the official documentation.
 */
class Window {
public:
    Window()                         = default;
    virtual ~Window()                = default;
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Register the window class and create the window.
     *
     * @param[in] instance Module instance handle from wWinMain.
     * @param[in] title Window title, shown in the caption.
     * @param[in] width Initial width in pixels.
     * @param[in] height Initial height in pixels.
     * @return true when the system accepts both steps.
     */
    bool create(HINSTANCE instance, const wchar_t* title, int width, int height);

    /**
     * @brief Show the window and force the first paint.
     *
     * @param[in] show_command One of the SW_* values, usually straight from
     * wWinMain's nShowCmd.
     */
    void show(int show_command) const;

    /**
     * @brief Mark the whole client area for repainting.
     */
    void invalidate();

    /**
     * @brief Read the native window handle.
     *
     * @return The handle, or nullptr before create() succeeds.
     */
    [[nodiscard]] HWND handle() const { return hwnd_; }

protected:
    /**
     * @brief Handle one message.
     *
     * A subclass handles the messages it knows and delegates everything else to
     * Window::handle_message.
     *
     * @param[in] msg Message identifier.
     * @param[in] wparam Message-specific word parameter.
     * @param[in] lparam Message-specific long parameter.
     * @return The value the system expects for this message.
     */
    virtual LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

private:
    // The C callback the system calls. It cannot be an ordinary member
    // function: the system knows nothing about `this`.
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND hwnd_ = nullptr;
};
