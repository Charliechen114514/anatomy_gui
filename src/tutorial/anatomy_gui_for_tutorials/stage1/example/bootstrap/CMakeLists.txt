add_executable(xcb_window_demo main.cpp)

target_include_directories(xcb_window_demo PRIVATE
    ${XCB_INCLUDE_DIRS}
)

target_link_libraries(xcb_window_demo PRIVATE
    ${XCB_LIBRARIES}
)

target_compile_options(xcb_window_demo PRIVATE
    ${XCB_CFLAGS_OTHER}
)
