#pragma once
/// @file double_buffer.h
/// @brief Double-buffered rendering to eliminate flicker.

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <xcb/xcb.h>

namespace miniui {

/// Manages a back-buffer Cairo surface for flicker-free rendering.
class DoubleBuffer {
public:
    DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                 xcb_visualtype_t* visual, int width, int height);
    ~DoubleBuffer();

    /// Begin painting. Returns a cairo_t* for drawing on the back buffer.
    /// Returns nullptr if the back buffer is invalid.
    cairo_t* beginPaint();

    /// End painting. Copies the back buffer to the front (the XCB window).
    void endPaint();

    /// Resize the buffers (e.g., on configure notify).
    void resize(int width, int height);

private:
    void createBackBuffer();
    void destroyBackBuffer();

    xcb_connection_t*  conn_;
    xcb_window_t       win_;
    xcb_visualtype_t*  visual_;
    int                width_;
    int                height_;

    cairo_surface_t*   frontSurface_ = nullptr; // XCB window surface
    cairo_surface_t*   backSurface_  = nullptr; // Image surface (back buffer)
    cairo_t*           frontCr_      = nullptr;
    bool               painting_     = false;
};

} // namespace miniui
