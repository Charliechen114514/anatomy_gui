#pragma once

#include "widget.h"

#include <vector>

namespace miniui {

/// The top-level root window widget.  Owns the child widget tree and
/// delegates painting and events to children.
class RootWindow : public Widget {
public:
    RootWindow() = default;
    ~RootWindow() override = default;

    /// Set the single content child.
    void setContent(std::shared_ptr<Widget> child);

    // --- Widget overrides ---
    Size measure(const Size& available) override;
    void arrange(const Rect& rect) override;
    void paint(Painter& p) override;
    Widget* hitTest(const Point& pt) override;
    void repaint();

    /// Set by Application to trigger a repaint from the GUI thread.
    std::function<void()> onRepaintRequested;

private:
    std::shared_ptr<Widget> content_;
};

} // namespace miniui
