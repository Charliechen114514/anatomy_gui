#pragma once
/// @file application.h
/// @brief Application singleton with poll-based event loop and coroutine scheduler.

#include "raii.h"
#include "geometry.h"

#include <xcb/xcb.h>
#include <queue>
#include <coroutine>
#include <functional>
#include <vector>
#include <atomic>

namespace miniui {

class Widget;
class RootWindow;
class DoubleBuffer;

/// Singleton application managing the XCB event loop.
class Application {
public:
    /// Get the singleton instance. Must call init() first.
    static Application& instance();

    /// Initialize with window dimensions and title.
    static void init(int width = 800, int height = 600,
                     const char* title = "MiniUI Stage 8");

    /// Run the event loop. Blocks until quit() is called.
    int run();

    /// Request the event loop to exit.
    void quit(int exitCode = 0);

    /// Schedule a coroutine handle to be resumed on the next loop iteration.
    void scheduleResume(std::coroutine_handle<> handle);

    /// Post a task (callback) to be executed on the next loop iteration.
    void postTask(std::function<void()> task);

    /// Access the root window.
    RootWindow* rootWindow() const { return rootWindow_; }

    /// Access the XCB connection.
    xcb_connection_t* connection() const { return conn_.get(); }

    /// Access the XCB screen.
    xcb_screen_t* screen() const { return screen_; }

    /// Access the XCB window.
    xcb_window_t window() const { return win_; }

    /// Get the double buffer.
    DoubleBuffer* doubleBuffer() const { return doubleBuffer_; }

    /// Trigger a full repaint.
    void requestRepaint();

    /// Wake up the event loop from blocking wait.
    void wakeupEventLoop();

private:
    Application(int width, int height, const char* title);
    ~Application();

    void processXcbEvents();
    void processCoroutineResumes();
    void processPostedTasks();
    void processRepaints();
    void waitForNextEvent(int timeoutMs = 50);
    bool hasPendingEvents() const;

    static Application* instance_;

    // XCB resources
    XcbConnectionPtr conn_;
    xcb_screen_t*    screen_ = nullptr;
    xcb_visualtype_t* visual_ = nullptr;
    xcb_window_t     win_ = 0;

    // Widget tree
    RootWindow*   rootWindow_  = nullptr;
    DoubleBuffer* doubleBuffer_ = nullptr;

    // Coroutine scheduler
    std::queue<std::coroutine_handle<>> resumeQueue_;

    // Posted tasks
    std::vector<std::function<void()>> pendingTasks_;

    // State
    bool running_  = false;
    int  exitCode_ = 0;
    bool repaintRequested_ = false;
    int  winWidth_  = 0;
    int  winHeight_ = 0;
};

} // namespace miniui
