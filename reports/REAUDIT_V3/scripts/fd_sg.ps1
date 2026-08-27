$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$env:PATH = 'C:/msys64/mingw64/bin;' + (Join-Path $root 'lib/phase2/build') + ';' + $env:PATH
$exe = Join-Path $root 'lib/phase2/build/phase2_synthetic_gate.exe'
Set-Location -LiteralPath (Join-Path $root 'lib/phase2/build')
& $exe 2>&1 | Select-Object -Last 12
Write-Output ('EXIT=' + $LASTEXITCODE)
