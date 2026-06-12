#include "widget.h"
#include "painter.h"

namespace miniui {

Widget::Widget(Widget* parent) : parent_(parent) {
    if (parent_) {
        parent_->addChild(this);
    }
}

Widget::~Widget() {
    if (parent_) {
        parent_->removeChild(this);
    }
    // Delete children in reverse order
    while (!children_.empty()) {
        auto* child = children_.back();
        child->parent_ = nullptr; // prevent double removal
        delete child;
    }
}

void Widget::addChild(Widget* child) {
    if (child) {
        children_.push_back(child);
    }
}

void Widget::removeChild(Widget* child) {
    children_.erase(
        std::remove(children_.begin(), children_.end(), child),
        children_.end());
}

Point Widget::mapToRoot(Point local) const {
    Point result = local;
    const Widget* w = this;
    while (w) {
        result = result + w->pos_;
        w = w->parent_;
    }
    return result;
}

Size Widget::measure(Size /*available*/) {
    // Default: report the current size or a minimum size
    desiredSize_ = size_;
    return desiredSize_;
}

void Widget::arrange(Rect allocated) {
    pos_  = allocated.origin();
    size_ = allocated.size();
}

void Widget::paint(Painter& painter) {
    if (!visible_) return;

    painter.save();
    // Translate to this widget's local coordinates
    cairo_translate(painter.cairoContext(), pos_.x, pos_.y);

    onPaint(painter);

    // Paint children on top
    for (auto* child : children_) {
        if (child->isVisible()) {
            child->paint(painter);
        }
    }

    painter.restore();
}

void Widget::onPaint(Painter& /*painter*/) {
    // Default: nothing to draw
}

bool Widget::onKeyEvent(xcb_keycode_t keycode) {
    // Propagate to children first (last-added gets priority)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->isVisible() && (*it)->onKeyEvent(keycode)) {
            return true;
        }
    }
    return false;
}

Widget* Widget::hitTest(Point local) {
    if (!visible_) return nullptr;
    if (!bounds().contains(local)) return nullptr;

    // Check children in reverse order (top-most first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Point childLocal = local - (*it)->pos();
        Widget* result = (*it)->hitTest(childLocal);
        if (result) return result;
    }
    return this;
}

void Widget::markDirty() {
    dirty_ = true;
    // Propagate up so parent knows to repaint
    if (parent_) {
        parent_->markDirty();
    }
}

void Widget::repaint() {
    markDirty();
}

void Widget::update() {
    markDirty();
}

} // namespace miniui
