# 00_bootstrap —— 工具链自检
#
# 一个控制台程序，唯一目的是验证整条链路通不通：
#   - CMake → Visual Studio 生成器 → .sln
#   - /std:c++latest 真的把 C++23/26 拉上来了（见 main.cpp 里的 static_assert）
#   - MSVC 14.5x 的 <print> / <expected> 可用
#
# 编不过？先看 main.cpp 里 static_assert 报的是哪个特性宏——多半是标准没拉上去。

add_executable(stage00_bootstrap main.cpp)
