#include "button.h"
#include "painter.h"
namespace miniui {
Size Button::measure(Size) {
    int w = static_cast<int>(text_.get().size())*(fontSize_*3/5)+20;
    return {std::max(w,30), std::max(fontSize_+20,24)};
}
void Button::paint(Painter& p) {
    p.roundedRect({0,0,rect_.width,rect_.height},6.0);
    p.fill(pressed_?bgPressed_:bgNormal_);
    p.roundedRect({0,0,rect_.width,rect_.height},6.0);
    p.stroke(borderCol_,1.5);
    const auto& str = text_.get();
    if(!str.empty()){
        int tx = (rect_.width-static_cast<int>(str.size())*(fontSize_*3/5))/2;
        int ty = (rect_.height+fontSize_)/2-2;
        if(tx<4) tx=4;
        p.drawText(tx,ty,str,textCol_,fontSize_);
    }
}
void Button::onMousePress(Point,int){ pressed_=true; repaint(); }
void Button::onMouseRelease(Point pos,int){
    bool was=pressed_; pressed_=false; repaint();
    if(was && Rect{0,0,rect_.width,rect_.height}.contains(pos)) clicked_.emit();
}
} // namespace miniui
