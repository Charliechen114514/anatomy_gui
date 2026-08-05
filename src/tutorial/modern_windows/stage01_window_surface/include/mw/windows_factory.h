#pragma once
#include "define.h"
#include "platform/error.h"
#include "window.h"
#include <memory>
#include <string>
#include <string_view>

namespace mw {

class WindowsObserver; // fwd —— observer 由外面实现，factory 只捏着指针传进建窗调用

struct WindowFactory {
    Result<std::unique_ptr<Window>> make();

    WindowFactory& setTitle(std::wstring_view window_title);
    WindowFactory& setSize(Size_t window_size);
    WindowFactory& setPosition(Point_t position);
    WindowFactory& setObserver(WindowsObserver* observer);

  private:
    std::wstring     title_    = L"ModernWindows";
    Size_t           size_     {960, 600};
    Point_t          pos_      {0, 0};
    WindowsObserver* observer_ = nullptr;
};

} // namespace mw
