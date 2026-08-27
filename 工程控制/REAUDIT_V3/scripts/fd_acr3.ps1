$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$bin = Join-Path $root 'lib/acr/build2/bin'
$bridge = Join-Path $root 'lib/acr/backends/cuda/bridge/acr_cuda_bridge.dll'
$p2build = Join-Path $root 'lib/phase2/build'
Write-Output '=== copy bridge dll next to test exe ===';
Copy-Item -LiteralPath $bridge -Destination (Join-Path $bin 'acr_cuda_bridge.dll') -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $bridge -Destination $p2build -Force -ErrorAction SilentlyContinue
Write-Output '=== phase2/build dlls (astrocs-stage2 needs acr_cuda_bridge too?) ===';
Get-ChildItem -LiteralPath $p2build -Filter *.dll -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
Set-Location -LiteralPath $bin
Write-Output '=== run acr_test_cuda_bridge.exe ===';
& (Join-Path $bin 'acr_test_cuda_bridge.exe') 2>&1 | Select-Object -Last 12
Write-Output ('EXIT=' + $LASTEXITCODE)
