# build.ps1 - Photometric Calib C++ DLL build script
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1

# 全局强制UTF-8编码（覆盖系统默认GBK）
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
[System.Text.Encoding]::Default = [System.Text.Encoding]::UTF8

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$gpp = "C:\msys64\mingw64\bin\g++.exe"

if (-not (Test-Path $gpp)) {
    Write-Host "Error: g++ not found. Please install MSYS2 MinGW64." -ForegroundColor Red
    Write-Host "Expected path: $gpp" -ForegroundColor Yellow
    exit 1
}

Write-Host "=== Photometric Calib C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

$srcFiles = @(
    "src/pc_api.cpp",
    "src/star_matcher.cpp",
    "src/image_corrector.cpp",
    "src/wcs_transform.cpp"
)

$includeDir = "include"
$srcDir = "src"
$outputDll = "photometric_calib.dll"

$compileArgs = @(
    "-O2", "-std=c++17", "-fopenmp", "-fPIC", "-Wall",
    "-shared", "-fopenmp",
    "-o", $outputDll,
    "-I$includeDir",
    "-I$srcDir",
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
