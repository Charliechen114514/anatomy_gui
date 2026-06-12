#include "widget.h"
#include "painter.h"

namespace miniui {

void Widget::paint(Painter& /*painter*/) {
    // Base widget has no children — nothing to paint.
}

Widget* Widget::hitTest(Point localPos) {
    // Base widget with no children — check own bounds.
    // A zero-area widget (like root wrapper) claims everything.
    if (bounds_.width == 0 && bounds_.height == 0) return this;
    if (bounds_.contains(localPos)) return this;
    return nullptr;
}

} // namespace miniui
