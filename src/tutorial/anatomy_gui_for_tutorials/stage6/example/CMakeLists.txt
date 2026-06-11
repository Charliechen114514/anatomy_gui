add_executable(stage6_calculator main.cpp)
target_link_libraries(stage6_calculator PRIVATE miniui6)
target_compile_options(stage6_calculator PRIVATE
    ${XCB_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)
