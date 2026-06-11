#pragma once
/// @file widget.h
/// @brief Base widget class for MiniUI widget tree.

#include "geometry.h"
#include "signal.h"

#include <cstdint>
#include <vector>
#include <xcb/xcb.h>

namespace miniui {

class Painter;

/// Base class for all widgets in the MiniUI widget tree.
class Widget {
public:
    explicit Widget(Widget* parent = nullptr);
    virtual ~Widget();

    // Non-copyable
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // ── Tree structure ────────────────────────────────────────────────────────

    Widget* parent() const { return parent_; }
    const std::vector<Widget*>& children() const { return children_; }
    void addChild(Widget* child);
    void removeChild(Widget* child);

    // ── Geometry ──────────────────────────────────────────────────────────────

    Point pos() const { return pos_; }
    Size  size() const { return size_; }
    Rect  bounds() const { return Rect(pos_.x, pos_.y, size_.width, size_.height); }

    Size  desiredSize() const { return desiredSize_; }
    void setPos(Point p) { pos_ = p; }
    void setSize(Size s) { size_ = s; }

    /// Position relative to the root window (absolute).
    Point mapToRoot(Point local) const;

    // ── Layout protocol ───────────────────────────────────────────────────────

    /// Measure phase: compute the desired size given available space.
    virtual Size measure(Size available);

    /// Arrange phase: position children within the allocated rect.
    virtual void arrange(Rect allocated);

    // ── Rendering ─────────────────────────────────────────────────────────────

    /// Paint this widget and all children.
    void paint(Painter& painter);

    /// Override to draw custom content.
    virtual void onPaint(Painter& painter);

    // ── Events ────────────────────────────────────────────────────────────────

    /// Dispatch a key press event. Returns true if consumed.
    virtual bool onKeyEvent(xcb_keycode_t keycode);

    /// Hit-test: find the deepest child that contains the given local point.
    virtual Widget* hitTest(Point local);

    // ── Dirty marking ─────────────────────────────────────────────────────────

    void markDirty();
    bool isDirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    /// Schedule a repaint on the next frame.
    void repaint();

    /// Schedule a layout pass then repaint.
    void update();

    // ── Visibility ────────────────────────────────────────────────────────────

    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; markDirty(); }

protected:
    Widget* parent_ = nullptr;
    std::vector<Widget*> children_;

    Point pos_{0, 0};
    Size  size_{0, 0};
    Size  desiredSize_{0, 0};

    bool dirty_   = true;
    bool visible_ = true;
};

} // namespace miniui
