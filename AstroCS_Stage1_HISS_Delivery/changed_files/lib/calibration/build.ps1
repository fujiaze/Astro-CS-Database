# build.ps1 - Astro Calibration C++ DLL build script
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1

[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$gpp = "C:\msys64\mingw64\bin\g++.exe"

if (-not (Test-Path $gpp)) {
    Write-Host "Error: g++ not found. Please install MSYS2 MinGW64." -ForegroundColor Red
    Write-Host "Expected path: $gpp" -ForegroundColor Yellow
    exit 1
}

# 将 g++ 所在目录加入 PATH：cc1plus/as/ld 等子进程依赖 mingw64\bin 下的运行时 DLL
# (libwinpthread/libgcc_s_seh 等)，否则会以 0xC0000135 静默失败。
$gppBinDir = Split-Path -Parent $gpp
if ($env:Path -notlike "*$gppBinDir*") {
    $env:Path = "$gppBinDir;$env:Path"
}

Write-Host "=== Astro Calibration C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

$srcFiles = @(
    "src/master_generator.cpp",
    "src/calibrator.cpp",
    "src/dark_optimizer.cpp",
    "src/cosmetic_corrector.cpp",
    "src/ac_api.cpp"
)

$includeDir = "include"
$outputDll = "astro_calibration.dll"

$compileArgs = @(
    "-O2", "-march=native", "-Wall", "-std=c++17", "-shared", "-fopenmp",
    "-o", $outputDll,
    "-I$includeDir",
    "-I../astro_image_io/include",
    "-static",
    "-lm"
)
$compileArgs += $srcFiles

Write-Host "Sources: $($srcFiles -join ', ')"
Write-Host "Output: $outputDll"
Write-Host ""

$proc = Start-Process -FilePath $gpp -ArgumentList $compileArgs -NoNewWindow -PassThru -RedirectStandardError "build_error.txt" -RedirectStandardOutput "build_output.txt" -WorkingDirectory $scriptDir
$proc.WaitForExit()

if (Test-Path "build_error.txt") {
    $errContent = Get-Content "build_error.txt" -Raw -Encoding UTF8
    if ($errContent) { Write-Host $errContent -ForegroundColor Yellow }
}

if ($proc.ExitCode -eq 0 -and (Test-Path (Join-Path $scriptDir $outputDll))) {
    $dllInfo = Get-Item (Join-Path $scriptDir $outputDll)
    Write-Host ""
    Write-Host "Build SUCCESS!" -ForegroundColor Green
    Write-Host "  DLL: $($dllInfo.FullName)"
    Write-Host "  Size: $([math]::Round($dllInfo.Length / 1KB, 1)) KB"
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $($proc.ExitCode)" -ForegroundColor Red
}

Remove-Item "build_error.txt", "build_output.txt" -ErrorAction SilentlyContinue
