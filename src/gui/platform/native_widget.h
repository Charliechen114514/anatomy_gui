/**
 * @file native_widget.h
 * @brief A widget node backed by a real child window.
 */

#pragma once

#include <windows.h>

#include <string>

#include "framework/widget.h"

/**
 * @brief A widget that owns a child window.
 *
 * The system paints it, hit-tests it and gives it an accessibility story for
 * free. The price is that its pixels are not ours, and that it only exists
 * where Windows exists: this class lives in the platform layer for that reason.
 *
 * The node's id doubles as the Win32 control id, which is what lets a
 * WM_COMMAND turn back into a node with one lookup.
 */
class NativeWidget : public Widget {
public:
    /**
     * @brief Build a node for a system control class.
     *
     * @param[in] extended_style WS_EX_* flags for the child window.
     * @param[in] class_name A pre-registered class name, such as L"EDIT".
     * @param[in] text The initial text.
     * @param[in] style WS_* and class-specific flags for the child window.
     */
    NativeWidget(DWORD extended_style, const wchar_t* class_name, std::wstring text, DWORD style);

    ~NativeWidget() override;

    NativeWidget(const NativeWidget&)            = delete;
    NativeWidget& operator=(const NativeWidget&) = delete;

    /**
     * @brief Create the child window under a parent window.
     *
     * @param[in] parent_window The window that hosts the control.
     * @param[in] instance Module instance handle.
     * @return true when the system creates the control.
     */
    bool create(HWND parent_window, HINSTANCE instance);

    /**
     * @brief Replace the text the control shows.
     *
     * @param[in] text The new text.
     */
    void set_text(const std::wstring& text);

    /**
     * @brief Read the text the control shows.
     *
     * @return The current text.
     */
    [[nodiscard]] std::wstring text() const;

    /**
     * @brief Read the native handle.
     *
     * @return The handle, or nullptr before create() succeeds.
     */
    [[nodiscard]] HWND handle() const { return hwnd_; }

protected:
    void on_bounds_changed() override;

private:
    DWORD          extended_style_ = 0;
    const wchar_t* class_name_     = nullptr;
    std::wstring   text_;
    DWORD          style_ = 0;
    HWND           hwnd_  = nullptr;
};
