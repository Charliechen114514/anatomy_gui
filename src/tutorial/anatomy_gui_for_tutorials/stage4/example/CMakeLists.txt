add_executable(stage4_demo main.cpp)
target_link_libraries(stage4_demo PRIVATE miniui4 ${XCB_LIBRARIES} ${CAIRO_LIBRARIES})
target_compile_options(stage4_demo PRIVATE ${XCB_CFLAGS_OTHER} ${CAIRO_CFLAGS_OTHER})
