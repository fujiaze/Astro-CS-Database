$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$bin = Join-Path $root 'lib/acr/build2/bin'
$p2 = Join-Path $root 'lib/phase2/build'
$bridge = Join-Path $root 'lib/acr/backends/cuda/bridge'
$cudabin = 'C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin'
$env:PATH = $p2 + ';' + $bin + ';' + $bridge + ';' + $cudabin + ';C:/msys64/mingw64/bin;' + $env:PATH
Set-Location -LiteralPath $bin
Write-Output '=== acr_test_cuda_bridge.exe ===';
& (Join-Path $bin 'acr_test_cuda_bridge.exe') 2>&1 | Select-Object -Last 12
Write-Output ('EXIT=' + $LASTEXITCODE)
