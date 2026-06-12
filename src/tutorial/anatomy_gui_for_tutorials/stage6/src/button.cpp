#include "button.h"
#include "painter.h"

namespace miniui {

Button::Button() {
    text_.changed().connect([this](const std::string&) {
        repaint();
    });
}

Size Button::measure(Size /*available*/) {
    int estimatedWidth  = static_cast<int>(text_.get().size()) * (fontSize_ * 3 / 5) + 20;
    int estimatedHeight = fontSize_ + 20;
    return {std::max(estimatedWidth, 30), std::max(estimatedHeight, 24)};
}

void Button::paint(Painter& painter) {
    Color bg = pressed_ ? bgPressed_ : bgNormal_;

    // Background
    painter.roundedRect({0, 0, rect_.width, rect_.height}, 6.0);
    painter.fill(bg);

    // Border
    painter.roundedRect({0, 0, rect_.width, rect_.height}, 6.0);
    painter.stroke(borderColor_, 1.5);

    // Text — centered
    const auto& str = text_.get();
    if (!str.empty()) {
        int textX = (rect_.width - static_cast<int>(str.size()) * (fontSize_ * 3 / 5)) / 2;
        int textY = (rect_.height + fontSize_) / 2 - 2;
        if (textX < 4) textX = 4;
        painter.drawText(textX, textY, str, textColor_, fontSize_);
    }
}

void Button::onMousePress(Point /*pos*/, int /*button*/) {
    pressed_ = true;
    repaint();
}

void Button::onMouseRelease(Point pos, int /*button*/) {
    bool wasPressed = pressed_;
    pressed_ = false;
    repaint();

    if (wasPressed) {
        Rect bounds{0, 0, rect_.width, rect_.height};
        if (bounds.contains(pos)) {
            clicked_.emit();
        }
    }
}

} // namespace miniui
