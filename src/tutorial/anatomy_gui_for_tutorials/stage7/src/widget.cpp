#include "widget.h"
#include "application.h"
#include <algorithm>
namespace miniui {
std::shared_ptr<Widget> Widget::hitTest(Point pt) {
    for(auto it=children_.rbegin(); it!=children_.rend(); ++it){
        auto& child=*it;
        Point cl={pt.x-child->rect().x, pt.y-child->rect().y};
        if(child->rect().contains(pt)) return child->hitTest(cl);
    }
    return shared_from_this();
}
void Widget::addChild(std::shared_ptr<Widget> child){ child->parent_=this; children_.push_back(std::move(child)); }
void Widget::removeChild(const std::shared_ptr<Widget>& child){
    auto it=std::find(children_.begin(),children_.end(),child);
    if(it!=children_.end()){(*it)->parent_=nullptr; children_.erase(it);}
}
void Widget::repaint(){
    // Walk up to root, then notify Application to schedule a repaint
    Widget* root = this;
    while (root->parent_) root = root->parent_;
    root->markDirty();
    Application::instance().markDirty(root->rect());
}
} // namespace miniui
