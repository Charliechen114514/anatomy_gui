#pragma once

#include "geometry.h"
#include "widget.h"

#include <concepts>
#include <vector>

namespace miniui {

// ---------------------------------------------------------------------------
// LayoutStrategy concept
// ---------------------------------------------------------------------------

/// A LayoutStrategy must provide:
///   Size measure(const std::vector<Size>& childSizes, const Size& available)
///   void arrange(std::vector<Widget*>& children, const Rect& hostRect)
template <typename L>
concept LayoutStrategy = requires(L layout, std::vector<Size> sizes,
                                  Size avail, std::vector<Widget*> children,
                                  Rect rect) {
    { layout.measure(sizes, avail) } -> std::same_as<Size>;
    { layout.arrange(children, rect) } -> std::same_as<void>;
};

// ---------------------------------------------------------------------------
// HBoxLayout — lay children out horizontally
// ---------------------------------------------------------------------------

class HBoxLayout {
public:
    HBoxLayout() = default;
    explicit HBoxLayout(double spacing) : spacing_(spacing) {}

    Size measure(const std::vector<Size>& childSizes, const Size& available) const;
    void arrange(std::vector<Widget*>& children, const Rect& hostRect) const;

private:
    double spacing_ = 4.0;
};

// ---------------------------------------------------------------------------
// VBoxLayout — lay children out vertically
// ---------------------------------------------------------------------------

class VBoxLayout {
public:
    VBoxLayout() = default;
    explicit VBoxLayout(double spacing) : spacing_(spacing) {}

    Size measure(const std::vector<Size>& childSizes, const Size& available) const;
    void arrange(std::vector<Widget*>& children, const Rect& hostRect) const;

private:
    double spacing_ = 4.0;
};

// ---------------------------------------------------------------------------
// Inline implementations
// ---------------------------------------------------------------------------

inline Size HBoxLayout::measure(const std::vector<Size>& childSizes,
                                const Size& available) const {
    double totalW = 0;
    double maxH   = 0;
    for (size_t i = 0; i < childSizes.size(); ++i) {
        totalW += childSizes[i].width;
        maxH = std::max(maxH, childSizes[i].height);
        if (i > 0) totalW += spacing_;
    }
    return {std::min(totalW, available.width),
            std::min(maxH, available.height)};
}

inline void HBoxLayout::arrange(std::vector<Widget*>& children,
                                const Rect& hostRect) const {
    double x = hostRect.x;
    for (auto* child : children) {
        auto pref = child->measure(hostRect.size());
        child->arrange({x, hostRect.y, pref.width, hostRect.height});
        x += pref.width + spacing_;
    }
}

inline Size VBoxLayout::measure(const std::vector<Size>& childSizes,
                                const Size& available) const {
    double totalH = 0;
    double maxW   = 0;
    for (size_t i = 0; i < childSizes.size(); ++i) {
        totalH += childSizes[i].height;
        maxW = std::max(maxW, childSizes[i].width);
        if (i > 0) totalH += spacing_;
    }
    return {std::min(maxW, available.width),
            std::min(totalH, available.height)};
}

inline void VBoxLayout::arrange(std::vector<Widget*>& children,
                                const Rect& hostRect) const {
    double y = hostRect.y;
    for (auto* child : children) {
        auto pref = child->measure(hostRect.size());
        child->arrange({hostRect.x, y, hostRect.width, pref.height});
        y += pref.height + spacing_;
    }
}

} // namespace miniui
