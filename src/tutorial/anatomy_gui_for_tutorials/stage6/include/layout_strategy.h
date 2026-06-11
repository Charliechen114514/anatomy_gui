#pragma once
/// @file layout_strategy.h
/// @brief LayoutStrategy concept + HBoxLayout / VBoxLayout implementations.

#include "geometry.h"
#include "widget.h"

#include <vector>

namespace miniui {

template <typename L>
concept LayoutStrategy = requires(const std::vector<std::shared_ptr<Widget>>& children,
                                  Rect available) {
    { L::apply(children, available) } -> std::same_as<void>;
};

struct HBoxLayout {
    static void apply(const std::vector<std::shared_ptr<Widget>>& children,
                      Rect available) {
        if (children.empty()) return;
        int n     = static_cast<int>(children.size());
        int slotW = available.width / n;
        int slotH = available.height;
        int x     = available.x;

        for (auto& child : children) {
            child->measure({slotW, slotH});
            child->arrange({x, available.y, slotW, slotH});
            x += slotW;
        }
    }
};

struct VBoxLayout {
    static void apply(const std::vector<std::shared_ptr<Widget>>& children,
                      Rect available) {
        if (children.empty()) return;
        int n     = static_cast<int>(children.size());
        int slotW = available.width;
        int slotH = available.height / n;
        int y     = available.y;

        for (auto& child : children) {
            child->measure({slotW, slotH});
            child->arrange({available.x, y, slotW, slotH});
            y += slotH;
        }
    }
};

} // namespace miniui
