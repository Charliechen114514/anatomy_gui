#pragma once
/// @file layout_strategy.h
/// @brief Layout strategies: HBoxLayout, VBoxLayout.

#include "geometry.h"
#include "widget.h"

#include <vector>
#include <concepts>

namespace miniui {

/// Concept for a layout strategy.
template <typename L>
concept LayoutStrategy = requires(L layout, const std::vector<Widget*>& children, Size available) {
    { layout.measure(children, available) } -> std::same_as<Size>;
    { layout.arrange(children, Rect{}) } -> std::same_as<void>;
};

/// Horizontal box layout: arranges children left to right.
class HBoxLayout {
public:
    Size measure(const std::vector<Widget*>& children, Size available) const {
        int totalWidth  = 0;
        int maxHeight   = 0;
        int spacing     = spacing_;

        int count = 0;
        for (auto* child : children) {
            if (!child->isVisible()) continue;
            count++;
            Size childSize = child->measure(available);
            totalWidth += childSize.width;
            maxHeight = std::max(maxHeight, childSize.height);
        }
        if (count > 1) totalWidth += (count - 1) * spacing;

        return {std::min(totalWidth, available.width),
                std::min(maxHeight, available.height)};
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) const {
        int x = allocated.x;
        int count = 0;
        for (auto* child : children) {
            if (!child->isVisible()) continue;
            count++;
            Size childDesired = child->measure(allocated.size());
            int w = childDesired.width;
            int h = allocated.height;
            child->arrange(Rect(x, allocated.y, w, h));
            x += w + spacing_;
        }
    }

    void setSpacing(int s) { spacing_ = s; }

private:
    int spacing_ = 4;
};

/// Vertical box layout: arranges children top to bottom.
class VBoxLayout {
public:
    Size measure(const std::vector<Widget*>& children, Size available) const {
        int maxWidth   = 0;
        int totalHeight = 0;
        int spacing     = spacing_;

        int count = 0;
        for (auto* child : children) {
            if (!child->isVisible()) continue;
            count++;
            Size childSize = child->measure(available);
            maxWidth = std::max(maxWidth, childSize.width);
            totalHeight += childSize.height;
        }
        if (count > 1) totalHeight += (count - 1) * spacing;

        return {std::min(maxWidth, available.width),
                std::min(totalHeight, available.height)};
    }

    void arrange(const std::vector<Widget*>& children, Rect allocated) const {
        if (children.empty()) return;
        int n = static_cast<int>(children.size());

        // Pass 1: measure all children, identify stretchy ones (want full height)
        std::vector<int> heights(n, 0);
        int totalFixed = 0;
        int stretchCount = 0;
        for (int i = 0; i < n; ++i) {
            auto* child = children[i];
            if (!child->isVisible()) continue;
            Size d = child->measure(allocated.size());
            if (d.height >= allocated.height) {
                stretchCount++;
            } else {
                heights[i] = d.height;
                totalFixed += d.height;
            }
        }

        // Stretchy children share the remaining space
        int totalSpacing = (n - 1) * spacing_;
        int stretchH = 0;
        if (stretchCount > 0) {
            int remaining = allocated.height - totalFixed - totalSpacing;
            stretchH = std::max(0, remaining) / stretchCount;
        }

        // Pass 2: arrange
        int y = allocated.y;
        for (int i = 0; i < n; ++i) {
            auto* child = children[i];
            if (!child->isVisible()) continue;
            int w = allocated.width;
            int h = (heights[i] == 0 && stretchCount > 0) ? stretchH : heights[i];
            child->arrange(Rect(allocated.x, y, w, h));
            y += h + spacing_;
        }
    }

    void setSpacing(int s) { spacing_ = s; }

private:
    int spacing_ = 0;
};

} // namespace miniui
