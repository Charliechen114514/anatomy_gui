#pragma once
#include "base/point.hpp"
#include "base/size.h"
#include "base/weak_ptr/weak_ptr.h"
#include "base/weak_ptr/weak_ptr_factory.h"
#include "window_handle.h"
#include <memory>

namespace anatomy_gui::gui {

class Window {
  public:
    friend class WindowBuilder;
    void setSize(base::Size size);
    void setPosition(base::Point point);
    void show();

    base::WeakPtr<Window> GetSelf() noexcept { return factory_.GetWeakPtr(); }

  private:
    Window(std::unique_ptr<WindowInternal> internal);

  private:
    std::unique_ptr<WindowInternal> internal_handle;
    base::WeakPtrFactory<Window> factory_;
};

} // namespace anatomy_gui::gui
