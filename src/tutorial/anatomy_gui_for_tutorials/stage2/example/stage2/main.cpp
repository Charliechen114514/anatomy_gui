#include "application.h"
#include "window_builder.h"

int main(int argc, char* argv[]) {
    anatomy_gui::gui::Application app(argc, argv);
    anatomy_gui::gui::WindowBuilder builder;
    auto window =
        builder.setPosition({100, 100}).setSize({500, 500}).create_window();

    window->show();

    return app.execution();
}