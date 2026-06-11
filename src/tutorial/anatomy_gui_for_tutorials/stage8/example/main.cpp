/// @file main.cpp
/// @brief MiniUI Stage 8 — Mini Text Editor with C++20 Coroutines.
///
/// Demonstrates the full MVP architecture:
///   Model  : TextBuffer (text data, cursor, undo/redo)
///   View   : EditorView (MenuBar + TextCanvas + StatusBar)
///   Presenter: EditorPresenter (wires signals, coroutine file I/O)
///
/// Controls:
///   Type characters     -> insert at cursor
///   Backspace           -> delete before cursor
///   Enter               -> new line
///   F1                  -> undo
///   F2                  -> redo
///   F3                  -> save file (/tmp/miniui_editor_tmp.txt)
///   F4                  -> open file
///   F5                  -> new file
///   Escape              -> quit
///
/// Menu buttons:
///   "File"  -> open file (coroutine)
///   "Edit"  -> undo
///   "Help"  -> save file (coroutine)

#include "application.h"
#include "root_window.h"
#include "editor_view.h"
#include "editor_presenter.h"
#include "text_buffer.h"

#include <cstdio>

int main() {
    std::printf("=== MiniUI Stage 8: Mini Text Editor ===\n");
    std::printf("Controls:\n");
    std::printf("  Type          -> insert characters\n");
    std::printf("  Backspace     -> delete before cursor\n");
    std::printf("  Enter         -> new line\n");
    std::printf("  F1            -> undo\n");
    std::printf("  F2            -> redo\n");
    std::printf("  F3            -> save file\n");
    std::printf("  F4            -> open file\n");
    std::printf("  F5            -> new file\n");
    std::printf("  Escape        -> quit\n");
    std::printf("  File button   -> open file (coroutine)\n");
    std::printf("  Edit button   -> undo\n");
    std::printf("  Help button   -> save file (coroutine)\n");
    std::printf("\n");

    // Initialize the application (XCB window + event loop)
    miniui::Application::init(800, 600, "MiniUI Stage 8 - Mini Editor");
    auto& app = miniui::Application::instance();

    // Model: TextBuffer
    miniui::TextBuffer model;

    // View: EditorView (MenuBar + TextCanvas + StatusBar)
    auto* editorView = new miniui::EditorView(app.rootWindow());

    // Presenter: wires everything together
    miniui::EditorPresenter presenter(&model, editorView);
    presenter.connectSignals();

    // Run the event loop
    int exitCode = app.run();

    // Cleanup: editorView is a child of rootWindow, deleted automatically
    std::printf("Mini Editor exited with code %d\n", exitCode);
    return exitCode;
}
