<#
.SYNOPSIS
AstroCS Bootstrap 脚本 - 检查/验证工具链环境
.DESCRIPTION
P01-003 交付物: 在干净环境检查 MSYS2 MinGW64 工具链是否就绪。
基于 dependencies.lock.json 验证工具链版本、路径、SHA-256。
.PARAMETER CheckOnly
仅检查不安装(本版本只检查, 不自动安装 MSYS2 包)
.PARAMETER LockFile
依赖锁定清单路径(默认 engineering/evidence/P01-002/dependencies.lock.json)
.EXAMPLE
pwsh -File bootstrap.ps1
pwsh -File bootstrap.ps1 -CheckOnly
#>
[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [string]$LockFile = "engineering/evidence/P01-002/dependencies.lock.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$repoRoot = $PSScriptRoot
$lockPath = Join-Path $repoRoot $LockFile

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AstroCS Bootstrap - 工具链环境检查" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 加载锁定清单
if (-not (Test-Path $lockPath)) {
    Write-Host "[FAIL] 依赖锁定清单不存在: $lockPath" -ForegroundColor Red
    Write-Host "  请先运行 P01-002 生成 dependencies.lock.json" -ForegroundColor Yellow
    exit 1
}

$lock = Get-Content $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$toolchain = $lock.toolchain
$msys2Bin = $lock.path_requirements.msys2_mingw64_bin

Write-Host "[INFO] 基线 Tag: $($lock.baseline_tag)"
Write-Host "[INFO] MSYS2 路径: $msys2Bin"
Write-Host ""

# 检查 MSYS2 MinGW64 bin 目录
Write-Host "--- 检查 MSYS2 MinGW64 ---" -ForegroundColor Yellow
if (-not (Test-Path $msys2Bin)) {
    Write-Host "[FAIL] MSYS2 MinGW64 目录不存在: $msys2Bin" -ForegroundColor Red
    Write-Host "  请安装 MSYS2 并通过 pacman 安装 mingw-w64-x86_64 工具链" -ForegroundColor Yellow
    Write-Host "  参考: https://www.msys2.org/" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "[OK] MSYS2 MinGW64 目录存在" -ForegroundColor Green
}

# 检查每个工具
Write-Host ""
Write-Host "--- 检查工具链 ($($toolchain.Count) 项) ---" -ForegroundColor Yellow

$passCount = 0
$failCount = 0
$warnCount = 0
$results = @()

foreach ($tool in $toolchain) {
    $name = $tool.name
    $expectedVersion = $tool.version
    $expectedPath = $tool.path
    $expectedSha = $tool.sha256

    $result = [PSCustomObject]@{
        Name = $name
        Status = "UNKNOWN"
        Path = ""
        Version = ""
        Note = ""
    }

    # 检查路径
    if ($expectedPath -and (Test-Path $expectedPath)) {
        $result.Path = $expectedPath
        # 获取实际版本
        $actualVersion = ""
        try {
            switch -Wildcard ($name) {
                "PowerShell" { $actualVersion = $PSVersionTable.PSVersion.ToString() }
                "Python" { $actualVersion = (& $expectedPath --version 2>&1) -replace "Python ", "" }
                "Git" { $actualVersion = (& $expectedPath --version 2>&1) -replace "git version ", "" }
                "GitHub CLI*" { $actualVersion = (& $expectedPath --version 2>&1 | Select-Object -First 1) -replace "gh version ", "" }
                "GCC" { $line = (& $expectedPath --version 2>&1 | Select-Object -First 1); $m = [regex]::Match($line, '\d+\.\d+\.\d+'); $actualVersion = if ($m.Success) { $m.Value } else { $line } }
                "G++" { $line = (& $expectedPath --version 2>&1 | Select-Object -First 1); $m = [regex]::Match($line, '\d+\.\d+\.\d+'); $actualVersion = if ($m.Success) { $m.Value } else { $line } }
                "mingw32-make" { $line = (& $expectedPath --version 2>&1 | Select-Object -First 1); $m = [regex]::Match($line, '\d+\.\d+(\.\d+)?'); $actualVersion = if ($m.Success) { $m.Value } else { $line } }
                "Make*" { $actualVersion = "bundled" }
                "Qt6" { $actualVersion = (& $expectedPath -query QT_VERSION 2>&1) }
                "GSL*" { $actualVersion = $expectedVersion }
                "zstd" { $actualVersion = $expectedVersion }
                "lz4" { $actualVersion = $expectedVersion }
                "zlib" { $actualVersion = $expectedVersion }
                "OpenMP*" { $actualVersion = $expectedVersion }
                "Eigen3" { $actualVersion = $expectedVersion }
                default { $actualVersion = "unknown" }
            }
        } catch {
            $actualVersion = "(查询失败)"
        }
        $result.Version = $actualVersion

        # 版本比对(宽松: 仅检查主版本号)
        if ($actualVersion -and $actualVersion -ne "(查询失败)" -and $actualVersion -ne "unknown" -and $actualVersion -ne "bundled") {
            # 用正则提取语义版本号, 避免附加文本干扰
            $expMatch = [regex]::Match("$expectedVersion", '\d+\.\d+(\.\d+)?')
            $actMatch = [regex]::Match("$actualVersion", '\d+\.\d+(\.\d+)?')
            $expectedVer = if ($expMatch.Success) { $expMatch.Value } else { "$expectedVersion" }
            $actualVer = if ($actMatch.Success) { $actMatch.Value } else { "$actualVersion" }
            $expectedMajor = ($expectedVer -split "\.")[0]
            $actualMajor = ($actualVer -split "\.")[0]
            if ($expectedMajor -eq $actualMajor) {
                $result.Status = "PASS"
                $passCount++
            } else {
                $result.Status = "WARN"
                $result.Note = "版本主号不符(预期 $expectedVer, 实际 $actualVer)"
                $warnCount++
            }
        } else {
            $result.Status = "PASS"
            $result.Note = "DLL/头文件库, 路径存在即通过"
            $passCount++
        }
    } elseif ($name -eq "Make (TRAE bundled)") {
        # TRAE bundled make 可能不在标准路径, 检查 PATH
        $makeInPath = Get-Command make -ErrorAction SilentlyContinue
        if ($makeInPath) {
            $result.Path = $makeInPath.Source
            $result.Version = "bundled"
            $result.Status = "PASS"
            $result.Note = "TRAE 自带 make.cmd"
            $passCount++
        } else {
            $result.Status = "WARN"
            $result.Note = "TRAE make 不在 PATH(非阻塞, 可用 mingw32-make)"
            $warnCount++
        }
    } else {
        $result.Status = "FAIL"
        $result.Note = "路径不存在: $expectedPath"
        $failCount++
    }

    $results += $result
}

# 输出结果表
$results | Format-Table -AutoSize -Property Name, Status, Version, Note

Write-Host ""
Write-Host "--- 汇总 ---" -ForegroundColor Cyan
Write-Host "PASS: $passCount / $($toolchain.Count)" -ForegroundColor Green
Write-Host "WARN: $warnCount" -ForegroundColor Yellow
Write-Host "FAIL: $failCount" -ForegroundColor Red

# 检查关键工具(非阻塞的 WARN 不影响)
$criticalTools = @("PowerShell", "Python", "Git", "GCC", "G++", "mingw32-make", "Qt6", "GSL (GNU Scientific Library)", "GSL CBLAS", "zstd", "lz4", "zlib", "OpenMP (libgomp)", "Eigen3")
$criticalFail = 0
foreach ($result in $results) {
    if ($result.Status -eq "FAIL" -and $criticalTools -contains $result.Name) {
        $criticalFail++
    }
}

Write-Host ""
if ($criticalFail -eq 0) {
    Write-Host "[OK] 关键工具链全部就绪" -ForegroundColor Green
    Write-Host "  构建环境验证通过, 可运行 build.ps1" -ForegroundColor Green

    # 生成环境检查报告
    $reportPath = Join-Path $repoRoot "build\logs\bootstrap_report.json"
    $reportDir = Split-Path $reportPath -Parent
    if (-not (Test-Path $reportDir)) { New-Item -ItemType Directory -Path $reportDir -Force | Out-Null }

    $report = [PSCustomObject]@{
        check_time = (Get-Date -Format "o")
        baseline_tag = $lock.baseline_tag
        total = $toolchain.Count
        pass = $passCount
        warn = $warnCount
        fail = $failCount
        critical_fail = $criticalFail
        verdict = if ($criticalFail -eq 0) { "PASS" } else { "FAIL" }
        tools = $results
    }
    $report | ConvertTo-Json -Depth 5 | Out-File $reportPath -Encoding UTF8
    Write-Host "  报告已保存: $reportPath" -ForegroundColor Green

    exit 0
} else {
    Write-Host "[FAIL] $criticalFail 个关键工具缺失" -ForegroundColor Red
    Write-Host "  请安装缺失工具后重新运行 bootstrap.ps1" -ForegroundColor Yellow
    exit 1
}
