#pragma once
#include "application_handle.h"
#include "base/weak_ptr/weak_ptr.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace anatomy_gui::gui {
class Window;

class Application {
  public:
    explicit Application(int argc, char** argv);
    static Application& app() noexcept;

    // Runs the Eventloop
    int execution();

    ApplicationInternal* private_handle() const { return handle.get(); }

  private:
    void add_window(base::WeakPtr<Window> window);
    void remove_window(base::WeakPtr<Window> window);

  private:
    std::unique_ptr<ApplicationInternal> handle;

  private:
    Application(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};
} // namespace anatomy_gui::gui
