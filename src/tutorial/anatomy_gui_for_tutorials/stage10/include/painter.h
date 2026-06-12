#pragma once

#include "geometry.h"

#include <cairo/cairo.h>

namespace miniui {

/// Thin RAII-free wrapper around cairo_t*.
/// The Painter does NOT own the cairo context; its lifetime is managed externally.
class Painter {
public:
    explicit Painter(cairo_t* cr) : cr_(cr) {}

    cairo_t* native() const { return cr_; }

    // --- State management ---
    void save()   { cairo_save(cr_); }
    void restore() { cairo_restore(cr_); }

    // --- Source ---
    void setSourceRGB(double r, double g, double b) {
        cairo_set_source_rgb(cr_, r, g, b);
    }

    void setSourceRGBA(double r, double g, double b, double a) {
        cairo_set_source_rgba(cr_, r, g, b, a);
    }

    void setSourceColor(const Color& c) {
        cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    }

    // --- Paths ---
    void rectangle(double x, double y, double w, double h) {
        cairo_rectangle(cr_, x, y, w, h);
    }

    void rectangle(const Rect& r) {
        cairo_rectangle(cr_, r.x, r.y, r.width, r.height);
    }

    // --- Drawing ---
    void fill()   { cairo_fill(cr_); }
    void stroke() { cairo_stroke(cr_); }

    void moveTo(double x, double y) { cairo_move_to(cr_, x, y); }
    void lineTo(double x, double y) { cairo_line_to(cr_, x, y); }

    void setLineWidth(double w) { cairo_set_line_width(cr_, w); }

    // --- Text ---
    void selectFontFace(const char* family,
                        cairo_font_slant_t slant  = CAIRO_FONT_SLANT_NORMAL,
                        cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL) {
        cairo_select_font_face(cr_, family, slant, weight);
    }

    void setFontSize(double size) { cairo_set_font_size(cr_, size); }

    void showText(const char* text) { cairo_show_text(cr_, text); }

    cairo_text_extents_t textExtents(const char* text) {
        cairo_text_extents_t ext;
        cairo_text_extents(cr_, text, &ext);
        return ext;
    }

    // --- Transform ---
    void translate(double tx, double ty) { cairo_translate(cr_, tx, ty); }

    // --- Clipping ---
    void clip()            { cairo_clip(cr_); }
    void resetClip()       { cairo_reset_clip(cr_); }

    // --- Convenience ---
    void clear(double r, double g, double b) {
        cairo_set_source_rgb(cr_, r, g, b);
        cairo_paint(cr_);
    }

    void paintSurface() { cairo_paint(cr_); }

private:
    cairo_t* cr_;
};

} // namespace miniui
