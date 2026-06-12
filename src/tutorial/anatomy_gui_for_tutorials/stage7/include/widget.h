#pragma once
#include "geometry.h"
#include <memory>
#include <vector>
namespace miniui {
class Painter;
class Widget : public std::enable_shared_from_this<Widget> {
public:
    virtual ~Widget() = default;
    virtual Size measure(Size available) = 0;
    void arrange(Rect r) { rect_ = r; }
    virtual void paint(Painter& painter) = 0;
    Rect  rect()const{ return rect_; }
    Size  size()const{ return rect_.size(); }
    Point origin()const{ return rect_.origin(); }
    virtual std::shared_ptr<Widget> hitTest(Point pt);
    virtual void onMousePress(Point,int){}
    virtual void onMouseRelease(Point,int){}
    virtual void onMouseMove(Point){}
    virtual void addChild(std::shared_ptr<Widget> child);
    virtual void removeChild(const std::shared_ptr<Widget>& child);
    const std::vector<std::shared_ptr<Widget>>& children()const{ return children_; }
    Widget* parent()const{ return parent_; }
    virtual void repaint();
    void markDirty() { dirty_ = true; }
    bool isDirty()const{ return dirty_; }
    void clearDirty() { dirty_ = false; }
protected:
    Rect rect_;
    std::vector<std::shared_ptr<Widget>> children_;
    Widget* parent_ = nullptr;
    bool dirty_ = false;
};
} // namespace miniui
