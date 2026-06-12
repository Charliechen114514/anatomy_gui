#include "drag_canvas.h"
#include "painter.h"

namespace miniui {

DragCanvas::DragCanvas() {
    setupDragDrawing();
}

Size DragCanvas::measure(Size available) { return available; }

void DragCanvas::paint(Painter& painter) {
    // White background
    painter.setSourceRGB(1.0, 1.0, 1.0);
    painter.rectangle(0, 0, rect().width, rect().height);
    painter.fill();

    // Draw all strokes
    painter.setSourceRGB(0.0, 0.0, 0.0);
    painter.setLineWidth(2.0);
    for (const auto& stroke : strokes_) {
        if (stroke.size() < 2) continue;
        painter.moveTo(stroke[0].x, stroke[0].y);
        for (size_t i = 1; i < stroke.size(); ++i) {
            painter.lineTo(stroke[i].x, stroke[i].y);
        }
        painter.stroke();
    }

    // Draw current stroke in blue
    if (currentStroke_ && currentStroke_->size() >= 2) {
        painter.setSourceRGB(0.2, 0.4, 0.8);
        painter.setLineWidth(2.5);
        painter.moveTo((*currentStroke_)[0].x, (*currentStroke_)[0].y);
        for (size_t i = 1; i < currentStroke_->size(); ++i) {
            painter.lineTo((*currentStroke_)[i].x, (*currentStroke_)[i].y);
        }
        painter.stroke();
    }
}

void DragCanvas::setupDragDrawing() {
    // On mousePress: start a new stroke
    mousePress_.subscribe([this](Point start) {
        currentStroke_ = std::make_shared<std::vector<Point>>();
        currentStroke_->push_back(start);

        // Subscribe to mouseMove for this drag session
        // Store the unsubscribe callable so we can clean it up on release
        auto moveUnsub = mouseMove_.subscribe([this](Point pos) {
            if (currentStroke_) {
                currentStroke_->push_back(pos);
                repaint();
            }
        });

        // On mouseRelease: finalize stroke, unsubscribe from move,
        // and return an unsubscribe for the release subscription itself.
        auto releaseUnsub = mouseRelease_.subscribe(
            [this, moveUnsub = std::move(moveUnsub)](Point) mutable {
                if (currentStroke_ && currentStroke_->size() >= 2) {
                    strokes_.push_back(*currentStroke_);
                }
                currentStroke_.reset();
                moveUnsub();  // unsubscribe from move
                repaint();
            });

        // Store the release unsubscribe so it can be cleaned up
        // after this drag session completes (prevents accumulation)
        pendingReleaseUnsub_ = std::move(releaseUnsub);
    });
}

} // namespace miniui
