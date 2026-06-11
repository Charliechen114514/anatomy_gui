#pragma once
/// @file application.h — poll()-based event loop with postTask and dirty-region repaint.

#include "double_buffer.h"
#include "geometry.h"
#include "raii.h"
#include "widget.h"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <xcb/xcb.h>

namespace miniui {

class Application {
public:
    static Application& instance();

    /// Create app + XCB window. Call once.
    void init(int width = 800, int height = 600);

    /// poll()-based event loop (~60 FPS). Returns exit code.
    int run();

    void quit();

    // ── Task posting (thread-safe) ─────────────────────
    void postTask(std::function<void()> task);

    // ── Dirty region management ────────────────────────
    void markDirty(Rect region);

    // ── Access ─────────────────────────────────────────
    xcb_connection_t* connection()const{ return conn_; }
    xcb_window_t window()const{ return window_; }
    xcb_visualtype_t* visual()const{ return visual_; }

    void setRoot(std::shared_ptr<Widget> root);
    Widget* root()const{ return root_.get(); }

private:
    Application() = default;
    void processXcbEvents();
    void processTaskQueue();
    void processRepaint();

    xcb_connection_t*  conn_   = nullptr;
    xcb_screen_t*      screen_ = nullptr;
    xcb_visualtype_t*  visual_ = nullptr;
    xcb_window_t       window_ = 0;
    xcb_atom_t         wmProtocols_ = XCB_ATOM_NONE;
    xcb_atom_t         wmDeleteWin_ = XCB_ATOM_NONE;
    int width_ = 800, height_ = 600;
    bool running_ = false;

    std::unique_ptr<DoubleBuffer> doubleBuffer_;
    std::shared_ptr<Widget> root_;
    std::vector<Rect> dirtyRegions_;

    // Task queue (thread-safe)
    std::mutex taskMutex_;
    std::deque<std::function<void()>> taskQueue_;

    // Pipe for waking poll()
    int pipeFds_[2] = {-1, -1};
};

} // namespace miniui
