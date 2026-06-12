#include "root_window.h"
#include "painter.h"

#include <cairo/cairo-xcb.h>
#include <cstdio>
#include <cstring>

namespace miniui {

RootWindow::RootWindow(int width, int height, const char* title)
    : winW_(width), winH_(height) {
    int screenIdx = 0;
    conn_.reset(xcb_connect(nullptr, &screenIdx));
    if (xcb_connection_has_error(conn_.get())) {
        std::fprintf(stderr, "Cannot connect to X server\n");
        std::abort();
    }

    xcb_screen_t*     screen = default_screen(conn_.get());
    xcb_visualtype_t* visual = find_visual(screen);
    if (!visual) {
        std::fprintf(stderr, "Cannot find root visual\n");
        std::abort();
    }

    win_ = xcb_generate_id(conn_.get());

    uint32_t mask   = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen->white_pixel,
        static_cast<uint32_t>(
            XCB_EVENT_MASK_EXPOSURE        |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_BUTTON_PRESS    |
            XCB_EVENT_MASK_BUTTON_RELEASE  |
            XCB_EVENT_MASK_POINTER_MOTION)
    };

    xcb_create_window(conn_.get(),
                      XCB_COPY_FROM_PARENT, win_, screen->root,
                      100, 100,
                      static_cast<uint16_t>(winW_),
                      static_cast<uint16_t>(winH_),
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      mask, values);

    xcb_change_property(conn_.get(),
                        XCB_PROP_MODE_REPLACE, win_,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8,
                        static_cast<uint32_t>(std::strlen(title)),
                        title);

    xcb_map_window(conn_.get(), win_);
    xcb_flush(conn_.get());

    surface_.reset(cairo_xcb_surface_create(conn_.get(), win_, visual,
                                             winW_, winH_));
    cairo_.reset(cairo_create(surface_.get()));
}

void RootWindow::run() {
    xcb_generic_event_t* ev = nullptr;
    while ((ev = xcb_wait_for_event(conn_.get())) != nullptr) {
        uint8_t type = ev->response_type & ~0x80;

        switch (type) {
        case XCB_EXPOSE: {
            auto* ex = reinterpret_cast<xcb_expose_event_t*>(ev);
            if (ex->count == 0) handleExpose();
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            handleConfigure(cfg);
            break;
        }
        case XCB_BUTTON_PRESS: {
            auto* bp = reinterpret_cast<xcb_button_press_event_t*>(ev);
            handleButtonPress(bp);
            break;
        }
        case XCB_BUTTON_RELEASE: {
            auto* br = reinterpret_cast<xcb_button_release_event_t*>(ev);
            handleButtonRelease(br);
            break;
        }
        case XCB_MOTION_NOTIFY: {
            auto* mn = reinterpret_cast<xcb_motion_notify_event_t*>(ev);
            handleMotion(mn);
            break;
        }
        default:
            break;
        }

        free(ev);

        if (needsRepaint_) {
            needsRepaint_ = false;
            doFullPaint();
        }
    }
}

void RootWindow::repaint() {
    needsRepaint_ = true;
}

void RootWindow::handleExpose() {
    doFullPaint();
}

void RootWindow::handleConfigure(xcb_configure_notify_event_t* cfg) {
    if (cfg->width != winW_ || cfg->height != winH_) {
        winW_ = cfg->width;
        winH_ = cfg->height;
        cairo_xcb_surface_set_size(surface_.get(), winW_, winH_);
        layout();
        doFullPaint();
    }
}

void RootWindow::doFullPaint() {
    Painter painter(cairo_.get());
    painter.fillRect({0, 0, winW_, winH_}, Color::rgb(1, 1, 1));
    paint(painter);
    cairo_surface_flush(surface_.get());
    xcb_flush(conn_.get());
}

void RootWindow::layout() {
    measure({winW_, winH_});
    arrange({0, 0, winW_, winH_});
    // RootWindow's children must also be arranged to fill the root area
    for (auto& child : children_) {
        child->arrange({0, 0, winW_, winH_});
    }
}

Size RootWindow::measure(Size available) {
    for (auto& child : children_) {
        child->measure(available);
    }
    return {winW_, winH_};
}

void RootWindow::paint(Painter& painter) {
    for (auto& child : children_) {
        painter.save();
        painter.translate(child->rect().x, child->rect().y);
        child->paint(painter);
        painter.restore();
    }
}

/// Walk up the parent chain to compute a widget's absolute (root-window) origin.
static Point absoluteOrigin(const Widget* w) {
    Point origin = w->rect().origin();
    for (auto* p = w->parent(); p; p = p->parent()) {
        origin.x += p->rect().x;
        origin.y += p->rect().y;
    }
    return origin;
}

std::shared_ptr<Widget> RootWindow::hitTest(Point pt) {
    // RootWindow is stack-allocated, never shared_ptr-owned,
    // so we must NOT call shared_from_this().
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        auto& child = *it;
        if (child->rect().contains(pt)) {
            Point childLocal{pt.x - child->rect().x, pt.y - child->rect().y};
            return child->hitTest(childLocal);
        }
    }
    return nullptr;
}

void RootWindow::handleButtonPress(xcb_button_press_event_t* ev) {
    Point pt{static_cast<int>(ev->event_x), static_cast<int>(ev->event_y)};
    auto target = hitTest(pt);
    if (target && target.get() != this) {
        Point absOrigin = absoluteOrigin(target.get());
        Point local{pt.x - absOrigin.x, pt.y - absOrigin.y};
        pressedWidget_ = target;
        target->onMousePress(local, ev->detail);
    }
}

void RootWindow::handleButtonRelease(xcb_button_release_event_t* ev) {
    Point pt{static_cast<int>(ev->event_x), static_cast<int>(ev->event_y)};
    if (pressedWidget_) {
        Point absOrigin = absoluteOrigin(pressedWidget_.get());
        Point local{pt.x - absOrigin.x, pt.y - absOrigin.y};
        pressedWidget_->onMouseRelease(local, ev->detail);
        pressedWidget_.reset();
    }
}

void RootWindow::handleMotion(xcb_motion_notify_event_t* ev) {
    Point pt{static_cast<int>(ev->event_x), static_cast<int>(ev->event_y)};
    auto target = hitTest(pt);
    if (target && target.get() != this) {
        Point absOrigin = absoluteOrigin(target.get());
        Point local{pt.x - absOrigin.x, pt.y - absOrigin.y};
        target->onMouseMove(local);
    }
}

} // namespace miniui
