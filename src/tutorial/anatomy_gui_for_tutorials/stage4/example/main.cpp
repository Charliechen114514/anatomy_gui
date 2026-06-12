/// Stage 4 demo — Widget Tree + RAII + hitTest
///
/// Creates a window with:
///   - A light-blue background ColorRect (root widget)
///   - "Hello"  Label at top-left
///   - "MiniUI" Label at bottom-right
///
/// Click anywhere → prints which widget was hit.
/// Resize → background rescales.
/// Press any key → quit.

#include "color_rect.h"
#include "geometry.h"
#include "label.h"
#include "root_window.h"
#include "widget.h"

#include <cstdio>

using namespace miniui;

/// A background widget that prints hit-test results.
class Background : public ColorRect {
public:
    Background() : ColorRect(Color::rgb(0.85, 0.9, 0.95)) {}

    bool onMousePress(Point p) override {
        std::printf("[hitTest] Background clicked at (%d, %d)\n", p.x, p.y);
        return true;
    }
};

int main() {
    RootWindow win(800, 600);

    // ── Build the widget tree ──
    auto bg = std::make_unique<Background>();

    auto hello = std::make_unique<Label>("Hello");
    hello->setColor(Color::rgb(0, 0, 0));
    hello->setFontSize(24.0);
    hello->setBounds({20, 20, 200, 40});

    auto miniuiLabel = std::make_unique<Label>("MiniUI");
    miniuiLabel->setColor(Color::rgb(0.2, 0.4, 0.8));
    miniuiLabel->setFontSize(28.0);
    miniuiLabel->setBounds({600, 530, 200, 50});

    bg->addChild(std::move(hello));
    bg->addChild(std::move(miniuiLabel));
    win.setRoot(std::move(bg));

    std::printf("Stage 4 — Widget Tree demo\n");
    std::printf("Click to test hit detection. Press Escape to quit.\n");

    return win.eventLoop();
}
