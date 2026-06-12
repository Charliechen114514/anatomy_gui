#pragma once

#include "label.h"
#include "signal.h"
#include "widget.h"

#include <string>

namespace miniui {

/// A clickable button widget.
class Button : public Widget {
public:
    Button() = default;
    explicit Button(const std::string& text) : text_(std::move(text)) {}

    /// Button text.
    void setText(const std::string& t) { text_ = t; }
    const std::string& text() const { return text_; }

    /// Signal emitted when the button is clicked.
    Signal<>& clicked() { return clicked_; }

    // --- Widget overrides ---
    Size measure(const Size& available) override;
    void paint(Painter& p) override;
    void onMousePress(Point p) override;
    void onMouseRelease(Point p) override;

private:
    std::string text_;
    bool pressed_ = false;
    Signal<> clicked_;
};

} // namespace miniui
