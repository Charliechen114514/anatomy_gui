#include <memory>
#include <utility>

#include "framework/event_router.h"
#include "framework/property.h"
#include "framework/stack_layout.h"
#include "platform/native_widget.h"
#include "platform/window.h"

namespace {

// A leaf that owns the text it paints. Wiring the invalidation inside the
// node is what removes pain point 5: forgetting the repaint stops being
// something a caller can do.
class DrawnLine : public Widget {
public:
    DrawnLine() {
        text.on_changed([this] { invalidate(); });
    }

    Property<std::wstring> text;
};

// The same demo as the concrete pass, with the controls moved into a widget
// tree. Pain point 2 is gone: a control is one node now, and its id, its
// native window and its teardown all follow from that node.
class DemoWindow : public Window {
public:
    void create_controls(HINSTANCE instance);

protected:
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    // One line per control: no id constant, no CreateWindowExW, no teardown.
    NativeWidget& add_native(DWORD extended_style, const wchar_t* class_name, std::wstring text,
                             DWORD style);

    void layout_controls();
    void on_button_clicked();
    void paint();

    HINSTANCE   instance_ = nullptr;
    Widget      root_;
    EventRouter router_;

    NativeWidget* edit_   = nullptr;
    NativeWidget* button_ = nullptr;
    NativeWidget* label_  = nullptr;

    // A leaf that owns no window: the tree holds it, the layout gives it a
    // rectangle, and paint() draws it. Two kinds of node in one tree.
    DrawnLine* drawn_ = nullptr;
};

}  // namespace

NativeWidget& DemoWindow::add_native(DWORD extended_style, const wchar_t* class_name,
                                     std::wstring text, DWORD style) {
    auto widget =
        std::make_unique<NativeWidget>(extended_style, class_name, std::move(text), style);

    NativeWidget& node = *widget;
    root_.add_child(std::move(widget));
    node.create(handle(), instance_);
    return node;
}

void DemoWindow::create_controls(HINSTANCE instance) {
    instance_ = instance;

    edit_   = &add_native(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL);
    button_ = &add_native(0, L"BUTTON", L"显示", WS_CHILD | WS_VISIBLE);
    label_  = &add_native(0, L"STATIC", L"（系统画的 STATIC：会自己更新）", WS_CHILD | WS_VISIBLE);

    // What the button does lives next to where the button is created, not in
    // a switch far below. Adding a control and teaching it to react are now
    // the same edit.
    button_->set_handler([this](Event& event) {
        if (event.kind() != EventKind::Click) {
            return false;
        }
        on_button_clicked();
        return true;
    });
    auto drawn = std::make_unique<DrawnLine>();
    drawn_     = drawn.get();
    root_.add_child(std::move(drawn));

    // Each node declares how big it wants to be; the layout decides where.
    edit_->set_preferred_size(Size{.width = 0, .height = 28});
    button_->set_preferred_size(Size{.width = 120, .height = 36});
    label_->set_preferred_size(Size{.width = 0, .height = 32});
    drawn_->set_preferred_size(Size{.width = 0, .height = 32});

    // One line describes the whole arrangement: twenty pixels of padding,
    // eight between neighbours.
    root_.set_layout(std::make_unique<StackLayout>(20, 8));

    drawn_->text.set(L"（这行字是本程序自己画的，还没被更新过）");

    // The root owns the hook that turns "somebody wants a repaint" into a
    // real invalidate on the window.
    root_.set_repaint_handler([this] { invalidate(); });

    layout_controls();
}

void DemoWindow::layout_controls() {
    // No arithmetic is left: hand the root the client area and let the layout
    // walk the tree. The ordering trap is gone with it - a node without a
    // native window simply has nothing to move yet.
    RECT client{};
    GetClientRect(handle(), &client);

    root_.set_bounds(Rect{.left   = 0,
                          .top    = 0,
                          .right  = client.right - client.left,
                          .bottom = client.bottom - client.top});
}

void DemoWindow::on_button_clicked() {
    const std::wstring kText = edit_->text();

    // Output 1: a system-drawn STATIC. SetWindowTextW invalidates the control
    // itself, so the new text appears with no help from here.
    label_->set_text(kText);

    // Output 2: the line painted by paint(). The node owns its text, so
    // writing it also schedules the repaint. Forgetting was the whole bug.
    drawn_->text.set(kText);
}

void DemoWindow::paint() {
    PAINTSTRUCT paint_state{};
    HDC         window_dc = BeginPaint(handle(), &paint_state);

    // Compose the frame off screen and blit it in one go; painting straight
    // into the window DC leaves a visible gap between the two steps.
    RECT client{};
    GetClientRect(handle(), &client);
    HDC     buffer_dc = CreateCompatibleDC(window_dc);
    HBITMAP buffer    = CreateCompatibleBitmap(window_dc, client.right, client.bottom);
    HGDIOBJ previous  = SelectObject(buffer_dc, buffer);

    // WM_ERASEBKGND refuses to erase, so the frame starts here.
    // NOLINTBEGIN(performance-no-int-to-ptr)
    FillRect(buffer_dc, &client, reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)));
    // NOLINTEND(performance-no-int-to-ptr)

    // The framework rectangle becomes a Win32 RECT here and only here: this is
    // the seam between the two layers.
    const Rect& node_bounds = drawn_->bounds();
    RECT        native_bounds{.left   = node_bounds.left,
                              .top    = node_bounds.top,
                              .right  = node_bounds.right,
                              .bottom = node_bounds.bottom};

    SetBkMode(buffer_dc, TRANSPARENT);
    DrawTextW(buffer_dc, drawn_->text.value().c_str(), -1, &native_bounds,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    BitBlt(window_dc, 0, 0, client.right, client.bottom, buffer_dc, 0, 0, SRCCOPY);

    SelectObject(buffer_dc, previous);
    DeleteObject(buffer);
    DeleteDC(buffer_dc);

    EndPaint(handle(), &paint_state);
}

LRESULT DemoWindow::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_COMMAND:
        // The window procedure is a translation layer now, not a decision
        // maker: turn the Win32 notification into a framework event and let
        // the router find somebody willing to handle it.
        if (HIWORD(wparam) == BN_CLICKED) {
            if (Widget* target = root_.find_by_id(LOWORD(wparam)); target != nullptr) {
                Event event(EventKind::Click, target);
                if (router_.dispatch(event)) {
                    return 0;
                }
            }
        }
        break;

    case WM_SIZE:
        // Nothing else moves the controls. layout_controls reads the fresh
        // client size straight from the window.
        if (wparam != SIZE_MINIMIZED) {
            layout_controls();
        }
        return 0;

    case WM_ERASEBKGND:
        // Claim the erase and do nothing: paint() owns the whole client area.
        return 1;

    case WM_PAINT:
        paint();
        return 0;

    case WM_DPICHANGED: {
        // Windows suggests a new rectangle and expects SetWindowPos back. The
        // WM_SIZE that follows re-runs layout_controls.
        // NOLINTBEGIN(performance-no-int-to-ptr)
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        // NOLINTEND(performance-no-int-to-ptr)
        SetWindowPos(handle(), nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    default:
        break;
    }

    // Everything else, WM_DESTROY included, belongs to the base class.
    return Window::handle_message(msg, wparam, lparam);
}

// Naming exemption: the parameter names follow the winbase.h declaration and
// MSDN.
// NOLINTBEGIN(readability-identifier-naming)
int WINAPI wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                    [[maybe_unused]] PWSTR pCmdLine, int nShowCmd) {
    // NOLINTEND(readability-identifier-naming)
    DemoWindow window;
    if (!window.create(hInstance, L"Anatomy GUI · 抽象 2/5", 720, 480)) {
        return 1;
    }
    window.create_controls(hInstance);
    window.show(nShowCmd);

    // The message loop stays in the application: it is fifteen lines and it is
    // the clearest thing in the file.
    MSG msg{};
    while (true) {
        const BOOL kResult = GetMessageW(&msg, nullptr, 0, 0);
        if (kResult == 0) {
            break;  // WM_QUIT
        }
        if (kResult == -1) {
            return 1;  // failure
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
