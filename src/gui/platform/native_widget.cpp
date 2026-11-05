#include "platform/native_widget.h"

#include <utility>

NativeWidget::NativeWidget(DWORD extended_style, const wchar_t* class_name, std::wstring text,
                           DWORD style)
    : extended_style_(extended_style),
      class_name_(class_name),
      text_(std::move(text)),
      style_(style) {}

NativeWidget::~NativeWidget() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

bool NativeWidget::create(HWND parent_window, HINSTANCE instance) {
    // NOLINTBEGIN(performance-no-int-to-ptr)
    hwnd_ = CreateWindowExW(extended_style_, class_name_, text_.c_str(), style_, 0, 0, 0, 0,
                            parent_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id())),
                            instance, nullptr);
    // NOLINTEND(performance-no-int-to-ptr)
    if (hwnd_ == nullptr) {
        return false;
    }

    // Controls default to the antique SYSTEM_FONT, which renders Chinese small
    // and cramped. Every native control gets a sane default here, once.
    // Stock objects are shared: never delete them.
    const auto* gui_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

    // Apply whatever bounds the parent already assigned.
    on_bounds_changed();
    return true;
}

void NativeWidget::set_text(const std::wstring& text) {
    text_ = text;
    if (hwnd_ != nullptr) {
        SetWindowTextW(hwnd_, text_.c_str());
    }
}

std::wstring NativeWidget::text() const {
    if (hwnd_ == nullptr) {
        return text_;
    }

    const int    kLength = GetWindowTextLengthW(hwnd_);
    std::wstring result(static_cast<size_t>(kLength) + 1, L'\0');
    const int    kCopied = GetWindowTextW(hwnd_, result.data(), kLength + 1);
    result.resize(static_cast<size_t>(kCopied));
    return result;
}

void NativeWidget::on_bounds_changed() {
    if (hwnd_ == nullptr) {
        return;
    }

    const Rect& box = bounds();
    SetWindowPos(hwnd_, nullptr, box.left, box.top, box.width(), box.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}
