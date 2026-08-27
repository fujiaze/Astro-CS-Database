$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2build = Join-Path $root 'lib/phase2/build'
$orchex = Join-Path $root 'lib/orchestrator/cpp/orchestrator.exe'
Set-Location -LiteralPath $p2build
Write-Output '=== astrocs-stage2.exe (no args -> usage) ===';
& (Join-Path $p2build 'astrocs-stage2.exe') 2>&1 | Select-Object -First 5
Write-Output ('STAGE2_EXIT=' + $LASTEXITCODE)
Write-Output '=== orchestrator.exe --help ===';
if (Test-Path -LiteralPath $orchex) {
  & $orchex --help 2>&1 | Select-Object -First 5
  Write-Output ('ORCH_EXIT=' + $LASTEXITCODE)
}
