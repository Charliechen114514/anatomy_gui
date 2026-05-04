# build_msvc.ps1 — 使用 MSVC 快速编译验证
# 用法: pwsh build_msvc.ps1 [native_win32 | graphics | all]
# 默认: all

param(
    [ValidateSet("native_win32", "graphics", "all")]
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

# ============================================================================
# 查找并初始化 MSVC 环境
# ============================================================================

function Find-MSVC {
    # PATH 中已有
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) { return $true }

    # 常见 VS 安装路径
    $vsPaths = @(
        "D:\VS2026\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    )

    foreach ($p in $vsPaths) {
        if (Test-Path $p) {
            Write-Host "[INFO] 找到 VS: $p" -ForegroundColor Cyan
            # 调用 vcvarsall.bat 设置环境变量
            $output = cmd /c "`"$p`" x64 >nul 2>&1 && set" 2>&1
            foreach ($line in $output) {
                if ($line -match "^([^=]+)=(.*)$") {
                    [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
                }
            }
            return $true
        }
    }
    return $false
}

# 初始化 MSVC
if (-not (Find-MSVC)) {
    Write-Host "[ERROR] Visual Studio 未找到。请安装 VS 2022 C++ 桌面开发工作负载。" -ForegroundColor Red
    exit 1
}

$clVersion = & cl.exe 2>&1 | Select-Object -First 1
Write-Host "[INFO] $clVersion" -ForegroundColor Green

# ============================================================================
# 构建流程
# ============================================================================

$rootDir = Split-Path $MyInvocation.MyCommand.Path
$failed = @()
$built = 0

function Build-Target {
    param([string]$Name, [string]$Dir)

    Write-Host "--------------------------------------------" -ForegroundColor Yellow
    Write-Host "[$Name] Configuring..." -ForegroundColor Yellow

    $buildDir = Join-Path $Dir "build_msvc"
    if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }

    Push-Location $Dir
    try {
        cmake -B $buildDir -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release 2>&1
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
