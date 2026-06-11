#pragma once
/// @file layout_strategy.h
/// @brief LayoutStrategy C++20 Concept + HBoxLayout / VBoxLayout implementations.

#include "geometry.h"

#include <algorithm>
#include <concepts>
#include <vector>

namespace miniui {

class Widget; // forward

// ── C++20 Concept ────────────────────────────────────────────

/// A type satisfies LayoutStrategy if it provides measure() and arrange()
/// with the expected signatures.
template<typename T>
concept LayoutStrategy = requires(T t, const std::vector<Widget*>& children,
                                  Size available, Rect allocated) {
    { t.measure(children, available) } -> std::same_as<Size>;
    { t.arrange(children, allocated) } -> std::same_as<void>;
};

// ── HBoxLayout ───────────────────────────────────────────────

/// Lays out children left-to-right in a single row.
class HBoxLayout {
public:
    explicit HBoxLayout(int spacing = 0) : spacing_(spacing) {}

    Size measure(const std::vector<Widget*>& children, Size available) const;
    void arrange(const std::vector<Widget*>& children, Rect allocated) const;

private:
    int spacing_;
};

// ── VBoxLayout ───────────────────────────────────────────────

/// Lays out children top-to-bottom in a single column.
class VBoxLayout {
public:
    explicit VBoxLayout(int spacing = 0) : spacing_(spacing) {}

    Size measure(const std::vector<Widget*>& children, Size available) const;
    void arrange(const std::vector<Widget*>& children, Rect allocated) const;

private:
    int spacing_;
};

// ── Compile-time verification ────────────────────────────────

static_assert(LayoutStrategy<HBoxLayout>, "HBoxLayout must satisfy LayoutStrategy");
static_assert(LayoutStrategy<VBoxLayout>, "VBoxLayout must satisfy LayoutStrategy");

} // namespace miniui
