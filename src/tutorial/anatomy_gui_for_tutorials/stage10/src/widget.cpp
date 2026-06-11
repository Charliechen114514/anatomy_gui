#include "widget.h"
#include "painter.h"

namespace miniui {

// --- Static ID generation ---
static uint64_t g_widgetIdCounter = 1;

Widget::WidgetId Widget::nextId() { return g_widgetIdCounter++; }

Widget::Widget() : id_(nextId()) {}

Widget::~Widget() = default;

// --- Geometry ---

void Widget::setBounds(const Rect& b) {
    bounds_ = b;
    frame_  = b;   // default: frame == bounds
}

// --- Layout protocol ---

Size Widget::measure(const Size& /*available*/) {
    return {frame_.width, frame_.height};
}

void Widget::arrange(const Rect& rect) {
    setBounds(rect);
}

// --- Hit testing ---

Widget* Widget::hitTest(const Point& pt) {
    if (visible_ && frame_.contains(pt)) return this;
    return nullptr;
}

// --- Mouse events ---

void Widget::onMousePress(Point p) {
    mousePress_.next(p);
}

void Widget::onMouseMove(Point p) {
    mouseMove_.next(p);
}

void Widget::onMouseRelease(Point p) {
    mouseRelease_.next(p);
}

// --- Repaint / update ---

void Widget::repaint() {
    // Walk up to the root and request a repaint.
    if (parent_) {
        parent_->repaint();
    }
    // Root Window's repaint() override handles the actual trigger.
}

void Widget::update() {
    if (parent_) {
        parent_->update();
    }
}

Widget* Widget::root() {
    Widget* w = this;
    while (w->parent_) w = w->parent_;
    return w;
}

} // namespace miniui
