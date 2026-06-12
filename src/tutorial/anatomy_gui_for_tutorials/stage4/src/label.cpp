#include "label.h"
#include "painter.h"

#include <cairo/cairo.h>

namespace miniui {

Label::Label(const std::string& text) : text_(text) {}

void Label::setText(const std::string& text) { text_ = text; }
void Label::setColor(const Color& c)         { color_ = c; }
void Label::setFontSize(double size)         { fontSize_ = size; }

Size Label::measure(Size /*available*/) {
    // Create a temporary image surface for text measurement
    cairo_surface_t* tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(tmp);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, fontSize_);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, text_.c_str(), &ext);

    cairo_destroy(cr);
    cairo_surface_destroy(tmp);

    return {static_cast<int>(ext.width + 4), static_cast<int>(ext.height + 4)};
}

void Label::paint(Painter& painter) {
    if (text_.empty()) return;

    painter.setSourceColor(color_);
    painter.selectFontFace("sans");
    painter.setFontSize(fontSize_);

    // Position baseline so the top of the inked area sits at y = 0.
    // y_bearing is negative (top of glyphs is above the baseline),
    // so -y_bearing gives the correct baseline y-coordinate.
    auto ext = painter.textExtents(text_);
    painter.moveTo(0, -ext.y_bearing);
    painter.showText(text_);
}

} // namespace miniui
