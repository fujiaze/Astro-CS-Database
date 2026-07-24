# build.ps1 - Photometric Calib C++ DLL build script
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1

# 全局强制UTF-8编码（覆盖系统默认GBK）
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try { [System.Text.Encoding]::Default = [System.Text.Encoding]::UTF8 } catch {}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$gpp = "C:\msys64\mingw64\bin\g++.exe"
$mingwBin = "C:\msys64\mingw64\bin"

if (-not (Test-Path $gpp)) {
    Write-Host "Error: g++ not found. Please install MSYS2 MinGW64." -ForegroundColor Red
    Write-Host "Expected path: $gpp" -ForegroundColor Yellow
    exit 1
}

# 把 mingw64/bin 加入 PATH (g++ 需要找到 cc1plus/ld 等子进程)
if (Test-Path $mingwBin) {
    $env:Path = "$mingwBin;" + $env:Path
}

Write-Host "=== Photometric Calib C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

# gaia_client 依赖 (头文件 + dll)
$gaiaDir = Join-Path $scriptDir "..\..\gaia_xpsd_client"
$gaiaIncDir = Join-Path $gaiaDir "src"
$gaiaDll = Join-Path $gaiaDir "gaia_client.dll"
$outputDll = "photometric_calib.dll"

if (-not (Test-Path $gaiaDll)) {
    Write-Host "Error: gaia_client.dll not found at $gaiaDll" -ForegroundColor Red
    Write-Host "Please build gaia_xpsd_client first (run make in lib/gaia_xpsd_client/)" -ForegroundColor Yellow
    exit 1
}
Write-Host "Gaia client DLL: $gaiaDll"

Set-Location $scriptDir

# 用 & 直接调用 g++, 正确处理含空格的路径参数
$srcFiles = @(
    "src/pc_api.cpp",
    "src/star_matcher.cpp",
    "src/image_corrector.cpp",
    "src/wcs_transform.cpp",
    "src/spectrum_integrator.cpp"
)

Write-Host "Sources: $($srcFiles -join ', ')"
Write-Host "Output: $outputDll"
Write-Host ""

# 捕获输出 (2>&1 合并 stderr)
$output = & $gpp -O2 -std=c++17 -fopenmp -fPIC -Wall -shared -fopenmp `
    -o $outputDll `
    -Iinclude -Isrc "-I$gaiaIncDir" `
    "$gaiaDll" `
    -static -lm `
    $srcFiles 2>&1

$exitCode = $LASTEXITCODE

if ($output) {
    Write-Host "---- compiler output ----" -ForegroundColor Yellow
    $output | ForEach-Object { Write-Host $_ }
}

if ($exitCode -eq 0 -and (Test-Path (Join-Path $scriptDir $outputDll))) {
    $dllInfo = Get-Item (Join-Path $scriptDir $outputDll)
    Write-Host ""
    Write-Host "Build SUCCESS!" -ForegroundColor Green
    Write-Host "  DLL: $($dllInfo.FullName)"
    Write-Host "  Size: $([math]::Round($dllInfo.Length / 1KB, 1)) KB"
    # 复制 gaia_client.dll 到当前目录, 保证运行时 Python 能加载依赖
    Copy-Item -Path $gaiaDll -Destination (Join-Path $scriptDir "gaia_client.dll") -Force
    Write-Host "  Copied gaia_client.dll -> $scriptDir" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
