/// Stage 9 demo — Observable<T> with Rx operators + DragCanvas.
///
/// A white canvas where you can draw lines by clicking and dragging.
/// Demonstrates Observable composition for drag gestures:
///   mousePress → start stroke
///   mouseMove (during drag) → extend stroke
///   mouseRelease → end stroke
///
/// Also demonstrates filter: left-click draws, right-click erases last stroke.
/// Press Escape to quit.

#include "application.h"
#include "drag_canvas.h"
#include "geometry.h"
#include "observable.h"
#include "widget.h"

#include <cstdio>
#include <memory>

using namespace miniui;

int main() {
    auto& app = Application::instance();
    app.init(800, 600);

    auto canvas = std::make_shared<DragCanvas>();
    app.setRoot(canvas);

    std::printf("Stage 9 — Observable + Rx Pattern + Drag Canvas\n");
    std::printf("Click and drag to draw. Press Escape to quit.\n");

    return app.run();
}
