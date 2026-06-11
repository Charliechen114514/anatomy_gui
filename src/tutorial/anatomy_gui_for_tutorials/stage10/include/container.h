#pragma once

#include "layout_strategy.h"
#include "widget.h"

#include <memory>
#include <vector>

namespace miniui {

/// A generic container that holds children and applies a layout strategy.
template <LayoutStrategy Layout>
class Container : public Widget {
public:
    Container() = default;
    explicit Container(Layout layout) : layout_(std::move(layout)) {}

    /// Add a child widget.
    void addChild(std::shared_ptr<Widget> child) {
        child->setParent(this);
        children_.push_back(std::move(child));
    }

    /// Access the children.
    const std::vector<std::shared_ptr<Widget>>& children() const {
        return children_;
    }

    // --- Widget overrides ---

    Size measure(const Size& available) override {
        std::vector<Size> childSizes;
        for (auto& c : children_) {
            childSizes.push_back(c->measure(available));
        }
        auto sz = layout_.measure(childSizes, available);
        frame_.width  = sz.width;
        frame_.height = sz.height;
        return sz;
    }

    void arrange(const Rect& rect) override {
        setBounds(rect);
        std::vector<Widget*> raw;
        for (auto& c : children_) {
            raw.push_back(c.get());
        }
        layout_.arrange(raw, rect);
    }

    void paint(Painter& p) override {
        for (auto& c : children_) {
            if (!c->visible()) continue;
            p.save();
            p.translate(c->frame().x - frame_.x,
                        c->frame().y - frame_.y);
            c->paint(p);
            p.restore();
        }
    }

    Widget* hitTest(const Point& pt) override {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if (!(*it)->visible()) continue;
            if (auto* hit = (*it)->hitTest(pt)) return hit;
        }
        return nullptr;
    }

private:
    Layout layout_;
    std::vector<std::shared_ptr<Widget>> children_;
};

// Common aliases
using HBoxContainer = Container<HBoxLayout>;
using VBoxContainer = Container<VBoxLayout>;

} // namespace miniui
