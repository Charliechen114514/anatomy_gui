#include "editor_presenter.h"
#include "application.h"
#include "async_file.h"

#include <xcb/xcb_keysyms.h>
#include <xcb/xcb.h>
#include <cctype>
#include <cstdio>

namespace miniui {

EditorPresenter::EditorPresenter(TextBuffer* model, EditorView* view)
    : model_(model), view_(view) {
}

void EditorPresenter::connectSignals() {
    // Keyboard input from canvas -> presenter
    auto canvas = view_->textCanvas();
    {
        auto id = canvas->keyPressed().connect([this](xcb_keycode_t kc) {
            onCharInput(kc);
        });
        connections_.add(ScopedConnection(canvas->keyPressed().makeDisconnecter(id)));
    }

    // File menu button
    {
        auto id = view_->menuBar()->fileButton()->clicked().connect([this]() {
            auto task = onOpenFile();
            task.start();
        });
        connections_.add(ScopedConnection(
            view_->menuBar()->fileButton()->clicked().makeDisconnecter(id)));
    }

    // Edit menu button -> triggers undo
    {
        auto id = view_->menuBar()->editButton()->clicked().connect([this]() {
            onUndo();
        });
        connections_.add(ScopedConnection(
            view_->menuBar()->editButton()->clicked().makeDisconnecter(id)));
    }

    // Help menu button -> save
    {
        auto id = view_->menuBar()->helpButton()->clicked().connect([this]() {
            auto task = onSaveFile();
            task.start();
        });
        connections_.add(ScopedConnection(
            view_->menuBar()->helpButton()->clicked().makeDisconnecter(id)));
    }

    // Model content changed -> sync view
    {
        auto id = model_->contentChanged().connect([this]() {
            syncView();
        });
        connections_.add(ScopedConnection(
            model_->contentChanged().makeDisconnecter(id)));
    }

    // Initial sync
    syncView();
}

// ── Key input handling ───────────────────────────────────────────────────────

char EditorPresenter::keycodeToChar(xcb_keycode_t keycode) {
    // Basic US keyboard layout mapping for lowercase ASCII.
    // XCB keycode 10 = '1', 11 = '2', ... , 18 = '9', 19 = '0'
    // XCB keycode 24 = 'q', 25 = 'w', ... (QWERTY row)
    // This is a simplified mapping that works for basic ASCII input.

    // Row 1: numbers
    static const char row1[] = "1234567890";
    if (keycode >= 10 && keycode <= 19) {
        return row1[keycode - 10];
    }

    // Row 2: qwertyuiop
    static const char row2[] = "qwertyuiop";
    if (keycode >= 24 && keycode <= 33) {
        return row2[keycode - 24];
    }

    // Row 3: asdfghjkl
    static const char row3[] = "asdfghjkl";
    if (keycode >= 38 && keycode <= 46) {
        return row3[keycode - 38];
    }

    // Row 4: zxcvbnm
    static const char row4[] = "zxcvbnm";
    if (keycode >= 52 && keycode <= 58) {
        return row4[keycode - 52];
    }

    // Space
    if (keycode == 65) return ' ';

    // Period
    if (keycode == 60) return '.';

    // Comma
    if (keycode == 59) return ',';

    // Semicolon
    if (keycode == 47) return ';';

    // Slash
    if (keycode == 61) return '/';

    // Enter
    if (keycode == 36) return '\n';

    // Backspace is handled separately

    return 0;
}

bool EditorPresenter::isControlKey(xcb_keycode_t keycode) {
    // We handle Ctrl+Z (undo) and Ctrl+Y (redo) at the application level.
    // For simplicity, we detect these via keycode combinations.
    return false;
}

void EditorPresenter::onCharInput(xcb_keycode_t keycode) {
    // Escape key -> quit
    if (keycode == 9) {
        Application::instance().quit(0);
        return;
    }

    // Backspace (keycode 22)
    if (keycode == 22) {
        model_->deleteChar();
        return;
    }

    // Enter (keycode 36)
    if (keycode == 36) {
        model_->newLine();
        return;
    }

    // Ctrl+Z: undo (keycode 52 = 'z', with Ctrl modifier)
    // In XCB, Ctrl+key has the same keycode. We use a simple heuristic:
    // keycode 52 with state check would need XKB, so we use a different approach.
    // For the demo, we map F1 (67) to undo and F2 (68) to redo.
    if (keycode == 67) { // F1 -> undo
        onUndo();
        return;
    }
    if (keycode == 68) { // F2 -> redo
        onRedo();
        return;
    }

    // Ctrl+S: save (F3 for demo)
    if (keycode == 69) { // F3 -> save
        auto task = onSaveFile();
        task.start();
        return;
    }

    // Ctrl+O: open (F4 for demo)
    if (keycode == 70) { // F4 -> open
        auto task = onOpenFile();
        task.start();
        return;
    }

    // Ctrl+N: new (F5 for demo)
    if (keycode == 71) { // F5 -> new file
        onNewFile();
        return;
    }

    // Regular character
    char c = keycodeToChar(keycode);
    if (c != 0) {
        model_->insertChar(c);
    }
}

void EditorPresenter::onUndo() {
    model_->undo();
}

void EditorPresenter::onRedo() {
    model_->redo();
}

void EditorPresenter::onNewFile() {
    model_->loadFromString("");
    filePath_.clear();
    syncView();
}

// ── Coroutine file operations ────────────────────────────────────────────────

Task<void> EditorPresenter::onOpenFile() {
    // For this demo, open a hardcoded path or the last saved path
    std::string path = filePath_.empty() ? "/tmp/miniui_editor_tmp.txt" : filePath_;

    view_->statusBar()->setStatus("Opening: " + path + "...");

    try {
        std::string content = co_await readFileAsync(path);
        model_->loadFromString(content);
        filePath_ = path;
        view_->statusBar()->setStatus("Opened: " + path);
    } catch (const std::exception& e) {
        view_->statusBar()->setStatus("Failed to open: " + path);
    }

    syncView();
    co_return;
}

Task<void> EditorPresenter::onSaveFile() {
    if (filePath_.empty()) {
        filePath_ = "/tmp/miniui_editor_tmp.txt";
    }

    view_->statusBar()->setStatus("Saving: " + filePath_ + "...");

    try {
        co_await writeFileAsync(filePath_, model_->toString());
        model_->saveFile(filePath_);
        view_->statusBar()->setStatus("Saved: " + filePath_);
    } catch (const std::exception& e) {
        view_->statusBar()->setStatus("Failed to save: " + filePath_);
    }

    syncView();
    co_return;
}

// ── View sync ────────────────────────────────────────────────────────────────

void EditorPresenter::syncView() {
    view_->textCanvas()->setLines(model_->lines());
    view_->textCanvas()->setCursor(model_->cursorLine(), model_->cursorCol());
    view_->statusBar()->setCursorPosition(model_->cursorLine(), model_->cursorCol());
    view_->statusBar()->setModified(model_->isModified());
    view_->textCanvas()->repaint();
    Application::instance().requestRepaint();
}

} // namespace miniui
