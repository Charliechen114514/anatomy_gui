#pragma once
#include "geometry.h"
#include "property.h"
#include "signal.h"
#include "widget.h"
#include <string>
namespace miniui {
class Button : public Widget {
public:
    Button() { text_.changed().connect([this](const std::string&){ repaint(); }); }
    Property<std::string>& text(){ return text_; }
    Signal<>& clicked(){ return clicked_; }
    void setFontSize(int sz){ fontSize_=sz; }
    Size measure(Size) override;
    void paint(Painter& painter) override;
    void onMousePress(Point,int) override;
    void onMouseRelease(Point,int) override;
private:
    Property<std::string> text_;
    Signal<> clicked_;
    bool pressed_ = false;
    int fontSize_ = 16;
    Color bgNormal_  = Color::rgb(0.85,0.85,0.85);
    Color bgPressed_ = Color::rgb(0.65,0.65,0.75);
    Color textCol_   = Color::rgb(0,0,0);
    Color borderCol_ = Color::rgb(0.4,0.4,0.4);
};
} // namespace miniui
