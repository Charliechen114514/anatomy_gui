#include "layout_strategy.h"
#include "widget.h"

namespace miniui {

// ── HBoxLayout ───────────────────────────────────────────────

Size HBoxLayout::measure(const std::vector<Widget*>& children, Size available) const {
    if (children.empty()) return {0, 0};

    int totalWidth  = 0;
    int maxHeight   = 0;

    for (auto* child : children) {
        Size s = child->measure(available);
        totalWidth += s.width;
        maxHeight = std::max(maxHeight, s.height);
    }
    totalWidth += spacing_ * static_cast<int>(children.size() - 1);

    return {std::min(totalWidth, available.width), std::min(maxHeight, available.height)};
}

void HBoxLayout::arrange(const std::vector<Widget*>& children, Rect allocated) const {
    if (children.empty()) return;

    int n = static_cast<int>(children.size());
    int childWidth = (allocated.width - spacing_ * (n - 1)) / n;
    int x = allocated.x;

    for (auto* child : children) {
        child->arrange({x, allocated.y, childWidth, allocated.height});
        x += childWidth + spacing_;
    }
}

// ── VBoxLayout ───────────────────────────────────────────────

Size VBoxLayout::measure(const std::vector<Widget*>& children, Size available) const {
    if (children.empty()) return {0, 0};

    int maxWidth    = 0;
    int totalHeight = 0;

    for (auto* child : children) {
        Size s = child->measure(available);
        maxWidth = std::max(maxWidth, s.width);
        totalHeight += s.height;
    }
    totalHeight += spacing_ * static_cast<int>(children.size() - 1);

    return {std::min(maxWidth, available.width), std::min(totalHeight, available.height)};
}

void VBoxLayout::arrange(const std::vector<Widget*>& children, Rect allocated) const {
    if (children.empty()) return;

    int n = static_cast<int>(children.size());
    int childHeight = (allocated.height - spacing_ * (n - 1)) / n;
    int y = allocated.y;

    for (auto* child : children) {
        child->arrange({allocated.x, y, allocated.width, childHeight});
        y += childHeight + spacing_;
    }
}

} // namespace miniui
