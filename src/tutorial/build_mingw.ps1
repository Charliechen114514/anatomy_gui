# build_mingw.ps1 — 使用 MinGW 快速编译验证
# 用法: pwsh build_mingw.ps1 [native_win32 | graphics | all]
# 默认: all

param(
    [ValidateSet("native_win32", "graphics", "all")]
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

# 查找 g++
function Find-GCC {
    # PATH 中已有
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gpp) { return $gpp.Source }

    # 常见安装路径
    $paths = @(
        "D:\QT\Qt6.6.0\Tools\llvm-mingw1706_64\bin\g++.exe"
        "C:\msys64\mingw64\bin\g++.exe"
        "D:\msys64\mingw64\bin\g++.exe"
        "C:\MinGW\bin\g++.exe"
    )
    foreach ($p in $paths) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Find-MingwMake {
    $make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if ($make) { return $make.Source }

    $make = Get-Command make -ErrorAction SilentlyContinue
    if ($make) { return $make.Source }

    # 与 g++ 同目录
    $gppDir = Split-Path (Find-GCC)
    if ($gppDir) {
        $candidate = Join-Path $gppDir "mingw32-make.exe"
        if (Test-Path $candidate) { return $candidate }
        $candidate = Join-Path $gppDir "make.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

# 初始化
$gpp = Find-GCC
if (-not $gpp) {
    Write-Host "[ERROR] g++ not found. Install MinGW or add to PATH." -ForegroundColor Red
    exit 1
}

$make = Find-MingwMake
if (-not $make) {
    Write-Host "[ERROR] mingw32-make not found." -ForegroundColor Red
    exit 1
}

# 把 g++ 和 make 所在目录加到 PATH
$binDir = Split-Path $gpp
if ($env:PATH -notlike "*$binDir*") {
    $env:PATH = "$binDir;$env:PATH"
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " MinGW Quick Build Verification" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[INFO] g++: $gpp" -ForegroundColor Green

$gppVersion = & g++ --version | Select-Object -First 1
Write-Host "[INFO] $gppVersion" -ForegroundColor Green
Write-Host ""

$rootDir = Split-Path $MyInvocation.MyCommand.Path
$failed = @()
$built = 0

function Build-Target {
    param([string]$Name, [string]$Dir)

    Write-Host "--------------------------------------------" -ForegroundColor Yellow
    Write-Host "[$Name] Configuring..." -ForegroundColor Yellow

    $buildDir = Join-Path $Dir "build_mingw"
    if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }

    Push-Location $Dir
    try {
        cmake -B $buildDir -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[$Name] CMake configure FAILED" -ForegroundColor Red
            $script:failed += "$Name (configure)"
            return
        }

        Write-Host "[$Name] Building..." -ForegroundColor Yellow
        cmake --build $buildDir --parallel 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-Host "[$Name] Build FAILED" -ForegroundColor Red
            $script:failed += "$Name (build)"
            return
        }

        $exeCount = (Get-ChildItem -Path "$buildDir\bin" -Filter "*.exe" -ErrorAction SilentlyContinue).Count
        $script:built += $exeCount
        Write-Host "[$Name] OK — $exeCount executables" -ForegroundColor Green
    }
    finally {
        Pop-Location
    }
}

# 执行构建
if ($Target -in @("native_win32", "all")) {
    Build-Target "Win32" (Join-Path $rootDir "native_win32")
}

if ($Target -in @("graphics", "all")) {
    Build-Target "Graphics" (Join-Path $rootDir "graphics")
}

# 汇总
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
if ($failed.Count -eq 0) {
    Write-Host " ALL PASSED — $built executables built" -ForegroundColor Green
} else {
    Write-Host " FAILED ($($failed.Count)):" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    Write-Host " Built: $built executables" -ForegroundColor Yellow
}
Write-Host "============================================" -ForegroundColor Cyan

if ($failed.Count -gt 0) { exit 1 }
