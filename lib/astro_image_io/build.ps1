# build.ps1 - Astro Image IO (aio) C++ DLL build script
# Usage:
#   powershell -ExecutionPolicy Bypass -File build.ps1                      # default config (aio_build_config.json)
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Config "minimal"    # preset (minimal/full/healpix)
#   powershell -ExecutionPolicy Bypass -File build.ps1 -ConfigPath "path"   # explicit config path (overrides -Config)

param(
    [string]$Config = "",       # preset name: "" (default) / "full" / "minimal" / "healpix"
    [string]$ConfigPath = ""    # full path to config file (overrides -Config)
)

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

Write-Host "=== Astro Image IO (aio) C++ DLL Build ===" -ForegroundColor Cyan
Write-Host "Compiler: $gpp"
Write-Host "Working dir: $scriptDir"

Set-Location $scriptDir

# ============================================================================
# 1. Read build config (aio_build_config.json)
# ============================================================================
if ($ConfigPath -ne "") {
    $configFile = $ConfigPath
} elseif ($Config -ne "") {
    $configFile = Join-Path $scriptDir "aio_build_config.$Config.json"
} else {
    $configFile = Join-Path $scriptDir "aio_build_config.json"
}

if (-not (Test-Path $configFile)) {
    Write-Host "Error: config file not found: $configFile" -ForegroundColor Red
    exit 1
}

$buildConfig = Get-Content $configFile -Raw | ConvertFrom-Json
Write-Host "Config: $configFile"
Write-Host "  enable_fits      = $($buildConfig.enable_fits)"
Write-Host "  enable_xisf      = $($buildConfig.enable_xisf)"
Write-Host "  enable_ahpx      = $($buildConfig.enable_ahpx)"
Write-Host "  enable_healpix   = $($buildConfig.enable_healpix)"
Write-Host "  enable_compressor= $($buildConfig.enable_compressor)"
Write-Host "  enable_pipeline  = $($buildConfig.enable_pipeline)"
Write-Host "  enable_zstd      = $($buildConfig.enable_zstd)"
Write-Host "  enable_lz4       = $($buildConfig.enable_lz4)"

# ============================================================================
# 2. Build conditional compile defines
# ============================================================================
$defines = @()
if ($buildConfig.enable_fits)       { $defines += "-DAIO_ENABLE_FITS" }
if ($buildConfig.enable_xisf)       { $defines += "-DAIO_ENABLE_XISF" }
if ($buildConfig.enable_ahpx)       { $defines += "-DAIO_ENABLE_AHPX" }
if ($buildConfig.enable_healpix)    { $defines += "-DAIO_ENABLE_HEALPIX" }
if ($buildConfig.enable_compressor) { $defines += "-DAIO_ENABLE_COMPRESSOR" }
if ($buildConfig.enable_pipeline)   { $defines += "-DAIO_ENABLE_PIPELINE" }
if ($buildConfig.enable_zstd)       { $defines += "-DHAS_ZSTD" }
if ($buildConfig.enable_lz4)        { $defines += "-DHAS_LZ4" }

# ============================================================================
# 3. Build source file list
# ============================================================================
$srcFiles = @()
# core required sources
$srcFiles += "src/aio_log.cpp"
$srcFiles += "src/aio_api.cpp"
# HISS codec registry (RAW always built; LZ4/Zstd gated by enable_lz4/enable_zstd defines)
$srcFiles += "src/hiss_codec.cpp"
# HISS 共享方法 (compute_tile_*, DrizzleTileAccumulator::finalize_*, HissMetadata::to_json/from_json)
# 集中在此文件避免 Writer/Reader 重复定义
$srcFiles += "src/hiss_common.cpp"
# HISS Tile 几何模型 (步骤1: compute_tile_depth/nside, make_tile_geometry)
$srcFiles += "src/hiss_tile_model.cpp"
# HISS Transform 正式路径 (WP-G 步骤12: BYTE_SHUFFLE/DELTA/DELTA_VARINT)
$srcFiles += "src/hiss_transform.cpp"
# HISS writer (XISF 式 Header + attachments 单体容器)
$srcFiles += "src/hiss_writer.cpp"
# HISS 流式写入器 (步骤10: 临时子块池管理, 原子替换)
$srcFiles += "src/hiss_stream_writer.cpp"
# HISS reader (按目录读取, 按需加载 Tile)
$srcFiles += "src/hiss_reader.cpp"

if ($buildConfig.enable_fits)       { $srcFiles += "src/aio_fits.cpp" }
if ($buildConfig.enable_xisf)       { $srcFiles += "src/aio_xisf.cpp" }
if ($buildConfig.enable_compressor) { $srcFiles += "src/aio_compressor.cpp" }
if ($buildConfig.enable_pipeline)   {
    $srcFiles += "src/aio_pipeline.cpp"
    $srcFiles += "src/aio_pipeline_engine.cpp"
}
if ($buildConfig.enable_ahpx)       {
    $srcFiles += "src/ahpx/aio_ahpx_api.cpp"
    $srcFiles += "src/ahpx/aio_ahpx_reader.cpp"
    $srcFiles += "src/ahpx/aio_ahpx_writer.cpp"
}
if ($buildConfig.enable_healpix)    {
    $srcFiles += "src/healpix/aio_healpix_io.cpp"
}

Write-Host ""
Write-Host "Source files ($($srcFiles.Count)):"
$srcFiles | ForEach-Object { Write-Host "  $_" }

# ============================================================================
# 4. Build linker library list
# ============================================================================
$libs = @("-lm")
if ($buildConfig.enable_zstd)       { $libs += "-lzstd" }
if ($buildConfig.enable_lz4)        { $libs += "-llz4" }

# ============================================================================
# 5. Compile
# ============================================================================
$outputDll = "astro_image_io.dll"
Write-Host ""
Write-Host "Output: $outputDll"
Write-Host ""

# Build compile args via += (避免 @() 数组字面量中 $outputDll 被解析为 null 的问题)
# -I../common/include: PrecisionContext / AstroScalarType 双精度 ABI 头文件
$cmdArgs = @()
$cmdArgs += "-O2"
$cmdArgs += "-std=c++17"
$cmdArgs += "-Wall"
$cmdArgs += "-fopenmp"
$cmdArgs += "-shared"
$cmdArgs += "-o"
$cmdArgs += $outputDll
$cmdArgs += "-Iinclude"
$cmdArgs += "-Isrc"
$cmdArgs += "-I../common/include"
foreach ($d in $defines) { $cmdArgs += $d }
foreach ($s in $srcFiles) { $cmdArgs += $s }
$cmdArgs += "-static-libgcc"
$cmdArgs += "-static-libstdc++"
foreach ($l in $libs) { $cmdArgs += $l }

# 用 [string]::Join 生成参数字符串 (避免 PowerShell splatting/子表达式解析问题)
$argString = [string]::Join(' ', $cmdArgs)

Write-Host "Compile command:"
Write-Host "  g++ $argString"
Write-Host ""

# 用 & 运算符直接调用 g++ (避免 Start-Process 的 stdout/stderr 死锁)
# PowerShell 会自动将 $cmdArgs 数组展开为单独的参数传递给 g++
$output = & $gpp $cmdArgs 2>&1
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

    $aioSymbols = $exports | Where-Object { $_ -match 'aio_' }
    Write-Host "  aio_* exported symbols: $($aioSymbols.Count)"
    $aioSymbols | ForEach-Object {
        $sym = ($_ -split '\s+')[-1]
        Write-Host "    $sym" -ForegroundColor Green
    }

    if ($buildConfig.enable_healpix) {
        $hioSymbols = $exports | Where-Object { $_ -match 'aio_hiss_|aio_hcsd_|aio_hio_' }
        Write-Host ""
        Write-Host "  HEALPix I/O symbols (aio_hiss_/aio_hcsd_/aio_hio_): $($hioSymbols.Count)" -ForegroundColor Cyan
        $hioSymbols | ForEach-Object {
            $sym = ($_ -split '\s+')[-1]
            Write-Host "    $sym" -ForegroundColor Green
        }
    }
} else {
    Write-Host ""
    Write-Host "Build FAILED! Exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}
