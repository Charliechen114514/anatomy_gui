#include "framework/widget.h"

#include <utility>

#include "framework/layout.h"

namespace {

// Ids start at 1: zero is reserved so a zeroed Win32 control id (or a stray
// default-constructed message parameter) never matches a real node.
constexpr int kFirstId = 1;

}  // namespace

int Widget::next_id_ = kFirstId;

Widget::Widget() : id_(next_id_++) {}

Widget& Widget::add_child(std::unique_ptr<Widget> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));

    // The tree owns the child from here: destroying the parent destroys the
    // whole subtree, with no hand-written teardown anywhere.
    return *children_.back();
}

Widget* Widget::find_by_id(int node_id) {
    if (id_ == node_id) {
        return this;
    }
    for (auto& child : children_) {
        if (Widget* found = child->find_by_id(node_id); found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

bool Widget::accepts(const Event& /*unused*/) const {
    return true;
}

bool Widget::on_event(Event& event) {
    return handler_ != nullptr && handler_(event);
}

void Widget::set_handler(EventHandler handler) {
    handler_ = std::move(handler);
}

Widget::~Widget() = default;

void Widget::set_bounds(const Rect& bounds) {
    bounds_ = bounds;
    on_bounds_changed();

    // A container lays its children out again after every move, so nobody has
    // to remember to call the layout by hand.
    layout_children();
}

void Widget::set_layout(std::unique_ptr<Layout> layout) {
    layout_ = std::move(layout);
    layout_children();
}

void Widget::layout_children() {
    if (layout_ != nullptr) {
        layout_->arrange(*this, bounds_);
    }
}

void Widget::invalidate() {
    // A rectangle is relative to the parent, so a node cannot compute its own
    // screen area yet. The whole client area is repainted instead: the M1
    // simplification, with damage regions arriving in M2.
    const Widget* root = this;
    while (root->parent_ != nullptr) {
        root = root->parent_;
    }
    if (root->repaint_ != nullptr) {
        root->repaint_();
    }
}
