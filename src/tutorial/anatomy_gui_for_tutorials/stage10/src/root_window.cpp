#include "root_window.h"
#include "painter.h"

namespace miniui {

void RootWindow::setContent(std::shared_ptr<Widget> child) {
    content_ = std::move(child);
    if (content_) {
        content_->setParent(this);
    }
}

Size RootWindow::measure(const Size& available) {
    if (content_) {
        return content_->measure(available);
    }
    return available;
}

void RootWindow::arrange(const Rect& rect) {
    setBounds(rect);
    if (content_) {
        content_->arrange(rect);
    }
}

void RootWindow::paint(Painter& p) {
    // Clear to white
    p.setSourceRGB(1.0, 1.0, 1.0);
    p.rectangle(bounds_);
    p.fill();

    if (content_) {
        p.save();
        p.translate(bounds_.x, bounds_.y);
        content_->paint(p);
        p.restore();
    }
}

Widget* RootWindow::hitTest(const Point& pt) {
    if (content_ && bounds_.contains(pt)) {
        return content_->hitTest(pt);
    }
    return nullptr;
}

void RootWindow::repaint() {
    // Root reached — trigger a repaint through the Application's wake-up mechanism.
    if (onRepaintRequested) onRepaintRequested();
}

} // namespace miniui
