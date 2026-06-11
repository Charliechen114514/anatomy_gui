#include "widget.h"
#include "painter.h"

namespace miniui {

void Widget::addChild(std::unique_ptr<Widget> child) {
    if (!child) return;
    // Detach from old parent
    if (child->parent_) {
        // Not implemented for simplicity — transfer requires old parent to release
    }
    child->parent_ = this;
    children_.push_back(std::move(child));
}

std::unique_ptr<Widget> Widget::removeChild(size_t index) {
    if (index >= children_.size()) return nullptr;
    auto it = children_.begin() + index;
    auto child = std::move(*it);
    children_.erase(it);
    child->parent_ = nullptr;
    return child;
}

void Widget::paint(Painter& painter) {
    for (auto& child : children_) {
        if (!child->visible()) continue;
        painter.save();
        painter.translate(child->bounds().x, child->bounds().y);
        painter.clip(0, 0, child->bounds().width, child->bounds().height);
        child->paint(painter);
        painter.restore();
    }
}

Widget* Widget::hitTest(Point localPos) {
    // Traverse children back-to-front (later = higher Z-order, hit first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        auto& child = *it;
        if (!child->visible()) continue;
        Point childLocal = localPos - child->bounds().origin();
        if (child->bounds().contains(localPos)) {
            Widget* hit = child->hitTest(childLocal);
            if (hit) return hit;
        }
    }
    // No child was hit — check own bounds
    if (bounds_.contains(localPos) || bounds_.width == 0) {
        // A zero-size widget (e.g. root wrapper) passes through to itself
        return this;
    }
    return nullptr;
}

} // namespace miniui
