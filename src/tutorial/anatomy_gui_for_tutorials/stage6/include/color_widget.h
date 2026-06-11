#pragma once
/// @file color_widget.h
/// @brief A trivial widget that fills its bounds with a solid colour.

#include "geometry.h"
#include "widget.h"

namespace miniui {

class ColorWidget : public Widget {
public:
    explicit ColorWidget(Color color = Color::rgb(0.5, 0.5, 0.5));

    void setColor(Color c) { color_ = c; }
    Color color() const    { return color_; }

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    Color color_;
};

} // namespace miniui
