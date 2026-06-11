#pragma once
#include "geometry.h"
#include "painter.h"
#include "widget.h"

namespace miniui {

/// Simple widget that fills its bounds with a solid color.
class ColorWidget : public Widget {
public:
    explicit ColorWidget(Color c = Color::rgb(0.5, 0.5, 0.5)) : color_(c) {}

    Size measure(Size available) override { return available; }

    void paint(Painter& painter) override {
        painter.setSourceColor(color_);
        painter.rectangle(0, 0, bounds().width, bounds().height);
        painter.fill();
    }

    void setColor(Color c) { color_ = c; }

private:
    Color color_;
};

} // namespace miniui
