# Win32 Global Variable State Management Example
cmake_minimum_required(VERSION 3.15)
project(GlobalStateExample)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create executable
add_executable(GlobalStateExample
    main.cpp
)

# Link against Windows libraries
target_link_libraries(GlobalStateExample
    user32
    gdi32
)

# Set subsystem to WINDOWS (no console window)
if(MSVC)
    set_target_properties(GlobalStateExample PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup"
    )
else()
    set_target_properties(GlobalStateExample PROPERTIES
        LINK_FLAGS "-mwindows"
    )
endif()
