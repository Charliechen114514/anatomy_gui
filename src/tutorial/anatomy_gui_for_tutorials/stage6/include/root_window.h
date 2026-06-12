#pragma once
/// @file root_window.h
/// @brief RootWindow — owns the XCB window, Cairo surface, event loop.

#include "geometry.h"
#include "raii.h"
#include "widget.h"

#include <xcb/xcb.h>

namespace miniui {

class RootWindow : public Widget {
public:
    RootWindow(int width, int height, const char* title = "MiniUI");
    void run();
    void repaint() override;
    void layout();

    Size measure(Size available) override;
    void paint(Painter& painter) override;
    std::shared_ptr<Widget> hitTest(Point pt) override;

private:
    void handleExpose();
    void handleConfigure(xcb_configure_notify_event_t* cfg);
    void handleButtonPress(xcb_button_press_event_t* ev);
    void handleButtonRelease(xcb_button_release_event_t* ev);
    void handleMotion(xcb_motion_notify_event_t* ev);
    void doFullPaint();

    XcbConnPtr conn_;
    xcb_window_t win_ = 0;
    SurfPtr      surface_;
    CtxPtr       cairo_;
    int          winW_;
    int          winH_;
    bool         needsRepaint_ = false;

    std::shared_ptr<Widget> pressedWidget_;
};

} // namespace miniui
