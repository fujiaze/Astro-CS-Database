$ErrorActionPreference = 'Stop'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== stage2 / orchestrator / drizzle exes ===';
Get-ChildItem -LiteralPath $root -Recurse -Include *stage2*.exe,*orchestr*.exe,*drizzle*.exe,*phase2*.exe -ErrorAction SilentlyContinue | Select-Object -First 20 -ExpandProperty FullName
Write-Output '=== run ACR cuda_bridge test ===';
$exe = Join-Path $root 'lib/acr/build2/bin/acr_test_cuda_bridge.exe'
if (Test-Path -LiteralPath $exe) {
  $out = & $exe 2>&1 | Select-Object -Last 15
  $out
  Write-Output ('ACR_CUDA_TEST_EXIT=' + $LASTEXITCODE)
} else { Write-Output 'cuda bridge test exe missing' }
