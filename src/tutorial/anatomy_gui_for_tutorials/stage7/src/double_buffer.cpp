#include "double_buffer.h"
#include "painter.h"
#include <cstdio>

namespace miniui {

DoubleBuffer::DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                           xcb_visualtype_t* visual, uint16_t w, uint16_t h)
    : conn_(conn), window_(win), visual_(visual), width_(w), height_(h), backSurface_(nullptr)
{
    resize(w, h);
}

DoubleBuffer::~DoubleBuffer() {
    if (paintCr_) cairo_destroy(paintCr_);
    if (backSurface_) cairo_surface_destroy(backSurface_);
}

Painter DoubleBuffer::beginPaint(const std::vector<Rect>& dirtyRegions) {
    (void)dirtyRegions;
    // Destroy previous paint context (flushes pending ops to back surface)
    if (paintCr_) {
        cairo_destroy(paintCr_);
        paintCr_ = nullptr;
    }
    paintCr_ = cairo_create(backSurface_);
    return Painter(paintCr_);
}

void DoubleBuffer::endPaint() {
    // Destroy the paint context first — this flushes all drawing to the back surface
    if (paintCr_) {
        cairo_destroy(paintCr_);
        paintCr_ = nullptr;
    }
    cairo_surface_flush(backSurface_);

    // Blit back buffer → window
    cairo_surface_t* front = cairo_xcb_surface_create(conn_, window_, visual_, width_, height_);
    cairo_t* cr = cairo_create(front);
    cairo_set_source_surface(cr, backSurface_, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(front);
    cairo_surface_destroy(front);
    xcb_flush(conn_);
}

void DoubleBuffer::resize(uint16_t w, uint16_t h) {
    if (paintCr_) { cairo_destroy(paintCr_); paintCr_ = nullptr; }
    if (backSurface_) cairo_surface_destroy(backSurface_);
    width_ = w;
    height_ = h;
    backSurface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
}

} // namespace miniui
