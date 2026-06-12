#include "menu_bar.h"
#include "painter.h"

namespace miniui {

// ── Button ───────────────────────────────────────────────────────────────────

Button::Button(const std::string& label, Widget* parent)
    : Widget(parent), label_(label) {}

Button::~Button() = default;

void Button::setLabel(const std::string& label) {
    label_ = label;
    markDirty();
}

Size Button::measure(Size /*available*/) {
    // Approximate size based on label length
    int w = static_cast<int>(label_.size()) * 8 + 20;
    int h = 24;
    desiredSize_ = {w, h};
    return desiredSize_;
}

void Button::onPaint(Painter& painter) {
    Color bg = Color::rgb(0.93, 0.93, 0.93);
    Color border = Color::rgb(0.7, 0.7, 0.7);

    painter.fillRect(Rect(0, 0, size_.width, size_.height), bg);
    painter.strokeRect(Rect(0, 0, size_.width, size_.height), border, 1.0);

    // Center the text
    Size textSize = painter.measureText(label_, "sans", 13.0);
    int tx = (size_.width - textSize.width) / 2;
    int ty = (size_.height - textSize.height) / 2 + static_cast<int>(textSize.height * 0.8);
    painter.drawText(label_, Point(tx, ty), Color::rgb(0.0, 0.0, 0.0), "sans", 13.0);
}

bool Button::onKeyEvent(xcb_keycode_t /*keycode*/) {
    // Buttons don't handle key events directly in this simple impl
    return false;
}

// ── MenuBar ──────────────────────────────────────────────────────────────────

MenuBar::MenuBar(Widget* parent) : Widget(parent) {
    fileBtn_ = new Button("File", this);
    editBtn_ = new Button("Edit", this);
    helpBtn_ = new Button("Help", this);
}

MenuBar::~MenuBar() = default;

Size MenuBar::measure(Size available) {
    // Measure children and compute total width
    int totalWidth = 0;
    for (auto* child : children_) {
        Size childSize = child->measure(available);
        totalWidth += childSize.width + 4;
    }
    desiredSize_ = {std::min(totalWidth + 4, available.width), kMenuBarHeight};
    return desiredSize_;
}

void MenuBar::arrange(Rect allocated) {
    pos_  = allocated.origin();
    size_ = allocated.size();

    int x = 4;
    int y = 2;
    for (auto* child : children_) {
        int w = child->desiredSize().width;
        child->arrange(Rect(x, y, w, kMenuBarHeight - 4));
        x += w + 4;
    }
}

void MenuBar::onPaint(Painter& painter) {
    // Draw background
    painter.fillRect(Rect(0, 0, size_.width, size_.height),
                     Color::rgb(0.95, 0.95, 0.95));
    // Draw bottom border
    painter.drawLine(Point(0, size_.height - 1),
                     Point(size_.width, size_.height - 1),
                     Color::rgb(0.7, 0.7, 0.7), 1.0);
}

} // namespace miniui
