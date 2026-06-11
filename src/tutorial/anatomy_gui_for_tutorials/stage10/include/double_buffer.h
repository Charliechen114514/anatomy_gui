#pragma once

#include "geometry.h"

#include <cairo/cairo.h>

namespace miniui {

/// Double-buffered drawing surface.
/// Renders to an off-screen pixmap, then blits to the target surface
/// in a single operation to eliminate flicker.
class DoubleBuffer {
public:
    DoubleBuffer() = default;
    ~DoubleBuffer();

    DoubleBuffer(const DoubleBuffer&) = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;

    /// Ensure the buffer is at least `w` x `h`.  Re-creates if needed.
    void ensureSize(int w, int h);

    /// Get the off-screen cairo context for drawing.
    cairo_t* context() const { return cr_; }

    /// Blit the off-screen buffer to `target`.
    void blitTo(cairo_t* target, int targetW, int targetH);

private:
    cairo_surface_t* surface_ = nullptr;
    cairo_t* cr_ = nullptr;
    int width_  = 0;
    int height_ = 0;
};

} // namespace miniui
