$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== CUDA present on Fatduck? ===';
$cuda = $env:CUDA_PATH; Write-Output ('CUDA_PATH=' + $cuda)
if (Test-Path 'C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA') { Write-Output 'CUDA toolkit dir exists' } else { Write-Output 'no CUDA toolkit dir' }
$nvcc = Get-Command nvcc -ErrorAction SilentlyContinue; Write-Output ('nvcc=' + ($nvcc -ne $null))
Write-Output '=== ACR build2 layout ===';
Get-ChildItem -LiteralPath (Join-Path $root 'lib/acr/build2') -Name | Select-Object -First 10
$bin = Join-Path $root 'lib/acr/build2/bin'
Get-ChildItem -LiteralPath $bin -Include *.dll -Recurse -ErrorAction SilentlyContinue | Select-Object -First 10 -ExpandProperty FullName
