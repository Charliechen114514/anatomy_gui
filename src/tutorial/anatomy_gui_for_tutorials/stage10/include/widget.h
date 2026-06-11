#pragma once

#include "geometry.h"
#include "observable.h"
#include "painter.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace miniui {

class Painter;

/// Base class for all UI widgets.
/// Provides: measure/arrange layout protocol, paint, hit-testing, mouse
/// events (virtual + Observable), repaint request, update lifecycle.
class Widget : public std::enable_shared_from_this<Widget> {
public:
    Widget();
    virtual ~Widget();

    // Non-copyable
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // --- Identity / tree ---
    using WidgetId = uint64_t;
    WidgetId id() const { return id_; }

    Widget* parent() const { return parent_; }
    void setParent(Widget* p) { parent_ = p; }

    // --- Geometry ---
    const Rect& frame()    const { return frame_; }
    const Rect& bounds()   const { return bounds_; }   // layout-computed

    void setFrame(const Rect& r) { frame_ = r; }
    void setBounds(const Rect& b);

    double x() const { return frame_.x; }
    double y() const { return frame_.y; }
    double width()  const { return frame_.width; }
    double height() const { return frame_.height; }

    // --- Visibility ---
    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    // --- Layout protocol ---
    /// Called by parent to ask this widget how big it wants to be.
    virtual Size measure(const Size& available);
    /// Called by parent to assign final position and size.
    virtual void arrange(const Rect& rect);

    // --- Painting ---
    virtual void paint(Painter& p) = 0;

    // --- Hit testing ---
    virtual Widget* hitTest(const Point& pt);

    // --- Mouse events (virtual, for subclass override) ---
    virtual void onMousePress(Point p);
    virtual void onMouseMove(Point p);
    virtual void onMouseRelease(Point p);

    // --- Observable event sources (for external composition) ---
    Observable<Point>& mousePressObservable()   { return mousePress_; }
    Observable<Point>& mouseMoveObservable()    { return mouseMove_; }
    Observable<Point>& mouseReleaseObservable() { return mouseRelease_; }

    // --- Repaint / update ---
    /// Request a repaint.  Propagates to the root window.
    void repaint();
    /// Mark layout as dirty; the next layout pass will re-measure/arrange.
    void update();

    /// Returns the root-most widget (the window).
    Widget* root();

protected:
    Rect frame_;
    Rect bounds_;         // assigned by layout
    bool visible_ = true;

    Observable<Point> mousePress_;
    Observable<Point> mouseMove_;
    Observable<Point> mouseRelease_;

private:
    WidgetId id_;
    Widget* parent_ = nullptr;

    static WidgetId nextId();
};

} // namespace miniui
