#include "status_bar.h"
#include "painter.h"

namespace miniui {

// ── Label ────────────────────────────────────────────────────────────────────

Label::Label(Widget* parent) : Widget(parent) {}
Label::~Label() = default;

void Label::setText(const std::string& text) {
    text_ = text;
    markDirty();
}

Size Label::measure(Size available) {
    // Approximate
    int w = static_cast<int>(text_.size()) * 8 + 8;
    desiredSize_ = {std::min(w, available.width), 20};
    return desiredSize_;
}

void Label::onPaint(Painter& painter) {
    painter.drawText(text_, Point(4, 14),
                     Color::rgb(0.0, 0.0, 0.0), "sans", 13.0);
}

// ── StatusBar ────────────────────────────────────────────────────────────────

StatusBar::StatusBar(Widget* parent) : Widget(parent) {
    updateText();
}

StatusBar::~StatusBar() = default;

void StatusBar::setStatus(const std::string& status) {
    statusText_ = status;
    updateText();
}

void StatusBar::setCursorPosition(int line, int col) {
    cursorLine_ = line;
    cursorCol_  = col;
    updateText();
}

void StatusBar::setModified(bool modified) {
    modified_ = modified;
    updateText();
}

void StatusBar::updateText() {
    displayText_ = "Ln " + std::to_string(cursorLine_ + 1) +
                   "  Col " + std::to_string(cursorCol_ + 1);
    if (modified_) {
        displayText_ += "  [Modified]";
    }
    if (!statusText_.empty()) {
        displayText_ += "  |  " + statusText_;
    }
    markDirty();
}

Size StatusBar::measure(Size available) {
    int w = static_cast<int>(displayText_.size()) * 8 + 16;
    desiredSize_ = {std::min(w, available.width), kStatusBarHeight};
    return desiredSize_;
}

void StatusBar::onPaint(Painter& painter) {
    // Background
    painter.fillRect(Rect(0, 0, size_.width, size_.height),
                     Color::rgb(0.93, 0.93, 0.93));
    // Top border
    painter.drawLine(Point(0, 0), Point(size_.width, 0),
                     Color::rgb(0.7, 0.7, 0.7), 1.0);

    // Text
    painter.drawText(displayText_, Point(8, 16),
                     Color::rgb(0.2, 0.2, 0.2), "sans", 12.0);
}

} // namespace miniui
