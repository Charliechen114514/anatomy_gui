#include "widget.h"
#include "painter.h"

#include <algorithm>

namespace miniui {

std::shared_ptr<Widget> Widget::hitTest(Point pt) {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        auto& child = *it;
        if (child->rect().contains(pt)) {
            Point childLocal{pt.x - child->rect().x, pt.y - child->rect().y};
            return child->hitTest(childLocal);
        }
    }
    return shared_from_this();
}

void Widget::addChild(std::shared_ptr<Widget> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));
}

void Widget::removeChild(const std::shared_ptr<Widget>& child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
    }
}

void Widget::repaint() {
    if (parent_) {
        parent_->repaint();
    }
}

} // namespace miniui
