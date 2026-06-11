#include "text_canvas.h"
#include "painter.h"

#include <cmath>
#include <algorithm>

namespace miniui {

TextCanvas::TextCanvas(Widget* parent) : Widget(parent) {
    displayLines_.push_back("");
}

TextCanvas::~TextCanvas() = default;

void TextCanvas::cacheFontMetrics(Painter& painter) const {
    if (fontMetricsCached_) return;
    painter.fontMetrics(fontAscent_, fontDescent_, fontHeight_,
                        kFontFamily, kFontSize);
    fontMetricsCached_ = true;
}

int TextCanvas::lineHeight() const {
    return static_cast<int>(fontHeight_ > 0 ? fontHeight_ + 4 : 20);
}

int TextCanvas::gutterWidth() const {
    int numLines = static_cast<int>(displayLines_.size());
    int digits = 1;
    while (numLines >= 10) { numLines /= 10; ++digits; }
    return digits * 8 + 16; // rough estimate
}

void TextCanvas::setLines(const std::vector<std::string>& lines) {
    displayLines_ = lines;
    if (displayLines_.empty()) {
        displayLines_.push_back("");
    }
    markDirty();
}

void TextCanvas::setCursor(int line, int col) {
    cursorLine_ = std::clamp(line, 0,
                             static_cast<int>(displayLines_.size()) - 1);
    cursorCol_  = std::clamp(col, 0,
                             static_cast<int>(
                                 displayLines_[cursorLine_].size()));
    ensureCursorVisible();
    markDirty();
}

void TextCanvas::ensureCursorVisible() {
    int lh = lineHeight();
    if (lh <= 0) return;

    int cursorY = cursorLine_ * lh;
    int viewTop = scrollY_;
    int viewBottom = scrollY_ + size_.height;

    if (cursorY < viewTop) {
        scrollY_ = cursorY;
    } else if (cursorY + lh > viewBottom) {
        scrollY_ = cursorY + lh - size_.height;
        if (scrollY_ < 0) scrollY_ = 0;
    }
}

Size TextCanvas::measure(Size available) {
    // The canvas wants all available space
    desiredSize_ = available;
    return desiredSize_;
}

void TextCanvas::onPaint(Painter& painter) {
    cacheFontMetrics(painter);

    int lh = lineHeight();
    if (lh <= 0) return;

    int gutter = gutterWidth();

    // Background
    painter.fillRect(Rect(0, 0, size_.width, size_.height),
                     Color::rgb(1.0, 1.0, 1.0));

    // Gutter background
    painter.fillRect(Rect(0, 0, gutter, size_.height),
                     Color::rgb(0.95, 0.95, 0.95));

    // Clip to text area
    painter.setClipRect(Rect(gutter, 0, size_.width - gutter, size_.height));

    // Draw visible lines
    int firstLine = scrollY_ / lh;
    int lastLine  = (scrollY_ + size_.height) / lh + 1;
    lastLine = std::min(lastLine, static_cast<int>(displayLines_.size()));

    for (int i = firstLine; i < lastLine; ++i) {
        int y = i * lh - scrollY_ + static_cast<int>(fontAscent_) + 2;

        // Draw line text
        if (i >= 0 && i < static_cast<int>(displayLines_.size())) {
            painter.drawText(displayLines_[i],
                             Point(gutter + 4, y),
                             Color::rgb(0.0, 0.0, 0.0),
                             kFontFamily, kFontSize);
        }
    }

    painter.resetClip();

    // Draw line numbers in gutter
    painter.setClipRect(Rect(0, 0, gutter, size_.height));
    for (int i = firstLine; i < lastLine; ++i) {
        int y = i * lh - scrollY_ + static_cast<int>(fontAscent_) + 2;
        std::string num = std::to_string(i + 1);
        painter.drawText(num,
                         Point(4, y),
                         Color::rgb(0.5, 0.5, 0.5),
                         kFontFamily, kFontSize);
    }
    painter.resetClip();

    // Draw cursor (blinking line)
    int cursorY = cursorLine_ * lh - scrollY_;
    // Measure cursor X position based on text before cursor
    int cursorX = gutter + 4;
    if (cursorLine_ >= 0 && cursorLine_ < static_cast<int>(displayLines_.size())) {
        std::string beforeCursor =
            displayLines_[cursorLine_].substr(0, cursorCol_);
        Size textWidth = painter.measureText(beforeCursor,
                                              kFontFamily, kFontSize);
        cursorX += textWidth.width;
    }

    // Draw cursor as a thin vertical line
    painter.fillRect(Rect(cursorX, cursorY + 2, 2, lh - 4),
                     Color::rgb(0.0, 0.0, 0.0));
}

bool TextCanvas::onKeyEvent(xcb_keycode_t keycode) {
    keyPressed_(keycode);
    return true; // We consume all key events
}

} // namespace miniui
