#pragma once
/// @file drag_canvas.h — DragCanvas: drawing canvas using Observable composition.

#include "geometry.h"
#include "widget.h"
#include <vector>

namespace miniui {

class DragCanvas : public Widget {
public:
    DragCanvas();

    Size measure(Size available) override;
    void paint(Painter& painter) override;

private:
    void setupDragDrawing();

    std::vector<std::vector<Point>> strokes_;
    std::shared_ptr<std::vector<Point>> currentStroke_;
    std::function<void()> pendingReleaseUnsub_; ///< cleans up the release subscription after each drag
};

} // namespace miniui
