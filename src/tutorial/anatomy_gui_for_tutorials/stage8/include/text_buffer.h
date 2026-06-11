#pragma once
/// @file text_buffer.h
/// @brief TextBuffer — the Model layer for the mini text editor.
///
/// Manages text lines, cursor position, undo/redo, and file I/O.
/// Purely a data model: no GUI dependency.

#include "signal.h"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace miniui {

class TextBuffer {
public:
    TextBuffer();
    ~TextBuffer() = default;

    // ── Editing operations ─────────────────────────────────────────────────

    /// Insert a character at the current cursor position.
    void insertChar(char c);

    /// Delete the character before the cursor (backspace behavior).
    void deleteChar();

    /// Insert a newline at the cursor, splitting the current line.
    void newLine();

    // ── Undo / Redo ────────────────────────────────────────────────────────

    void undo();
    void redo();

    // ── Cursor movement ────────────────────────────────────────────────────

    void moveCursor(int lineDelta, int colDelta);
    void setCursor(int line, int col);

    // ── File I/O ───────────────────────────────────────────────────────────

    void loadFile(const std::string& path);
    void saveFile(const std::string& path);

    /// Load content from a string (useful for coroutine-based async load).
    void loadFromString(const std::string& content);

    /// Get all lines joined as a single string (for saving).
    std::string toString() const;

    // ── Query interface ────────────────────────────────────────────────────

    const std::vector<std::string>& lines() const { return lines_; }
    int cursorLine() const { return cursorLine_; }
    int cursorCol()  const { return cursorCol_; }
    bool isModified() const { return modified_; }
    int lineCount() const { return static_cast<int>(lines_.size()); }

    // ── Change notification ────────────────────────────────────────────────

    Signal<>& contentChanged() { return contentChanged_; }

private:
    void pushUndoSnapshot();
    void clampCursor();
    void notifyChanged();

    struct Snapshot {
        std::vector<std::string> lines;
        int cursorLine;
        int cursorCol;
    };

    std::vector<std::string> lines_;
    int cursorLine_ = 0;
    int cursorCol_  = 0;
    bool modified_  = false;

    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;

    Signal<> contentChanged_;
};

} // namespace miniui
