#pragma once
#include "geometry.h"
#include "raii.h"
#include "widget.h"
#include <memory>
#include <xcb/xcb.h>

namespace miniui {

class Painter;

class RootWindow {
public:
    RootWindow(int width = 800, int height = 600);
    ~RootWindow();

    void setRoot(std::unique_ptr<Widget> root);
    int eventLoop();

private:
    void repaintAll();
    void resize(int w, int h);

    xcb_connection_t*  conn_;
    xcb_screen_t*      screen_;
    xcb_visualtype_t*  visual_;
    xcb_window_t       window_;
    cairo_surface_t*   surface_;
    int width_, height_;
    std::unique_ptr<Widget> root_;
};

} // namespace miniui
