#pragma once
/// @file text_canvas.h
/// @brief TextCanvas — self-drawn text editing area widget.

#include "widget.h"
#include "signal.h"

#include <string>
#include <vector>
#include <xcb/xcb.h>

namespace miniui {

/// A widget that renders text lines and a blinking cursor using Cairo.
/// Captures keyboard events and emits signals for the presenter to handle.
class TextCanvas : public Widget {
public:
    explicit TextCanvas(Widget* parent = nullptr);
    ~TextCanvas() override;

    // ── Display interface (called by Presenter) ────────────────────────────

    void setLines(const std::vector<std::string>& lines);
    void setCursor(int line, int col);

    // ── Signals (connected by Presenter) ───────────────────────────────────

    Signal<xcb_keycode_t>& keyPressed() { return keyPressed_; }

    // ── Widget overrides ───────────────────────────────────────────────────

    Size measure(Size available) override;
    void onPaint(Painter& painter) override;
    bool onKeyEvent(xcb_keycode_t keycode) override;

private:
    /// Compute the fixed line height from font metrics.
    int lineHeight() const;

    /// Compute the width of the line number gutter.
    int gutterWidth() const;

    /// Clamp scroll offset so the cursor is visible.
    void ensureCursorVisible();

    Signal<xcb_keycode_t> keyPressed_;

    std::vector<std::string> displayLines_;
    int cursorLine_ = 0;
    int cursorCol_  = 0;

    // Scroll / viewport
    int scrollY_ = 0; // vertical pixel offset

    // Font settings
    static constexpr double kFontSize = 14.0;
    static constexpr const char* kFontFamily = "monospace";

    // Derived font metrics (computed on first paint)
    mutable double fontAscent_  = 0;
    mutable double fontDescent_ = 0;
    mutable double fontHeight_  = 0;
    mutable bool   fontMetricsCached_ = false;

    void cacheFontMetrics(Painter& painter) const;
};

} // namespace miniui
