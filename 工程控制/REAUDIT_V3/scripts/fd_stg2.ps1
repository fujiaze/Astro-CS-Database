$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2 = Join-Path $root 'lib/phase2/build'
Write-Output '=== phase2/build dlls + exes ===';
Get-ChildItem -LiteralPath $p2 | Select-Object Name, Length | Format-Table -AutoSize | Out-String -Width 120
Write-Output '=== try stage2 with DLL dir on PATH ===';
$env:PATH = $p2 + ';' + $env:PATH
Set-Location -LiteralPath $p2
& (Join-Path $p2 'astrocs-stage2.exe') 2>&1 | Select-Object -First 6
Write-Output ('EXIT=' + $LASTEXITCODE)
