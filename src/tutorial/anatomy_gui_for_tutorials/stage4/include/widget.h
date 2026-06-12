#pragma once
/// @file widget.h
/// @brief Widget base class — the node type of the MiniUI widget tree.

#include "geometry.h"

#include <memory>
#include <vector>

namespace miniui {

class Painter;

/// Base class for all UI elements.
///
/// Each widget occupies a rectangular region (bounds_) relative to its parent.
/// The widget tree follows strict parent-owns-children ownership via
/// std::unique_ptr<Widget>.
class Widget {
public:
    virtual ~Widget() = default;

    // ── Geometry ──────────────────────────────────────
    Rect  bounds() const      { return bounds_; }
    void  setBounds(Rect b)   { bounds_ = b; }
    bool  visible() const     { return visible_; }
    void  setVisible(bool v)  { visible_ = v; }

    // ── Tree ──────────────────────────────────────────
    Widget* parent() const { return parent_; }
    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

    /// Takes ownership of @p child. If the child already has a parent, it is
    /// removed from the old parent first.
    void addChild(std::unique_ptr<Widget> child);

    /// Remove and return the child at @p index. Returns nullptr if out of range.
    std::unique_ptr<Widget> removeChild(size_t index);

    // ── Measurement protocol (Stage 4 stub — Stage 5 fills in) ──
    virtual Size measure(Size /*available*/) { return {0, 0}; }
    virtual void arrange(Rect finalRect) { bounds_ = finalRect; }

    // ── Drawing ───────────────────────────────────────
    /// Paint this widget. The Painter's origin is already translated to
    /// this widget's top-left corner by the caller.
    virtual void paint(Painter& painter);

    // ── Hit testing ───────────────────────────────────
    /// Given a point in THIS widget's local coordinate space, return the
    /// deepest widget that contains that point (may be a descendant or this).
    virtual Widget* hitTest(Point localPos);

    // ── Input events (return true = handled) ───────────
    virtual bool onMousePress(Point /*localPos*/)   { return false; }
    virtual bool onMouseRelease(Point /*localPos*/) { return false; }
    virtual bool onMouseMove(Point /*localPos*/)    { return false; }
    virtual bool onKeyPress(int /*keyCode*/)        { return false; }

protected:
    Widget*                                 parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>>    children_;
    Rect                                    bounds_;
    bool                                    visible_ = true;
};

} // namespace miniui
