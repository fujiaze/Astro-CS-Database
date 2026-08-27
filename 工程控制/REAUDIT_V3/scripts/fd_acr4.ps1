$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$bin = Join-Path $root 'lib/acr/build2/bin'
$p2build = Join-Path $root 'lib/phase2/build'
$tbb = Get-ChildItem -LiteralPath (Join-Path $root 'lib/acr/build2/_deps') -Recurse -Include *.dll -ErrorAction SilentlyContinue | Select-Object -First 5
Write-Output '=== tbb dlls ==='; $tbb | Select-Object -ExpandProperty FullName
$cudabin = 'C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin'
$env:PATH = $bin + ';' + $p2build + ';' + $cudabin + ';' + (Join-Path $root 'lib/acr/backends/cuda/bridge') + ';' + $env:PATH
Set-Location -LiteralPath $p2build
Write-Output '=== run from phase2/build with full PATH ===';
& (Join-Path $bin 'acr_test_cuda_bridge.exe') 2>&1 | Select-Object -Last 12
Write-Output ('EXIT=' + $LASTEXITCODE)
