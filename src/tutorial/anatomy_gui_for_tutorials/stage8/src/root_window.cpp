#include "root_window.h"
#include "painter.h"

namespace miniui {

RootWindow::RootWindow(Widget* parent) : Widget(parent) {}

RootWindow::~RootWindow() = default;

Size RootWindow::measure(Size available) {
    desiredSize_ = available;
    // Measure all children with the full available space
    for (auto* child : children_) {
        if (child->isVisible()) {
            child->measure(available);
        }
    }
    return desiredSize_;
}

void RootWindow::arrange(Rect allocated) {
    pos_  = allocated.origin();
    size_ = allocated.size();

    // Arrange children to fill the entire root area
    for (auto* child : children_) {
        if (child->isVisible()) {
            child->arrange(allocated);
        }
    }
}

void RootWindow::onPaint(Painter& /*painter*/) {
    // Root window has no visual content of its own;
    // children paint on top.
}

} // namespace miniui
