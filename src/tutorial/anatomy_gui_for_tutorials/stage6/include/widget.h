#pragma once
/// @file widget.h
/// @brief Base Widget class for the MiniUI widget tree.

#include "geometry.h"

#include <memory>
#include <vector>

namespace miniui {

class Painter;

class Widget : public std::enable_shared_from_this<Widget> {
public:
    virtual ~Widget() = default;

    // ── Layout protocol ───────────────────────────────────────────────────
    virtual Size measure(Size available) = 0;
    void arrange(Rect rect) { rect_ = rect; }
    virtual void paint(Painter& painter) = 0;

    // ── Geometry access ───────────────────────────────────────────────────
    Rect  rect()   const { return rect_; }
    Size  size()   const { return rect_.size(); }
    Point origin() const { return rect_.origin(); }

    // ── Hit testing ───────────────────────────────────────────────────────
    virtual std::shared_ptr<Widget> hitTest(Point pt);

    // ── Mouse events (local-space coordinates) ────────────────────────────
    virtual void onMousePress(Point pos, int button)   { (void)pos; (void)button; }
    virtual void onMouseRelease(Point pos, int button)  { (void)pos; (void)button; }
    virtual void onMouseMove(Point pos)                  { (void)pos; }

    // ── Widget tree ───────────────────────────────────────────────────────
    virtual void addChild(std::shared_ptr<Widget> child);
    virtual void removeChild(const std::shared_ptr<Widget>& child);
    const std::vector<std::shared_ptr<Widget>>& children() const { return children_; }
    Widget* parent() const { return parent_; }

    // ── Repaint request ───────────────────────────────────────────────────
    virtual void repaint();

protected:
    Rect rect_;
    std::vector<std::shared_ptr<Widget>> children_;
    Widget* parent_ = nullptr;
};

} // namespace miniui
