#pragma once
#include "geometry.h"
#include "widget.h"
#include <vector>
namespace miniui {
template<typename L>
concept LayoutStrategy = requires(const std::vector<std::shared_ptr<Widget>>& ch, Rect avail) {
    { L::apply(ch, avail) } -> std::same_as<void>;
};
struct HBoxLayout {
    static void apply(const std::vector<std::shared_ptr<Widget>>& ch, Rect a) {
        if(ch.empty()) return;
        int n=static_cast<int>(ch.size());
        int sw=a.width/n;
        int x=a.x;
        for(auto& c:ch){ c->measure({sw,a.height}); c->arrange({x,a.y,sw,a.height}); x+=sw; }
    }
};
struct VBoxLayout {
    static void apply(const std::vector<std::shared_ptr<Widget>>& ch, Rect a) {
        if(ch.empty()) return;
        int n=static_cast<int>(ch.size());
        int sh=a.height/n;
        int y=a.y;
        for(auto& c:ch){ c->measure({a.width,sh}); c->arrange({a.x,y,a.width,sh}); y+=sh; }
    }
};
} // namespace miniui
