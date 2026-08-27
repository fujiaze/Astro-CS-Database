$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$orch = Join-Path $root 'lib/orchestrator/cpp/orchestrator.exe'
$cfg = Join-Path $root 'lib/orchestrator/configs/stage1_gc_panel1_Red.json'
$env:PATH = 'C:/msys64/mingw64/bin;' + $env:PATH
Write-Output '=== orchestrator --version ===';
& $orch --version 2>&1 | Select-Object -First 4
Write-Output ('VER_EXIT=' + $LASTEXITCODE)
Write-Output '=== orchestrator --validate <stage1_gc_panel1_Red.json> ===';
& $orch --validate $cfg 2>&1 | Select-Object -First 10
Write-Output ('VAL_EXIT=' + $LASTEXITCODE)
