cmake_minimum_required(VERSION 3.20)
project(GraphicsTutorials VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_WIN32_EXECUTABLE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(GRAPHICS_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# MinGW provides a stub directxmath.h without XMMATRIX/XMMatrix functions.
# Provide the real Microsoft DirectXMath headers (header-only, MIT license).
if(NOT MSVC)
    include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/common/DirectXMath)
endif()

# =============================================================================
# GDI 图形系列 (ch18-25)
# =============================================================================
add_subdirectory(gdi)

# =============================================================================
# GDI+ 系列 (ch26-28)
# =============================================================================
add_subdirectory(gdiplus)

# =============================================================================
# Direct2D / DirectWrite 系列 (ch29-33)
# =============================================================================
add_subdirectory(direct2d)

# =============================================================================
# HLSL 着色器系列 (ch34-36)
# =============================================================================
add_subdirectory(hlsl)

# =============================================================================
# Direct3D 11 系列 (ch37-42)
# =============================================================================
add_subdirectory(d3d11)

# =============================================================================
# Direct3D 12 系列 (ch43-47)
# =============================================================================
add_subdirectory(d3d12)

# =============================================================================
# 自定义控件系列 (ch48-50)
# =============================================================================
add_subdirectory(custom_controls)

# =============================================================================
# OpenGL 系列 (ch51)
# =============================================================================
add_subdirectory(opengl)
