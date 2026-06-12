#pragma once
/// @file double_buffer.h — Off-screen back buffer for flicker-free rendering.

#include "geometry.h"
#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <xcb/xcb.h>
#include <vector>

namespace miniui {

class Painter;

/// Manages an off-screen image surface and blits to the XCB window surface.
class DoubleBuffer {
public:
    DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                 xcb_visualtype_t* visual, uint16_t w, uint16_t h);
    ~DoubleBuffer();

    /// Create a Painter bound to the back buffer. Caller must clip to dirty regions.
    Painter beginPaint(const std::vector<Rect>& dirtyRegions);

    /// Blit back buffer → window surface.
    void endPaint();

    /// Resize the back buffer (call on XCB_CONFIGURE_NOTIFY).
    void resize(uint16_t w, uint16_t h);

private:
    xcb_connection_t*  conn_;
    xcb_window_t       window_;
    xcb_visualtype_t*  visual_;
    cairo_surface_t*   backSurface_;
    cairo_t*           paintCr_ = nullptr;
    uint16_t width_, height_;
};

} // namespace miniui
