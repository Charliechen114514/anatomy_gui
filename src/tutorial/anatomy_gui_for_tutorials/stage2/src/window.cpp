#include "window.h"
#include <memory>

namespace anatomy_gui::gui {

Window::Window(std::unique_ptr<WindowInternal> internal)
    : internal_handle(std::move(internal)), factory_(this) {}

void Window::show() { internal_handle->show(); }

void Window::setSize(base::Size size) { internal_handle->setSize(size); }

void Window::setPosition(base::Point point) {
    internal_handle->setPosition(point);
}

} // namespace anatomy_gui::gui
