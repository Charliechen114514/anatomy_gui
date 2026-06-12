#include "editor_view.h"

namespace miniui {

EditorView::EditorView(Widget* parent)
    : VBox(parent) {
    menu_   = new MenuBar(this);
    canvas_ = new TextCanvas(this);
    status_ = new StatusBar(this);

    layout().setSpacing(0);
}

EditorView::~EditorView() = default;

} // namespace miniui
