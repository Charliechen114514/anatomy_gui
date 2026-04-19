@echo off
setlocal

echo ============================================
echo  Building Win32 Tutorial Examples
echo ============================================
echo.

echo [1/2] Configuring CMake...
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    exit /b %errorlevel%
)

echo.
echo [2/2] Building all examples (Release)...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    exit /b %errorlevel%
)

echo.
echo ============================================
echo  All examples built successfully!
echo  Executables are in build\bin\
echo ============================================
