#include "double_buffer.h"
#include <cstdio>

namespace miniui {

DoubleBuffer::DoubleBuffer(xcb_connection_t* conn, xcb_window_t win,
                           xcb_visualtype_t* visual, int width, int height)
    : conn_(conn), win_(win), visual_(visual),
      width_(width), height_(height) {

    // Create the front surface (tied to the XCB window)
    frontSurface_ = cairo_xcb_surface_create(conn_, win_, visual_,
                                              width_, height_);
    frontCr_ = cairo_create(frontSurface_);

    // Create the back buffer (image surface)
    createBackBuffer();
}

DoubleBuffer::~DoubleBuffer() {
    destroyBackBuffer();
    if (frontCr_) cairo_destroy(frontCr_);
    if (frontSurface_) cairo_surface_destroy(frontSurface_);
}

void DoubleBuffer::createBackBuffer() {
    if (width_ <= 0 || height_ <= 0) return;
    backSurface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               width_, height_);
}

void DoubleBuffer::destroyBackBuffer() {
    if (backSurface_) {
        cairo_surface_destroy(backSurface_);
        backSurface_ = nullptr;
    }
}

cairo_t* DoubleBuffer::beginPaint() {
    if (painting_) return nullptr;
    if (!backSurface_) return nullptr;

    painting_ = true;
    // Return a new context for the back buffer
    return cairo_create(backSurface_);
}

void DoubleBuffer::endPaint() {
    if (!painting_) return;
    painting_ = false;

    if (backSurface_ && frontCr_) {
        // Copy back buffer to front
        cairo_set_source_surface(frontCr_, backSurface_, 0, 0);
        cairo_paint(frontCr_);
        cairo_surface_flush(frontSurface_);
    }
}

void DoubleBuffer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_  = width;
    height_ = height;

    // Resize front surface
    if (frontSurface_) {
        cairo_xcb_surface_set_size(frontSurface_, width_, height_);
    }

    // Recreate back buffer at new size
    destroyBackBuffer();
    createBackBuffer();
}

} // namespace miniui
