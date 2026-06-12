#include "label.h"
#include "painter.h"

namespace miniui {

Label::Label() {
    text_.changed().connect([this](const std::string&) {
        repaint();
    });
}

Size Label::measure(Size /*available*/) {
    int estimatedWidth  = static_cast<int>(text_.get().size()) * (fontSize_ * 3 / 5);
    int estimatedHeight = fontSize_ + 8;
    return {std::max(estimatedWidth, 1), std::max(estimatedHeight, 1)};
}

void Label::paint(Painter& painter) {
    const auto& str = text_.get();
    if (str.empty()) return;

    int textY = (rect_.height + fontSize_) / 2 - 2;
    int textX = 4;
    painter.drawText(textX, textY, str, color_, fontSize_);
}

} // namespace miniui
