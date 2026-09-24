# ============================================================================
# ACSD Windows 侧统一工具链入口 (eng/build/toolchain.ps1)
#
# 依赖面（硬约束）：
#   - 只用**仓内 vendored 依赖**与**系统工具链**（MSVC + 仓库根 CMake）；
#   - 构建入口唯一 = 仓库根 CMakeLists.txt / CMakePresets.json（ENGINEERING_SPEC §7/§10）；
#   - **禁** MSYS2/MinGW（含把其 bin 目录加进 PATH 或引用其头文件）；
#   - **禁**机器绝对用户路径（C:/Users/<user> 等）。
#   依据：eng/packaging/dependency-lock.json 的
#   msys2_mingw=FORBIDDEN 与 machine_absolute_path=FORBIDDEN。
#
# 用法: .\eng\build\toolchain.ps1 {check|env|build|test}
# ============================================================================
param(
    [Parameter(Position=0)][string]$Command = "check"
)

$ErrorActionPreference = "Stop"

# 仓库根 = eng/build/ 的上两级（不写死绝对路径）
$AstroCS_ROOT   = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$AstroCS_CONFIG = "win-msvc-17.14.39-x64"   # CMakePresets.json 的正式 Windows configure preset
$AstroCS_BUILD  = "win-rel"                 # buildPresets / testPresets 的同名项

function Test-AstroCSToolchain {
    Write-Host "===== ACSD 工具链自检 =====" -ForegroundColor Cyan
    $ok = $true
    foreach ($name in @("cmake", "ctest", "git")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            Write-Host ("{0,-10} {1}" -f ($name + ":"), $cmd.Source) -ForegroundColor Green
        } else {
            Write-Host ("{0,-10} 缺失" -f ($name + ":")) -ForegroundColor Red
            $ok = $false
        }
    }
    $py = Get-Command python -ErrorAction SilentlyContinue
    if ($py) { Write-Host ("{0,-10} {1}" -f "python:", $py.Source) -ForegroundColor Green }
    else { Write-Host ("{0,-10} 缺失（仅影响检查器脚本）" -f "python:") -ForegroundColor Yellow }

    # 仓内 vendored 依赖与机器合同（不引入任何包管理器）
    $vendored = [ordered]@{
        "nlohmann/json" = "lib\third_party\nlohmann\json.hpp"
        "cfitsio 清单"  = "eng\cmake\cfitsio_sources.cmake"
        "工具链合同"    = "eng\packaging\schemas\preset-contract.json"
        "preset 入口"   = "CMakePresets.json"
    }
    foreach ($k in $vendored.Keys) {
        $p = Join-Path $AstroCS_ROOT $vendored[$k]
        if (Test-Path -LiteralPath $p) {
            Write-Host ("{0,-10} 在位" -f ($k + ":")) -ForegroundColor Green
        } else {
            Write-Host ("{0,-10} 缺失!" -f ($k + ":")) -ForegroundColor Red
            $ok = $false
        }
    }
    if (-not $ok) { exit 1 }
}

function Build-AstroCSAll {
    Push-Location $AstroCS_ROOT
    try {
        & cmake --preset $AstroCS_CONFIG
        if ($LASTEXITCODE -ne 0) { Write-Host "configure 失败" -ForegroundColor Red; exit 1 }
        & cmake --build --preset $AstroCS_BUILD
        if ($LASTEXITCODE -ne 0) { Write-Host "构建失败" -ForegroundColor Red; exit 1 }
        Write-Host "构建完成" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}

function Test-AstroCSAll {
    Push-Location $AstroCS_ROOT
    try {
        & ctest --preset $AstroCS_BUILD --output-on-failure
        exit $LASTEXITCODE
    } finally {
        Pop-Location
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    switch ($Command.ToLower()) {
        "check" { Test-AstroCSToolchain }
        "env"   { Write-Host "[ACSD] 工具链 = 仓库根 CMakePresets.json 的 preset '$AstroCS_CONFIG'；取值落点 = eng/packaging/windows/README.md" }
        "build" { Build-AstroCSAll }
        "test"  { Test-AstroCSAll }
        default { Write-Host "用法: .\eng\build\toolchain.ps1 {check|env|build|test}" }
    }
}
