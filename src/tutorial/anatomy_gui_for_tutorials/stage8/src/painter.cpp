#include "painter.h"

namespace miniui {

Painter::Painter(cairo_t* cr) : cr_(cr) {}

void Painter::save() { cairo_save(cr_); }
void Painter::restore() { cairo_restore(cr_); }

void Painter::applyColor(const Color& c) {
    cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a);
}

void Painter::setFont(const std::string& family, double size) const {
    cairo_select_font_face(cr_, family.c_str(),
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, size);
}

void Painter::clear(const Color& color) {
    applyColor(color);
    cairo_paint(cr_);
}

void Painter::fillRect(const Rect& rect, const Color& color) {
    applyColor(color);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr_);
}

void Painter::strokeRect(const Rect& rect, const Color& color, double lineWidth) {
    applyColor(color);
    cairo_set_line_width(cr_, lineWidth);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_stroke(cr_);
}

void Painter::drawLine(Point p1, Point p2, const Color& color, double lineWidth) {
    applyColor(color);
    cairo_set_line_width(cr_, lineWidth);
    cairo_move_to(cr_, p1.x, p1.y);
    cairo_line_to(cr_, p2.x, p2.y);
    cairo_stroke(cr_);
}

void Painter::drawText(const std::string& text, Point pos, const Color& color,
                        const std::string& fontFamily, double fontSize) {
    applyColor(color);
    setFont(fontFamily, fontSize);
    cairo_move_to(cr_, pos.x, pos.y);
    cairo_show_text(cr_, text.c_str());
}

Size Painter::measureText(const std::string& text,
                           const std::string& fontFamily,
                           double fontSize) const {
    setFont(fontFamily, fontSize);
    cairo_text_extents_t extents;
    cairo_text_extents(cr_, text.c_str(), &extents);
    return {static_cast<int>(extents.width + 0.5),
            static_cast<int>(extents.height + 0.5)};
}

void Painter::fontMetrics(double& ascent, double& descent, double& height,
                           const std::string& fontFamily,
                           double fontSize) const {
    setFont(fontFamily, fontSize);
    cairo_font_extents_t extents;
    cairo_font_extents(cr_, &extents);
    ascent = extents.ascent;
    descent = extents.descent;
    height = extents.height;
}

void Painter::setClipRect(const Rect& rect) {
    cairo_reset_clip(cr_);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(cr_);
}

void Painter::resetClip() {
    cairo_reset_clip(cr_);
}

} // namespace miniui
