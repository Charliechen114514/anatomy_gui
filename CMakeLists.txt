# anatomy_gui —— 从具象到抽象，再落回具象
#
# 构建（两条命令，不用 preset，也不用开任何"开发者命令行"）：
#     cmake -G Ninja -B build -S .
#     cmake --build build -j
#
# 工具链：clang-cl + Ninja + Windows SDK。
#   · clang-cl 自己会找到 Visual Studio / Windows SDK —— 不需要 vcvars，
#     也不需要开 "Developer PowerShell"，更不会产出 .sln。这里没有 VS 工程这回事。
#   · 只需要一个 LLVM：winget install LLVM.LLVM
#   · 选 clang-cl 还有一个副作用：它的 /showIncludes 恒为英文，Ninja 的头文件
#     依赖扫描能正常工作（中文版 cl 的本地化前缀会让依赖静默失效）。
#
# 关于生成器：CMake 规定生成器只能在命令行或环境变量里定，CMakeLists 里改不了。
#   · 想要上面那两条命令里的 Ninja —— 加 -G Ninja，或者设一次：
#         setx CMAKE_GENERATOR Ninja
#   · 不加 -G 且本机装了 Visual Studio 时，CMake 会选 VS 生成器并产出 .sln。

cmake_minimum_required(VERSION 3.23)

# 没指定编译器就自己找 clang-cl；找不到就当场说清楚，别让读者对着 CMake 报错猜。
if(NOT DEFINED CMAKE_CXX_COMPILER)
  find_program(ANATOMY_CLANG_CL NAMES clang-cl)
  if(ANATOMY_CLANG_CL)
    set(CMAKE_CXX_COMPILER "${ANATOMY_CLANG_CL}")
  else()
    message(FATAL_ERROR
      "找不到 clang-cl。装一个 LLVM 即可：winget install LLVM.LLVM\n"
      "（也可以用 -DCMAKE_CXX_COMPILER=<完整路径> 指定。）")
  endif()
endif()

project(anatomy_gui LANGUAGES CXX)

# 必须是 clang-cl：它在 MSVC 兼容模式下 COMPILER_ID 是 Clang、模拟目标是 MSVC。
# 纯 MSVC 或 MinGW 都会在这里被挡住。
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR
    "本骨架用 clang-cl，当前编译器是 "
    "${CMAKE_CXX_COMPILER_ID}/${CMAKE_CXX_SIMULATE_ID}（${CMAKE_CXX_COMPILER}）。")
endif()

# 单配置生成器（Ninja）：默认 Debug —— 教学骨架要能断点、能看变量。
# 多配置生成器（VS）不吃 CMAKE_BUILD_TYPE，改由 --config 决定。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# 静态 CRT（/MT）：产物双击就跑，不依赖 msvcp140.dll。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

if(NOT CMAKE_GENERATOR MATCHES "Ninja")
  message(STATUS "当前生成器：${CMAKE_GENERATOR}（骨架推荐 Ninja，见文件头注释）")
endif()

# 全系列基线，放在顶层让所有目标继承。
#
# 两个关掉的警告都跟"我们到底在写什么"有关：
#   · -Wno-c++98-compat：我们在写 C++20，"这在 C++98 里不行"是纯噪音
#   · -Wno-nonportable-system-include-path：SDK 里盘上那个文件叫 Windows.h，
#     而所有人写 <windows.h>，为大小写吵一架没有意义
add_compile_options(
    /utf-8 /std:c++20
    -Wall -Wextra
    -Wno-c++98-compat
    -Wno-nonportable-system-include-path)
add_compile_definitions(UNICODE _UNICODE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src/gui)
