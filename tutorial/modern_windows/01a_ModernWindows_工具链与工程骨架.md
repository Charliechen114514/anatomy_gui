# 01a · 工具链与工程骨架——把解剖台搭起来

> 从这一篇起，咱们正式动手造器官一（窗口与绘制面）。但磨刀不误砍柴工——在写哪怕一行 D3D 之前，得先把"工具链 + 工程骨架"搭好：CMake 怎么配、Ninja + clangd 怎么让 IntelliSense 活过来、每个坑在哪。这一篇是 stage01 的地基，后面六篇（01b–01g）全在它上面长。
>
> "为什么是 CMake + Ninja + clangd、而不是手写 .sln"——这个**为什么**序章已经讲透了，不重复（去序章翻"构建架构"那一节）。本篇只管**照着做**：一步步把骨架立起来，最后编出一个工具链自检程序。

## 这一篇结束你能看到什么

一个 `cmake --preset default` 能配置、`cmake --build` 能编过、**clangd 有补全**的空项目，外加一个 `00_bootstrap` 自检程序跑出 "self-check OK"。没有窗、没有画面——那是 01d / 01f 的事。这一篇的交付物就是"地基"本身：工具链通、补全活、骨架立。

## 环境基线

一句话交代清楚，后面所有代码默认就在这套环境下成立：**Visual Studio 2026 / MSVC 14.5x**（PlatformToolset `v145`），编译选项 `/std:c++latest`（吃 C++23/26）+ `/utf-8`（中文注释不被当 GBK 乱码）+ `UNICODE`，目标 x64，Windows SDK 最新。编辑器是 **VS Code + CMake Tools + clangd**——clangd 是咱们的 IntelliSense 引擎，而它硬性要 `compile_commands.json`，这正是后面选 Ninja 生成器、而不是 Visual Studio 生成器的根本原因（VS 生成器产不出这个文件）。

## 目标骨架长这样

```
src/tutorial/modern_windows/
├─ CMakePresets.json      ← Ninja preset，导出 compile_commands.json
├─ CMakeLists.txt         ← 顶层：project + 全局选项 + add_subdirectory
├─ .vscode/
│  ├─ cmake-vs.cmd        ← cmake 包装：注入 vcvars64 + 真 ninja（绕开两个坑）
│  └─ settings.json       ← CMake Tools 用包装脚本；clangd 指 build/
└─ 00_bootstrap/          ← 工具链自检
   ├─ CMakeLists.txt
   └─ main.cpp
```

`stage01_window_surface/` 那一摊（窗口/绘制面的器官代码）从 **01b** 才开始建，本篇先不碰。

## 第一步：顶层 `CMakeLists.txt`

整个工程的入口，做的事很简单：声明项目、把全局编译选项钉在顶上（所有子目录自动继承）、再把 `00_bootstrap` 子目录串进来。

```cmake
# src/tutorial/modern_windows/CMakeLists.txt
cmake_minimum_required(VERSION 3.26)

project(ModernWindows LANGUAGES CXX)

# 全系列工具链基线：拉到最新标准 + UTF-8 源码/执行字符集 + UNICODE。
# 放顶层 → 所有子目录的 target 自动继承，不会出现"某个 stage 漏了 /utf-8
# 导致中文注释编成乱码"这种灵异事件。
add_compile_options(/std:c++latest /utf-8)
add_compile_definitions(UNICODE _UNICODE)

add_subdirectory(00_bootstrap)      # 工具链自检
# 01b 起会加：add_subdirectory(stage01_window_surface)
```

把 `/std:c++latest` 和 `/utf-8` 放顶层是有意为之的——这俩是全系列的"空气和水"，与其在每个 stage 里重复写，不如在最高处钉一次，谁都不会漏。

## 第二步：`CMakePresets.json`（Ninja + 导出 compile_commands）

preset 把"用什么生成器、产物放哪、导不导出 compile_commands"一次性钉死，命令行 `cmake --preset default` 就能配：

```json
{
  "version": 4,
  "cmakeMinimumRequired": { "major": 3, "minor": 23, "patch": 0 },
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Ninja (MSVC) · Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CMAKE_BUILD_TYPE": "Debug"
      }
    }
  ],
  "buildPresets": [
    { "name": "default", "configurePreset": "default" }
  ]
}
```

两个要点值得讲一下。第一，`CMAKE_EXPORT_COMPILE_COMMANDS: "ON"` 让 CMake 产出 `build/compile_commands.json`——这正是 clangd 的命脉，没有它补全和跳转全废（**这是整个 Ninja 选择的根本动机**：VS 生成器产不出它）。第二，注意这里**没有** `CMAKE_MAKE_PROGRAM`——哪个真 ninja 该用、怎么避开假 ninja，下一节的包装脚本统一解决，不在 preset 里写（带空格的 ninja 路径塞进 preset 再经命令行传递会被空格拆碎，是个我们踩过的坑）。

## 第三步：两个一定会踩的坑，与 `cmake-vs.cmd` 的由来

配到这一步，如果你直接 `cmake --preset default`，**大概率会连撞两个坑**——咱们一个个拆，然后用一个小包装脚本一次性解决。

⚠️ **坑一：Ninja 找不到 `cl.exe`。** Visual Studio 生成器会自己去 VS 装载里把编译器、头文件、SDK 找齐；而 **Ninja 生成器不会**——它要求 `cl.exe` 已经在环境里（也就是 `vcvarsall` / VS 开发者环境已经加载过）。可 VS Code 普通启动时，CMake 子进程的环境里并没有 `cl.exe`，于是 CMake 报 `No CMAKE_CXX_COMPILER could be found`。

⚠️ **坑二：PATH 上有个"假 ninja"。** 如果你机器上装过 Chromium 的 `depot_tools`，它会在 PATH 里塞一个**没有 `.exe` 后缀的 `ninja`**（实际是个脚本壳子）。CMake 默认去 PATH 找 ninja，找到这个壳子一执行，就报 `inappropriate file type or format`，配置直接挂。

两个坑的根子是同一个：**CMake 跑 Ninja 时，环境里既没有 `cl.exe`、又可能踩到假 ninja。** 解法——写个 cmake 的"包装脚本"，在每次调 cmake 之前先把环境弄对：

```bat
@echo off
rem CMake wrapper: load MSVC x64 env (cl.exe) and put VS-bundled Ninja first on
rem PATH, so `cmake -G Ninja` works on every normal VS Code launch -- no need
rem to start VS Code from a developer prompt. Machine-specific paths below.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
"C:\Program Files\CMake\bin\cmake.exe" %*
```

它就干两件事再转发给真正的 cmake：先 `call vcvars64.bat`（把 `cl.exe` 和一堆 SDK 路径灌进环境），再把 **VS 自带的真 ninja 目录 prepend 到 PATH 最前面**（这样 CMake 找 ninja 时第一个命中真 ninja，绕开后面 depot_tools 的假壳子）。

注意这里的路径是**本机特定的**——`18\Community` 是 VS 2026 Community，`C:\Program Files\CMake` 是系统 CMake。换机器或 VS 版本/edition 不同要改。怎么找你机器上的真路径？vcvars64 在 `<VS>\VC\Auxiliary\Build\vcvars64.bat`；VS 自带 ninja 在 `<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`（`<VS>` 是你的 VS 安装目录）。一个快速找法是用 `vswhere`：

```bash
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
```

## 第四步：让 CMake Tools 和 clangd 用上包装脚本

光有包装脚本还不够，得告诉 CMake Tools"用这个脚本当 cmake"，再告诉 clangd"去 `build/` 读 compile_commands"：

```json
// .vscode/settings.json
{
  "cmake.cmakePath": "${workspaceFolder}/.vscode/cmake-vs.cmd",
  "clangd.arguments": ["--compile-commands-dir=build"]
}
```

`cmake.cmakePath` 指向包装脚本后，CMake Tools 每次配置/构建都会先经过它 → 环境自动就位。`clangd.arguments` 把 compile_commands 的位置告诉 clangd（CMake 把它产在 `build/` 下）。

## 第五步：`00_bootstrap` 工具链自检

地基搭好，先别急着造器官——编一个最小的自检程序，确认"工具链通、`/std:c++latest` 真的把 C++23 拉上去了"。这就是 `00_bootstrap` 的全部意义：一个带断言的 Hello World。

```cmake
# 00_bootstrap/CMakeLists.txt
add_executable(stage00_bootstrap main.cpp)
```

```cpp
// 00_bootstrap/main.cpp
#include <expected>
#include <print>

// 编译期自检：这些特性宏存在，说明 /std:c++latest 真的把 C++23 拉上来了。
// 编不过？看 static_assert 报的是哪个宏——多半是标准没拉上去。
static_assert(__cpp_lib_print    >= 202207L, "需要 C++23 <print>：检查 /std:c++latest 是否生效");
static_assert(__cpp_lib_expected >= 202211L, "需要 C++23 <expected>：检查 /std:c++latest 是否生效");

int main() {
    const std::expected<int, int> ok = 42;   // 顺带编一下 expected——后面整个错误模型就靠它
    std::println("ModernWindows toolchain self-check OK: expected={}", *ok);
    return 0;
}
```

它不只是"Hello World"——两个 `static_assert` 卡的是 C++23 的特性宏，**标准没拉上去就编不过**，所以能真正起到自检作用，而不是"能编但悄悄退化到 C++17"。

## 第六步：配置、构建、验证

三条命令，全在 `src/tutorial/modern_windows/` 下跑（或直接在 VS Code 里靠 CMake Tools 点一下，效果一样）：

```bash
cmake --preset default          # 配置（走包装脚本 → vcvars64 + 真 ninja → 产出 build/compile_commands.json）
cmake --build build             # 构建
./build/00_bootstrap/stage00_bootstrap.exe   # 应打印 "ModernWindows toolchain self-check OK: expected=42"
```

看到那行 `self-check OK`，工具链就通了。同时在 VS Code 里随便打开一个 `.cpp`——clangd 应该有补全和跳转（如果还没，等它索引几秒，或 `Ctrl+Shift+P` → "clangd: Restart language server"）。

## 踩坑清单

这一篇看着简单，但坑都集中在工具链上，笔者把最容易卡住的几个再钉一遍：

第一，**别用 Visual Studio 生成器**——它产不出 compile_commands.json，clangd 直接没补全。认准 Ninja。

第二，**`No CMAKE_CXX_COMPILER`**——Ninja 找不到 cl.exe，环境没加载 vcvars64。要么用上面的 `cmake-vs.cmd` 包装，要么从"x64 Native Tools Command Prompt for VS 2026"启动 VS Code（让环境被继承）。包装脚本更省事，一次配好。

第三，**`inappropriate file type or format`**——PATH 上的假 ninja（depot_tools 那个）。包装脚本里把 VS 自带真 ninja prepend 到 PATH 最前，绕开。

第四，**clangd 没补全但能编**——多半是 `--compile-commands-dir` 指错了（要和 preset 的 `binaryDir` 一致，咱们是 `build`），或 clangd 没重新索引，重启一下。

## 收尾与下一篇

到这里，"解剖台"搭好了：CMake + Ninja + clangd 跑通，`00_bootstrap` 自检过，骨架立住。后面的 01b–01g 都是在这个地基上长器官。

下一篇 **01b · 平台地基**——我们往 `stage01_window_surface/` 里落第一批代码：`define.h`（`Size_t`/`Point_t`）、`platform/error.h`（`Error`/`Result`/`from_hresult`/`from_win32`）、`comptr.h`，以及第一个 RAII 类 `unique_hwnd`（顺带把"句柄的 deleter 必须匹配"那张对照表讲透）。这一层没有任何 D3D，但后面所有器官都踩在它上面。我们 01b 见。

---

**相关资源**

- [CMake · CMakePresets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)（preset 的版本字段、`configurePresets`/`buildPresets` 写法）
- [CMAKE_EXPORT_COMPILE_COMMANDS — CMake 文档](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)（为什么只有 Ninja/Make 生成器才产出它）
- [clangd · compile_commands.json](https://clangd.llvm.org/design.html)（clangd 怎么靠它做补全/跳转）
- [vswhere](https://github.com/microsoft/vswhere)（编程化地找 VS 安装路径 / 自带 ninja 路径）
