#include "painter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace miniui {

Painter::Painter(cairo_t* cr) : cr_(cr) {}

void Painter::save()    { cairo_save(cr_); }
void Painter::restore() { cairo_restore(cr_); }

void Painter::translate(double dx, double dy) {
    cairo_translate(cr_, dx, dy);
}

void Painter::fillRect(Rect rect, const Color& c) {
    cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr_);
}

void Painter::strokeRect(Rect rect, const Color& c, double lineWidth) {
    cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    cairo_set_line_width(cr_, lineWidth);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_stroke(cr_);
}

void Painter::drawText(int x, int y, const std::string& text,
                       const Color& color, int fontSize) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_select_font_face(cr_, "sans",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, static_cast<double>(fontSize));
    cairo_move_to(cr_, static_cast<double>(x), static_cast<double>(y));
    cairo_show_text(cr_, text.c_str());
}

void Painter::roundedRect(Rect rect, double radius) {
    double x = rect.x;
    double y = rect.y;
    double w = rect.width;
    double h = rect.height;

    cairo_new_sub_path(cr_);
    cairo_arc(cr_, x + radius, y + radius, radius, M_PI, 3 * M_PI / 2);
    cairo_arc(cr_, x + w - radius, y + radius, radius, -M_PI / 2, 0);
    cairo_arc(cr_, x + w - radius, y + h - radius, radius, 0, M_PI / 2);
    cairo_arc(cr_, x + radius, y + h - radius, radius, M_PI / 2, M_PI);
    cairo_close_path(cr_);
}

void Painter::fill(const Color& c) {
    cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    cairo_fill(cr_);
}

void Painter::stroke(const Color& c, double lineWidth) {
    cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
    cairo_set_line_width(cr_, lineWidth);
    cairo_stroke(cr_);
}

} // namespace miniui
