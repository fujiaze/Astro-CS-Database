# build.ps1 - HEALpix Stack C++ DLL build script
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1
#
# 2026-07-16: healpix_io merged into aio (spec G1). Now links only astro_image_io.dll.

# Force UTF-8 encoding (override system default GBK)
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

# Prepend mingw64/bin to PATH (g++ needs cc1plus/ld etc.)
if (Test-Path $mingwBin) {
    $env:Path = "$mingwBin;" + $env:Path
}

Write-Host "=== HEALpix Stack C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

Set-Location $scriptDir

# Dependency directories
# aio now provides FITS/XISF/HEALPix I/O (healpix_io merged in 2026-07-16)
$aioDir = Join-Path $scriptDir "..\..\astro_image_io"
$gaiaDir = Join-Path $scriptDir "..\..\gaia_xpsd_client"

# Check dependency DLL
$aioDll = Join-Path $aioDir "astro_image_io.dll"

if (-not (Test-Path $aioDll)) {
    Write-Host "Error: astro_image_io.dll not found at $aioDll" -ForegroundColor Red
    Write-Host "Please build astro_image_io first (with enable_healpix=true)." -ForegroundColor Yellow
    exit 1
}

Write-Host "Dependencies:"
Write-Host "  astro_image_io.dll: $aioDll (provides FITS/XISF/HEALPix I/O)"
Write-Host "  gaia_client source: $gaiaDir\src\gaia_client.c"

# Ensure astro_image_io.dll has import library (libastro_image_io.dll.a)
# MinGW links DLL via import library; generate if missing
$aioImplib = Join-Path $aioDir "libastro_image_io.dll.a"
if (-not (Test-Path $aioImplib)) {
    Write-Host ""
    Write-Host "Generating import library for astro_image_io.dll..." -ForegroundColor Yellow
    $dlltool = "C:\msys64\mingw64\bin\dlltool.exe"
    $defFile = Join-Path $env:TEMP "astro_image_io.def"

    # Generate .def file from dll exports
    $exports = & $dlltool --exports $aioDll 2>$null
    if ($LASTEXITCODE -eq 0 -and $exports) {
        "EXPORTS" | Out-File -FilePath $defFile -Encoding ASCII
        $exports | ForEach-Object {
            if ($_ -match '^\s*\d+\s+\w+') {
                $parts = $_ -split '\s+' | Where-Object { $_ }
                if ($parts.Count -ge 2) {
                    $parts[1] | Out-File -FilePath $defFile -Encoding ASCII -Append
                }
            }
        }
        & $dlltool -d $defFile -l $aioImplib -D $aioDll 2>$null
        if (Test-Path $aioImplib) {
            Write-Host "  [OK] Generated: $aioImplib" -ForegroundColor Green
        }
    }
}

$outputDll = "healpix_stack.dll"
Write-Host ""
Write-Host "Output: $outputDll"
Write-Host ""

# Compile command
# -lzstd -llz4: astro_image_io dependencies
# -lz: gaia_client dependency (zlib)
# -L$(AIO_DIR) -lastro_image_io: link astro_image_io.dll (FITS/XISF/HEALPix I/O)
# AIO_ENABLE_HEALPIX: enable aio_healpix_io.h declarations + backward-compat macros
# gradient/ subdirectory: gradient correction modules (snr_evaluator, spherical_spline,
#                        gradient_sampler, gradient_fitter, corrected_stacker)
# gaia_client.c: compiled into DLL (avoid external DLL dependency)
$srcFiles = @(
    "healpix_core.cpp",
    "ahps_reader.cpp",
    "ahps_writer.cpp",
    "stack_db.cpp",
    "stack_engine.cpp",
    "hp_stack_api.cpp",
    "hp_stack_hiss.cpp",
    "gradient\snr_evaluator.cpp",
    "gradient\spherical_spline.cpp",
    "gradient\gradient_sampler.cpp",
    "gradient\gradient_fitter.cpp",
    "gradient\corrected_stacker.cpp",
    "$gaiaDir\src\gaia_client.c"
)

$eigenInclude = "C:\msys64\mingw64\include\eigen3"

$output = & $gpp -O3 -std=c++17 -Wall -fopenmp -shared `
    -o $outputDll `
    -I. `
    -I"gradient" `
    -I"$aioDir\include" `
    -I"$aioDir\src" `
    -I"$gaiaDir\src" `
    -I"$eigenInclude" `
    -DHAS_ZSTD -DHAS_LZ4 -DAIO_ENABLE_HEALPIX `
    -static-libgcc -static-libstdc++ `
    $srcFiles `
    -L"$aioDir" -lastro_image_io `
    -lzstd -llz4 -lz -lm 2>&1

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

    # Verify exported symbols
    Write-Host ""
    Write-Host "Verifying exported symbols..." -ForegroundColor Cyan
    $nmOutput = & "C:\msys64\mingw64\bin\nm.exe" -g --defined-only (Join-Path $scriptDir $outputDll) 2>&1
    $exports = $nmOutput | Where-Object { $_ -match ' T ' }
    $hpStackSymbols = $exports | Where-Object { $_ -match 'hp_stack' }
    Write-Host "  hp_stack_* exported symbols:"
    $hpStackSymbols | ForEach-Object {
        $sym = ($_ -split '\s+')[-1]
        Write-Host "    $sym" -ForegroundColor Green
    }
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
