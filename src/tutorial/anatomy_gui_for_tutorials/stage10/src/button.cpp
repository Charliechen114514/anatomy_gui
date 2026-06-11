#include "button.h"
#include "painter.h"

namespace miniui {

Size Button::measure(const Size& available) {
    // Measure text to determine button size.
    cairo_surface_t* dummy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(dummy);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, text_.c_str(), &ext);

    cairo_destroy(cr);
    cairo_surface_destroy(dummy);

    Size sz{ext.width + 24, ext.height + 16};
    frame_.width  = sz.width;
    frame_.height = sz.height;
    return sz;
}

void Button::paint(Painter& p) {
    // Background
    if (pressed_) {
        p.setSourceRGB(0.6, 0.7, 0.85);
    } else {
        p.setSourceRGB(0.8, 0.85, 0.95);
    }
    p.rectangle(frame_);
    p.fill();

    // Border
    p.setSourceRGB(0.3, 0.3, 0.5);
    p.setLineWidth(1.5);
    p.rectangle(frame_);
    p.stroke();

    // Text
    p.setSourceRGB(0.1, 0.1, 0.1);
    p.selectFontFace("sans", CAIRO_FONT_SLANT_NORMAL,
                     CAIRO_FONT_WEIGHT_BOLD);
    p.setFontSize(14.0);

    auto ext = p.textExtents(text_.c_str());
    double tx = (frame_.width - ext.width) / 2.0;
    double ty = (frame_.height + ext.height) / 2.0;

    p.moveTo(tx, ty);
    p.showText(text_.c_str());
}

void Button::onMousePress(Point p) {
    std::fprintf(stderr, "[DBG] Button::onMousePress local=(%f,%f) frame=(%f,%f,%f,%f)\n",
                 p.x, p.y, frame_.x, frame_.y, frame_.width, frame_.height);
    pressed_ = true;
    repaint();
    Widget::onMousePress(p);
}

void Button::onMouseRelease(Point p) {
    std::fprintf(stderr, "[DBG] Button::onMouseRelease local=(%f,%f) pressed=%d\n",
                 p.x, p.y, pressed_);
    bool wasPressed = pressed_;
    pressed_ = false;
    // p is in widget-local coords; check against (0,0,width,height)
    Rect localBounds{0, 0, frame_.width, frame_.height};
    if (wasPressed && localBounds.contains(p)) {
        std::fprintf(stderr, "[DBG]   → clicked_.emit()\n");
        clicked_.emit();
    }
    repaint();
    Widget::onMouseRelease(p);
}

} // namespace miniui
