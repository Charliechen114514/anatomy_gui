#include "color_widget.h"
#include "painter.h"

namespace miniui {

ColorWidget::ColorWidget(Color color) : color_(color) {}

Size ColorWidget::measure(Size available) {
    // Measure and arrange children to fill our bounds
    for (auto& child : children_) {
        child->measure(available);
        child->arrange({0, 0, available.width, available.height});
    }
    return available;
}

void ColorWidget::paint(Painter& painter) {
    painter.fillRect({0, 0, rect_.width, rect_.height}, color_);
    // Paint children
    for (auto& child : children_) {
        painter.save();
        painter.translate(child->rect().x, child->rect().y);
        child->paint(painter);
        painter.restore();
    }
}

} // namespace miniui
