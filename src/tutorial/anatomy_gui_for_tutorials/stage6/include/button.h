#pragma once
/// @file button.h
/// @brief Button widget — clickable with Signal<> clicked and Property text.

#include "geometry.h"
#include "property.h"
#include "signal.h"
#include "widget.h"

#include <string>

namespace miniui {

class Button : public Widget {
public:
    Button();

    Property<std::string>& text() { return text_; }
    const Property<std::string>& text() const { return text_; }

    Signal<>& clicked() { return clicked_; }

    void setFontSize(int sz) { fontSize_ = sz; }
    int  fontSize() const    { return fontSize_; }

    Size measure(Size available) override;
    void paint(Painter& painter) override;
    void onMousePress(Point pos, int button) override;
    void onMouseRelease(Point pos, int button) override;

private:
    Property<std::string> text_;
    Signal<>              clicked_;
    bool                  pressed_  = false;
    int                   fontSize_ = 16;

    Color bgNormal_   = Color::rgb(0.85, 0.85, 0.85);
    Color bgPressed_  = Color::rgb(0.65, 0.65, 0.75);
    Color textColor_  = Color::rgb(0.0, 0.0, 0.0);
    Color borderColor_= Color::rgb(0.4, 0.4, 0.4);
};

} // namespace miniui
