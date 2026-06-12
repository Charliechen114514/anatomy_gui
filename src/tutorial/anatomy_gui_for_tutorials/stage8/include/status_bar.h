#pragma once
/// @file status_bar.h
/// @brief StatusBar — shows cursor position and modified status.

#include "widget.h"
#include "property.h"
#include "signal.h"

#include <string>

namespace miniui {

/// A simple label widget that displays a text string.
class Label : public Widget {
public:
    explicit Label(Widget* parent = nullptr);
    ~Label() override;

    void setText(const std::string& text);
    const std::string& text() const { return text_; }

    Size measure(Size available) override;
    void onPaint(Painter& painter) override;

private:
    std::string text_;
};

/// Status bar showing editor state: cursor position and modified flag.
class StatusBar : public Widget {
public:
    explicit StatusBar(Widget* parent = nullptr);
    ~StatusBar() override;

    /// Update the status text.
    void setStatus(const std::string& status);

    /// Update cursor position display.
    void setCursorPosition(int line, int col);

    /// Set the modified indicator.
    void setModified(bool modified);

    // Widget overrides
    Size measure(Size available) override;
    void onPaint(Painter& painter) override;

private:
    void updateText();

    int  cursorLine_ = 0;
    int  cursorCol_  = 0;
    bool modified_   = false;
    std::string statusText_;
    std::string displayText_;

    static constexpr int kStatusBarHeight = 24;
};

} // namespace miniui
