@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1

echo ============================================
echo  Graphics Tutorial Examples - Build Script
echo ============================================
echo.

where cl.exe >nul 2>&1
if %errorlevel% equ 0 goto :has_compiler

echo [INFO] cl.exe not in PATH, searching for Visual Studio...

if exist "D:\VS2026\VC\Auxiliary\Build\vcvarsall.bat" (
    call "D:\VS2026\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    goto :check_compiler
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    goto :check_compiler
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    goto :check_compiler
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    goto :check_compiler
)

echo [ERROR] Visual Studio not found.
echo         Please install VS 2022 with C++ Desktop Development workload.
exit /b 1

:check_compiler
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] MSVC environment initialization failed.
    exit /b 1
)
echo [INFO] MSVC environment initialized ^(.x64^).

:has_compiler
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    set "GENERATOR=NMake Makefiles"
) else (
    set "GENERATOR=MinGW Makefiles"
)
echo [INFO] Using generator: %GENERATOR%
echo.

echo [1/2] Configuring CMake...
cmake -B build -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    exit /b %errorlevel%
)

echo.
echo [2/2] Building all examples...
cmake --build build
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    exit /b %errorlevel%
)

echo.
echo ============================================
echo  All graphics examples built successfully!
echo  Executables are in build\bin\
echo ============================================
