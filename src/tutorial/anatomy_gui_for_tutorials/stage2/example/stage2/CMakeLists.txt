add_executable(stage2_minidemo main.cpp)

target_include_directories(stage2_minidemo PRIVATE
    ${XCB_INCLUDE_DIRS}
)

target_link_libraries(stage2_minidemo PRIVATE
    AnatomyGui
    ${XCB_LIBRARIES}
)

target_compile_options(stage2_minidemo PRIVATE
    ${XCB_CFLAGS_OTHER}
)
