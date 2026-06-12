#pragma once

#include "property.h"
#include "widget.h"

namespace miniui {

/// A simple colored rectangle widget.
class ColorWidget : public Widget {
public:
    ColorWidget() = default;
    ColorWidget(const Color& color, const Size& preferredSize);

    /// The fill color.
    Property<Color>& color() { return color_; }

    /// Preferred size hint.
    void setPreferredSize(const Size& s) { preferred_ = s; }

    // --- Widget overrides ---
    Size measure(const Size& available) override;
    void paint(Painter& p) override;

private:
    Property<Color> color_;
    Size preferred_;
};

} // namespace miniui
