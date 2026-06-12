#include "application.h"
#include "painter.h"
#include "widget.h"

#include <cairo/cairo-xcb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <xcb/xcb_atom.h>

namespace miniui {

/// Walk up the parent chain to compute a widget's absolute (root-window) origin.
static Point absoluteOrigin(const Widget* w) {
    Point origin = w->rect().origin();
    for (auto* p = w->parent(); p; p = p->parent()) {
        origin.x += p->rect().x;
        origin.y += p->rect().y;
    }
    return origin;
}

Application& Application::instance() {
    static Application app;
    return app;
}

void Application::init(int width, int height) {
    width_ = width;
    height_ = height;

    int screenNum = 0;
    conn_ = xcb_connect(nullptr, &screenNum);
    if (xcb_connection_has_error(conn_)) {
        std::fprintf(stderr, "Cannot connect to X server\n");
        std::exit(1);
    }

    screen_ = default_screen(conn_);
    visual_ = find_visual(screen_);
    if (!visual_) { std::fprintf(stderr, "No visual\n"); std::exit(1); }

    window_ = xcb_generate_id(conn_);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen_->white_pixel,
        static_cast<uint32_t>(
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION)
    };
    xcb_create_window(conn_, XCB_COPY_FROM_PARENT, window_, screen_->root,
                      0, 0, width_, height_, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen_->root_visual, mask, values);
    xcb_map_window(conn_, window_);
    xcb_flush(conn_);

    // Register WM_DELETE_WINDOW so the window manager sends us a close event
    xcb_intern_atom_cookie_t protoCookie  = xcb_intern_atom(conn_, 0, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t delCookie    = xcb_intern_atom(conn_, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t* protoReply   = xcb_intern_atom_reply(conn_, protoCookie, nullptr);
    xcb_intern_atom_reply_t* delReply     = xcb_intern_atom_reply(conn_, delCookie, nullptr);
    if (protoReply && delReply) {
        wmProtocols_ = protoReply->atom;
        wmDeleteWin_ = delReply->atom;
        xcb_change_property(conn_, XCB_PROP_MODE_REPLACE, window_,
                            protoReply->atom, 4 /*ATOM*/, 32,
                            1, &delReply->atom);
    }
    free(protoReply);
    free(delReply);
    xcb_flush(conn_);

    doubleBuffer_ = std::make_unique<DoubleBuffer>(conn_, window_, visual_, width_, height_);

    // Create wake-up pipe (non-blocking so drain loop won't hang)
    pipe(pipeFds_);
    fcntl(pipeFds_[0], F_SETFL, O_NONBLOCK);
}

void Application::setRoot(std::shared_ptr<Widget> root) {
    root_ = std::move(root);
    if (root_) {
        root_->measure({width_, height_});
        root_->arrange({0, 0, width_, height_});
        markDirty({0, 0, width_, height_});
    }
}

void Application::quit() { running_ = false; }

void Application::postTask(std::function<void()> task) {
    {
        std::lock_guard lock(taskMutex_);
        taskQueue_.push_back(std::move(task));
    }
    if (pipeFds_[1] >= 0) {
        char c = 'x';
        write(pipeFds_[1], &c, 1);
    }
}

void Application::markDirty(Rect region) {
    dirtyRegions_.push_back(region);
}

int Application::run() {
    running_ = true;
    int xcbFd = xcb_get_file_descriptor(conn_);

    while (running_) {
        struct pollfd fds[2];
        fds[0].fd = xcbFd;
        fds[0].events = POLLIN;
        fds[1].fd = pipeFds_[0];
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, 16); // ~60 FPS
        if (ret < 0) break;

        processXcbEvents();
        processTaskQueue();
        processRepaint();
    }

    close(pipeFds_[0]);
    close(pipeFds_[1]);
    xcb_disconnect(conn_);
    return 0;
}

void Application::processXcbEvents() {
    while (auto* ev = xcb_poll_for_event(conn_)) {
        uint8_t type = ev->response_type & ~0x80;
        switch (type) {
        case XCB_EXPOSE: {
            auto* ex = reinterpret_cast<xcb_expose_event_t*>(ev);
            if (ex->count == 0) markDirty({ex->x, ex->y, ex->width, ex->height});
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            width_ = cfg->width;
            height_ = cfg->height;
            doubleBuffer_->resize(width_, height_);
            if (root_) {
                root_->measure({width_, height_});
                root_->arrange({0, 0, width_, height_});
            }
            markDirty({0, 0, width_, height_});
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            auto* cm = reinterpret_cast<xcb_client_message_event_t*>(ev);
            if (cm->data.data32[0] == wmDeleteWin_) {
                running_ = false;
            }
            break;
        }
        case XCB_BUTTON_PRESS: {
            auto* bp = reinterpret_cast<xcb_button_press_event_t*>(ev);
            if (root_) {
                Point p{bp->event_x, bp->event_y};
                auto target = root_->hitTest(p);
                if (target) {
                    Point abs = absoluteOrigin(target.get());
                    target->onMousePress({p.x - abs.x, p.y - abs.y}, bp->detail);
                }
            }
            break;
        }
        case XCB_BUTTON_RELEASE: {
            auto* br = reinterpret_cast<xcb_button_release_event_t*>(ev);
            if (root_) {
                Point p{br->event_x, br->event_y};
                auto target = root_->hitTest(p);
                if (target) {
                    Point abs = absoluteOrigin(target.get());
                    target->onMouseRelease({p.x - abs.x, p.y - abs.y}, br->detail);
                }
            }
            break;
        }
        case XCB_MOTION_NOTIFY: {
            auto* mn = reinterpret_cast<xcb_motion_notify_event_t*>(ev);
            if (root_) {
                Point p{mn->event_x, mn->event_y};
                auto target = root_->hitTest(p);
                if (target) {
                    Point abs = absoluteOrigin(target.get());
                    target->onMouseMove({p.x - abs.x, p.y - abs.y});
                }
            }
            break;
        }
        case XCB_KEY_PRESS: {
            auto* kp = reinterpret_cast<xcb_key_press_event_t*>(ev);
            if (kp->detail == 9) { running_ = false; }
            break;
        }
        }
        free(ev);
    }
}

void Application::processTaskQueue() {
    if (pipeFds_[0] >= 0) {
        char buf[64];
        while (read(pipeFds_[0], buf, sizeof(buf)) > 0) {}
    }
    std::deque<std::function<void()>> tasks;
    {
        std::lock_guard lock(taskMutex_);
        tasks.swap(taskQueue_);
    }
    for (auto& task : tasks) task();
}

void Application::processRepaint() {
    if (dirtyRegions_.empty() || !root_) return;

    auto painter = doubleBuffer_->beginPaint(dirtyRegions_);
    painter.setSourceRGB(1, 1, 1);
    painter.paint();
    painter.save();
    root_->paint(painter);
    painter.restore();

    doubleBuffer_->endPaint();
    dirtyRegions_.clear();
}

} // namespace miniui
