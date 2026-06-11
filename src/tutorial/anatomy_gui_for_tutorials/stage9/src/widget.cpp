// Widget in Stage 9 — repaint() moved out-of-line to break circular include.
#include "widget.h"
#include "application.h"

namespace miniui {

void Widget::repaint() {
    // Walk up to root, then notify Application to schedule a repaint
    Widget* root = this;
    while (root->parent_) root = root->parent_;
    Application::instance().markDirty(root->rect());
}

} // namespace miniui
