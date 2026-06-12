#pragma once
/// @file editor_view.h
/// @brief EditorView — VBoxLayout of MenuBar, TextCanvas, StatusBar.

#include "container.h"
#include "menu_bar.h"
#include "text_canvas.h"
#include "status_bar.h"

namespace miniui {

/// The main editor view, assembling MenuBar + TextCanvas + StatusBar.
/// Uses VBoxLayout to arrange children vertically.
class EditorView : public VBox {
public:
    explicit EditorView(Widget* parent = nullptr);
    ~EditorView() override;

    /// Access sub-widgets for Presenter wiring.
    MenuBar*     menuBar()   { return menu_; }
    TextCanvas*  textCanvas() { return canvas_; }
    StatusBar*   statusBar() { return status_; }

private:
    MenuBar*     menu_;
    TextCanvas*  canvas_;
    StatusBar*   status_;
};

} // namespace miniui
