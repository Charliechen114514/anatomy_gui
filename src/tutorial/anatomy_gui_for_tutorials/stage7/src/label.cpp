#include "label.h"
#include "painter.h"
namespace miniui {
Size Label::measure(Size) {
    int w = static_cast<int>(text_.get().size())*(fontSize_*3/5);
    return {std::max(w,1), std::max(fontSize_+8,1)};
}
void Label::paint(Painter& p) {
    const auto& str = text_.get();
    if(str.empty()) return;
    int ty = (rect_.height+fontSize_)/2 - 2;
    p.drawText(4, ty, str, color_, fontSize_);
}
} // namespace miniui
