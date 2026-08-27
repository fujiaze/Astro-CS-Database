$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$bin = Join-Path $root 'lib/acr/build2/bin'
$lib = Join-Path $root 'lib/acr/build2/lib'
Write-Output ('bin DLLs: ' + ((Get-ChildItem -LiteralPath $bin -Filter *.dll -ErrorAction SilentlyContinue).Count))
Write-Output ('lib DLLs: ' + ((Get-ChildItem -LiteralPath $lib -Filter *.dll -ErrorAction SilentlyContinue).Count))
$env:PATH = $bin + ';' + $lib + ';' + $env:PATH
Set-Location -LiteralPath $bin
Write-Output '=== run acr_test_cuda_bridge.exe with PATH ===';
& (Join-Path $bin 'acr_test_cuda_bridge.exe') 2>&1 | Select-Object -Last 12
Write-Output ('EXIT=' + $LASTEXITCODE)
