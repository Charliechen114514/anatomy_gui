#pragma once
#include "geometry.h"
#include "observable.h"
#include <memory>
#include <vector>
namespace miniui {
class Painter;
class Widget : public std::enable_shared_from_this<Widget> {
public:
  virtual ~Widget() = default;
  virtual Size measure(Size) = 0;
  void arrange(Rect r) { rect_ = r; }
  virtual void paint(Painter &) = 0;
  Rect rect() const { return rect_; }
  Size size() const { return rect_.size(); }
  Point origin() const { return rect_.origin(); }
  virtual std::shared_ptr<Widget> hitTest(Point pt) {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
      auto &c = *it;
      Point cl = {pt.x - c->rect().x, pt.y - c->rect().y};
      if (c->rect().contains(pt))
        return c->hitTest(cl);
    }
    return shared_from_this();
  }
  // ── Dual interface: virtual methods + Observable events ──
  virtual void onMousePress(Point p, int b) { mousePress_.next(p); }
  virtual void onMouseRelease(Point p, int b) { mouseRelease_.next(p); }
  virtual void onMouseMove(Point p) { mouseMove_.next(p); }

  // ── Observable event sources ──
  Observable<Point> &mousePress() { return mousePress_; }
  Observable<Point> &mouseRelease() { return mouseRelease_; }
  Observable<Point> &mouseMove() { return mouseMove_; }

  void addChild(std::shared_ptr<Widget> c) {
    c->parent_ = this;
    children_.push_back(std::move(c));
  }
  const std::vector<std::shared_ptr<Widget>> &children() const {
    return children_;
  }
  Widget *parent() const { return parent_; }
  virtual void repaint();

protected:
  Rect rect_;
  std::vector<std::shared_ptr<Widget>> children_;
  Widget *parent_ = nullptr;
  Observable<Point> mousePress_;
  Observable<Point> mouseRelease_;
  Observable<Point> mouseMove_;
};
} // namespace miniui
