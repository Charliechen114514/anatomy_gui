#pragma once

#include "geometry.h"
#include "raii.h"
#include "thread_pool.h"

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <xcb/xcb.h>

#include <functional>
#include <mutex>
#include <queue>
#include <string>

namespace miniui {

class RootWindow;

/// The Application owns the XCB connection, the main window, and the
/// ThreadPool.  It runs the event loop and provides runAsync() + postTask()
/// for safe cross-thread communication.
class Application {
public:
    Application();
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Create the main window with the given title and size.
    void createWindow(const std::string& title, int width, int height);

    /// Set the root widget tree for the window.
    void setRoot(std::shared_ptr<RootWindow> root);

    /// Run the blocking event loop.  Returns exit code.
    int run();

    /// Request the event loop to exit.
    void quit();

    // --- Thread Pool helpers ---

    /// Submit `work` to the thread pool.  When `work` completes, `onDone`
    /// is posted to the GUI thread via postTask().
    ///
    /// Work:  callable executed on a background thread.
    /// OnDone: callable executed on the GUI thread with the result.
    template <typename Work, typename Callback>
    void runAsync(Work&& work, Callback&& onDone);

    /// Post a task to be executed on the GUI thread during the next
    /// event-loop iteration.  Thread-safe.
    void postTask(std::function<void()> task);

    /// Direct access to the thread pool (advanced usage).
    ThreadPool& threadPool() { return *threadPool_; }

private:
    /// Wake the GUI thread by sending a synthetic XCB ClientMessage.
    void wakeGuiThread();

    /// Drain all pending GUI tasks (executed outside the lock).
    void drainGuiTaskQueue();

    /// Dispatch a single XCB event.
    void dispatchEvent(xcb_generic_event_t* event);

    // --- XCB / Cairo resources ---
    XcbConnectionPtr connection_;
    xcb_window_t mainWindow_ = 0;
    xcb_screen_t* screen_ = nullptr;
    xcb_visualtype_t* visual_ = nullptr;
    CairoSurfacePtr surface_;
    CairoContextPtr cairoCtx_;
    int winWidth_  = 800;
    int winHeight_ = 600;

    // --- Wake-up mechanism ---
    xcb_atom_t wakeAtom_ = XCB_ATOM_NONE;
    static constexpr uint32_t WAKEUP_MESSAGE = 0x57414B45; // "WAKE"

    // --- WM_DELETE_WINDOW ---
    xcb_atom_t wmProtocols_ = XCB_ATOM_NONE;
    xcb_atom_t wmDeleteWin_ = XCB_ATOM_NONE;

    // --- Root widget tree ---
    std::shared_ptr<RootWindow> root_;

    // --- Thread Pool ---
    std::unique_ptr<ThreadPool> threadPool_;

    // --- GUI task queue (written by workers, consumed by GUI thread) ---
    std::queue<std::function<void()>> guiTaskQueue_;
    std::mutex guiQueueMutex_;

    // --- State ---
    bool running_ = false;
};

// ---------------------------------------------------------------------------
// Template implementation
// ---------------------------------------------------------------------------

template <typename Work, typename Callback>
void Application::runAsync(Work&& work, Callback&& onDone) {
    using ResultType = std::invoke_result_t<Work>;

    // Capture the raw `this` pointer — the Application outlives all async
    // work because the event loop is blocked in run().
    auto* app = this;

    threadPool_->submit(
        [app,
         w = std::forward<Work>(work),
         cb = std::forward<Callback>(onDone)]() mutable {
            // Executed on a background worker thread.
            if constexpr (std::is_void_v<ResultType>) {
                w();
                app->postTask([cb = std::move(cb)]() { cb(); });
            } else {
                auto result = w();
                app->postTask(
                    [cb = std::move(cb),
                     result = std::move(result)]() mutable {
                        cb(std::move(result));
                    });
            }
        });
}

} // namespace miniui
