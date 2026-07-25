<#
.SYNOPSIS
AstroCS 根级构建编排器 (ADR-004 方案 B)
.DESCRIPTION
P01-004 交付物: 统一构建入口, 按依赖图分层构建, 产物输出到 build/artifacts/, 生成 manifest.json
各模块保留现有 build.ps1/Makefile, 本脚本仅做编排与产物收集。
.PARAMETER Target
all (默认) / clean / <module_name>
.PARAMETER Config
Release (默认) / Debug (透传给 CMake 模块)
.PARAMETER Clean
先清理再构建 (clean build), 等价于 -Target clean 后再 -Target all
.EXAMPLE
pwsh -File build.ps1
pwsh -File build.ps1 -Target all
pwsh -File build.ps1 -Target clean
pwsh -File build.ps1 -Target astro_image_io
pwsh -File build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [string]$Target = "all",
    [string]$Config = "Release",
    [switch]$Clean,
    [string]$LockFile = "engineering/evidence/P01-002/dependencies.lock.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$repoRoot = $PSScriptRoot
$lockPath = Join-Path $repoRoot $LockFile

# UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try { [System.Text.Encoding]::Default = [System.Text.Encoding]::UTF8 } catch {}

# ============================================================================
# 读取 lock.json
# ============================================================================
if (-not (Test-Path $lockPath)) {
    Write-Host "[FAIL] 依赖锁定清单不存在: $lockPath" -ForegroundColor Red
    Write-Host "  请先运行 P01-002 生成 dependencies.lock.json" -ForegroundColor Yellow
    exit 1
}

$lock = Get-Content $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json

# ============================================================================
# 注入 MSYS2 MinGW64 bin 到 PATH
# ============================================================================
$msys2Bin = $lock.path_requirements.msys2_mingw64_bin
if (Test-Path $msys2Bin) {
    $env:Path = "$msys2Bin;$env:Path"
} else {
    Write-Host "[FAIL] MSYS2 MinGW64 不存在: $msys2Bin" -ForegroundColor Red
    exit 1
}

# ============================================================================
# 目录
# ============================================================================
$artifactsDir = Join-Path $repoRoot "build\artifacts"
$logsDir = Join-Path $repoRoot "build\logs"
$manifestPath = Join-Path $repoRoot "build\manifest.json"
New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
New-Item -ItemType Directory -Path $logsDir -Force | Out-Null

# ============================================================================
# 构建顺序与模块表
# ============================================================================
$buildOrder = @()
$buildOrder += $lock.build_order.layer_1_base
$buildOrder += $lock.build_order.layer_2_middle
$buildOrder += $lock.build_order.layer_3_top

$modulesMap = @{}
foreach ($m in $lock.modules) { $modulesMap[$m.name] = $m }

# ============================================================================
# 辅助函数
# ============================================================================
function Get-FileSha256 {
    param([string]$path)
    if (-not (Test-Path $path)) { return $null }
    return (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToUpper()
}

function Invoke-ModuleClean {
    param($module)
    $name = $module.name
    $modPath = Join-Path $repoRoot $module.path
    if (-not (Test-Path $modPath)) {
        Write-Host "  [SKIP] $name : 路径不存在" -ForegroundColor Yellow
        return
    }

    $makefile = Join-Path $modPath "Makefile"
    # 1. 调用 make clean (如果有 Makefile)
    if (Test-Path $makefile) {
        try { & mingw32-make -C $modPath -f Makefile clean 2>&1 | Out-Null } catch {}
    }
    # 2. 删除产物 (排除 archive/test/tests/build 子目录)
    try {
        Get-ChildItem -Path $modPath -Recurse -Include *.dll,*.exe,*.o,*.a -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch "\\archive\\|\\test\\|\\tests\\|\\build\\" } |
            Remove-Item -Force -ErrorAction SilentlyContinue
    } catch {}
    Write-Host "  [OK] $name 清理完成" -ForegroundColor Green
}

function Invoke-ModuleBuild {
    param($module)

    $name = $module.name
    $modPath = Join-Path $repoRoot $module.path
    $logFile = Join-Path $logsDir "build_${name}.log"

    $result = [PSCustomObject]@{
        module = $name
        path = $module.path
        authority = $module.authority
        status = "UNKNOWN"
        output = $module.output
        output_path = ""
        sha256 = ""
        size_bytes = 0
        build_time_sec = 0
        error = ""
    }

    if (-not (Test-Path $modPath)) {
        $result.status = "SKIP"; $result.error = "路径不存在"
        Write-Host "  [SKIP] $name : 路径不存在" -ForegroundColor Yellow
        return $result
    }
    if ($module.authority -eq "none") {
        $result.status = "SKIP"; $result.error = "no_build"
        Write-Host "  [SKIP] $name : no_build" -ForegroundColor Yellow
        return $result
    }

    Write-Host "  [BUILD] $name ($($module.authority))" -ForegroundColor Cyan
    $startTime = Get-Date
    $buildOutput = ""
    $ok = $false

    try {
        $buildPs1 = Join-Path $modPath "build.ps1"
        $makefile = Join-Path $modPath "Makefile"
        $cmakeLists = Join-Path $modPath "CMakeLists.txt"

        if ($module.authority -eq "build.ps1" -and (Test-Path $buildPs1)) {
            # 调用模块 build.ps1 (astro_image_io 接受 -Config preset, 其余无参数)
            if ($name -eq "astro_image_io") {
                $buildOutput = & pwsh -NoProfile -File $buildPs1 2>&1
            } else {
                $buildOutput = & pwsh -NoProfile -File $buildPs1 2>&1
            }
            $ok = ($LASTEXITCODE -eq 0)
        } elseif ($module.authority -eq "cmake" -and (Test-Path $cmakeLists)) {
            # CMake 构建 (healpix_browser_qt)
            $cmakeBuildDir = Join-Path $modPath "build"
            if (-not (Test-Path (Join-Path $cmakeBuildDir "CMakeCache.txt"))) {
                $buildOutput = & cmake -B $cmakeBuildDir -S $modPath 2>&1
            }
            $buildOutput += & cmake --build $cmakeBuildDir --config $Config 2>&1
            $ok = ($LASTEXITCODE -eq 0)
        } elseif (Test-Path $makefile) {
            # Makefile 构建
            $buildOutput = & mingw32-make -C $modPath -f Makefile 2>&1
            $ok = ($LASTEXITCODE -eq 0)
        } else {
            throw "无可用构建系统 (authority=$($module.authority))"
        }
    } catch {
        $buildOutput += "`n[EXCEPTION] $_"
        $ok = $false
    }

    # 保存日志
    $buildOutput | Out-File -FilePath $logFile -Encoding UTF8 -ErrorAction SilentlyContinue

    # 查找并收集产物
    if ($ok) {
        $outputFile = $module.output
        if ($outputFile) {
            $outputPath = Join-Path $modPath $outputFile
            if (Test-Path $outputPath) {
                $destPath = Join-Path $artifactsDir $outputFile
                Copy-Item -Path $outputPath -Destination $destPath -Force
                $result.output_path = $destPath
                $result.sha256 = Get-FileSha256 $destPath
                $result.size_bytes = (Get-Item $destPath).Length
                $result.status = "OK"
            } else {
                $result.status = "FAIL"; $result.error = "产物未找到: $outputFile (日志见 $logFile)"
            }
        } else {
            # 无明确产物名 (cmake app), 尝试收集 build 目录下的 exe
            $cmakeBuildDir = Join-Path $modPath "build"
            $exes = Get-ChildItem -Path $cmakeBuildDir -Recurse -Include *.exe -ErrorAction SilentlyContinue
            if ($exes) {
                foreach ($exe in $exes) {
                    $destPath = Join-Path $artifactsDir $exe.Name
                    Copy-Item -Path $exe.FullName -Destination $destPath -Force
                }
                $result.output = ($exes | Select-Object -First 1).Name
                $result.output_path = Join-Path $artifactsDir $result.output
                $result.sha256 = Get-FileSha256 $result.output_path
                $result.size_bytes = (Get-Item $result.output_path).Length
                $result.status = "OK"
            } else {
                $result.status = "OK"; $result.error = "无明确产物名但构建退出码为 0"
            }
        }
    } else {
        $result.status = "FAIL"; $result.error = "构建失败 (日志见 $logFile)"
    }

    $result.build_time_sec = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)

    if ($result.status -eq "OK") {
        Write-Host "  [OK] $name ($($result.build_time_sec)s)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $name : $($result.error)" -ForegroundColor Red
    }
    return $result
}

# ============================================================================
# 主流程
# ============================================================================
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AstroCS 根级构建 (ADR-004 方案 B)" -ForegroundColor Cyan
Write-Host "  Target: $Target | Config: $Config | Clean: $Clean" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# === Clean 模式 ===
if ($Target -eq "clean") {
    Write-Host "--- 清理所有模块 ---" -ForegroundColor Yellow
    foreach ($name in $buildOrder) {
        $module = $modulesMap[$name]
        if ($module) { Invoke-ModuleClean -module $module }
    }
    if (Test-Path $artifactsDir) {
        Remove-Item -Path $artifactsDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] 已清理 $artifactsDir" -ForegroundColor Green
    }
    if (Test-Path $manifestPath) {
        Remove-Item -Path $manifestPath -Force -ErrorAction SilentlyContinue
    }
    Write-Host "[OK] 清理完成" -ForegroundColor Green
    exit 0
}

# === 确定构建目标 ===
if ($Target -eq "all") {
    $targets = $buildOrder
} else {
    if (-not $modulesMap.ContainsKey($Target)) {
        Write-Host "[FAIL] 未知模块: $Target" -ForegroundColor Red
        Write-Host "  可用模块: $($buildOrder -join ', ')" -ForegroundColor Yellow
        exit 1
    }
    $targets = @($Target)
}

# === Clean Build ===
if ($Clean) {
    Write-Host "--- 清理目标模块 ---" -ForegroundColor Yellow
    foreach ($name in $targets) {
        $module = $modulesMap[$name]
        if ($module) { Invoke-ModuleClean -module $module }
    }
    Write-Host ""
}

# === 构建 ===
Write-Host "--- 构建模块 ($($targets.Count) 个, 按依赖分层) ---" -ForegroundColor Yellow
$results = @()
$failCount = 0
foreach ($name in $targets) {
    $module = $modulesMap[$name]
    if (-not $module) {
        Write-Host "  [WARN] 模块不在 lock 中: $name" -ForegroundColor Yellow
        continue
    }
    $result = Invoke-ModuleBuild -module $module
    $results += $result
    if ($result.status -eq "FAIL") { $failCount++ }
}

# ============================================================================
# 生成 manifest
# ============================================================================
Write-Host ""
Write-Host "--- 生成 manifest ---" -ForegroundColor Yellow

$manifest = [PSCustomObject]@{
    schema = "build.manifest/v1"
    project = "AstroCS"
    generated_at = (Get-Date -Format "o")
    baseline_tag = $lock.baseline_tag
    config = $Config
    clean_build = $Clean.IsPresent
    toolchain_snapshot = @{
        gcc_version = (& gcc -dumpversion 2>&1)
        gxx_version = (& g++ -dumpversion 2>&1)
        mingw32_make = ((& mingw32-make --version 2>&1 | Select-Object -First 1) -replace "^GNU Make\s+", "")
        msys2_bin = $msys2Bin
    }
    build_order = $targets
    results = $results
    summary = [PSCustomObject]@{
        total = $results.Count
        ok = @($results | Where-Object { $_.status -eq "OK" }).Count
        fail = @($results | Where-Object { $_.status -eq "FAIL" }).Count
        skip = @($results | Where-Object { $_.status -eq "SKIP" }).Count
        total_build_time_sec = ($results | Measure-Object -Property build_time_sec -Sum).Sum
    }
}

$manifest | ConvertTo-Json -Depth 6 | Out-File -FilePath $manifestPath -Encoding UTF8
Write-Host "  manifest: $manifestPath" -ForegroundColor Green

# ============================================================================
# 汇总
# ============================================================================
Write-Host ""
Write-Host "--- 构建汇总 ---" -ForegroundColor Cyan
$results | Format-Table -AutoSize -Property module, status, build_time_sec, output, sha256

$okCount = @($results | Where-Object { $_.status -eq "OK" }).Count
Write-Host ""
Write-Host "OK: $okCount / $($results.Count)  FAIL: $failCount" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Red" })
if ($failCount -gt 0) {
    Write-Host "  构建失败, 请查看日志: $logsDir" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "[OK] 所有模块构建成功" -ForegroundColor Green
    exit 0
}
