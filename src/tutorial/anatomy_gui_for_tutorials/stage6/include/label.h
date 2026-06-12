#pragma once
/// @file label.h
/// @brief Label widget — displays a text string with reactive Property.

#include "geometry.h"
#include "property.h"
#include "widget.h"

#include <string>

namespace miniui {

class Label : public Widget {
public:
    Label();

    Property<std::string>& text() { return text_; }
    const Property<std::string>& text() const { return text_; }

    void setFontSize(int sz) { fontSize_ = sz; }
    int  fontSize() const    { return fontSize_; }

    void setColor(Color c) { color_ = c; }
    Color color() const    { return color_; }

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    Property<std::string> text_;
    int    fontSize_ = 18;
    Color  color_    = Color::rgb(0, 0, 0);
};

} // namespace miniui
