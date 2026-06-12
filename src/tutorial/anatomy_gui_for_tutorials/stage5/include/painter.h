#pragma once

#include "geometry.h"
#include <cairo/cairo.h>
#include <string>

namespace miniui {

/// Thin wrapper around cairo_t* (non-owning).
class Painter {
public:
    explicit Painter(cairo_t* cr) : cr_(cr) {}

    void save()    { cairo_save(cr_); }
    void restore() { cairo_restore(cr_); }

    void translate(double dx, double dy) { cairo_translate(cr_, dx, dy); }
    void clip(double x, double y, double w, double h) {
        cairo_rectangle(cr_, x, y, w, h); cairo_clip(cr_);
    }

    void setSourceRGB(double r, double g, double b) { cairo_set_source_rgb(cr_, r, g, b); }
    void setSourceRGBA(double r, double g, double b, double a) { cairo_set_source_rgba(cr_, r, g, b, a); }
    void setSourceColor(const Color& c) { cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a); }

    void rectangle(double x, double y, double w, double h) { cairo_rectangle(cr_, x, y, w, h); }
    void fill()    { cairo_fill(cr_); }
    void moveTo(double x, double y) { cairo_move_to(cr_, x, y); }
    void lineTo(double x, double y) { cairo_line_to(cr_, x, y); }
    void stroke()  { cairo_stroke(cr_); }
    void setLineWidth(double w) { cairo_set_line_width(cr_, w); }

    void selectFontFace(const char* family,
                        cairo_font_slant_t s = CAIRO_FONT_SLANT_NORMAL,
                        cairo_font_weight_t w = CAIRO_FONT_WEIGHT_NORMAL) {
        cairo_select_font_face(cr_, family, s, w);
    }
    void setFontSize(double sz) { cairo_set_font_size(cr_, sz); }
    void showText(const std::string& t) { cairo_show_text(cr_, t.c_str()); }

    struct TextExtents {
        double x_bearing, y_bearing, width, height, x_advance, y_advance;
    };
    TextExtents textExtents(const std::string& t) {
        cairo_text_extents_t e; cairo_text_extents(cr_, t.c_str(), &e);
        return {e.x_bearing, e.y_bearing, e.width, e.height, e.x_advance, e.y_advance};
    }

    void paint() { cairo_paint(cr_); }
    cairo_t* raw() const { return cr_; }

private:
    cairo_t* cr_;
};

} // namespace miniui
