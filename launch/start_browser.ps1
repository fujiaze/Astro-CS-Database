#!/usr/bin/env pwsh
<#
AstroCS HiPS Browser — 一键启动（V9）

用法:
  pwsh -File .\launch\start_browser.ps1                 # 默认 Galactic Center 3-panel Red
  pwsh -File .\launch\start_browser.ps1 -HipsPath <path> # 自定义 HiPS 产品集目录
  pwsh -File .\launch\start_browser.ps1 -Preset "Overlap 1-2"

行为:
  - 自动定位/构建 healpix_browser_qt.exe（已有 build 则直接使用）；
  - 默认加载 run/phase2/v7/gc_3panel.mosaic.hips（V8 银心三 panel 真实 mosaic）；
  - 本地进程内渲染，无 HTTP server（不绑定任何端口；如需 server 仅允许 127.0.0.1）；
  - readiness wait 带 30s timeout；Ctrl+C 或关闭窗口后清理子进程。
#>
param(
    [string]$HipsPath = "",
    [string]$Preset = "GC Wide"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "lib\healpix_db\healpix_browser_qt\build\healpix_browser_qt.exe"

# ---------------------------------------------------------------------------
# 1. 定位/构建浏览器
# ---------------------------------------------------------------------------
if (!(Test-Path -LiteralPath $exe)) {
    Write-Host "[start_browser] 未找到 $exe，尝试构建浏览器 ..."
    $env:Path = "C:\msys64\mingw64\bin;$env:Path"
    Push-Location (Join-Path $repo "lib\healpix_db\healpix_browser_qt")
    try {
        cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DBUILD_TESTS=OFF -G Ninja
        ninja -C build healpix_browser_qt
    } finally {
        Pop-Location
    }
    if (!(Test-Path -LiteralPath $exe)) {
        Write-Error "浏览器构建失败：$exe 不存在。请先手动构建。"
        exit 1
    }
}

# ---------------------------------------------------------------------------
# 2. 解析默认 HiPS 路径（V8 银心三 panel Red mosaic）
# ---------------------------------------------------------------------------
if ([string]::IsNullOrEmpty($HipsPath)) {
    $candidates = @(
        (Join-Path $repo "run\phase2\v7\gc_3panel.mosaic.hips"),
        (Join-Path $repo "run\phase2\v6\real_3frame.mosaic.hips"),
        (Join-Path $repo "run\temp\phase1_freeze\T2_v3.hips")
    )
    $found = $candidates | Where-Object {
        Test-Path -LiteralPath (Join-Path $_ "signal")
    } | Select-Object -First 1
    if ($found) {
        $HipsPath = $found
    } else {
        Write-Host "[start_browser] 默认 GC 路径未找到，尝试从 V7/V8 证据 manifest 搜索 ..."
        $hits = Get-ChildItem -Path (Join-Path $repo "run\temp") -Recurse -Filter "manifest.json" `
            -ErrorAction SilentlyContinue | Where-Object {
                $_.FullName -match "gc_3panel|real_3frame"
            }
        foreach ($h in $hits) {
            $dir = $h.DirectoryName
            if (Test-Path -LiteralPath (Join-Path $dir "signal")) {
                $HipsPath = $dir
                break
            }
        }
    }
    if ([string]::IsNullOrEmpty($HipsPath)) {
        Write-Error @"
找不到 Galactic Center 3-panel Red HiPS。
候选路径（均需含 signal/support 子目录）：
  run\phase2\v7\gc_3panel.mosaic.hips
  run\phase2\v6\real_3frame.mosaic.hips
  run\temp\phase1_freeze\T2_v3.hips
请用 -HipsPath <路径> 显式指定。
"@
        exit 1
    }
    Write-Host "[start_browser] 默认 HiPS: $HipsPath"
}

if (!(Test-Path -LiteralPath (Join-Path $HipsPath "signal"))) {
    Write-Error "指定路径不是 HiPS 产品集（缺少 signal 子目录）：$HipsPath"
    exit 1
}

# ---------------------------------------------------------------------------
# 3. 启动 + readiness wait（30s timeout，localhost-only 说明）
# ---------------------------------------------------------------------------
$env:Path = "C:\msys64\mingw64\bin;" +
            (Join-Path $repo "lib\astro_image_io") + ";" + $env:Path

Write-Host "[start_browser] 启动浏览器: $exe"
Write-Host "[start_browser] 产品集: $HipsPath  Preset: $Preset"
Write-Host "[start_browser] 本地进程渲染，不启动 HTTP server（不监听任何端口）"

$args = "--hips `"$HipsPath`" --preset `"$Preset`""
$proc = Start-Process -FilePath $exe -ArgumentList $args -PassThru `
    -WorkingDirectory $repo -WindowStyle Normal

$ready = $false
$deadline = (Get-Date).AddSeconds(30)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ($proc.HasExited) { break }
    $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    if ($p -and $p.MainWindowTitle -ne "") {
        $ready = $true
        break
    }
}

if (!$ready) {
    if ($proc.HasExited) {
        Write-Error "浏览器提前退出（exit=$($proc.ExitCode)）。请检查 PATH/DLL。"
    } else {
        Write-Error "30s 内未等到浏览器窗口就绪，超时退出。"
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    exit 1
}

Write-Host ""
Write-Host "浏览器已就绪 (PID=$($proc.Id))。"
Write-Host "  - 平移: 按住左键拖动; 缩放: 滚轮 / 工具栏 放大/缩小"
Write-Host "  - Signal/Support: 工具栏 'Support' 按钮切换"
Write-Host "  - 预设: 状态栏右侧下拉框 (GC Wide / Overlap 1-2 / Overlap 2-3 / Seam / Support)"
Write-Host "  - 关闭: 关闭浏览器窗口，或在此按 Ctrl+C"
Write-Host ""

try {
    Wait-Process -Id $proc.Id -ErrorAction SilentlyContinue
    Write-Host "[start_browser] 浏览器已退出 (exit=$($proc.ExitCode))"
} finally {
    if (!$proc.HasExited) {
        Write-Host "[start_browser] 清理子进程 PID=$($proc.Id)"
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}
