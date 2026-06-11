#pragma once
/// @file root_window.h
/// @brief RootWindow: the top-level widget that fills the entire XCB window.

#include "widget.h"

namespace miniui {

/// The root of the widget tree. Occupies the full window area.
class RootWindow : public Widget {
public:
    explicit RootWindow(Widget* parent = nullptr);
    ~RootWindow() override;

    Size measure(Size available) override;
    void arrange(Rect allocated) override;
    void onPaint(Painter& painter) override;
};

} // namespace miniui
