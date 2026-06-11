#pragma once
/// @file color_rect.h
/// @brief ColorRect — a widget that fills its bounds with a solid color.

#include "geometry.h"
#include "widget.h"

namespace miniui {

/// Fills its allocated area with a single color. Useful as a background or
/// for visual testing of layout.
class ColorRect : public Widget {
public:
    explicit ColorRect(const Color& color = Color::rgb(0.5, 0.5, 0.5));

    void setColor(const Color& c);

    void paint(Painter& painter) override;

private:
    Color color_;
};

} // namespace miniui
