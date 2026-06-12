#include "root_window.h"
#include "painter.h"
#include <algorithm>
#include <cairo/cairo-xcb.h>
#include <cstdio>
#include <cstdlib>

namespace miniui {

RootWindow::RootWindow(int w, int h) : width_(w), height_(h), surface_(nullptr) {
    int sn = 0;
    conn_ = xcb_connect(nullptr, &sn);
    if (xcb_connection_has_error(conn_)) { std::fprintf(stderr, "XCB fail\n"); std::exit(1); }

    const xcb_setup_t* setup = xcb_get_setup(conn_);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < sn; ++i) xcb_screen_next(&it);
    screen_ = it.data;
    visual_ = find_visual(screen_);
    if (!visual_) { std::fprintf(stderr, "No visual\n"); std::exit(1); }

    window_ = xcb_generate_id(conn_);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t vals[] = { screen_->white_pixel,
        static_cast<uint32_t>(XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE) };
    xcb_create_window(conn_, XCB_COPY_FROM_PARENT, window_, screen_->root,
        0, 0, width_, height_, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen_->root_visual, mask, vals);
    xcb_map_window(conn_, window_);
    xcb_flush(conn_);
}

RootWindow::~RootWindow() {
    if (surface_) cairo_surface_destroy(surface_);
    if (conn_) xcb_disconnect(conn_);
}

void RootWindow::setRoot(std::unique_ptr<Widget> root) {
    root_ = std::move(root);
    if (root_) root_->arrange({0, 0, width_, height_});
}

int RootWindow::eventLoop() {
    xcb_generic_event_t* ev = nullptr;
    while ((ev = xcb_wait_for_event(conn_)) != nullptr) {
        uint8_t type = ev->response_type & ~0x80;
        if (type == XCB_EXPOSE) {
            auto* ex = reinterpret_cast<xcb_expose_event_t*>(ev);
            if (ex->count == 0) repaintAll();
        } else if (type == XCB_CONFIGURE_NOTIFY) {
            auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            resize(cfg->width, cfg->height);
        } else if (type == XCB_KEY_PRESS) {
            free(ev); return 0;
        }
        free(ev);
    }
    return 0;
}

void RootWindow::repaintAll() {
    if (!root_) return;
    if (surface_) cairo_surface_destroy(surface_);
    surface_ = cairo_xcb_surface_create(conn_, window_, visual_, width_, height_);
    cairo_t* cr = cairo_create(surface_);
    { Painter p(cr); p.setSourceRGB(1,1,1); p.paint(); p.save(); root_->paint(p); p.restore(); }
    cairo_destroy(cr);
    cairo_surface_flush(surface_);
    xcb_flush(conn_);
}

void RootWindow::resize(int w, int h) {
    width_ = w; height_ = h;
    if (root_) { root_->measure({w, h}); root_->arrange({0, 0, w, h}); }
    if (surface_) cairo_xcb_surface_set_size(surface_, w, h);
    repaintAll();
}

} // namespace miniui
