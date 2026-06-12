#include "double_buffer.h"

namespace miniui {

DoubleBuffer::~DoubleBuffer() {
    if (cr_) cairo_destroy(cr_);
    if (surface_) cairo_surface_destroy(surface_);
}

void DoubleBuffer::ensureSize(int w, int h) {
    if (w == width_ && h == height_) return;
    if (cr_) cairo_destroy(cr_);
    if (surface_) cairo_surface_destroy(surface_);

    width_  = w;
    height_ = h;
    surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cr_ = cairo_create(surface_);
}

void DoubleBuffer::blitTo(cairo_t* target, int targetW, int targetH) {
    if (!surface_) return;
    cairo_set_source_surface(target, surface_, 0, 0);
    cairo_rectangle(target, 0, 0, targetW, targetH);
    cairo_fill(target);
}

} // namespace miniui
