#pragma once
/// @file label.h
/// @brief Label — a simple text-display widget.

#include "geometry.h"
#include "widget.h"

#include <string>

namespace miniui {

class Painter;

/// Displays a single line of text.
class Label : public Widget {
public:
    explicit Label(const std::string& text = "");

    void setText(const std::string& text);
    void setColor(const Color& c);
    void setFontSize(double size);

    const std::string& text() const { return text_; }

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    std::string text_;
    Color       color_   = Color::rgb(0, 0, 0);
    double      fontSize_ = 14.0;
};

} // namespace miniui
