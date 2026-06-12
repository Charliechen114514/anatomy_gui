#pragma once
#include "geometry.h"
#include "widget.h"
#include <memory>
namespace miniui {
/// RootWindow — thin wrapper that holds the root widget for the Application.
/// In Stage 7, Application manages the XCB connection and event loop,
/// so RootWindow is simplified to just hold the widget tree root.
class RootWindow {
public:
    RootWindow() = default;
    void setRoot(std::shared_ptr<Widget> root) { root_ = std::move(root); }
    std::shared_ptr<Widget> root()const{ return root_; }
private:
    std::shared_ptr<Widget> root_;
};
} // namespace miniui
