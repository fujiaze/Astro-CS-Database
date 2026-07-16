# build.ps1 - Gradient2D C++ DLL build script
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1
#
# 编译 lib/photometric_calib/cpp/gradient_2d/ 下的源码为 gradient_2d.dll
# 该 DLL 实现 STAGE_GRADIENT_2D (单帧 2D 测光校准)
# 不依赖 photometric_calib.dll (源码级复用 wcs_transform/star_matcher)

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

Write-Host "=== Gradient2D C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

Set-Location $scriptDir

$outputDll = "gradient_2d.dll"

# 源文件列表 (gradient_2d 模块自包含, 不链接 photometric_calib.dll)
$srcFiles = @(
    "src/gradient_2d_api.cpp",
    "src/gradient_fitter.cpp",
    "src/image_corrector.cpp",
    "src/star_matcher.cpp",
    "src/wcs_transform.cpp"
)

Write-Host "Sources: $($srcFiles -join ', ')"
Write-Host "Output: $outputDll"
Write-Host ""

# 捕获输出 (2>&1 合并 stderr)
$output = & $gpp -O2 -std=c++17 -fopenmp -fPIC -Wall -shared -fopenmp `
    -o $outputDll `
    -Iinclude -Isrc `
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

    # 验证导出符号 (PE 格式用 objdump -p 检查导出表)
    $exports = & "C:\msys64\mingw64\bin\objdump.exe" -p (Join-Path $scriptDir $outputDll) 2>$null |
               Select-String "gradient_2d_calibrate"
    if ($exports) {
        Write-Host "  Export check: gradient_2d_calibrate FOUND" -ForegroundColor Cyan
    } else {
        Write-Host "  Export check: gradient_2d_calibrate NOT FOUND (WARNING)" -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
