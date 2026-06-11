// Stage 5 Demo — Layout Engine with C++20 Concepts
//
// Three color blocks in an HBoxLayout. Resize to see relayout. Any key quits.

#include "color_widget.h"
#include "container.h"
#include "layout_strategy.h"
#include "root_window.h"

#include <cstdio>
#include <memory>

using namespace miniui;

int main() {
    std::printf("=== MiniUI Stage 5: Layout Engine Demo ===\n");
    std::printf("Three color blocks in HBoxLayout. Resize to relayout. Any key quits.\n\n");

    RootWindow window(800, 600);

    auto root = std::make_unique<Container<HBoxLayout>>();
    root->addChild(std::make_unique<ColorWidget>(Color::rgb(0.90, 0.20, 0.20)));
    root->addChild(std::make_unique<ColorWidget>(Color::rgb(0.20, 0.80, 0.20)));
    root->addChild(std::make_unique<ColorWidget>(Color::rgb(0.20, 0.40, 0.90)));

    window.setRoot(std::move(root));
    return window.eventLoop();
}
