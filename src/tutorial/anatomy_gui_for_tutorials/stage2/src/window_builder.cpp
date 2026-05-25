#include "window_builder.h"
#include "application.h"
#include "platform/application_xcb.h"
#include "platform/window_xcb.h"
#include <memory>

namespace anatomy_gui::gui {

WindowBuilder::WindowBuilder(WindowPolicy p) : policy_(p) {}

WindowBuilder& WindowBuilder::setSize(base::Size size) {
    size_ = size;
    return *this;
}

WindowBuilder& WindowBuilder::setPosition(base::Point tl) {
    position_ = tl;
    return *this;
}

std::unique_ptr<Window> WindowBuilder::create_window() {
    auto* xcb_internal = dynamic_cast<XcbApplicationInternal*>(
        Application::app().private_handle());
    if (!xcb_internal)
        return nullptr;

    auto window_handle = xcb_internal->create_window_handle();
    auto window_internal = std::make_unique<XcbWindowInternal>(
        xcb_internal->get_connection(), window_handle);

    auto result =
        std::unique_ptr<Window>(new Window(std::move(window_internal)));

    result->setSize(size_);
    result->setPosition(position_);

    return result;
}

} // namespace anatomy_gui::gui
