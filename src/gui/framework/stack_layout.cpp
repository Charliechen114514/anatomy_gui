#include "framework/stack_layout.h"

#include "framework/widget.h"

StackLayout::StackLayout(int padding, int gap) : padding_(padding), gap_(gap) {}

void StackLayout::arrange(Widget& container, const Rect& area) {
    int top = area.top + padding_;

    for (const auto& child : container.children()) {
        const Size& wanted = child->preferred_size();
        const int   kWidth = wanted.width > 0 ? wanted.width : area.width() - (padding_ * 2);

        child->set_bounds(Rect{.left   = area.left + padding_,
                               .top    = top,
                               .right  = area.left + padding_ + kWidth,
                               .bottom = top + wanted.height});
        top += wanted.height + gap_;
    }
}
