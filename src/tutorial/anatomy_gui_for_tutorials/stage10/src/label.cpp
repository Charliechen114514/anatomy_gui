#include "label.h"
#include "painter.h"

namespace miniui {

Size Label::measure(const Size& available) {
    // Create a temporary cairo context to measure text.
    // We approximate by using the frame if available.
    cairo_surface_t* dummy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(dummy);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, fontSize_);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, text_.get().c_str(), &ext);

    cairo_destroy(cr);
    cairo_surface_destroy(dummy);

    Size sz{ext.width + 8, ext.height + 8};
    frame_.width  = sz.width;
    frame_.height = sz.height;
    return sz;
}

void Label::paint(Painter& p) {
    p.setSourceColor(color_);
    p.selectFontFace("sans", CAIRO_FONT_SLANT_NORMAL,
                     CAIRO_FONT_WEIGHT_NORMAL);
    p.setFontSize(fontSize_);

    // Vertically center the text within the frame.
    auto ext = p.textExtents(text_.get().c_str());
    double textY = (frame_.height + ext.height) / 2.0;

    p.moveTo(4, textY);
    p.showText(text_.get().c_str());
}

} // namespace miniui
