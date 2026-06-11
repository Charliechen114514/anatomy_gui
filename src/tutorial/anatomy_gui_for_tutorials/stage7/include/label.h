#pragma once
#include "geometry.h"
#include "property.h"
#include "widget.h"
#include <string>
namespace miniui {
class Label : public Widget {
public:
    Label() { conn_ = text_.changed().connect([this](const std::string&){ repaint(); }); }
    Property<std::string>& text(){ return text_; }
    void setFontSize(int sz){ fontSize_=sz; }
    void setColor(Color c){ color_=c; }
    Size measure(Size) override;
    void paint(Painter& painter) override;
private:
    Property<std::string> text_;
    ScopedConnection conn_;
    int fontSize_ = 16;
    Color color_ = Color::rgb(0,0,0);
};
} // namespace miniui
