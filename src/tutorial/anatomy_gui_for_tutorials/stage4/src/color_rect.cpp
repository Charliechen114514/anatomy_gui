#include "color_rect.h"
#include "painter.h"

namespace miniui {

ColorRect::ColorRect(const Color& color) : color_(color) {}

void ColorRect::setColor(const Color& c) { color_ = c; }

void ColorRect::paint(Painter& painter) {
    painter.setSourceColor(color_);
    painter.rectangle(0, 0, bounds().width, bounds().height);
    painter.fill();

    // Also paint children
    for (auto& child : children()) {
        if (!child->visible()) continue;
        painter.save();
        painter.translate(child->bounds().x, child->bounds().y);
        painter.clip(0, 0, child->bounds().width, child->bounds().height);
        child->paint(painter);
        painter.restore();
    }
}

} // namespace miniui
