#pragma once
/// @file menu_bar.h
/// @brief MenuBar — horizontal bar of clickable buttons.

#include "widget.h"
#include "signal.h"
#include "geometry.h"

#include <string>
#include <vector>

namespace miniui {

/// A simple button widget that emits a signal when clicked (or activated).
class Button : public Widget {
public:
    explicit Button(const std::string& label, Widget* parent = nullptr);
    ~Button() override;

    const std::string& label() const { return label_; }
    void setLabel(const std::string& label);

    Signal<>& clicked() { return clicked_; }

    // Widget overrides
    Size measure(Size available) override;
    void onPaint(Painter& painter) override;
    bool onKeyEvent(xcb_keycode_t keycode) override;

private:
    std::string label_;
    Signal<> clicked_;
    bool hovered_ = false;
};

/// MenuBar: a horizontal bar of buttons.
class MenuBar : public Widget {
public:
    explicit MenuBar(Widget* parent = nullptr);
    ~MenuBar() override;

    /// Access individual menu buttons.
    Button* fileButton()   { return fileBtn_; }
    Button* editButton()   { return editBtn_; }
    Button* helpButton()   { return helpBtn_; }

    // Widget overrides
    Size measure(Size available) override;
    void arrange(Rect allocated) override;
    void onPaint(Painter& painter) override;

private:
    Button* fileBtn_;
    Button* editBtn_;
    Button* helpBtn_;
    static constexpr int kMenuBarHeight = 28;
};

} // namespace miniui
