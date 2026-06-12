#pragma once
/// @file root_window.h
/// @brief RootWindow — the bridge between XCB and the widget tree.

#include "geometry.h"
#include "raii.h"

#include <memory>
#include <xcb/xcb.h>

namespace miniui {

class Widget;
class Painter;

/// Owns an XCB window and hosts a widget tree.
///
/// Translates raw XCB events into Widget method calls:
///   XCB_EXPOSE         → repaintAll()
///   XCB_BUTTON_PRESS   → hitTest + onMousePress
///   XCB_BUTTON_RELEASE → hitTest + onMouseRelease
///   XCB_MOTION_NOTIFY  → hitTest + onMouseMove
///   XCB_KEY_PRESS      → keyPress → walk parent chain
///   XCB_CONFIGURE_NOTIFY → resize
class RootWindow {
public:
    RootWindow(int width = 800, int height = 600);
    ~RootWindow();

    /// Set the root widget of the tree (takes ownership).
    void setRoot(std::unique_ptr<Widget> root);
    Widget* root() const { return root_.get(); }

    /// Enter the blocking event loop. Returns 0 on normal exit.
    int eventLoop();

    xcb_window_t windowId() const { return window_; }
    Size clientSize() const { return {width_, height_}; }

private:
    void repaintAll();
    void handleXcbEvent(xcb_generic_event_t* ev);
    void resize(int w, int h);

    // Propagate an event up the parent chain until handled
    template<typename Fn>
    void propagate(Widget* target, Fn&& fn);

    // Helper: find the visual for the default screen
    static xcb_visualtype_t* findVisual(xcb_screen_t* screen);

    int width_, height_;

    xcb_connection_t*    conn_;
    xcb_screen_t*        screen_;
    xcb_visualtype_t*    visual_;
    xcb_window_t         window_;
    cairo_surface_t*     surface_;   // managed manually for resize

    std::unique_ptr<Widget> root_;
};

} // namespace miniui
