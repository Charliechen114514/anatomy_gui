add_executable(stage01_window_surface
    main.cpp
    src/window.cpp
    src/windows_factory.cpp
    src/drawing_src.cpp
    src/platform/error.cpp
    src/platform/registry_window.cpp
    src/platform/windows_window_cb.cpp)
target_include_directories(stage01_window_surface PRIVATE include)   # 拿到 include/mw/...
target_link_libraries(stage01_window_surface PRIVATE d3d11 dxgi d2d1 dcomp)
set_target_properties(stage01_window_surface PROPERTIES WIN32_EXECUTABLE TRUE)
