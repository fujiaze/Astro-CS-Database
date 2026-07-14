# build.ps1 - HEALPix IO C++ DLL build script
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

Write-Host "=== HEALPix IO C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

Set-Location $scriptDir

$outputDll = "healpix_io.dll"

Write-Host "Output: $outputDll"
Write-Host ""

# 用 & 直接调用 g++, 正确处理含空格的路径参数
# 注意: -lzstd 必须在源文件之后, 否则链接器找不到符号
$output = & $gpp -O2 -std=c++17 -Wall -fPIC -shared `
    -o $outputDll `
    -Iinclude `
    src/healpix_io.cpp `
    -lzstd -static 2>&1

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
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
