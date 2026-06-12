#pragma once
/// @file widget.h — Stage 5 variant with measure/arrange support.

#include "geometry.h"
#include <memory>
#include <vector>

namespace miniui {

class Painter;

class Widget {
public:
    virtual ~Widget() = default;

    Rect  bounds() const    { return bounds_; }
    void  setBounds(Rect b) { bounds_ = b; }
    bool  visible() const   { return visible_; }
    void  setVisible(bool v){ visible_ = v; }
    Widget* parent() const  { return parent_; }
    void    setParent(Widget* p) { parent_ = p; }

    // ── Measurement ────────────────────────────────────
    virtual Size measure(Size /*available*/) { return {0, 0}; }
    virtual void arrange(Rect finalRect)     { bounds_ = finalRect; }

    // ── Drawing ───────────────────────────────────────
    virtual void paint(Painter& painter);

    // ── Hit testing ───────────────────────────────────
    virtual Widget* hitTest(Point localPos);

    // ── Input events ──────────────────────────────────
    virtual bool onMousePress(Point)   { return false; }
    virtual bool onMouseRelease(Point) { return false; }
    virtual bool onMouseMove(Point)    { return false; }
    virtual bool onKeyPress(int)       { return false; }

protected:
    Widget*                              parent_ = nullptr;
    Rect                                 bounds_;
    bool                                 visible_ = true;
    // Note: Container manages its own children; base Widget has none.
};

} // namespace miniui
