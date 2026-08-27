$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2 = Join-Path $root 'lib/phase2/build'
$mingw = Join-Path $root 'lib/healpix_db/archive/healpix_browser_cpp'
Write-Output '=== mingw runtime dll versions present ===';
Get-ChildItem -LiteralPath $mingw -Filter lib*.dll | Select-Object Name, Length
$env:PATH = $p2 + ';' + $mingw + ';' + $env:PATH
Set-Location -LiteralPath $p2
Write-Output '=== stage2 no-args (usage expected) ===';
& (Join-Path $p2 'astrocs-stage2.exe') 2>&1 | Select-Object -First 6
Write-Output ('EXIT=' + $LASTEXITCODE)
