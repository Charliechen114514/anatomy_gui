#pragma once
/// @file container.h
/// @brief Generic container widget that delegates layout to a strategy.

#include "widget.h"
#include "layout_strategy.h"

namespace miniui {

/// A container widget that uses a LayoutStrategy to arrange its children.
template <LayoutStrategy Layout>
class Container : public Widget {
public:
    explicit Container(Widget* parent = nullptr)
        : Widget(parent) {}

    /// Access the layout strategy to configure spacing, etc.
    Layout& layout() { return layout_; }

    // ── Layout protocol ───────────────────────────────────────────────────

    Size measure(Size available) override {
        desiredSize_ = layout_.measure(children_, available);
        return desiredSize_;
    }

    void arrange(Rect allocated) override {
        pos_  = allocated.origin();
        size_ = allocated.size();
        layout_.arrange(children_, allocated);
    }

protected:
    Layout layout_;
};

/// Convenience aliases
using HBox = Container<HBoxLayout>;
using VBox = Container<VBoxLayout>;

} // namespace miniui
