#include "text_buffer.h"

#include <algorithm>
#include <cassert>

namespace miniui {

TextBuffer::TextBuffer() {
    lines_.push_back(""); // Always at least one line
}

void TextBuffer::pushUndoSnapshot() {
    Snapshot snap;
    snap.lines = lines_;
    snap.cursorLine = cursorLine_;
    snap.cursorCol  = cursorCol_;
    undoStack_.push_back(std::move(snap));
    // Any new edit clears the redo stack
    redoStack_.clear();
}

void TextBuffer::clampCursor() {
    cursorLine_ = std::clamp(cursorLine_, 0,
                             static_cast<int>(lines_.size()) - 1);
    cursorCol_ = std::clamp(cursorCol_, 0,
                            static_cast<int>(lines_[cursorLine_].size()));
}

void TextBuffer::notifyChanged() {
    contentChanged_();
}

// ── Editing ──────────────────────────────────────────────────────────────────

void TextBuffer::insertChar(char c) {
    pushUndoSnapshot();
    std::string& line = lines_[cursorLine_];
    line.insert(line.begin() + cursorCol_, c);
    ++cursorCol_;
    modified_ = true;
    notifyChanged();
}

void TextBuffer::deleteChar() {
    if (cursorCol_ > 0) {
        // Delete character before cursor on the same line
        pushUndoSnapshot();
        std::string& line = lines_[cursorLine_];
        line.erase(cursorCol_ - 1, 1);
        --cursorCol_;
        modified_ = true;
        notifyChanged();
    } else if (cursorLine_ > 0) {
        // At beginning of line: merge with previous line
        pushUndoSnapshot();
        std::string& prevLine = lines_[cursorLine_ - 1];
        int prevLen = static_cast<int>(prevLine.size());
        prevLine += lines_[cursorLine_];
        lines_.erase(lines_.begin() + cursorLine_);
        --cursorLine_;
        cursorCol_ = prevLen;
        modified_ = true;
        notifyChanged();
    }
}

void TextBuffer::newLine() {
    pushUndoSnapshot();
    std::string& line = lines_[cursorLine_];
    // Split the line at cursor position
    std::string afterCursor = line.substr(cursorCol_);
    line = line.substr(0, cursorCol_);
    lines_.insert(lines_.begin() + cursorLine_ + 1, afterCursor);
    ++cursorLine_;
    cursorCol_ = 0;
    modified_ = true;
    notifyChanged();
}

// ── Undo / Redo ──────────────────────────────────────────────────────────────

void TextBuffer::undo() {
    if (undoStack_.empty()) return;

    // Save current state to redo stack
    Snapshot current;
    current.lines = lines_;
    current.cursorLine = cursorLine_;
    current.cursorCol  = cursorCol_;
    redoStack_.push_back(std::move(current));

    // Restore from undo stack
    Snapshot& snap = undoStack_.back();
    lines_      = std::move(snap.lines);
    cursorLine_ = snap.cursorLine;
    cursorCol_  = snap.cursorCol;
    undoStack_.pop_back();

    // Ensure at least one line
    if (lines_.empty()) {
        lines_.push_back("");
    }
    clampCursor();
    notifyChanged();
}

void TextBuffer::redo() {
    if (redoStack_.empty()) return;

    // Save current state to undo stack
    pushUndoSnapshot();

    // Restore from redo stack (don't clear redo — pushUndoSnapshot already cleared)
    // Actually pushUndoSnapshot clears redo, so we need a different approach
    // Save the redo entry first
    Snapshot snap = std::move(redoStack_.back());
    redoStack_.pop_back();

    // Now push current to undo (manually, without clearing redo)
    Snapshot current;
    current.lines = lines_;
    current.cursorLine = cursorLine_;
    current.cursorCol  = cursorCol_;
    undoStack_.push_back(std::move(current));

    lines_      = std::move(snap.lines);
    cursorLine_ = snap.cursorLine;
    cursorCol_  = snap.cursorCol;

    if (lines_.empty()) {
        lines_.push_back("");
    }
    clampCursor();
    notifyChanged();
}

// ── Cursor movement ──────────────────────────────────────────────────────────

void TextBuffer::moveCursor(int lineDelta, int colDelta) {
    cursorLine_ += lineDelta;
    cursorCol_  += colDelta;
    clampCursor();
    notifyChanged();
}

void TextBuffer::setCursor(int line, int col) {
    cursorLine_ = line;
    cursorCol_  = col;
    clampCursor();
    notifyChanged();
}

// ── File I/O ─────────────────────────────────────────────────────────────────

void TextBuffer::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    lines_.clear();
    std::string line;
    while (std::getline(file, line)) {
        lines_.push_back(line);
    }
    if (lines_.empty()) {
        lines_.push_back("");
    }

    cursorLine_ = 0;
    cursorCol_  = 0;
    modified_   = false;
    undoStack_.clear();
    redoStack_.clear();
    notifyChanged();
}

void TextBuffer::saveFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

    for (size_t i = 0; i < lines_.size(); ++i) {
        file << lines_[i];
        if (i + 1 < lines_.size()) {
            file << '\n';
        }
    }
    modified_ = false;
    notifyChanged();
}

void TextBuffer::loadFromString(const std::string& content) {
    lines_.clear();
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(line);
    }
    if (lines_.empty()) {
        lines_.push_back("");
    }

    cursorLine_ = 0;
    cursorCol_  = 0;
    undoStack_.clear();
    redoStack_.clear();
    notifyChanged();
}

std::string TextBuffer::toString() const {
    std::string result;
    for (size_t i = 0; i < lines_.size(); ++i) {
        result += lines_[i];
        if (i + 1 < lines_.size()) {
            result += '\n';
        }
    }
    return result;
}

} // namespace miniui
