cmake_minimum_required(VERSION 3.15)
project(InstanceDataExample)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source files
set(SOURCES
    main.cpp
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Link Windows libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    user32
    gdi32
)

# Set subsystem to Windows
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
else()
    # MinGW: 使用 -municode 支持 wWinMain 入口点
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()
