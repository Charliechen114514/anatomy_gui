#include "application.h"
#include "painter.h"
#include "root_window.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xcb/xcb_atom.h>

namespace miniui {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Application::Application()
    : threadPool_(std::make_unique<ThreadPool>(
          std::max(2u, std::thread::hardware_concurrency()))) {}

Application::~Application() {
    if (threadPool_) {
        threadPool_->shutdown();
    }
}

// ---------------------------------------------------------------------------
// Window creation
// ---------------------------------------------------------------------------

void Application::createWindow(const std::string& title, int width, int height) {
    winWidth_  = width;
    winHeight_ = height;

    int screenIdx = 0;
    connection_ = connectXcb(&screenIdx);
    if (!connection_ || xcb_connection_has_error(connection_.get())) {
        std::fprintf(stderr, "Cannot connect to X server\n");
        std::exit(1);
    }

    screen_ = defaultScreen(connection_.get());
    visual_ = findVisual(screen_);
    if (!visual_) {
        std::fprintf(stderr, "Cannot find root visual\n");
        std::exit(1);
    }

    mainWindow_ = xcb_generate_id(connection_.get());

    uint32_t mask   = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
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

    xcb_void_cookie_t cookie = xcb_create_window_checked(
        connection_.get(), XCB_COPY_FROM_PARENT,
        mainWindow_, screen_->root,
        100, 100,
        static_cast<uint16_t>(width), static_cast<uint16_t>(height),
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen_->root_visual,
        mask, values);

    if (auto* err = xcb_request_check(connection_.get(), cookie)) {
        std::fprintf(stderr, "xcb_create_window failed: %u\n", err->error_code);
        free(err);
        std::exit(1);
    }

    // Set window title
    xcb_change_property(connection_.get(), XCB_PROP_MODE_REPLACE,
                        mainWindow_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8, title.size(), title.c_str());
    xcb_flush(connection_.get());

    // Intern the wake-up atom
    const char wakeName[] = "MINIUI_WAKE";
    xcb_intern_atom_cookie_t atomCookie =
        xcb_intern_atom(connection_.get(), 0, strlen(wakeName), wakeName);
    xcb_intern_atom_reply_t* atomReply =
        xcb_intern_atom_reply(connection_.get(), atomCookie, nullptr);
    if (atomReply) {
        wakeAtom_ = atomReply->atom;
        free(atomReply);
    }

    xcb_map_window(connection_.get(), mainWindow_);
    xcb_flush(connection_.get());

    // Register WM_DELETE_WINDOW so the window manager sends us a close event
    xcb_intern_atom_cookie_t protoCookie = xcb_intern_atom(connection_.get(), 0, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t delCookie   = xcb_intern_atom(connection_.get(), 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t* protoReply  = xcb_intern_atom_reply(connection_.get(), protoCookie, nullptr);
    xcb_intern_atom_reply_t* delReply    = xcb_intern_atom_reply(connection_.get(), delCookie, nullptr);
    if (protoReply && delReply) {
        wmProtocols_ = protoReply->atom;
        wmDeleteWin_ = delReply->atom;
        xcb_change_property(connection_.get(), XCB_PROP_MODE_REPLACE, mainWindow_,
                            protoReply->atom, 4 /*ATOM*/, 32,
                            1, &delReply->atom);
    }
    free(protoReply);
    free(delReply);
    xcb_flush(connection_.get());

    // Create Cairo surface + context
    surface_ = createXcbSurface(connection_.get(), mainWindow_,
                                visual_, winWidth_, winHeight_);
    cairoCtx_ = createContext(surface_.get());
}

void Application::setRoot(std::shared_ptr<RootWindow> root) {
    root_ = std::move(root);
    if (root_) {
        // Wire up the repaint callback so widgets can trigger re-renders
        root_->onRepaintRequested = [this]() {
            postTask([this]() {
                if (!root_) return;
                Painter p(cairoCtx_.get());
                root_->paint(p);
                cairo_surface_flush(surface_.get());
                xcb_flush(connection_.get());
            });
        };
    }
}

// ---------------------------------------------------------------------------
// Event loop
// ---------------------------------------------------------------------------

int Application::run() {
    running_ = true;

    while (running_) {
        // 1. Drain any GUI tasks that arrived before we block.
        drainGuiTaskQueue();

        // 2. Wait for the next XCB event (blocks here).
        auto* event = xcb_wait_for_event(connection_.get());
        if (!event) {
            // Connection closed or fatal error.
            running_ = false;
            break;
        }

        // 3. Dispatch the XCB event.
        dispatchEvent(event);
        free(event);

        // 4. After handling the XCB event, drain GUI tasks again.
        drainGuiTaskQueue();
    }

    return 0;
}

void Application::quit() {
    running_ = false;
    // Wake the event loop in case it is blocked in xcb_wait_for_event.
    wakeGuiThread();
}

// ---------------------------------------------------------------------------
// Task queue (thread-safe)
// ---------------------------------------------------------------------------

void Application::postTask(std::function<void()> task) {
    {
        std::unique_lock lock(guiQueueMutex_);
        guiTaskQueue_.push(std::move(task));
    }
    wakeGuiThread();
}

void Application::drainGuiTaskQueue() {
    std::queue<std::function<void()>> tasks;
    {
        std::unique_lock lock(guiQueueMutex_);
        tasks.swap(guiTaskQueue_);
    }
    // Execute outside the lock — avoids deadlock if a task calls postTask().
    while (!tasks.empty()) {
        tasks.front()();
        tasks.pop();
    }
}

// ---------------------------------------------------------------------------
// Wake-up mechanism (XCB ClientMessage)
// ---------------------------------------------------------------------------

void Application::wakeGuiThread() {
    if (!connection_ || mainWindow_ == 0 || wakeAtom_ == XCB_ATOM_NONE) {
        return;
    }

    xcb_client_message_event_t ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window        = mainWindow_;
    ev.type          = wakeAtom_;
    ev.format        = 32;
    ev.data.data32[0] = WAKEUP_MESSAGE;

    xcb_send_event(connection_.get(), false, mainWindow_,
                   XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char*>(&ev));
    xcb_flush(connection_.get());
}

// ---------------------------------------------------------------------------
// XCB event dispatch
// ---------------------------------------------------------------------------

void Application::dispatchEvent(xcb_generic_event_t* event) {
    uint8_t type = event->response_type & ~0x80;

    switch (type) {
    case XCB_EXPOSE: {
        auto* ex = reinterpret_cast<xcb_expose_event_t*>(event);
        if (ex->count == 0 && root_) {
            Painter p(cairoCtx_.get());
            root_->paint(p);
            cairo_surface_flush(surface_.get());
            xcb_flush(connection_.get());
        }
        break;
    }

    case XCB_CONFIGURE_NOTIFY: {
        auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(event);
        winWidth_  = cfg->width;
        winHeight_ = cfg->height;
        cairo_xcb_surface_set_size(surface_.get(), winWidth_, winHeight_);
        if (root_) {
            root_->arrange({0, 0, static_cast<double>(winWidth_),
                            static_cast<double>(winHeight_)});
            Painter p(cairoCtx_.get());
            root_->paint(p);
            cairo_surface_flush(surface_.get());
            xcb_flush(connection_.get());
        }
        break;
    }

    case XCB_BUTTON_PRESS: {
        auto* bp = reinterpret_cast<xcb_button_press_event_t*>(event);
        if (root_) {
            Point pt{static_cast<double>(bp->event_x),
                     static_cast<double>(bp->event_y)};
            std::fprintf(stderr, "[DBG] BUTTON_PRESS raw=(%f,%f)\n", pt.x, pt.y);
            if (auto* w = root_->hitTest(pt)) {
                Point local{pt.x - w->frame().x, pt.y - w->frame().y};
                std::fprintf(stderr, "[DBG]   hitTest → %p frame=(%f,%f,%f,%f) local=(%f,%f)\n",
                             (void*)w, w->frame().x, w->frame().y, w->frame().width, w->frame().height,
                             local.x, local.y);
                w->onMousePress(local);
            } else {
                std::fprintf(stderr, "[DBG]   hitTest → null\n");
            }
        }
        break;
    }

    case XCB_MOTION_NOTIFY: {
        auto* mn = reinterpret_cast<xcb_motion_notify_event_t*>(event);
        if (root_) {
            Point pt{static_cast<double>(mn->event_x),
                     static_cast<double>(mn->event_y)};
            if (auto* w = root_->hitTest(pt)) {
                Point local{pt.x - w->frame().x, pt.y - w->frame().y};
                w->onMouseMove(local);
            }
        }
        break;
    }

    case XCB_BUTTON_RELEASE: {
        auto* br = reinterpret_cast<xcb_button_release_event_t*>(event);
        if (root_) {
            Point pt{static_cast<double>(br->event_x),
                     static_cast<double>(br->event_y)};
            std::fprintf(stderr, "[DBG] BUTTON_RELEASE raw=(%f,%f)\n", pt.x, pt.y);
            if (auto* w = root_->hitTest(pt)) {
                Point local{pt.x - w->frame().x, pt.y - w->frame().y};
                std::fprintf(stderr, "[DBG]   hitTest → %p frame=(%f,%f,%f,%f) local=(%f,%f)\n",
                             (void*)w, w->frame().x, w->frame().y, w->frame().width, w->frame().height,
                             local.x, local.y);
                w->onMouseRelease(local);
            } else {
                std::fprintf(stderr, "[DBG]   hitTest → null\n");
            }
        }
        break;
    }

    case XCB_KEY_PRESS: {
        auto* kp = reinterpret_cast<xcb_key_press_event_t*>(event);
        // Escape => quit
        if (kp->detail == 9) {
            quit();
        }
        break;
    }

    case XCB_CLIENT_MESSAGE: {
        auto* cm = reinterpret_cast<xcb_client_message_event_t*>(event);
        if (cm->data.data32[0] == wmDeleteWin_) {
            // Window manager asked us to close
            running_ = false;
        } else if (cm->type == wakeAtom_) {
            // This is our wake-up ping — nothing to do here; the
            // drainGuiTaskQueue() call after dispatchEvent() will
            // handle the pending tasks.
        }
        break;
    }

    default:
        break;
    }
}

} // namespace miniui
