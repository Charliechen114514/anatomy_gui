#pragma once
/// @file container.h
/// @brief Container — a widget that applies a LayoutStrategy to its children.

#include "geometry.h"
#include "layout_strategy.h"
#include "widget.h"

#include <memory>
#include <vector>

namespace miniui {

/// A Container owns a LayoutPolicy (compile-time) and a list of children.
/// It delegates measure/arrange to the layout policy.
template<LayoutStrategy LayoutPolicy>
class Container : public Widget {
public:
    explicit Container(int spacing = 0) : layout_(spacing) {}

    /// Add a child widget. This Container takes ownership.
    void addChild(std::unique_ptr<Widget> child) {
        child->setParent(this);
        childPtrs_.push_back(child.get());
        children_.push_back(std::move(child));
    }

    // ── Layout ─────────────────────────────────────────
    Size measure(Size available) override {
        return layout_.measure(childPtrs_, available);
    }

    void arrange(Rect allocated) override {
        bounds_ = allocated;
        layout_.arrange(childPtrs_, allocated);
    }

    void paint(Painter& painter) override {
        for (auto& child : children_) {
            if (!child->visible()) continue;
            painter.save();
            painter.translate(child->bounds().x, child->bounds().y);
            painter.clip(0, 0, child->bounds().width, child->bounds().height);
            child->paint(painter);
            painter.restore();
        }
    }

    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

private:
    LayoutPolicy                       layout_;
    std::vector<std::unique_ptr<Widget>> children_;
    std::vector<Widget*>               childPtrs_;  // non-owning refs for layout
};

} // namespace miniui
