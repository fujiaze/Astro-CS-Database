# ============================================================================
# AstroCS 统一工具链 (唯一入口, 2026-08-04 确定)
# ============================================================================
param(
    [Parameter(Position=0)][string]$Command = "check",
    [string]$JsonPath,
    [string]$Topic,
    [string]$Date
)

$AstroCS_MSYS2  = "C:\msys64\mingw64\bin"
$AstroCS_PYTHON = "C:\Users\fujia\AppData\Local\Programs\Python\Python312\python.exe"
$AstroCS_GH     = "C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe"
$AstroCS_ROOT   = $PSScriptRoot

function Set-AstroCSEnv {
    if ($env:Path -notlike "*$AstroCS_MSYS2*") {
        $env:Path = "$AstroCS_MSYS2;$env:Path"
        Write-Host "[AstroCS] PATH 已加入 MSYS2 MinGW64" -ForegroundColor Green
    }
}

function Get-AstroCSPython {
    if (Test-Path -LiteralPath $AstroCS_PYTHON) { return $AstroCS_PYTHON }
    Write-Warning "[AstroCS] 未找到规范 Python $AstroCS_PYTHON, 回退到 PATH 中的 python"
    return (Get-Command python -ErrorAction SilentlyContinue).Source
}

function Test-AstroCSToolchain {
    Set-AstroCSEnv
    Write-Host "===== AstroCS 工具链自检 =====" -ForegroundColor Cyan
    $checks = @(
        @{n="g++ (MinGW64)"; c="g++";   a="--version"},
        @{n="GNU Make";      c="make";  a="--version"},
        @{n="CMake";         c="cmake"; a="--version"},
        @{n="Ninja";         c="ninja"; a="--version"},
        @{n="Git";           c="git";   a="--version"},
        @{n="gh";            c=$AstroCS_GH; a="--version"},
        @{n="Python 规范";   c=$AstroCS_PYTHON; a="--version"}
    )
    foreach ($it in $checks) {
        try {
            $out = & $it.c $it.a 2>&1 | Select-Object -First 1
            Write-Host ("{0,-16} {1}" -f ($it.n + ":"), $out) -ForegroundColor Green
        } catch {
            Write-Host ("{0,-16} 缺失 ({1})" -f ($it.n + ":"), $it.c) -ForegroundColor Red
        }
    }
    $nl = "C:\msys64\mingw64\include\nlohmann\json.hpp"
    Write-Host ("{0,-16} {1}" -f "nlohmann/json:", $(if (Test-Path $nl) { "已安装" } else { "缺失!" })) -ForegroundColor $(if (Test-Path $nl) { "Green" } else { "Red" })
    $orc = Join-Path $AstroCS_ROOT "lib\orchestrator\cpp\orchestrator.exe"
    if (Test-Path $orc) {
        $v = & $orc --version 2>&1 | Select-Object -First 1
        Write-Host ("{0,-16} {1}" -f "orchestrator:", $v) -ForegroundColor Green
    } else {
        Write-Host "orchestrator: 未编译 (先执行 build)" -ForegroundColor Yellow
    }
}

function Build-AstroCSAll {
    Set-AstroCSEnv
    $modules = @(
        "lib\astro_image_io",
        "lib\calibration",
        "lib\dynamic_psf",
        "lib\plate_solve\cpp\ipv",
        "lib\star_detector",
        "lib\snr_estimator\cpp",
        "lib\photometric_calib\cpp",
        "lib\healpix_db\healpix_drizzle",
        "lib\gaia_xpsd_client",
        "lib\orchestrator\cpp"
    )
    $fail = $false
    foreach ($m in $modules) {
        $dir = Join-Path $AstroCS_ROOT $m
        Write-Host "=== $m ===" -ForegroundColor Cyan
        Push-Location $dir
        try { & make 2>&1 | Select-Object -Last 3 } catch { $fail = $true }
        Pop-Location
        if ($LASTEXITCODE -ne 0) { $fail = $true; Write-Host "编译失败: $m" -ForegroundColor Red }
    }
    Write-Host "healpix_stack (Stage2 冻结): 跳过, 使用现有 DLL" -ForegroundColor Yellow
    if ($fail) { Write-Host "存在失败模块" -ForegroundColor Red; exit 1 }
    Write-Host "全部模块编译完成" -ForegroundColor Green
}

function Invoke-AstroCSOrchestrator {
    param([Parameter(Mandatory=$true)][string]$JsonPath)
    Set-AstroCSEnv
    $exe = Join-Path $AstroCS_ROOT "lib\orchestrator\cpp\orchestrator.exe"
    if (-not (Test-Path $exe)) { Write-Error "未找到 $exe, 先执行 build"; return 1 }
    & $exe $JsonPath
    return $LASTEXITCODE
}

function New-AstroCSReviewPack {
    param(
        [Parameter(Mandatory=$true)][string]$Topic,
        [string]$Date
    )
    if ($Topic -notmatch '^[A-Za-z0-9_-]+$') { throw "Topic 只能包含字母/数字/_/-" }
    if (-not $Date) { $Date = Get-Date -Format "yyyyMMdd" }

    $top = "AstroCS_Review_${Topic}_${Date}"
    $stage = Join-Path $AstroCS_ROOT "run\temp\review_${Topic}_${Date}"
    $topDir = Join-Path $stage $top
    $zip = Join-Path $AstroCS_ROOT "$top.zip"

    foreach ($p in @($stage, $zip)) {
        if (Test-Path -LiteralPath $p) {
            if ((Get-Item -LiteralPath $p).PSIsContainer) { Remove-Item -LiteralPath $p -Recurse -Force }
            else { Remove-Item -LiteralPath $p -Force }
        }
    }
    New-Item -ItemType Directory -Force -Path $topDir | Out-Null
    foreach ($d in @("wiki","source","docs","schemas","evidence","raw_logs")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $topDir $d) | Out-Null
    }

    $wikiSrc = Join-Path $AstroCS_ROOT "AstroCS.wiki"
    if (Test-Path $wikiSrc) {
        Get-ChildItem $wikiSrc -Filter *.md | Copy-Item -Destination (Join-Path $topDir "wiki")
    }

    $tmpLib = Join-Path $stage "_src_lib.zip"
    Push-Location $AstroCS_ROOT
    & git archive --format=zip -o $tmpLib HEAD lib
    Pop-Location
    if (Test-Path $tmpLib) {
        Expand-Archive -LiteralPath $tmpLib -DestinationPath (Join-Path $topDir "source") -Force
        Remove-Item -LiteralPath $tmpLib -Force
    }

    foreach ($fn in @("README.md","AGENTS.md","HANDOVER.md","toolchain.ps1")) {
        $src = Join-Path $AstroCS_ROOT $fn
        if (Test-Path $src) { Copy-Item $src (Join-Path $topDir "docs") }
    }

    foreach ($fn in @("stage1.schema.json","stage1.template.json")) {
        $src = Join-Path $AstroCS_ROOT "lib\orchestrator\configs\$fn"
        if (Test-Path $src) { Copy-Item $src (Join-Path $topDir "schemas") }
    }

    $evSrc = Join-Path $AstroCS_ROOT "工程控制\evidence\R10-001"
    if (Test-Path $evSrc) { Copy-Item -Recurse $evSrc (Join-Path $topDir "evidence") }

    $logDir = Join-Path $AstroCS_ROOT "run\logs\r10"
    foreach ($fn in @("fp32_snr_fix_verify_20260804.log","fp64_snr_fix_verify_20260804.log",
                      "synthetic_hiss_precision.log","python_entry_audit.csv")) {
        $src = Join-Path $logDir $fn
        if (Test-Path $src) { Copy-Item $src (Join-Path $topDir "raw_logs") }
    }

    Push-Location $AstroCS_ROOT
    $head = (& git rev-parse HEAD).Trim()
    $log = (& git log --oneline -25)
    $branches = (& git branch -a)
    $diff = (& git diff --stat HEAD~8..HEAD)
    Pop-Location
    $refs = "HEAD: $head`nbranch: main`n`n-- git log --oneline -25 --`n$log`n`n-- branches --`n$branches"
    Set-Content -LiteralPath (Join-Path $topDir "git_refs.txt") -Value $refs -Encoding utf8
    Set-Content -LiteralPath (Join-Path $topDir "git_diff_stats.txt") -Value "diff --stat HEAD~8..HEAD`n$diff" -Encoding utf8

    $packReadme = @(
        "# $top",
        "",
        "日期: $(Get-Date -Format 'yyyy-MM-dd') | HEAD: $head | 分支: main",
        "",
        "## 内容",
        "- wiki/         完整最新 Wiki",
        "- source/       main 必要源码 (git archive HEAD lib, 无生成产物)",
        "- docs/         README / AGENTS.md / HANDOVER.md / toolchain.ps1",
        "- schemas/      stage1.schema.json + stage1.template.json",
        "- evidence/     R10-001 报告 (实现/科学/ABI/SNR/已知问题/验收自审)",
        "- raw_logs/     合成测试 + FP32/FP64 单帧逐 Gate 原始日志",
        "- git_refs.txt / git_diff_stats.txt / SHA256SUMS.txt",
        "",
        "## 唯一运行入口",
        "orchestrator.exe <stage1.json>",
        "",
        "## 生成命令",
        ".\toolchain.ps1 review -Topic $Topic"
    )
    Set-Content -LiteralPath (Join-Path $topDir "README.md") -Value $packReadme -Encoding utf8

    $sums = Get-ChildItem $topDir -Recurse -File | Get-FileHash -Algorithm SHA256
    $lines = foreach ($s in $sums) {
        $rel = [IO.Path]::GetRelativePath($topDir, $s.Path).Replace('\','/')
        "{0}  {1}" -f $s.Hash.ToLower(), $rel
    }
    Set-Content -LiteralPath (Join-Path $topDir "SHA256SUMS.txt") -Value ($lines | Sort-Object) -Encoding ascii

    Add-Type -AssemblyName System.IO.Compression.FileSystem | Out-Null
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip)
    $zh = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
    $size = (Get-Item -LiteralPath $zip).Length
    Write-Host "[AstroCS] 审核包已生成: $zip" -ForegroundColor Green
    Write-Host "[AstroCS] 大小: $size bytes | SHA256: $zh" -ForegroundColor Cyan
    return $zip
}

if ($MyInvocation.InvocationName -ne ".") {
    switch ($Command.ToLower()) {
        "check"  { Test-AstroCSToolchain }
        "env"    { Set-AstroCSEnv; Write-Host "[AstroCS] 直接运行不会保留 PATH, 请 dot-source: . .\toolchain.ps1" -ForegroundColor Yellow }
        "build"  { Build-AstroCSAll }
        "run"    { if (-not $JsonPath) { Write-Error "用法: .\toolchain.ps1 run <stage1.json>"; exit 1 }
                   Invoke-AstroCSOrchestrator -JsonPath $JsonPath }
        "review" { if (-not $Topic) { Write-Error "用法: .\toolchain.ps1 review -Topic <主题>"; exit 1 }
                   New-AstroCSReviewPack -Topic $Topic -Date $Date }
        default  { Write-Host "用法: .\toolchain.ps1 {check|env|build|run|review}" }
    }
}
