#include "application.h"
#include "root_window.h"
#include "double_buffer.h"
#include "task.h"
#include "widget.h"
#include "painter.h"

#include <xcb/xcb.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>

namespace miniui {

Application* Application::instance_ = nullptr;

Application::Application(int width, int height, const char* title)
    : winWidth_(width), winHeight_(height) {

    int screenIdx = 0;
    conn_ = xcbConnect(&screenIdx);
    if (xcb_connection_has_error(conn_.get())) {
        std::fprintf(stderr, "Cannot connect to X server\n");
        std::exit(1);
    }

    screen_ = defaultScreen(conn_.get());
    visual_ = findVisual(screen_);
    if (!visual_) {
        std::fprintf(stderr, "Cannot find root visual\n");
        std::exit(1);
    }

    // Create XCB window
    win_ = xcb_generate_id(conn_.get());

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        screen_->white_pixel,
        static_cast<uint32_t>(
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_KEY_PRESS |
            XCB_EVENT_MASK_BUTTON_PRESS)
    };

    xcb_void_cookie_t cookie = xcb_create_window_checked(
        conn_.get(), XCB_COPY_FROM_PARENT, win_, screen_->root,
        100, 100, static_cast<uint16_t>(width), static_cast<uint16_t>(height),
        0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen_->root_visual,
        mask, values);

    xcb_generic_error_t* err = xcb_request_check(conn_.get(), cookie);
    if (err) {
        std::fprintf(stderr, "xcb_create_window failed: %u\n", err->error_code);
        free(err);
        std::exit(1);
    }

    // Set window title
    xcb_change_property(conn_.get(), XCB_PROP_MODE_REPLACE, win_,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        static_cast<uint32_t>(std::strlen(title)), title);

    xcb_map_window(conn_.get(), win_);
    xcb_flush(conn_.get());

    // Create double buffer
    doubleBuffer_ = new DoubleBuffer(conn_.get(), win_, visual_,
                                      width, height);

    // Create root window widget
    rootWindow_ = new RootWindow(nullptr);
    rootWindow_->setSize({width, height});
}

Application::~Application() {
    delete rootWindow_;
    delete doubleBuffer_;
}

Application& Application::instance() {
    if (!instance_) {
        std::fprintf(stderr, "Application not initialized. Call Application::init() first.\n");
        std::exit(1);
    }
    return *instance_;
}

void Application::init(int width, int height, const char* title) {
    if (instance_) {
        std::fprintf(stderr, "Application already initialized.\n");
        return;
    }
    instance_ = new Application(width, height, title);
}

int Application::run() {
    running_ = true;

    // Initial layout pass
    Size available{winWidth_, winHeight_};
    rootWindow_->measure(available);
    rootWindow_->arrange(Rect(0, 0, winWidth_, winHeight_));

    while (running_) {
        // 1. Process XCB events
        processXcbEvents();

        // 2. Resume pending coroutines
        processCoroutineResumes();

        // 3. Execute posted tasks
        processPostedTasks();

        // 4. Layout pass if needed
        if (repaintRequested_) {
            Size avail{winWidth_, winHeight_};
            rootWindow_->measure(avail);
            rootWindow_->arrange(Rect(0, 0, winWidth_, winHeight_));
            rootWindow_->markDirty();
        }

        // 5. Render
        processRepaints();

        // 6. If nothing to do, wait for next event
        if (resumeQueue_.empty() && pendingTasks_.empty() && !repaintRequested_) {
            waitForNextEvent(50);
        }
    }

    return exitCode_;
}

void Application::quit(int exitCode) {
    running_ = false;
    exitCode_ = exitCode;
}

void Application::scheduleResume(std::coroutine_handle<> handle) {
    if (handle) {
        resumeQueue_.push(handle);
        wakeupEventLoop();
    }
}

void Application::postTask(std::function<void()> task) {
    pendingTasks_.push_back(std::move(task));
    wakeupEventLoop();
}

void Application::requestRepaint() {
    repaintRequested_ = true;
}

void Application::wakeupEventLoop() {
    xcb_flush(conn_.get());
}

void Application::processXcbEvents() {
    while (true) {
        xcb_generic_event_t* ev = xcb_poll_for_event(conn_.get());
        if (!ev) break;

        uint8_t type = ev->response_type & ~0x80;

        switch (type) {
        case XCB_EXPOSE: {
            auto* ex = reinterpret_cast<xcb_expose_event_t*>(ev);
            if (ex->count == 0) {
                rootWindow_->markDirty();
                repaintRequested_ = true;
            }
            break;
        }

        case XCB_CONFIGURE_NOTIFY: {
            auto* cfg = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            winWidth_  = cfg->width;
            winHeight_ = cfg->height;
            doubleBuffer_->resize(winWidth_, winHeight_);
            rootWindow_->setSize({winWidth_, winHeight_});
            rootWindow_->markDirty();
            repaintRequested_ = true;
            break;
        }

        case XCB_KEY_PRESS: {
            auto* kp = reinterpret_cast<xcb_key_press_event_t*>(ev);
            rootWindow_->onKeyEvent(kp->detail);
            break;
        }

        case XCB_BUTTON_PRESS: {
            break;
        }

        default:
            break;
        }

        free(ev);
    }
}

void Application::processCoroutineResumes() {
    while (!resumeQueue_.empty()) {
        auto handle = resumeQueue_.front();
        resumeQueue_.pop();
        if (handle && !handle.done()) {
            handle.resume();
        }
    }
}

void Application::processPostedTasks() {
    auto tasks = std::move(pendingTasks_);
    pendingTasks_.clear();
    for (auto& task : tasks) {
        task();
    }
}

void Application::processRepaints() {
    if (!repaintRequested_ && !rootWindow_->isDirty()) {
        return;
    }

    repaintRequested_ = false;

    cairo_t* cr = doubleBuffer_->beginPaint();
    if (cr) {
        Painter painter(cr);

        painter.clear(Color::rgb(1.0, 1.0, 1.0));
        rootWindow_->paint(painter);

        doubleBuffer_->endPaint();
        xcb_flush(conn_.get());
    }
}

void Application::waitForNextEvent(int timeoutMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
}

bool Application::hasPendingEvents() const {
    return !resumeQueue_.empty() || !pendingTasks_.empty();
}

// ── Free function used by Task<T>::start() ───────────────────────────────────

void scheduleTaskResume(std::coroutine_handle<> handle) {
    Application::instance().scheduleResume(handle);
}

} // namespace miniui
