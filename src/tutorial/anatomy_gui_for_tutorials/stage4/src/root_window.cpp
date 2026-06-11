#include "root_window.h"
#include "painter.h"
#include "widget.h"

#include <cairo/cairo-xcb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// XCB keycodes for printable ASCII
#include <xcb/xcb_keysyms.h>

namespace miniui {

RootWindow::RootWindow(int width, int height)
    : width_(width), height_(height), surface_(nullptr)
{
    int screenNum = 0;
    conn_ = xcb_connect(nullptr, &screenNum);
    if (xcb_connection_has_error(conn_)) {
        std::fprintf(stderr, "Cannot connect to X server\n");
        std::exit(1);
    }

    const xcb_setup_t* setup = xcb_get_setup(conn_);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screenNum; ++i) xcb_screen_next(&iter);
    screen_ = iter.data;

    visual_ = findVisual(screen_);
    if (!visual_) {
        std::fprintf(stderr, "Cannot find root visual\n");
        std::exit(1);
    }

    window_ = xcb_generate_id(conn_);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen_->white_pixel,
        static_cast<uint32_t>(
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_KEY_PRESS |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION)
    };

    xcb_create_window(conn_, XCB_COPY_FROM_PARENT, window_, screen_->root,
                      0, 0, static_cast<uint16_t>(width_),
                      static_cast<uint16_t>(height_), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen_->root_visual, mask, values);

    xcb_map_window(conn_, window_);
    xcb_flush(conn_);
}

RootWindow::~RootWindow() {
    if (surface_) cairo_surface_destroy(surface_);
    if (conn_) xcb_disconnect(conn_);
}

void RootWindow::setRoot(std::unique_ptr<Widget> root) {
    root_ = std::move(root);
    if (root_) root_->setBounds({0, 0, width_, height_});
}

int RootWindow::eventLoop() {
    xcb_generic_event_t* ev = nullptr;
    while ((ev = xcb_wait_for_event(conn_)) != nullptr) {
        handleXcbEvent(ev);
        free(ev);
    }
    return 0;
}

void RootWindow::handleXcbEvent(xcb_generic_event_t* ev) {
    uint8_t type = ev->response_type & ~0x80;

    switch (type) {
    case XCB_EXPOSE: {
        auto* ex = reinterpret_cast<xcb_expose_event_t*>(ev);
        if (ex->count == 0) {
            repaintAll();
        }
        break;
    }

    case XCB_CONFIGURE_NOTIFY: {
        auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
        resize(cfg->width, cfg->height);
        break;
    }

    case XCB_BUTTON_PRESS: {
        auto* bp = reinterpret_cast<xcb_button_press_event_t*>(ev);
        if (!root_) break;
        Point p{bp->event_x, bp->event_y};
        Widget* target = root_->hitTest(p);
        if (!target) target = root_.get();
        propagate(target, [&](Widget* w) {
            return w->onMousePress(p - w->bounds().origin());
        });
        break;
    }

    case XCB_BUTTON_RELEASE: {
        auto* br = reinterpret_cast<xcb_button_release_event_t*>(ev);
        if (!root_) break;
        Point p{br->event_x, br->event_y};
        Widget* target = root_->hitTest(p);
        if (!target) target = root_.get();
        propagate(target, [&](Widget* w) {
            return w->onMouseRelease(p - w->bounds().origin());
        });
        break;
    }

    case XCB_MOTION_NOTIFY: {
        auto* mn = reinterpret_cast<xcb_motion_notify_event_t*>(ev);
        if (!root_) break;
        Point p{mn->event_x, mn->event_y};
        Widget* target = root_->hitTest(p);
        if (!target) target = root_.get();
        propagate(target, [&](Widget* w) {
            return w->onMouseMove(p - w->bounds().origin());
        });
        break;
    }

    case XCB_KEY_PRESS: {
        auto* kp = reinterpret_cast<xcb_key_press_event_t*>(ev);
        if (!root_) break;
        // Walk the whole tree — key events go to focused widget (simplified: to root)
        int keycode = static_cast<int>(kp->detail);
        // Escape quits
        if (keycode == 9) {
            // Destroy surface and disconnect — exit event loop
            return;
        }
        propagate(root_.get(), [&](Widget* w) {
            return w->onKeyPress(keycode);
        });
        break;
    }
    }
}

void RootWindow::repaintAll() {
    if (!root_) return;

    if (surface_) {
        cairo_surface_destroy(surface_);
    }
    surface_ = cairo_xcb_surface_create(conn_, window_, visual_,
                                         width_, height_);

    cairo_t* cr = cairo_create(surface_);
    {
        Painter painter(cr);
        painter.save();
        root_->paint(painter);
        painter.restore();
    }
    cairo_destroy(cr);

    cairo_surface_flush(surface_);
    xcb_flush(conn_);
}

void RootWindow::resize(int w, int h) {
    width_  = w;
    height_ = h;
    if (root_) {
        root_->setBounds({0, 0, w, h});
    }
    if (surface_) {
        cairo_xcb_surface_set_size(surface_, w, h);
    }
}

xcb_visualtype_t* RootWindow::findVisual(xcb_screen_t* screen) {
    xcb_depth_iterator_t depth_it = xcb_screen_allowed_depths_iterator(screen);
    for (; depth_it.rem; xcb_depth_next(&depth_it)) {
        xcb_visualtype_iterator_t vis_it = xcb_depth_visuals_iterator(depth_it.data);
        for (; vis_it.rem; xcb_visualtype_next(&vis_it)) {
            if (screen->root_visual == vis_it.data->visual_id) {
                return vis_it.data;
            }
        }
    }
    return nullptr;
}

template<typename Fn>
void RootWindow::propagate(Widget* target, Fn&& fn) {
    Widget* w = target;
    while (w) {
        if (fn(w)) return;  // handled
        w = w->parent();
    }
}

} // namespace miniui
