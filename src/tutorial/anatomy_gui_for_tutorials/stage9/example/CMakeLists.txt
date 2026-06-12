add_executable(stage9_demo main.cpp)
target_link_libraries(stage9_demo PRIVATE miniui9 ${XCB_LIBRARIES} ${CAIRO_LIBRARIES})
target_compile_options(stage9_demo PRIVATE ${XCB_CFLAGS_OTHER} ${CAIRO_CFLAGS_OTHER})
