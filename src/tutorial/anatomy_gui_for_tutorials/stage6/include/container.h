#pragma once
/// @file container.h
/// @brief Container<LayoutStrategy> — a widget that arranges its children
///        using the specified layout strategy.

#include "geometry.h"
#include "layout_strategy.h"
#include "painter.h"
#include "widget.h"

namespace miniui {

template <LayoutStrategy Layout>
class Container : public Widget {
public:
    Container() = default;

    Size measure(Size available) override {
        Layout::apply(children_, {0, 0, available.width, available.height});
        return available;
    }

    void paint(Painter& painter) override {
        for (auto& child : children_) {
            painter.save();
            painter.translate(child->rect().x, child->rect().y);
            child->paint(painter);
            painter.restore();
        }
    }
};

} // namespace miniui
