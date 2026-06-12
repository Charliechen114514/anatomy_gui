#include "color_widget.h"
#include "painter.h"

namespace miniui {

ColorWidget::ColorWidget(const Color& color, const Size& preferredSize)
    : color_(color), preferred_(preferredSize) {}

Size ColorWidget::measure(const Size& available) {
    Size sz = preferred_.isEmpty() ? available : preferred_;
    frame_.width  = sz.width;
    frame_.height = sz.height;
    return sz;
}

void ColorWidget::paint(Painter& p) {
    p.setSourceColor(color_.get());
    p.rectangle(frame_);
    p.fill();

    // Border
    p.setSourceRGB(0.4, 0.4, 0.4);
    p.setLineWidth(1.0);
    p.rectangle(frame_);
    p.stroke();
}

} // namespace miniui
