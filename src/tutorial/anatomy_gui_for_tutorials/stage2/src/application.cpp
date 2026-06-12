#include "application.h"
#include "application_handle.h"
#include "platform/application_xcb.h"
#include <atomic>
#include <memory>

namespace anatomy_gui::gui {
std::atomic<Application*> global_app = nullptr; // Global App
Application& Application::app() noexcept {
    return *global_app; // Force Crash
}

/* Leave blank */
Application::Application(int argc, char** argv) {
    /* Activate this first */
    global_app = this;
    /* Set the XCB Directly */
    handle = std::make_unique<XcbApplicationInternal>(argc, argv);
}

int Application::execution() {
    /* Redirect to the App Internal */
    return handle->execution();
}

} // namespace anatomy_gui::gui