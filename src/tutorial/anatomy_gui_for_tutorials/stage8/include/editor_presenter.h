#pragma once
/// @file editor_presenter.h
/// @brief EditorPresenter — MVP Presenter wiring View signals to Model.

#include "text_buffer.h"
#include "editor_view.h"
#include "task.h"
#include "scoped_connection.h"

#include <string>

namespace miniui {

/// Presenter in the MVP triad. Connects View signals to Model operations
/// and syncs Model state back to the View.
class EditorPresenter {
public:
    EditorPresenter(TextBuffer* model, EditorView* view);
    ~EditorPresenter() = default;

    /// Wire up all signal connections.
    void connectSignals();

    // ── Coroutine file operations ──────────────────────────────────────────

    /// Open a file using coroutine (co_await async file read).
    Task<void> onOpenFile();

    /// Save the current file using coroutine.
    Task<void> onSaveFile();

    /// Create a new empty file.
    void onNewFile();

    // ── Synchronous editing operations ─────────────────────────────────────

    /// Handle a key press from the text canvas.
    void onCharInput(xcb_keycode_t keycode);

    /// Undo the last operation.
    void onUndo();

    /// Redo the last undone operation.
    void onRedo();

private:
    /// Push model state to the view.
    void syncView();

    TextBuffer*  model_;
    EditorView*  view_;

    std::string  filePath_;

    /// Holds signal connections for cleanup.
    ScopedConnections connections_;

    // XCB keysym helpers
    static bool isControlKey(xcb_keycode_t keycode);
    static char keycodeToChar(xcb_keycode_t keycode);
};

} // namespace miniui
