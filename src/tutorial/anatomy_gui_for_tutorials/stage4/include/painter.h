#pragma once

#include "geometry.h"

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <string>

namespace miniui {

/// Thin RAII-safe wrapper around cairo_t*.
/// Does NOT own the cairo context — lifetime is managed by the caller.
class Painter {
public:
    explicit Painter(cairo_t* cr) : cr_(cr) {}

    // ── State management ──────────────────────────────
    void save()   { cairo_save(cr_); }
    void restore() { cairo_restore(cr_); }

    // ── Coordinate transforms ─────────────────────────
    void translate(double dx, double dy) { cairo_translate(cr_, dx, dy); }

    // ── Clipping ──────────────────────────────────────
    void clip(double x, double y, double w, double h) {
        cairo_rectangle(cr_, x, y, w, h);
        cairo_clip(cr_);
    }

    // ── Source / color ────────────────────────────────
    void setSourceRGB(double r, double g, double b) {
        cairo_set_source_rgb(cr_, r, g, b);
    }
    void setSourceRGBA(double r, double g, double b, double a) {
        cairo_set_source_rgba(cr_, r, g, b, a);
    }
    void setSourceColor(const Color& c) {
        cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    }

    // ── Drawing primitives ────────────────────────────
    void rectangle(double x, double y, double w, double h) {
        cairo_rectangle(cr_, x, y, w, h);
    }
    void fill() { cairo_fill(cr_); }

    void moveTo(double x, double y) { cairo_move_to(cr_, x, y); }
    void lineTo(double x, double y) { cairo_line_to(cr_, x, y); }
    void stroke() { cairo_stroke(cr_); }

    void setLineWidth(double w) { cairo_set_line_width(cr_, w); }

    // ── Text ──────────────────────────────────────────
    void selectFontFace(const char* family,
                        cairo_font_slant_t slant  = CAIRO_FONT_SLANT_NORMAL,
                        cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL) {
        cairo_select_font_face(cr_, family, slant, weight);
    }
    void setFontSize(double size) { cairo_set_font_size(cr_, size); }
    void showText(const std::string& text) { cairo_show_text(cr_, text.c_str()); }

    struct TextExtents {
        double x_bearing, y_bearing;
        double width, height;
        double x_advance, y_advance;
    };

    TextExtents textExtents(const std::string& text) {
        cairo_text_extents_t ext;
        cairo_text_extents(cr_, text.c_str(), &ext);
        return {ext.x_bearing, ext.y_bearing,
                ext.width, ext.height,
                ext.x_advance, ext.y_advance};
    }

    void paint() { cairo_paint(cr_); }

    // ── Raw access (for platform-specific code) ───────
    cairo_t* raw() const { return cr_; }

private:
    cairo_t* cr_;
};

} // namespace miniui
